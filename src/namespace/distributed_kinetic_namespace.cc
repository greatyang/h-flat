#include <algorithm>
#include <future>
#include "distributed_kinetic_namespace.h"
#include "debug.h"

static const string cv_base_name = "clusterversion_";
static const string logkey_prefix = "log_";

DistributedKineticNamespace::DistributedKineticNamespace(const std::vector< posixok::Partition > &cmap):
        cluster_map(cmap), connection_factory(kinetic::NewKineticConnectionFactory())
{
    if(selfCheck() == false)
        throw std::runtime_error("Invalid Clustermap");
    updateCapacityEstimate();
    printClusterMap();
}

DistributedKineticNamespace::~DistributedKineticNamespace()
{
}


posixok::Partition & DistributedKineticNamespace::keyToPartition(const std::string &key)
{
    size_t hashvalue = std::hash<std::string>()(key.substr(0,key.find_first_of("|")));
    int partition_num = hashvalue % cluster_map.size();
    pok_debug("Hashed key %s to partition #%d",key.c_str(),partition_num);
    return cluster_map[partition_num];
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
    for(auto &p : cluster_map){

        for(int i=0; i<p.drives_size(); i++)
            if(p.drives(i).status() == posixok::KineticDrive_Status_RED)
                enableDrive(p, i);

        if (validatePartition(p)) continue;
        if (getPartitionUpdate(p) && validatePartition(p)) continue;
        return false;
    }
    return true;
}


/* Try to get a higher-version partition from any of it's drives. */
bool DistributedKineticNamespace::getPartitionUpdate(posixok::Partition & p)
{
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
           printClusterMap();
           return true;
        }
    }
    return false;
}

bool DistributedKineticNamespace::putPartitionUpdate(posixok::Partition &p)
{
    p.set_cluster_version( p.cluster_version() + 1);
    std::string   key      = cv_base_name + std::to_string(p.cluster_version());
    KineticRecord record(p.SerializeAsString(), "yes", "", com::seagate::kinetic::proto::Message_Algorithm_SHA1);
    bool first_write = true;

     /* Serialize write accesses to ensure that multiple clients attempting to update the same cluster version don't conflict.
      * The first write may thus fail in case of concurrency. */
     for(int i=0; i < p.drives_size(); i++){
         if(p.drives(i).status() == posixok::KineticDrive_Status_RED) continue;

         std::shared_ptr<kinetic::ConnectionHandle> con = driveToConnection(p.drives(i));
         KineticStatus status = KineticStatus(kinetic::StatusCode::REMOTE_REMOTE_CONNECTION_ERROR, "connection error");
         if(con)       status = con->blocking().Put(key, "" , WriteMode::REQUIRE_SAME_VERSION, record);
         if(!status.ok()){
             if(first_write){
                 pok_debug("Putting partition update to drive %s:%d failed because of %s. Regressing cluster version.",
                         p.drives(i).host().data(),
                         p.drives(i).port(),
                         status.message().data()
                         );
                 p.set_cluster_version( p.cluster_version() - 1 );
             return false;
             }
             else{
                 pok_error("Partition update pushed incomplete.");
                 return false;
             }
         }
         con->blocking().SetClusterVersion(p.cluster_version());
         con->blocking().SetClientClusterVersion(p.cluster_version());
         first_write = false;
     }
     pok_debug("Putting partition update succeeded.");
     printClusterMap();
     return true;
}

bool DistributedKineticNamespace::validatePartition(posixok::Partition &p)
{
    int green=0;
    for(auto &d : p.drives()){
       if(d.status() == posixok::KineticDrive_Status_RED)
           continue;
       if(d.status() == posixok::KineticDrive_Status_GREEN)
           green++;

       std::unique_ptr<kinetic::DriveLog> unused;
       auto con = driveToConnection(d);
       if( !con || !(con->blocking().GetLog(unused)).ok()){
           pok_debug("Failed getLog on drive %s:%d (tried cluster version %d)",d.host().data(),d.port(),p.cluster_version());
           return false;
       }
    }
    if(green == 0){
        pok_error("Not a single GREEN drive available. Partition has empty read quorum. ");
        return false;
    }
    return true;
}


bool DistributedKineticNamespace::failDrive(posixok::Partition &p, int index)
{
    posixok::Partition saved = p;
    if(index >= p.drives_size()){
        pok_error("Tried to fail drive #%d of partition with maximum size %d. ",index,p.drives_size());
        return false;
    }

    if(p.drives(index).status() == posixok::KineticDrive_Status_RED) return true;

    /* Logdrive add-rule: Only add a new logdrive if none is set yet and all lights show green. */
    if(p.has_logdrive() == false && std::all_of(p.drives().begin(), p.drives().end(), [](const posixok::KineticDrive &d){return d.status() == d.GREEN;})){
        // choose random other drive that doesn't show a RED light, doesn't matter if it's located in the same partition or elsewhere
        std::uniform_int_distribution<int> cdist(0, cluster_map.size()-1);
        int ci, pi;
        do{
           ci = cdist(random_generator);
           std::uniform_int_distribution<int> pdist(0, cluster_map.at(ci).drives_size()-1);
           pi = pdist(random_generator);

        }while(cluster_map.at(ci).drives(pi).status() == posixok::KineticDrive_Status_RED ||
               cluster_map.at(ci).drives(pi).port() == p.drives(index).port());
        p.mutable_logdrive()->set_partitionid(ci);
        p.mutable_logdrive()->set_driveid(pi);
    }

     p.mutable_drives(index)->set_status(posixok::KineticDrive_Status_RED);
     if(validatePartition(p)){
        if(putPartitionUpdate(p)){
            connection_map.erase(p.drives(index));
            return true;
        }
        if(getPartitionUpdate(p))
            return failDrive(p, index);
    }

    pok_warning("Unexpected error code while failing a drive. Probably encountering multiple concurrent failures or network split.");
    p = saved;
    return false;
}



bool DistributedKineticNamespace::enableDrive(posixok::Partition &p, int index)
{
    assert(p.drives(index).status() == posixok::KineticDrive_Status_RED);

    std::shared_ptr<kinetic::ConnectionHandle>  con = driveToConnection(p.drives(index));
    if(!con) return false;

    std::unique_ptr<kinetic::DriveLog> unused;
    std::int64_t cversion = -1;
    KineticStatus status(kinetic::StatusCode::OK, "");
    do{
        cversion++;
        con->blocking().SetClientClusterVersion(cversion);
        status = con->blocking().GetLog(unused);
        pok_debug("trying cluster version %d for drive %s:%d",cversion,p.drives(index).host().c_str(),p.drives(index).port());
    }while(status.statusCode() == kinetic::StatusCode::REMOTE_CLUSTER_VERSION_MISMATCH);
    if(status.ok() == false){
        pok_debug("status == %s",status.message().data());
        return false;
    }

    p.mutable_drives(index)->set_status(posixok::KineticDrive_Status_YELLOW);
    if(validatePartition(p)){
      if(putPartitionUpdate(p)){
          std::thread( [&](){ synchronizeDrive(p, index);}).detach();
          return true;
      }
      p.mutable_drives(index)->set_status(posixok::KineticDrive_Status_RED);
      if(getPartitionUpdate(p)) return enableDrive(p, index);
    }
    p.mutable_drives(index)->set_status(posixok::KineticDrive_Status_RED);
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
    string keystart = " ";
    string keyend   = "|";
    string version;
    std::shared_ptr<kinetic::ConnectionHandle> con;

    if(p.has_logdrive()){
        keystart.insert(0, logkey_prefix);
        keyend.insert(0, logkey_prefix);
        con = driveToConnection( cluster_map[p.logdrive().partitionid()].drives(p.logdrive().driveid()));
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
            p.drives(index).host().data(),p.drives(index).port(),p.has_logdrive() ? "logdrive":"scratch... complete rebuild is required");

    do{
         if (keys->size())
             keystart = keys->back();
         keys->clear();
         if( con->blocking().GetKeyRange(keystart,true,keyend,true,false,maxsize,keys).ok() == false)
             return false;
         pok_debug("obtained %d keys that might need to be repaired",keys->size());

         for (auto& element : *keys) {
             if(p.has_logdrive()) element.erase(0, logkey_prefix.length());
             if(repairKey(element, version) == false){
                 pok_warning(" Error encountered while repairing key %s. Aborting drive synchronization.",element.data());
                 return false;
             }
             if(p.has_logdrive()){
                 element.insert(0, logkey_prefix);
                 con->blocking().Delete(element,"",kinetic::WriteMode::IGNORE_VERSION);
             }
         }
     } while (keys->size() == maxsize);

     pok_trace("drive completely synchronized.");
     p.mutable_drives(index)->set_status(posixok::KineticDrive_Status_GREEN);
     if(putPartitionUpdate(p))
         return true;
     p.mutable_drives(index)->set_status(posixok::KineticDrive_Status_YELLOW);
     pok_warning("Failed updating partition after successfully synchronizing drive.");
     return false;
}



/* In case of a version conflict due to concurrent writes to the same key by multiple clients and / or client crashes, up to #nodes different
*  versions could exist at the same time. Resolve by priority based on node index in partition. Key also might be deleted on part of the nodes.
*
*  This operation will only succeed if there are no unhandled connection / I/O problems */
bool DistributedKineticNamespace::repairKey(const string &key, string &v)
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
    if(!con) return false;
    std::unique_ptr<KineticRecord> record;
    KineticStatus status = con->blocking().Get(key,record);
    if(!status.ok() && status.statusCode() != kinetic::StatusCode::REMOTE_NOT_FOUND)
        return false;

    for(int i = 0; i<p.drives_size(); i++){
        if(p.drives(i).status() == posixok::KineticDrive_Status_RED) continue;

        con = driveToConnection(p.drives(i));
        if(!con) return false;

        std::unique_ptr<std::string> version(new std::string(""));
        status = con->blocking().GetVersion(key, version);

        if(!status.ok() && status.statusCode() != kinetic::StatusCode::REMOTE_NOT_FOUND)
            return false;

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
            return false;
    }
    if(record) v = *record->version();
    return true;
}


KineticStatus DistributedKineticNamespace::writeOperation (const string &key, std::function< KineticStatus(kinetic::BlockingKineticConnection&) > operation)
{
    /* Step 1) perform operation asynchronously on all non-down nodes using a thread per node.
     *         IFF partition contains any RED status drives, write to log if available */
    posixok::Partition & p    =  keyToPartition(key);
    std::vector<std::future<std::pair<KineticStatus,int>>> futures;
    std::future<KineticStatus> logged;

    if(p.has_logdrive() && std::any_of(p.drives().begin(), p.drives().end(), [](const posixok::KineticDrive &d){return d.status() == posixok::KineticDrive_Status_RED;}))
        logged = std::async( [&](){
                auto con = driveToConnection( cluster_map[p.logdrive().partitionid()].drives(p.logdrive().driveid()));
                if(!con) return KineticStatus(kinetic::StatusCode::REMOTE_REMOTE_CONNECTION_ERROR, "Unreachable");
                kinetic::KineticRecord record("", "", "", com::seagate::kinetic::proto::Message_Algorithm_SHA1);
                return con->blocking().Put(logkey_prefix+key,"",WriteMode::IGNORE_VERSION,record);
        });

    for(int i=0; i<p.drives_size(); i++){
        const posixok::KineticDrive &d = p.drives(i);
        if(d.status() == posixok::KineticDrive_Status_RED) continue;
        std::shared_ptr<kinetic::ConnectionHandle> con = driveToConnection(d);

        futures.push_back( std::async( [=](){
                KineticStatus status = KineticStatus(kinetic::StatusCode::REMOTE_REMOTE_CONNECTION_ERROR, "Unreachable");
                if(con) status = operation( con->blocking() );
                return std::pair<KineticStatus,int>(status,i);
        }));
    }

    std::vector<std::pair<KineticStatus,int>> results;
    for(auto &f : futures) results.push_back(f.get());


    /* Step 2: evaluate results */
    /* A) cluster version mismatch -> retry operation if partition can be updated to new cluster version successfully */
    for(auto &r : results){
       if( r.first.statusCode() == kinetic::StatusCode::REMOTE_CLUSTER_VERSION_MISMATCH){
           if(getPartitionUpdate(p)) return writeOperation(key, operation);
           pok_error("Non-resolvable cluster version mismatch");
           return r.first;
       }
    }

    /* B) the same result for all threads -> no replication specific error handling required. */
    if(std::all_of(results.begin(), results.end(), [&](const std::pair<KineticStatus,int> & r)
            {return r.first.statusCode() == results.front().first.statusCode();}
    )){
        // If operation was succesfull & logging enabled, ensure that logwrite was successfull
        if(results.front().first.ok() && p.has_logdrive() && logged.valid()){
            if(!logged.get().ok()){
                p.clear_logdrive();
                putPartitionUpdate(p);
            }
        }
        return results.front().first;
    }

    /* C) Any kind of unexpected I/O error -> retry operation if drive in question can be failed successfully. */
    for(auto &r : results){
        if( r.first.statusCode() != kinetic::StatusCode::OK &&
            r.first.statusCode() != kinetic::StatusCode::REMOTE_VERSION_MISMATCH &&
            r.first.statusCode() != kinetic::StatusCode::REMOTE_NOT_AUTHORIZED ){
            if(failDrive(p, r.second))
                return  KineticStatus(kinetic::StatusCode::REMOTE_OTHER_ERROR, "failed drive");
            pok_error("Couldn't fail drive after write error");
            return r.first;
        }
    }

    /* D) ... and that only leaves a partial write ( version missmatch due to client crash or concurrent writes ) */
    return KineticStatus(kinetic::StatusCode::REMOTE_OTHER_ERROR, "partial write");
}

KineticStatus DistributedKineticNamespace::readOperation (const string &key, std::function< KineticStatus(kinetic::BlockingKineticConnection&) > operation)
{
    /* Step 1) Pick a drive, obtain connection, execute operation */
    posixok::Partition & p = keyToPartition(key);
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
        if(failDrive(p, index))
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
        std::string version;
        if(repairKey(key, version)){
            if(version == *record.version())
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
        std::string version;
        if(repairKey(key, version)){
               if(version.empty())
                   result = KineticStatus( kinetic::StatusCode::OK, "");
               else
                   result = KineticStatus( kinetic::StatusCode::REMOTE_VERSION_MISMATCH, "version mismatch");
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


void DistributedKineticNamespace::printClusterMap()
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

        for (size_t i=0; i<cluster_map.size(); i++ ){
            auto &p = cluster_map[i];
            std::cout << "Partition #" << i << " ClusterVersion " << p.cluster_version() << std::endl;
            for (auto &d : p.drives()){
                std::cout << "\t" << d.host() << ":" << d.port() << " - " << statusToString(d.status()) << std::endl;
            }
            if(p.has_logdrive()){
                std::cout << "\t LOGDRIVE: partition#" << p.logdrive().partitionid() << "   drive#" << p.logdrive().driveid() << std::endl;
            }

        }
         std::cout << "[----------------------------------------]" << std::endl;
}

