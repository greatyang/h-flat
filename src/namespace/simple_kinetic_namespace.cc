#include "simple_kinetic_namespace.h"
#include <exception>

SimpleKineticNamespace::SimpleKineticNamespace(const hflat::KineticDrive &d)
{
    options.host = d.host();
    options.port = d.port();
    connect();
}

SimpleKineticNamespace::SimpleKineticNamespace()
{
    options.host = "localhost";
    options.port = 8123;
    connect();
}

SimpleKineticNamespace::~SimpleKineticNamespace()
{
}

void SimpleKineticNamespace::connect()
{
    options.user_id = 1;
    options.hmac_key = "asdfasdf";

    kinetic::KineticConnectionFactory factory = kinetic::NewKineticConnectionFactory();
    kinetic::Status status = factory.NewThreadsafeConnection(options, 5, con);
    if (status.notOk())
        throw std::runtime_error(status.ToString());
}

bool SimpleKineticNamespace::selfCheck()
{
    unique_ptr<kinetic::DriveLog> log;
    KineticStatus status = con->blocking().GetLog(log);
    if(status.ok())
        return true;
    return false;
}

#include<debug.h>
KineticStatus SimpleKineticNamespace::Run(std::function<KineticStatus()> op)
{
    int maxrepeat = 3;
    KineticStatus s(kinetic::StatusCode::CLIENT_INTERNAL_ERROR, "Invalid");
    do {
        s = op();
        if(s.statusCode() != kinetic::StatusCode::REMOTE_SERVICE_BUSY &&
           s.statusCode() != kinetic::StatusCode::REMOTE_INTERNAL_ERROR &&
           s.statusCode() != kinetic::StatusCode::CLIENT_IO_ERROR)
            break;
    }while(--maxrepeat);

    if(maxrepeat == 0)
    if(s.statusCode() == kinetic::StatusCode::REMOTE_SERVICE_BUSY ||
       s.statusCode() == kinetic::StatusCode::REMOTE_INTERNAL_ERROR ||
       s.statusCode() == kinetic::StatusCode::CLIENT_IO_ERROR)
        hflat_warning("Giving up retrying for error code %d. %s",s.statusCode(),s.message().c_str());
    return s;
}

KineticStatus SimpleKineticNamespace::Get(const string &key, unique_ptr<KineticRecord>& record)
{
    return Run([&](){return con->blocking().Get(key, record);});
}

KineticStatus SimpleKineticNamespace::Delete(const string &key, const string& version, WriteMode mode)
{
    return Run([&](){return con->blocking().Delete(key, version, mode);});
}

KineticStatus SimpleKineticNamespace::Put(const string &key, const string &current_version, WriteMode mode, const KineticRecord& record)
{
    return Run([&](){return con->blocking().Put(key, current_version, mode, record);});
}

KineticStatus SimpleKineticNamespace::GetVersion(const string &key, unique_ptr<string>& version)
{
    return Run([&](){return con->blocking().GetVersion(key, version);});
}

KineticStatus SimpleKineticNamespace::GetKeyRange(const string &start_key, const string &end_key, unsigned int max_results,
        unique_ptr<vector<string>> &keys)
{
    const bool start_key_inclusive = false;
    const bool end_key_inclusive = false;
    const bool reverse_results = false;
    return Run([&](){return con->blocking().GetKeyRange(start_key, start_key_inclusive, end_key, end_key_inclusive, reverse_results, max_results, keys);});
}

KineticStatus SimpleKineticNamespace::Capacity(kinetic::Capacity &cap)
{
    unique_ptr<kinetic::DriveLog> log;
    KineticStatus status = Run([&](){return con->blocking().GetLog(log);});
    if (status.ok()) {
        cap.remaining_bytes = log->capacity.remaining_bytes;
        cap.total_bytes     = log->capacity.total_bytes;
    }
    return status;
}
