#ifndef DISTRIBUTED_KINETIC_NAMESPACE_H_
#define DISTRIBUTED_KINETIC_NAMESPACE_H_
#include "kinetic_namespace.h"
#include "simple_kinetic_namespace.h"
#include "lru_cache.h"
#include "replication.pb.h"
#include <vector>
#include <random>
#include <future>

/* Template specializations for KineticDrive, allowing it to be used as a key in stl containers. */
namespace std {
  template <> struct hash<posixok::KineticDrive>
  {
    std::size_t operator()(const posixok::KineticDrive& d) const {
      return std::hash<string>()( d.host() + std::to_string(d.port()));
    }
  };
  template <> struct equal_to<posixok::KineticDrive>
  {
      bool operator() (const posixok::KineticDrive &a, const posixok::KineticDrive &b) const {
          return std::string(a.host()+std::to_string(a.port())).compare(
                             b.host()+std::to_string(b.port()))
                 ? false : true;
      }
  };
}

/* Aggregates a number of kinetic drives into a single namespace. Uses N-1-N replication with global node-state to provide redundancy. */
class DistributedKineticNamespace final : public KineticNamespace
{
private:
    /* get lock in all cases where the cluster_map is changed. */
    std::recursive_mutex failure_lock;

    posixok::Partition                                                                       log_partition;
    std::vector< posixok::Partition >                                                        cluster_map;
    std::unordered_map< posixok::KineticDrive, std::shared_ptr<kinetic::ConnectionHandle> >  connection_map;

    kinetic::KineticConnectionFactory connection_factory;
    std::default_random_engine        random_generator;
    kinetic::Capacity                 capacity_estimate;

private:
    posixok::Partition &                       keyToPartition(const std::string &key);
    std::shared_ptr<kinetic::ConnectionHandle> driveToConnection(const posixok::KineticDrive &drive);

    bool testConnection(const posixok::KineticDrive &drive);
    bool testPartition (const posixok::Partition &p);

    bool getPartitionUpdate(posixok::Partition &p);
    bool putPartitionUpdate(posixok::Partition &p);

    bool synchronizeDrive(posixok::Partition &p, int driveID); // parititonID << logdriveprefix
    bool disableDrive(posixok::Partition &p, int driveID);
    bool enableDrive(posixok::Partition &p, int driveID);     // partitionID

    /* repairs any version missmatches existing for the specified key.
      * fails if a drive in the partition fails during the operation.
      * if successful, the key-version considered to be correct will be stored in supplied version attribute */
    KineticStatus readRepair(const string &key, std::unique_ptr<KineticRecord> &record);

    /* Run PUT / DELETE operations on all drives of the partition associated with the key that are not marked DOWN. */
    KineticStatus writeOperation (const string &key, std::function< KineticStatus(kinetic::BlockingKineticConnection&) > operation);
    KineticStatus evaluateWriteOperation(posixok::Partition &p, std::vector<KineticStatus> &results );
    /* Run GET / GETVERSION / GETKEYRANGE operations on any single drive of the partition marked UP. */
    KineticStatus readOperation (const string &key, std::function< KineticStatus(kinetic::BlockingKineticConnection&) > operation);

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
    void printPartition(const posixok::Partition &p);
    void printClusterMap();

public:
    explicit DistributedKineticNamespace(const std::vector< posixok::Partition > &clustermap, const posixok::Partition &logpartition);
    ~DistributedKineticNamespace();
};


#endif /* SIMPLE_KINETIC_NAMESPACE_H_ */
