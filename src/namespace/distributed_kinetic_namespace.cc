#include <algorithm>
#include <future>
#include "distributed_kinetic_namespace.h"
#include "debug.h"

using com::seagate::kinetic::proto::Message_Algorithm_SHA1;

static const string cv_base_name = "clusterversion_";
static const string logkey_prefix = "log_";

DistributedKineticNamespace::DistributedKineticNamespace(const std::vector< posixok::Partition > &cmap, const posixok::Partition &lpart):
        failure_lock(), log_partition(lpart), cluster_map(cmap), connection_factory(kinetic::NewKineticConnectionFactory())
{
    if(selfCheck() == false)
        throw std::runtime_error("Invalid Clustermap");
    updateCapacityEstimate();
}

DistributedKineticNamespace::~DistributedKineticNamespace()
{
}


posixok::Partition & DistributedKineticNamespace::keyToPartition(const std::string &key)
{
    size_t hashvalue = std::hash<std::string>()(key.substr(0,key.find_first_of("|")));
    int index = hashvalue % cluster_map.size();
    return cluster_map[index];
}

std::shared_ptr<kinetic::ConnectionHandle>  DistributedKineticNamespace::driveToConnection(const posixok::KineticDrive &drive)
{
    if(connection_map.count(drive)) return connection_map[drive];

    std::unique_ptr<kinetic::ConnectionHandle> con;
    kinetic::ConnectionOptions options;
    options.host = drive.host();
    options.port = drive.port();
    options.user_id = 1;
    options.hmac_key = "asdfasdf";

    if( connection_factory.NewThreadsafeConnection(options, 5, con).ok() ){
        connection_map[drive] = std::shared_ptr<kinetic::ConnectionHandle>(con.release());
        return connection_map[drive];
    }

    if(drive.status() != posixok::KineticDrive_Status_RED)
        pok_warning("Failed connecting to drive @ %s:%d.",drive.host().c_str(),drive.port());
    return std::shared_ptr<kinetic::ConnectionHandle>();
}


bool DistributedKineticNamespace::testConnection(const posixok::KineticDrive &drive)
{
    std::unique_ptr<kinetic::DriveLog> unused;
    auto con = driveToConnection(drive);
    if( !con || !(con->blocking().GetLog(unused)).ok()){
        pok_debug("Failed getLog on drive %s:%d",drive.host().data(),drive.port());
        return false;
    }
    return true;
}


bool DistributedKineticNamespace::updateCapacityEstimate()
{
    std::vector<std::future<kinetic::Capacity>> futures;

    for(auto &p : cluster_map){
        for(auto &d: p.drives()){
            if(d.status() == posixok::KineticDrive_Status_GREEN){
                futures.push_back( std::async( [&](){
                    std::shared_ptr<kinetic::ConnectionHandle>  con = driveToConnection(d);
                    std::unique_ptr<kinetic::DriveLog> dlog;
                    kinetic::Capacity c;
                    if( con && (con->blocking().GetLog(dlog)).ok() )
                        c = dlog->capacity;
                    return c;
                    }));
                break;
            }
        }
    }

    kinetic::Capacity cap = {0.0, 0.0};
    for(auto &f : futures){
        kinetic::Capacity c = f.get();
        if(!c.total_bytes)
            return false;
        cap.total_bytes     += c.total_bytes;
        cap.remaining_bytes += c.remaining_bytes;
    }
    capacity_estimate = cap;
    return true;
}

bool DistributedKineticNamespace::selfCheck()
{
    pok_debug("Checking Clustermap: ");
    printClusterMap();

    auto check = [&](posixok::Partition &p){

        for(int i=0; i<p.drives_size(); i++)
               if(p.drives(i).status() == posixok::KineticDrive_Status_RED)
                   enableDrive(p, i);

           if (testPartition(p)) return true;
           if (getPartitionUpdate(p) && testPartition(p)) return true;
           return false;
    };


    for(auto &p : cluster_map)
        if(check(p) == false) return false;
    return check(log_partition);
}


/* Try to get a higher-version partition from any of it's drives. */
bool DistributedKineticNamespace::getPartitionUpdate(posixok::Partition & p)
{
    if(p.has_partitionid() == false) return false;
    std::lock_guard<std::recursive_mutex> l(failure_lock);

    for(auto &d : p.drives()){
        if(d.status() == posixok::KineticDrive_Status_RED) continue;
        auto con = driveToConnection(d);
        if( !con) continue;

        std::unique_ptr<KineticRecord> record;
        std::int64_t cversion = p.cluster_version() - 1;
        KineticStatus status(kinetic::StatusCode::OK, "");

        do{
            cversion++;
            con->blocking().SetClientClusterVersion(cversion);
            status = con->blocking().Get(cv_base_name+std::to_string(cversion), record);
            pok_debug("trying cluster version %d for drive %s:%d",cversion,d.host().c_str(),d.port());
        }while(status.statusCode() == kinetic::StatusCode::REMOTE_CLUSTER_VERSION_MISMATCH);

        if(status.ok()){
           p.ParseFromString(*record->value());
           pok_debug("Updated partition to cluster version %d from drive %s:%d.",p.cluster_version(),d.host().data(),d.port());
           return true;
        }
    }
    return false;
}

bool DistributedKineticNamespace::putPartitionUpdate(posixok::Partition &p)
{
    if(p.has_partitionid() == false) return true;
    std::lock_guard<std::recursive_mutex> l(failure_lock);

    std::string   key      = cv_base_name + std::to_string(p.cluster_version());
    KineticRecord record(p.SerializeAsString(), "yes", "", Message_Algorithm_SHA1);

     /* Serialize write accesses to ensure that multiple clients attempting to update the same cluster version don't conflict.
      * The first write may thus fail in case of concurrency. */
     for(int i=0; i < p.drives_size(); i++){
         if(p.drives(i).status() == posixok::KineticDrive_Status_RED) continue;

         std::shared_ptr<kinetic::ConnectionHandle> con = driveToConnection(p.drives(i));
         KineticStatus status = KineticStatus(kinetic::StatusCode::REMOTE_REMOTE_CONNECTION_ERROR, "connection error");
         if(con)       status = con->blocking().Put(key, "" , WriteMode::REQUIRE_SAME_VERSION, record);
         if(!status.ok()){
             pok_warning("Failure.");
             return false;
         }
         con->blocking().SetClusterVersion(p.cluster_version());
         con->blocking().SetClientClusterVersion(p.cluster_version());
     }
     pok_debug("Put Partition successful: ");
     printPartition(p);
     return true;
}

bool DistributedKineticNamespace::testPartition(const posixok::Partition &p)
{
    if(p.has_partitionid() == false) return true;
    /* count various states existing in the partition and test connections */
    int green=0; int yellow=0;
    for(auto &d : p.drives()){
       if(d.status() == d.RED)    continue;
       if(d.status() == d.GREEN)  green++;
       if(d.status() == d.YELLOW) yellow++;

       if(testConnection(d) == false)
           return false;
    }
    if(p.has_logid())
        if(testConnection(log_partition.drives(p.logid())) == false)
            return false;

    if(green == 0){
        pok_error("Not a single GREEN drive available. Partition has empty read quorum. ");
        return false;
    }
    if(green+yellow == 1 && p.has_logid() == false){
        pok_error("Only one drive reachable & no logdrive set for the partition. At least two drives "
                  "(including logdrive) have to be reachable at all times to detect a network split. ");
        return false;
    }

    return true;
}


bool DistributedKineticNamespace::disableDrive(posixok::Partition &p, int index)
{
    std::lock_guard<std::recursive_mutex> l(failure_lock);
    if(p.drives(index).status() == posixok::KineticDrive_Status_RED) return true;

    /* Logdrive add-rule: Only add a new logdrive if none is set yet and all lights show green. */
    if(p.has_logid() == false && std::all_of(p.drives().begin(), p.drives().end(), [](const posixok::KineticDrive &d){return d.status() == d.GREEN;})
                              && std::any_of(log_partition.drives().begin(), log_partition.drives().end(), [](const posixok::KineticDrive &d){return d.status() != d.RED;})){
        std::uniform_int_distribution<int> ldist(0, log_partition.drives_size()-1);
        int lid;
        do{ lid = ldist(random_generator); } while( log_partition.drives(lid).status() == posixok::KineticDrive_Status_RED );
        p.set_logid(lid);
    }

     p.mutable_drives(index)->set_status(posixok::KineticDrive_Status_RED);
     p.set_cluster_version( p.cluster_version()+1 );
     if(testPartition(p)){
        if(putPartitionUpdate(p)){
            connection_map.erase(p.drives(index));
            return true;
        }
        if(getPartitionUpdate(p))
            return disableDrive(p, index);
    }
    pok_warning("Unexpected error code while failing a drive. Probably encountering multiple concurrent failures or network split.");
    return false;
}



bool DistributedKineticNamespace::enableDrive(posixok::Partition &p, int index)
{
    std::lock_guard<std::recursive_mutex> l(failure_lock);
    if(p.drives(index).status() != posixok::KineticDrive_Status_RED) return true;

    std::shared_ptr<kinetic::ConnectionHandle>  con = driveToConnection(p.drives(index));
    if(!con) return false;

    std::unique_ptr<kinetic::DriveLog> unused;
    std::int64_t cversion = -1;
    KineticStatus status(kinetic::StatusCode::OK, "");
    do{
        cversion++;
        con->blocking().SetClientClusterVersion(cversion);
        status = con->blocking().GetLog(unused);
    }while(status.statusCode() == kinetic::StatusCode::REMOTE_CLUSTER_VERSION_MISMATCH);
    if(status.ok() == false){
        return false;
    }

    p.mutable_drives(index)->set_status(posixok::KineticDrive_Status_YELLOW);
    p.set_cluster_version(p.cluster_version()+1);
    if(testPartition(p)){
      if(putPartitionUpdate(p)){
          std::thread( [&](){ synchronizeDrive(p, index);}).detach();
          return true;
      }
      if(getPartitionUpdate(p))
          return enableDrive(p, index);
    }
    p.mutable_drives(index)->set_status(posixok::KineticDrive_Status_RED);
    p.set_cluster_version(p.cluster_version()-1);
    return false;
}

/*
 * If a logdrive is available, use it to get a list of keys that are possibly out of date.
 * Otherwise, use a drive of the partition with GREEN status to check every single key. */
bool DistributedKineticNamespace::synchronizeDrive(posixok::Partition &p, int index)
{
    if(p.drives(index).status() != posixok::KineticDrive_Status_YELLOW) return false;
    unsigned int maxsize = 100;
    unique_ptr<vector<std::string>> keys(new vector<string>());
    unique_ptr<KineticRecord> record;
    string keystart = " ";
    string keyend   = "|";
    std::shared_ptr<kinetic::ConnectionHandle> con;
    std::string prefix = std::to_string(p.partitionid()) + logkey_prefix;

    if(p.has_logid()){
        keystart.insert(0,prefix);
        keyend.insert(0,prefix);
        con = driveToConnection(log_partition.drives(p.logid()));
    }
    else{
        std::uniform_int_distribution<int> dist(0, p.drives_size()-1);
        int index;
        do{ index = dist(random_generator); }
        while(p.drives(index).status() != posixok::KineticDrive_Status_GREEN);
        con = driveToConnection(p.drives(index));
    }
    if(!con) return false;

    pok_trace("Synchronizing drive %s:%d from %s",
            p.drives(index).host().data(),p.drives(index).port(),p.has_logid() ? "logdrive":"scratch... complete rebuild is required");

    do{
         if (keys->size())
             keystart = keys->back();
         keys->clear();
         if( con->blocking().GetKeyRange(keystart,true,keyend,true,false,maxsize,keys).ok() == false)
             return false;
         pok_debug("obtained %d keys that might need to be repaired",keys->size());

         for (auto& element : *keys) {
             if(p.has_logid()) element.erase(0, prefix.length());
             if(readRepair(element,record).ok() == false){
                 pok_warning(" Error encountered while repairing key %s. Aborting drive synchronization.",element.data());
                 return false;
             }
             if(p.has_logid()){
                 element.insert(0, prefix);
                 con->blocking().Delete(element,"",kinetic::WriteMode::IGNORE_VERSION);
             }
         }
     } while (keys->size() == maxsize);

     pok_trace("drive completely synchronized.");

     std::lock_guard<std::recursive_mutex> l(failure_lock);
     p.mutable_drives(index)->set_status(posixok::KineticDrive_Status_GREEN);
     p.set_cluster_version(p.cluster_version()+1);
     if(p.has_logid() && std::all_of(p.drives().begin(), p.drives().end(), [](const posixok::KineticDrive &d){return d.status() == d.GREEN;}))
         p.clear_logid();
     if(putPartitionUpdate(p)){
         return true;
     }
     p.mutable_drives(index)->set_status(posixok::KineticDrive_Status_YELLOW);
     p.set_cluster_version(p.cluster_version()-1);
     pok_warning("Failed updating partition after successfully synchronizing drive.");
     return false;
}


KineticStatus DistributedKineticNamespace::readRepair(const string &key, std::unique_ptr<KineticRecord> &record)
{
    posixok::Partition &p = keyToPartition(key);

    /* Get reference drive & record */
    std::shared_ptr<kinetic::ConnectionHandle> con;
    for(int i=0; i<p.drives_size(); i++){
        if(p.drives(i).status() == posixok::KineticDrive_Status_GREEN){
            con = driveToConnection(p.drives(i));
            break;
        }
    }
    KineticStatus status = con->blocking().Get(key,record);
    if(!status.ok() && status.statusCode() != kinetic::StatusCode::REMOTE_NOT_FOUND)
        return status;

    for(int i = 0; i<p.drives_size(); i++){
          if(p.drives(i).status() == posixok::KineticDrive_Status_RED) continue;

          std::unique_ptr<std::string> version(new std::string(""));
          con = driveToConnection(p.drives(i));
          status = con->blocking().GetVersion(key, version);
          if(!status.ok() && status.statusCode() != kinetic::StatusCode::REMOTE_NOT_FOUND)
              return status;

          /* Copy reference record (or delete record). */
          if(!record && status.ok()){
              status = con->blocking().Delete(key, *version, WriteMode::REQUIRE_SAME_VERSION);
              pok_debug("Removing key %s from drive %s:%d returned %s",
                      key.c_str(),p.drives(i).host().c_str(), p.drives(i).port(), status.message().c_str());
          }
          else if (record){
              if(status.ok() && *version == *record->version())
                  continue;
              status = con->blocking().Put(key, *version, WriteMode::REQUIRE_SAME_VERSION, *record);
              pok_debug("Copying key %s to drive %s:%d returned %s",
                      key.c_str(),p.drives(i).host().c_str(), p.drives(i).port(), status.message().c_str());
          }

         /* Note that up to #nodes clients could be trying to resolve a partial write to the same key at the same time. Expect
          *  some interference accordingly... remote_version_mismatch errors at this point just means that some other client
          *  did our work for us. */
          if(status.statusCode() != kinetic::StatusCode::OK &&
             status.statusCode() != kinetic::StatusCode::REMOTE_NOT_FOUND &&
             status.statusCode() != kinetic::StatusCode::REMOTE_VERSION_MISMATCH)
              return status;
      }
    return KineticStatus(kinetic::StatusCode::OK, "repaired");
}



KineticStatus DistributedKineticNamespace::evaluateWriteOperation( posixok::Partition &p,  std::vector<KineticStatus> &results )
{
    /* B) the same result for all threads -> no replication specific error handling required. */
    bool allequal=true; int green = 0;
    while(p.drives(green).status() != posixok::KineticDrive_Status_GREEN) green++;
    for(int i=0; i<p.drives_size(); i++){
          if(p.drives(i).status() == posixok::KineticDrive_Status_RED) continue;
          if( results.at(i).statusCode() != results.at(green).statusCode() ) allequal = false;
    }
    if(allequal) return results.at(green);

    /* C) Any kind of unexpected I/O error -> retry operation if drive in question can be failed successfully. */
    for(int i=0; i<p.drives_size(); i++){
        if(p.drives(i).status() == posixok::KineticDrive_Status_RED) continue;

        if( results.at(i).statusCode() != kinetic::StatusCode::OK &&
            results.at(i).statusCode() != kinetic::StatusCode::REMOTE_VERSION_MISMATCH &&
            results.at(i).statusCode() != kinetic::StatusCode::REMOTE_NOT_AUTHORIZED ){
            if(disableDrive(p, i))
                return evaluateWriteOperation(p, results);
            pok_error("Couldn't fail drive after write error");
            return results.at(i);
        }
    }

    /* D) ... and that only leaves a partial write ( version missmatch due to client crash or concurrent writes ) */
    return KineticStatus(kinetic::StatusCode::REMOTE_OTHER_ERROR, "partial write");

}


KineticStatus DistributedKineticNamespace::writeOperation (const string &key, std::function< KineticStatus(kinetic::BlockingKineticConnection&) > operation)
{
    posixok::Partition &p = keyToPartition(key);
    std::vector<std::future<KineticStatus>> futures;
    std::vector<KineticStatus> results;
    for(auto d : p.drives()){
       futures.push_back( std::async( [this, d, operation](){
                       if(d.status() == d.RED) return KineticStatus(kinetic::StatusCode::REMOTE_REMOTE_CONNECTION_ERROR, "Unreachable");
                       std::shared_ptr<kinetic::ConnectionHandle> con = driveToConnection(d);
                       return operation( con->blocking() );
               }));
    }

    /* Try to log operation if at least one drive is missing from the write quorum. */
    if(p.has_logid() && std::any_of(p.drives().begin(), p.drives().end(), [](const posixok::KineticDrive &d){return d.status() == posixok::KineticDrive_Status_RED;})){

       std::shared_ptr<kinetic::ConnectionHandle> con = driveToConnection(log_partition.drives(p.logid()));

       kinetic::KineticRecord record("", "", "", Message_Algorithm_SHA1);
       auto status = con->blocking().Put(std::to_string(p.partitionid())+logkey_prefix+key,"",WriteMode::IGNORE_VERSION, record);

       if(status.ok() == false){
           disableDrive(log_partition, p.logid());
           std::lock_guard<std::recursive_mutex> l(failure_lock);

           p.clear_logid();
           p.set_cluster_version(p.cluster_version()+1);
           if(putPartitionUpdate(p) == false)
               pok_warning("Do not use logs to rebuild currently failed drives.");
       }
    }

    for(auto &f : futures) results.push_back(f.get());

    /* A) cluster version mismatch -> retry operation if partition can be updated to new cluster version successfully */
    for(auto &r : results){
     if( r.statusCode() == kinetic::StatusCode::REMOTE_CLUSTER_VERSION_MISMATCH){
         if(getPartitionUpdate(p)) return writeOperation(key, operation);
         pok_error("Non-resolvable cluster version mismatch");
         return r;
     }
    }
    return evaluateWriteOperation(p, results);
}

KineticStatus DistributedKineticNamespace::readOperation (const string &key, std::function< KineticStatus(kinetic::BlockingKineticConnection&) > operation)
{
    /* Step 1) Pick a drive, obtain connection, execute operation */
    posixok::Partition &p = keyToPartition(key);
    std::uniform_int_distribution<int> dist(0, p.drives_size()-1);
    int index;
    do{ index = dist(random_generator); }
    while(p.drives(index).status() != posixok::KineticDrive_Status_GREEN);

    std::shared_ptr<kinetic::ConnectionHandle> con = driveToConnection(p.drives(index));
    KineticStatus status = KineticStatus(kinetic::StatusCode::REMOTE_REMOTE_CONNECTION_ERROR, "");
    if(con)       status = operation(con->blocking());

    /* Step 2) Evaluate the results. */
    if(status.statusCode() == kinetic::StatusCode::REMOTE_CLUSTER_VERSION_MISMATCH){
        if(getPartitionUpdate(p))
            return readOperation(key, operation);
        pok_debug("Failed partition update after a cluster_version_mismatch for drive %s:%d. Returning %s.",
                p.drives(index).host().data(),p.drives(index).port(),status.message().data());
        return status;
    }

    if(     status.statusCode() != kinetic::StatusCode::OK &&
            status.statusCode() != kinetic::StatusCode::REMOTE_NOT_FOUND &&
            status.statusCode() != kinetic::StatusCode::REMOTE_NOT_AUTHORIZED ){
        if(disableDrive(p, index))
            return readOperation(key, operation);
        pok_debug("Didn't fail drive %s:%d successfully after encountering status %s.",p.drives(index).host().data(),p.drives(index).port(),status.message().data());
        return status;
    }

    return status;
}


KineticStatus DistributedKineticNamespace::Put(const string &key, const string &current_version, WriteMode mode, const KineticRecord& record)
{
    KineticStatus result = writeOperation(key,
            [&](kinetic::BlockingKineticConnection&b){return b.Put(std::cref(key), std::cref(current_version), mode, std::cref(record));}
    );

    if(result.statusCode() == kinetic::StatusCode::REMOTE_OTHER_ERROR){
       std::unique_ptr<KineticRecord> repair_record;
       result = readRepair(key,repair_record);

       if(result.statusCode() == kinetic::StatusCode::REMOTE_CLUSTER_VERSION_MISMATCH){
           if(getPartitionUpdate(keyToPartition(key)))
               result = readRepair(key,repair_record);
       }

       if(result.ok()){
           if(repair_record && (*record.version() == *repair_record->version()))
               result = KineticStatus( kinetic::StatusCode::OK, "");
           else
               result = KineticStatus( kinetic::StatusCode::REMOTE_VERSION_MISMATCH, "version mismatch");
       }
    }

    if(result.ok() && current_version.empty()) capacity_estimate.remaining_bytes -= 1024*1024;
    return result;
}

KineticStatus DistributedKineticNamespace::Delete(const string &key, const string& version, WriteMode mode)
{
    KineticStatus result = writeOperation(key,
            [&](kinetic::BlockingKineticConnection&b){return b.Delete(std::cref(key), std::cref(version), mode);}
    );

    if(result.statusCode() == kinetic::StatusCode::REMOTE_OTHER_ERROR){
        std::unique_ptr<KineticRecord> repair_record;
        result = readRepair(key,repair_record);

        if(result.statusCode() == kinetic::StatusCode::REMOTE_CLUSTER_VERSION_MISMATCH)
            if(getPartitionUpdate(keyToPartition(key)))
                result = readRepair(key,repair_record);

        if(result.ok()){
            if(repair_record)
                result = KineticStatus( kinetic::StatusCode::REMOTE_VERSION_MISMATCH, "version mismatch");
            else
                result = KineticStatus( kinetic::StatusCode::OK, "");
        }
    }

    if(result.ok()) capacity_estimate.remaining_bytes += 1024*1024;
    return result;
}

KineticStatus DistributedKineticNamespace::Get(const string &key, unique_ptr<KineticRecord>& record)
{
    return readOperation(key,
            [&](kinetic::BlockingKineticConnection&b){return b.Get(std::cref(key), std::ref(record));}
    );
}

KineticStatus DistributedKineticNamespace::GetVersion(const string &key, unique_ptr<string>& version)
{
    return readOperation(key,
            [&](kinetic::BlockingKineticConnection&b){return b.GetVersion(std::cref(key), std::ref(version));}
    );
}


KineticStatus DistributedKineticNamespace::GetKeyRange(const string &start_key, const string &end_key, unsigned int max_results, unique_ptr<vector<string>> &keys)
{
    const bool start_key_inclusive = false;
    const bool end_key_inclusive = false;
    const bool reverse_results = false;

    return readOperation(start_key,
               [&](kinetic::BlockingKineticConnection&b){return b.GetKeyRange(
                       start_key, start_key_inclusive, end_key, end_key_inclusive, reverse_results, max_results, keys);}
       );
}

KineticStatus DistributedKineticNamespace::Capacity(kinetic::Capacity &cap)
{
    cap = capacity_estimate;
    return KineticStatus(kinetic::StatusCode::OK, "");
}


void DistributedKineticNamespace::printPartition(const posixok::Partition &p)
{
    auto statusToString = [](posixok::KineticDrive_Status s) -> std::string {
           switch(s) {
               case posixok::KineticDrive_Status_GREEN:    return "GREEN";
               case posixok::KineticDrive_Status_YELLOW:   return "YELLOW";
               case posixok::KineticDrive_Status_RED:      return "RED";
           }
           return "INVALID";
       };
       std::cout << "[----------------------------------------]" << std::endl;
       if(p.has_partitionid())
           std::cout << "Partition #" << p.partitionid() << " ClusterVersion " << p.cluster_version() << std::endl;
       for (auto &d : p.drives())
           std::cout << "\t" << d.host() << ":" << d.port() << " - " << statusToString(d.status()) << std::endl;
       if(p.has_logid())
           std::cout << "\t LOGDRIVE #" << p.logid() << " - " << statusToString(log_partition.drives(p.logid()).status()) << std::endl;
       std::cout << "[----------------------------------------]" << std::endl;

}

void DistributedKineticNamespace::printClusterMap()
{
    for (size_t i=0; i<cluster_map.size(); i++ ){
        auto &p = cluster_map[i];
        printPartition(p);
    }
    std::cout << "LOG_PARTITION:" << std::endl;
    printPartition(log_partition);
}

