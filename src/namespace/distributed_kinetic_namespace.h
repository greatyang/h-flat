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
#ifndef DISTRIBUTED_KINETIC_NAMESPACE_H_
#define DISTRIBUTED_KINETIC_NAMESPACE_H_
#include "kinetic_namespace.h"
#include "simple_kinetic_namespace.h"
#include "WrapperConnection.h"
#include "lru_cache.h"
#include "replication.pb.h"
#include <vector>
#include <random>
#include <future>

/* Template specializations for protobuf KineticDrive, allowing it to be used as a key in STL containers. */
namespace std {
  template <> struct hash<hflat::KineticDrive>
  {
    std::size_t operator()(const hflat::KineticDrive& d) const {
      return std::hash<string>()( d.host() + std::to_string(d.port()));
    }
  };
  template <> struct equal_to<hflat::KineticDrive>
  {
      bool operator() (const hflat::KineticDrive &a, const hflat::KineticDrive &b) const {
          return std::string(a.host()+std::to_string(a.port())).compare(
                             b.host()+std::to_string(b.port()))
                 ? false : true;
      }
  };
}

typedef std::shared_ptr<kinetic::WrapperConnection> ConnectionPointer;

/* Aggregates a number of kinetic drives into a single namespace. Uses N-1-N replication with global node-state to provide redundancy. */
class DistributedKineticNamespace final : public KineticNamespace
{
private:
    /* get lock in all cases where the cluster_map is changed. */
    std::recursive_mutex failure_lock;

    hflat::Partition                                                                                log_partition;
    std::vector< hflat::Partition >                                                                 cluster_map;
    std::unordered_map< hflat::KineticDrive, ConnectionPointer >  connection_map;

    /* Number of partitions directory entry keys of a single directory are distributed over.
     * This can be configured to be anywhere from 1 (best for small directories) to all (best scaling create performance). */
    int                               direntry_clustersize;
    std::default_random_engine        random_generator;
    kinetic::Capacity                 capacity_estimate;
    float                             capacity_chunksize;

private:
    hflat::Partition & keyToPartition(const std::string &key);
    ConnectionPointer  driveToConnection(const hflat::Partition &p, int driveID);

    bool testPartition (const hflat::Partition &p);
    bool testConnection(const hflat::Partition &p, int driveID);

    bool enableDrive     (hflat::Partition &p, int driveID);
    bool disableDrive    (hflat::Partition &p, int driveID);
    bool synchronizeDrive(hflat::Partition &p, int driveID);

    bool getPartitionUpdate(hflat::Partition &p);
    bool putPartitionUpdate(hflat::Partition &p);

    /* repairs any version missmatches existing for the specified key.
      * fails if a drive in the partition fails during the operation.
      * if successful, the key-version considered to be correct will be stored in supplied version attribute */
    KineticStatus readRepair(const string &key, std::unique_ptr<KineticRecord> &record);

    /* Run PUT / DELETE operations on all drives of the partition associated with the key that are not marked DOWN. */
    KineticStatus writeOperation (const string &key, std::function< KineticStatus(ConnectionPointer&) > operation);
    KineticStatus evaluateWriteOperation(hflat::Partition &p, std::vector<KineticStatus> &results );
    /* Run GET / GETVERSION / GETKEYRANGE operations on any single drive of the partition marked UP. */
    KineticStatus readOperation (hflat::Partition &p, std::function< KineticStatus(ConnectionPointer&) > operation);

    bool updateCapacityEstimate();
public:

    KineticStatus Get(const string &key, unique_ptr<KineticRecord>& record);
    KineticStatus Delete(const string &key, const string& version, WriteMode mode);
    KineticStatus Put(const string &key, const string &current_version, WriteMode mode, const KineticRecord& record);
    KineticStatus GetVersion(const string &key, unique_ptr<string>& version);
    KineticStatus Capacity(kinetic::Capacity &cap);

    /* Key-Range requests are never multi-partition. The partition that is queried depends on the start key. */
    KineticStatus GetKeyRange(const string &start_key, const string &end_key, unsigned int max_results, unique_ptr<vector<string>> &keys);

    bool          selfCheck();

    /* DEBUG ONLY */
    void printPartition(const hflat::Partition &p);
    void printClusterMap();

public:
    explicit DistributedKineticNamespace(const std::vector< hflat::Partition > &clustermap, const hflat::Partition &logpartition, int dirclustersize);
    ~DistributedKineticNamespace();
};


#endif /* SIMPLE_KINETIC_NAMESPACE_H_ */
