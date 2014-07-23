/* h-flat file system: Hierarchical Functionality in a Flat Namespace
 * Copyright (c) 2014 Seagate
 * Written by Paul Hermann Lensing
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <algorithm>
#include <future>
#include <iostream>
#include "distributed_kinetic_namespace.h"
#include "threadsafe_blocking_connection.h"
#include "debug.h"

using com::seagate::kinetic::client::proto::Message_Algorithm_SHA1;
typedef std::shared_ptr<kinetic::BlockingKineticConnection> ConnectionPointer;


static const string cv_base_name =  "clusterversion_";
static const string logkey_prefix = "log_";

DistributedKineticNamespace::DistributedKineticNamespace(const std::vector< hflat::Partition > &cmap, const hflat::Partition &lpart):
        failure_lock(), log_partition(lpart), cluster_map(cmap), connection_factory(kinetic::NewKineticConnectionFactory())
{
    if(selfCheck() == false)
        throw std::runtime_error("Invalid Clustermap");

    hflat_warning("Capacity reporting disabled due to getlog performance issues. ");
    //updateCapacityEstimate();
}

DistributedKineticNamespace::~DistributedKineticNamespace()
{
}


hflat::Partition & DistributedKineticNamespace::keyToPartition(const std::string &key)
{
    size_t hashvalue = std::hash<std::string>()(key.substr(0,key.find_first_of("|")));
    int index = hashvalue % cluster_map.size();
    return cluster_map[index];
}

ConnectionPointer  DistributedKineticNamespace::driveToConnection(const hflat::Partition &p, int driveID)
{
    if(connection_map.count(p.drives(driveID))) return connection_map[p.drives(driveID)];

    std::shared_ptr<kinetic::NonblockingKineticConnection> nonblocking_con;
    ConnectionPointer con;
    kinetic::ConnectionOptions options;
    options.host = p.drives(driveID).host();
    options.port = p.drives(driveID).port();
    options.user_id = 1;
    options.hmac_key = "asdfasdf";

    if( connection_factory.NewThreadsafeNonblockingConnection(options, nonblocking_con).ok() ){
        con.reset(new kinetic::ThreadsafeBlockingConnection(nonblocking_con, 5));
        connection_map[p.drives(driveID)] = con;
        if(p.cluster_version()){
            connection_map[p.drives(driveID)]->SetClientClusterVersion(p.cluster_version());
            hflat_trace("Set cluster version of connection for drive %s:%d to %d",
                    p.drives(driveID).host().c_str(),p.drives(driveID).port(),p.cluster_version());
        }
        return connection_map[p.drives(driveID)];
    }

    if(p.drives(driveID).status() != hflat::KineticDrive_Status_RED)
        hflat_warning("Failed connecting to drive @ %s:%d.",p.drives(driveID).host().c_str(),p.drives(driveID).port());
    return ConnectionPointer();
}


bool DistributedKineticNamespace::testConnection(const hflat::Partition &p, int driveID)
{
    auto con = driveToConnection(p, driveID);
    if(! con) return false;
    if(!con->NoOp().ok()){
        hflat_debug("Failed connecting to drive %s:%d",p.drives(driveID).host().c_str(),p.drives(driveID).port());
        return false;
    }
    return true;
}


bool DistributedKineticNamespace::updateCapacityEstimate()
{
    std::vector<std::future<kinetic::Capacity>> futures;

    for(auto &p : cluster_map){
        for(int i=0; i<p.drives_size(); i++){
            if(p.drives(i).status() == hflat::KineticDrive_Status_GREEN){
                futures.push_back( std::async( [&](){
                    ConnectionPointer  con = driveToConnection(p,i);
                    std::unique_ptr<kinetic::DriveLog> dlog;
                    kinetic::Capacity c;
                    if( con && (con->GetLog(dlog)).ok() )
                        c = dlog->capacity;
                    return c;
                    }));
                break;
            }
        }
    }

    kinetic::Capacity cap = {0, 0};
    for(auto &f : futures){
        kinetic::Capacity c = f.get();
        if(!c.nominal_capacity_in_bytes){
            hflat_warning("Incomplete capacity update.");
            return false;
        }
        cap.nominal_capacity_in_bytes     += c.nominal_capacity_in_bytes;
        cap.portion_full += c.portion_full;
    }
    cap.portion_full /= futures.size();
    capacity_estimate = cap;
    capacity_chunksize = ((float)1024*1024) / capacity_estimate.nominal_capacity_in_bytes;
    return true;
}


bool DistributedKineticNamespace::selfCheck()
{
    hflat_debug("Checking Clustermap: ");
    printClusterMap();

    auto check = [&](hflat::Partition &p){

        for(int i=0; i<p.drives_size(); i++)
               if(p.drives(i).status() == hflat::KineticDrive_Status_RED)
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
bool DistributedKineticNamespace::getPartitionUpdate(hflat::Partition & p)
{
    if(p.has_partitionid() == false) return false;
    std::lock_guard<std::recursive_mutex> l(failure_lock);

    for(int i=0; i<p.drives_size(); i++){
        if(p.drives(i).status() == hflat::KineticDrive_Status_RED) continue;
        auto con = driveToConnection(p,i);
        if( !con) continue;

        std::unique_ptr<KineticRecord> record;
        std::int64_t cversion = p.cluster_version() - 1;
        KineticStatus status(kinetic::StatusCode::OK, "");

        do{
            cversion++;
            con->SetClientClusterVersion(cversion);
            status = con->Get(cv_base_name+std::to_string(cversion), record);
            hflat_debug("trying cluster version %d for drive %s:%d",cversion,p.drives(i).host().c_str(),p.drives(i).port());
        }while(status.statusCode() == kinetic::StatusCode::REMOTE_CLUSTER_VERSION_MISMATCH);

        if(status.ok()){
           p.ParseFromString(*record->value());
           hflat_debug("Updated partition to cluster version %d from drive %s:%d.",p.cluster_version(),p.drives(i).host().c_str(),p.drives(i).port());
           return true;
        }
    }
    return false;
}

bool DistributedKineticNamespace::putPartitionUpdate(hflat::Partition &p)
{
    if(p.has_partitionid() == false) return true;
    std::lock_guard<std::recursive_mutex> l(failure_lock);

    std::string   key      = cv_base_name + std::to_string(p.cluster_version());
    KineticRecord record(p.SerializeAsString(), "yes", "", Message_Algorithm_SHA1);

     /* Serialize write accesses to ensure that multiple clients attempting to update the same cluster version don't conflict.
      * The first write may thus fail in case of concurrency. */
     for(int i=0; i < p.drives_size(); i++){
         if(p.drives(i).status() == hflat::KineticDrive_Status_RED) continue;

         ConnectionPointer con = driveToConnection(p,i);
         KineticStatus status = KineticStatus(kinetic::StatusCode::REMOTE_REMOTE_CONNECTION_ERROR, "connection error");
         if(con)       status = con->Put(key, "" , WriteMode::REQUIRE_SAME_VERSION, record);
         if(!status.ok()){
             hflat_warning("Failure.");
             return false;
         }
         con->SetClusterVersion(p.cluster_version());
         con->SetClientClusterVersion(p.cluster_version());
     }
     hflat_debug("Put Partition successful: ");
     printPartition(p);
     return true;
}

bool DistributedKineticNamespace::testPartition(const hflat::Partition &p)
{
    if(p.has_partitionid() == false) return true;
    /* count various states existing in the partition and test connections */
    int green=0; int yellow=0;
    for(int i=0; i<p.drives_size(); i++){
       const hflat::KineticDrive &d = p.drives(i);
       if(d.status() == d.RED)    continue;
       if(d.status() == d.GREEN)  green++;
       if(d.status() == d.YELLOW) yellow++;

       if(testConnection(p,i) == false)
           return false;
    }
    if(p.has_logid())
        if(testConnection(log_partition, p.logid()) == false)
            return false;

    if(green == 0){
        hflat_error("Not a single GREEN drive available. Partition has empty read quorum. ");
        return false;
    }
    if(green+yellow == 1 && p.drives_size() > 1 && p.has_logid() == false){
        hflat_error("Only one drive reachable & no logdrive set for the partition. At least two drives "
                  "(including logdrive) have to be reachable at all times to detect a network split. ");
        return false;
    }

    return true;
}


bool DistributedKineticNamespace::disableDrive(hflat::Partition &p, int index)
{
    std::lock_guard<std::recursive_mutex> l(failure_lock);
    if(p.drives(index).status() == hflat::KineticDrive_Status_RED) return true;

    /* Logdrive add-rule: Only add a new logdrive if none is set yet and all lights show green:
     *   other color combinations without a logdrive mean that a previosly assigned logdrive has crashed and thus there are no
     *   reliable logs for the other non-green drives. We shouldn't confuse the repair algorithm. */
    if(p.has_logid() == false && std::all_of(p.drives().begin(), p.drives().end(), [](const hflat::KineticDrive &d){return d.status() == d.GREEN;})
                          && std::any_of(log_partition.drives().begin(), log_partition.drives().end(), [](const hflat::KineticDrive &d){return d.status() != d.RED;})){
        std::uniform_int_distribution<int> ldist(0, log_partition.drives_size()-1);
        int lid;
        do{ lid = ldist(random_generator); } while( log_partition.drives(lid).status() == hflat::KineticDrive_Status_RED );
        p.set_logid(lid);
    }

     p.mutable_drives(index)->set_status(hflat::KineticDrive_Status_RED);
     p.set_cluster_version( p.cluster_version()+1 );
     if(testPartition(p)){
        if(putPartitionUpdate(p)){
            connection_map.erase(p.drives(index));
            return true;
        }
        if(getPartitionUpdate(p))
            return disableDrive(p, index);
    }
    hflat_warning("Unexpected error code while failing a drive. Either no additional drives left in partition or encountering multiple concurrent failures or network split.");
    printPartition(p);
    return false;
}



bool DistributedKineticNamespace::enableDrive(hflat::Partition &p, int index)
{
    std::lock_guard<std::recursive_mutex> l(failure_lock);
    if(p.drives(index).status() != hflat::KineticDrive_Status_RED) return true;

    ConnectionPointer  con = driveToConnection(p, index);
    if(!con) return false;

    std::int64_t cversion = -1;
    KineticStatus status(kinetic::StatusCode::OK, "");
    do{
        cversion++;
        con->SetClientClusterVersion(cversion);
        status = con->NoOp();
    }while(status.statusCode() == kinetic::StatusCode::REMOTE_CLUSTER_VERSION_MISMATCH);
    if(status.ok() == false){
        return false;
    }

    p.mutable_drives(index)->set_status(hflat::KineticDrive_Status_YELLOW);
    p.set_cluster_version(p.cluster_version()+1);
    if(testPartition(p)){
      if(putPartitionUpdate(p)){
          std::thread( [&](){ synchronizeDrive(p, index);}).detach();
          return true;
      }
      if(getPartitionUpdate(p))
          return enableDrive(p, index);
    }
    p.mutable_drives(index)->set_status(hflat::KineticDrive_Status_RED);
    p.set_cluster_version(p.cluster_version()-1);
    return false;
}

/*
 * If a logdrive is available, use it to get a list of keys that are possibly out of date.
 * Otherwise, use a drive of the partition with GREEN status to check every single key. */
bool DistributedKineticNamespace::synchronizeDrive(hflat::Partition &p, int index)
{
    if(p.drives(index).status() != hflat::KineticDrive_Status_YELLOW) return false;
    unsigned int maxsize = 100;
    unique_ptr<vector<std::string>> keys(new vector<string>());
    unique_ptr<KineticRecord> record;
    string keystart = " ";
    string keyend   = "|";
    ConnectionPointer con;
    std::string prefix = std::to_string(p.partitionid()) + logkey_prefix;
    bool write_quorum_all = std::all_of(p.drives().begin(), p.drives().end(), [](const hflat::KineticDrive &d){return d.status() != d.RED;});

    if(p.has_logid()){
        keystart.insert(0,prefix);
        keyend.insert(0,prefix);
        con = driveToConnection(log_partition,p.logid());
    }
    else{
        std::uniform_int_distribution<int> dist(0, p.drives_size()-1);
        int index;
        do{ index = dist(random_generator); }
        while(p.drives(index).status() != hflat::KineticDrive_Status_GREEN);
        con = driveToConnection(p,index);
    }
    if(!con) return false;

    hflat_trace("Synchronizing drive %s:%d from %s",
            p.drives(index).host().data(),p.drives(index).port(),p.has_logid() ? "logdrive":"scratch... complete rebuild is required");

    do{
         if (keys->size())
             keystart = keys->back();
         keys->clear();
         if( con->GetKeyRange(keystart,true,keyend,true,false,maxsize,keys).ok() == false)
             return false;
         hflat_debug("obtained %d keys that might need to be repaired",keys->size());

         for (auto& element : *keys) {
             if(p.has_logid()) element.erase(0, prefix.length());

             if(readRepair(element,record).ok() == false){
                 hflat_warning(" Error encountered while repairing key %s. Aborting drive synchronization.",element.data());
                 return false;
             }

             if(p.has_logid()){
                 element.insert(0, prefix);
                 // Delete the repaired key from the logdrive, ONLY if there are no other failed drives in the same partition.
                 // Otherwise, the log could not be used to repair them once they come online.
                 if(write_quorum_all) con->Delete(element,"",kinetic::WriteMode::IGNORE_VERSION);
             }
         }
     } while (keys->size() == maxsize);

     hflat_trace("drive completely synchronized.");

     std::lock_guard<std::recursive_mutex> l(failure_lock);
     p.mutable_drives(index)->set_status(hflat::KineticDrive_Status_GREEN);
     p.set_cluster_version(p.cluster_version()+1);
     if(p.has_logid() && std::all_of(p.drives().begin(), p.drives().end(), [](const hflat::KineticDrive &d){return d.status() == d.GREEN;}))
         p.clear_logid();
     if(putPartitionUpdate(p)){
         return true;
     }
     p.mutable_drives(index)->set_status(hflat::KineticDrive_Status_YELLOW);
     p.set_cluster_version(p.cluster_version()-1);
     hflat_warning("Failed updating partition after successfully synchronizing drive.");
     return false;
}


KineticStatus DistributedKineticNamespace::readRepair(const string &key, std::unique_ptr<KineticRecord> &record)
{
    hflat::Partition &p = keyToPartition(key);

    /* Get reference drive & record: First green drive in partition. */
    int index = 0;
    while(p.drives(index).status() != hflat::KineticDrive_Status_GREEN) index++;

    auto con = driveToConnection(p,index);
    KineticStatus status = con->Get(key,record);
    if(!status.ok() && status.statusCode() != kinetic::StatusCode::REMOTE_NOT_FOUND)
        return status;


    /* Enforce reference record on all drives.
     * We have to be careful about races during this step though:
     *  - Up to #partition_size clients could be trying to resolve a partial write to the same key at the same time.
     *  - When used to rebuild a drive, read repair can actually be called for a key that has
     *    the same version on all drives. Following, a successful write operation could be executed
     *    by another client between reading & enforcing the reference record.  */
    for(int i = 0; i<p.drives_size(); i++){
          if(p.drives(i).status() == hflat::KineticDrive_Status_RED) continue;

          std::unique_ptr<std::string> version(new std::string(""));
          con = driveToConnection(p,i);
          status = con->GetVersion(key, version);
          if(!status.ok() && status.statusCode() != kinetic::StatusCode::REMOTE_NOT_FOUND)
              return status;

          /* Delete record or */
          if(!record && status.ok()){
              status = con->Delete(key, *version, WriteMode::REQUIRE_SAME_VERSION);
              hflat_debug("Removing key %s from drive %s:%d",
                      key.c_str(),p.drives(i).host().c_str(), p.drives(i).port());
          }
          /* Copy record. */
          else if (record){
              if(status.ok() && *version == *record->version())
                  continue;
              status = con->Put(key, *version, WriteMode::REQUIRE_SAME_VERSION, *record);
              hflat_debug("Copying key %s to drive %s:%d",
                      key.c_str(),p.drives(i).host().c_str(), p.drives(i).port());
          }

          /* Check results, taking concurrency into account as described above. */
          if(status.statusCode() != kinetic::StatusCode::OK &&
             status.statusCode() != kinetic::StatusCode::REMOTE_NOT_FOUND &&
             status.statusCode() != kinetic::StatusCode::REMOTE_VERSION_MISMATCH)
              return status;
          if(status.statusCode() == kinetic::StatusCode::REMOTE_VERSION_MISMATCH && index == i)
              return KineticStatus(kinetic::StatusCode::OK, "repair no longer required");

      }
    return KineticStatus(kinetic::StatusCode::OK, "repaired");
}



KineticStatus DistributedKineticNamespace::evaluateWriteOperation( hflat::Partition &p,  std::vector<KineticStatus> &results )
{
    /* B) the same result for all threads -> no replication specific error handling required. */
    bool allequal=true; int green = 0;
    while(p.drives(green).status() != hflat::KineticDrive_Status_GREEN) green++;
    for(int i=0; i<p.drives_size(); i++){
          if(p.drives(i).status() == hflat::KineticDrive_Status_RED) continue;
          if( results.at(i).statusCode() != results.at(green).statusCode() ) allequal = false;
    }
    if(allequal) return results.at(green);

    /* C) Any kind of unexpected I/O error -> retry operation if drive in question can be failed successfully. */
    for(int i=0; i<p.drives_size(); i++){
        if(p.drives(i).status() == hflat::KineticDrive_Status_RED) continue;

        if( results.at(i).statusCode() != kinetic::StatusCode::OK &&
            results.at(i).statusCode() != kinetic::StatusCode::REMOTE_VERSION_MISMATCH &&
            results.at(i).statusCode() != kinetic::StatusCode::REMOTE_NOT_AUTHORIZED ){
            if(disableDrive(p, i))
                return evaluateWriteOperation(p, results);
            hflat_error("Couldn't fail drive after write error");
            return results.at(i);
        }
    }

    /* D) ... and that only leaves a partial write ( version missmatch due to client crash or concurrent writes ) */
    return KineticStatus(kinetic::StatusCode::REMOTE_OTHER_ERROR, "partial write");

}


KineticStatus DistributedKineticNamespace::writeOperation (const string &key, std::function< KineticStatus(ConnectionPointer&) > operation)
{
    hflat::Partition &p = keyToPartition(key);
    std::vector<std::future<KineticStatus>> futures;
    std::vector<KineticStatus> results;
    for(int i=0; i<p.drives_size(); i++){
       futures.push_back( std::async( [this, i, &p, &operation](){
                       if(p.drives(i).status() == hflat::KineticDrive_Status_RED) return KineticStatus(kinetic::StatusCode::REMOTE_REMOTE_CONNECTION_ERROR, "Unreachable");
                       ConnectionPointer con = driveToConnection(p,i);
                       return operation( con );
               }));
    }

    /* Try to log operation if at least one drive is missing from the write quorum. */
    if(p.has_logid() && std::any_of(p.drives().begin(), p.drives().end(), [](const hflat::KineticDrive &d){return d.status() == hflat::KineticDrive_Status_RED;})){

       ConnectionPointer con = driveToConnection(log_partition, p.logid());

       kinetic::KineticRecord record("", "", "", Message_Algorithm_SHA1);
       auto status = con->Put(std::to_string(p.partitionid())+logkey_prefix+key,"",WriteMode::IGNORE_VERSION, record);

       if(status.ok() == false){
           disableDrive(log_partition, p.logid());
           std::lock_guard<std::recursive_mutex> l(failure_lock);

           p.clear_logid();
           p.set_cluster_version(p.cluster_version()+1);
           if(putPartitionUpdate(p) == false)
               hflat_warning("Do not use logs to rebuild currently failed drives.");
       }
    }

    for(auto &f : futures) results.push_back(f.get());

    /* A) cluster version mismatch -> retry operation if partition can be updated to new cluster version successfully */
    for(auto &r : results){
     if( r.statusCode() == kinetic::StatusCode::REMOTE_CLUSTER_VERSION_MISMATCH){
         if(getPartitionUpdate(p)) return writeOperation(key, operation);
         hflat_error("Non-resolvable cluster version mismatch");
         return r;
     }
    }
    return evaluateWriteOperation(p, results);
}

KineticStatus DistributedKineticNamespace::readOperation (const string &key, std::function< KineticStatus(ConnectionPointer&) > operation)
{
    /* Step 1) Pick a drive, obtain connection, execute operation */
    hflat::Partition &p = keyToPartition(key);
    std::uniform_int_distribution<int> dist(0, p.drives_size()-1);
    int index;
    do{ index = dist(random_generator); }
    while(p.drives(index).status() != hflat::KineticDrive_Status_GREEN);

    ConnectionPointer con = driveToConnection(p,index);
    KineticStatus status = KineticStatus(kinetic::StatusCode::REMOTE_REMOTE_CONNECTION_ERROR, "");
    if(con)       status = operation(con);

    /* Step 2) Evaluate the results. */
    if(status.statusCode() == kinetic::StatusCode::REMOTE_CLUSTER_VERSION_MISMATCH){
        if(getPartitionUpdate(p))
            return readOperation(key, operation);
        hflat_debug("Failed partition update after a cluster_version_mismatch for drive %s:%d. Returning %s.",
                p.drives(index).host().data(),p.drives(index).port(),status.message().data());
        return status;
    }

    if(     status.statusCode() != kinetic::StatusCode::OK &&
            status.statusCode() != kinetic::StatusCode::REMOTE_NOT_FOUND &&
            status.statusCode() != kinetic::StatusCode::REMOTE_NOT_AUTHORIZED ){
        if(disableDrive(p, index))
            return readOperation(key, operation);
        hflat_debug("Didn't fail drive %s:%d successfully after encountering status %s.",p.drives(index).host().data(),p.drives(index).port(),status.message().data());
        return status;
    }

    return status;
}


KineticStatus DistributedKineticNamespace::Put(const string &key, const string &current_version, WriteMode mode, const KineticRecord& record)
{
    KineticStatus result = writeOperation(key,
            [&](ConnectionPointer&b){return b->Put(std::cref(key), std::cref(current_version), mode, std::cref(record));}
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

    if(result.ok() && current_version.empty()) capacity_estimate.portion_full += capacity_chunksize;
    return result;
}

KineticStatus DistributedKineticNamespace::Delete(const string &key, const string& version, WriteMode mode)
{
    KineticStatus result = writeOperation(key,
            [&](ConnectionPointer&b){return b->Delete(std::cref(key), std::cref(version), mode);}
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

    if(result.ok()) capacity_estimate.portion_full -= capacity_chunksize;
    return result;
}

KineticStatus DistributedKineticNamespace::Get(const string &key, unique_ptr<KineticRecord>& record)
{
    return readOperation(key, [&](ConnectionPointer & b){return b->Get(std::cref(key), std::ref(record));}
    );
}

KineticStatus DistributedKineticNamespace::GetVersion(const string &key, unique_ptr<string>& version)
{
    return readOperation(key, [&](ConnectionPointer & b){return b->GetVersion(std::cref(key), std::ref(version));}
    );
}


KineticStatus DistributedKineticNamespace::GetKeyRange(const string &start_key, const string &end_key, unsigned int max_results, unique_ptr<vector<string>> &keys)
{
    const bool start_key_inclusive = false;
    const bool end_key_inclusive = false;
    const bool reverse_results = false;

    return readOperation(start_key,
            [&](ConnectionPointer & b){return b->GetKeyRange(
                       start_key, start_key_inclusive, end_key, end_key_inclusive, reverse_results, max_results, keys);}
    );
}

KineticStatus DistributedKineticNamespace::Capacity(kinetic::Capacity &cap)
{
    cap = capacity_estimate;
    return KineticStatus(kinetic::StatusCode::OK, "");
}


void DistributedKineticNamespace::printPartition(const hflat::Partition &p)
{
    auto statusToString = [](hflat::KineticDrive_Status s) -> std::string {
           switch(s) {
               case hflat::KineticDrive_Status_GREEN:    return "GREEN";
               case hflat::KineticDrive_Status_YELLOW:   return "YELLOW";
               case hflat::KineticDrive_Status_RED:      return "RED";
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

