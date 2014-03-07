#ifndef SIMPLE_KINETIC_NAMESPACE_H_
#define SIMPLE_KINETIC_NAMESPACE_H_
#include "kinetic_namespace.h"

/* Simple one-drive implementation. */
class SimpleKineticNamespace final : public KineticNamespace
{
private:
    kinetic::ConnectionOptions        options;
    unique_ptr<kinetic::ConnectionHandle> con;

private:
    void connect();

public:
    KineticStatus Get(const string &key, unique_ptr<KineticRecord>& record);
    KineticStatus Delete(const string &key, const string& version, WriteMode mode);
    KineticStatus Put(const string &key, const string &current_version, WriteMode mode, const KineticRecord& record);
    KineticStatus GetVersion(const string &key, unique_ptr<string>& version);
    KineticStatus GetKeyRange(const string &start_key, const string &end_key, unsigned int max_results, unique_ptr<vector<string>> &keys);
    KineticStatus Capacity(kinetic::Capacity &cap);
public:
    explicit SimpleKineticNamespace(kinetic::ConnectionOptions options);
    explicit SimpleKineticNamespace();
    ~SimpleKineticNamespace();
};


#endif /* SIMPLE_KINETIC_NAMESPACE_H_ */