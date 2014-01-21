#ifndef KINETIC_NAMESPACE_H_
#define KINETIC_NAMESPACE_H_
#include "kinetic/kinetic.h"

/* Kinetic Namespace
 *
 * Exposing partial kinetic API. From the file system view there is just a single namespace, no matter how
 * many drives are used. All clustering & distribution logic should therefore be implemented in a class
 * inheriting from KineticNamespace.
 */
using kinetic::KineticStatus;
using kinetic::KineticRecord;
using kinetic::WriteMode;

class KineticNamespace
{
public:
    virtual KineticStatus Get(const std::string &key, std::unique_ptr<KineticRecord>* record) = 0;
    virtual KineticStatus Delete(const std::string &key, const std::string& version, WriteMode mode) = 0;
    virtual KineticStatus Put(const std::string &key, const std::string &current_version, WriteMode mode, const KineticRecord& record) = 0;
    virtual KineticStatus GetVersion(const std::string &key, std::string* version) = 0;
    virtual KineticStatus GetKeyRange(const std::string &start_key, const std::string &end_key, unsigned int max_results, std::vector<std::string> *keys) = 0;
    virtual KineticStatus Capacity(kinetic::Capacity &cap) = 0;

    virtual ~KineticNamespace(){};
};

/* Simple one-drive implementation. */
class SimpleKineticNamespace final : public KineticNamespace
{
private:
    std::unique_ptr<kinetic::ConnectionHandle> con;

private:
    void connect(kinetic::ConnectionOptions options);

public:
    KineticStatus Get(const std::string &key, std::unique_ptr<KineticRecord>* record);
    KineticStatus Delete(const std::string &key, const std::string& version, WriteMode mode);
    KineticStatus Put(const std::string &key, const std::string &current_version, WriteMode mode, const KineticRecord& record);
    KineticStatus GetVersion(const std::string &key, std::string* version);
    KineticStatus GetKeyRange(const std::string &start_key, const std::string &end_key, unsigned int max_results, std::vector<std::string> *keys);
    KineticStatus Capacity(kinetic::Capacity &cap);
public:
    explicit SimpleKineticNamespace(kinetic::ConnectionOptions options);
    explicit SimpleKineticNamespace();
    ~SimpleKineticNamespace();
};

#endif /* KINETIC_NAMESPACE_H_ */
