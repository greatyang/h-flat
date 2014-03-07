#include "simple_kinetic_namespace.h"
#include <exception>

SimpleKineticNamespace::SimpleKineticNamespace(kinetic::ConnectionOptions opts):
options(opts)
{
    connect();
}

SimpleKineticNamespace::SimpleKineticNamespace()
{
    options.host = "localhost";
    options.port = 8123;
    options.user_id = 1;
    options.hmac_key = "asdfasdf";
    connect();
}

SimpleKineticNamespace::~SimpleKineticNamespace()
{
}

void SimpleKineticNamespace::connect()
{
    kinetic::KineticConnectionFactory factory = kinetic::NewKineticConnectionFactory();

    kinetic::Status status = factory.NewThreadsafeConnection(options, 5, con);
    if (status.notOk())
        throw std::runtime_error(status.ToString());
}

KineticStatus SimpleKineticNamespace::Get(const string &key, unique_ptr<KineticRecord>& record)
{
    return con->blocking().Get(key, record);
}

KineticStatus SimpleKineticNamespace::Delete(const string &key, const string& version, WriteMode mode)
{
    return con->blocking().Delete(key, version, mode);
}

KineticStatus SimpleKineticNamespace::Put(const string &key, const string &current_version, WriteMode mode, const KineticRecord& record)
{
    return con->blocking().Put(key, current_version, mode, record);
}

KineticStatus SimpleKineticNamespace::GetVersion(const string &key, unique_ptr<string>& version)
{
    return con->blocking().GetVersion(key, version);
}

KineticStatus SimpleKineticNamespace::GetKeyRange(const string &start_key, const string &end_key, unsigned int max_results,
        unique_ptr<vector<string>> &keys)
{
    const bool start_key_inclusive = false;
    const bool end_key_inclusive = false;
    const bool reverse_results = false;
    return con->blocking().GetKeyRange(start_key, start_key_inclusive, end_key, end_key_inclusive, reverse_results, max_results, keys);
}

KineticStatus SimpleKineticNamespace::Capacity(kinetic::Capacity &cap)
{
    unique_ptr<kinetic::DriveLog> log;
    KineticStatus status = con->blocking().GetLog(log);
    if (status.ok()) {
        cap.remaining_bytes = log->capacity.remaining_bytes;
        cap.total_bytes     = log->capacity.total_bytes;
    }
    return status;
}
