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
#include "simple_kinetic_namespace.h"
#include "threadsafe_blocking_connection.h"
#include <exception>
#include <stdexcept>

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

#include "debug.h"
void SimpleKineticNamespace::connect()
{
    options.user_id = 1;
    options.hmac_key = "asdfasdf";

    std::shared_ptr<kinetic::NonblockingKineticConnection> shared_con;
    kinetic::KineticConnectionFactory factory = kinetic::NewKineticConnectionFactory();
    kinetic::Status status = factory.NewThreadsafeNonblockingConnection(options, shared_con);
    if (status.notOk())
        throw std::runtime_error(status.ToString());
    con.reset(new kinetic::ThreadsafeBlockingConnection(shared_con, 5));

    unique_ptr<kinetic::DriveLog> log;
    KineticStatus logstatus = con->GetLog(log);
    if(logstatus.ok()){
        capacity_estimate  = log->capacity;
        capacity_chunksize = ((float)1024*1024) / (float)capacity_estimate.nominal_capacity_in_bytes;
    }
    else
       hflat_warning("Failed obtaining drive capacity: %s",logstatus.message().c_str());
}

bool SimpleKineticNamespace::selfCheck()
{
    if(con->NoOp().ok())
        return true;
    return false;
}

KineticStatus SimpleKineticNamespace::Get(const string &key, unique_ptr<KineticRecord>& record)
{
    return con->Get(key, record);
}

KineticStatus SimpleKineticNamespace::Delete(const string &key, const string& version, WriteMode mode)
{
    KineticStatus result = con->Delete(key, version, mode);

    if(result.ok()) capacity_estimate.portion_full -= capacity_chunksize;
    return result;
}

KineticStatus SimpleKineticNamespace::Put(const string &key, const string &current_version, WriteMode mode, const KineticRecord& record)
{
    KineticStatus result = con->Put(key, current_version, mode, record);
    if(result.ok() && current_version.empty()) capacity_estimate.portion_full += capacity_chunksize;
    return result;
}

KineticStatus SimpleKineticNamespace::GetVersion(const string &key, unique_ptr<string>& version)
{
    return con->GetVersion(key, version);
}

KineticStatus SimpleKineticNamespace::GetKeyRange(const string &start_key, const string &end_key, unsigned int max_results,
        unique_ptr<vector<string>> &keys)
{
    const bool start_key_inclusive = false;
    const bool end_key_inclusive = false;
    const bool reverse_results = false;
    return con->GetKeyRange(start_key, start_key_inclusive, end_key, end_key_inclusive, reverse_results, max_results, keys);
}

KineticStatus SimpleKineticNamespace::Capacity(kinetic::Capacity &cap)
{
    cap = capacity_estimate;
    return KineticStatus(kinetic::StatusCode::OK, "");
}
