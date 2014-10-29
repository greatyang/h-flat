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
#include <exception>
#include <stdexcept>
#include "debug.h"


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
    options.user_id  = 1;
    options.hmac_key = "asdfasdf";
    options.use_ssl  = false;
    kinetic::KineticConnectionFactory factory = kinetic::NewKineticConnectionFactory();
    unique_ptr<kinetic::ThreadsafeBlockingKineticConnection> bcon;
    kinetic::Status s = factory.NewThreadsafeBlockingConnection(options, bcon, 60);
    if(s.notOk())
        throw std::runtime_error(s.ToString());
    con = std::move(bcon);
}

bool SimpleKineticNamespace::selfCheck()
{
    return con->NoOp().ok();
}

KineticStatus SimpleKineticNamespace::Get(const string &key, unique_ptr<KineticRecord>& record)
{
    hflat_trace("Get '%s'",key.c_str());
    return con->Get(key, record);
}

KineticStatus SimpleKineticNamespace::Delete(const string &key, const string& version, WriteMode mode)
{
    hflat_trace("Delete '%s'",key.c_str());
    return con->Delete(key, version, mode);
}

KineticStatus SimpleKineticNamespace::Put(const string &key, const string &current_version, WriteMode mode, const KineticRecord& record)
{
    hflat_trace("Put '%s'",key.c_str());
    return con->Put(key, current_version, mode, record);
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

KineticStatus SimpleKineticNamespace::GetCapacity(kinetic::Capacity &cap)
{
   unique_ptr<kinetic::DriveLog> log;
   vector<kinetic::Command_GetLog_Type> types;
   types.push_back(kinetic::Command_GetLog_Type::Command_GetLog_Type_CAPACITIES);

   KineticStatus status = con->GetLog(types,log);
   if(status.ok())
       cap = log->capacity;
   return status;
}
