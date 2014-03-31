#ifndef DISTRIBUTED_KINETIC_NAMESPACE_H_
#define DISTRIBUTED_KINETIC_NAMESPACE_H_
#include "kinetic_namespace.h"
#include "simple_kinetic_namespace.h"
#include "lru_cache.h"
#include "replication.pb.h"
#include <vector>
#include <random>

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
    std::vector< posixok::Partition >                                                        cluster_map;
    std::unordered_map< posixok::KineticDrive, std::shared_ptr<kinetic::ConnectionHandle> >  connection_map;
    kinetic::KineticConnectionFactory connection_factory;
    std::default_random_engine        random_generator;
    kinetic::Capacity                 capacity_estimate;

private:
    const posixok::KineticDrive & drefToDrive(const posixok::KineticDriveReference & dref);
    std::shared_ptr<kinetic::ConnectionHandle> driveToConnection(const posixok::KineticDrive &drive);
    posixok::Partition & keyToPartition(const std::string &key);
    bool updateCapacityEstimate();

    bool validatePartition (posixok::Partition &p);
    bool getPartitionUpdate(posixok::Partition &p);
    bool putPartitionUpdate(posixok::Partition &p);

    bool failDrive(posixok::Partition &p, int index);
    bool enableDrive(posixok::Partition &p, int index);
    bool synchronizeDrive(posixok::Partition &p, int index);

    /* repairs any version missmatches existing for the specified key.
      * fails if a drive in the partition fails during the operation.
      * if successful, the key-version considered to be correct will be stored in supplied version attribute */
    bool repairKey(const string &key, string &version);

    /* Run PUT / DELETE operations on all drives of the partition associated with the key that are not marked DOWN. */
    KineticStatus writeOperation (const string &key, std::function< KineticStatus(kinetic::BlockingKineticConnection&) > operation);
    /* Run GET / GETVERSION / GETKEYRANGE operations on any single drive of the partition marked UP. */
    KineticStatus readOperation (const string &key, std::function< KineticStatus(kinetic::BlockingKineticConnection&) > operation);

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
    void printClusterMap();

public:
    explicit DistributedKineticNamespace(const std::vector< posixok::Partition > &clustermap);
    ~DistributedKineticNamespace();
};


#endif /* SIMPLE_KINETIC_NAMESPACE_H_ */
