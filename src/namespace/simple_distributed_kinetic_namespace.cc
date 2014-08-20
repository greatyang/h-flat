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
#include "simple_distributed_kinetic_namespace.h"
#include "threadsafe_blocking_connection.h"


using com::seagate::kinetic::client::proto::Message_Algorithm_SHA1;


SimpleDistributedKineticNamespace::SimpleDistributedKineticNamespace(const std::vector< hflat::Partition > &cmap):
        capacity_estimate({100000,0}),capacity_chunksize(0)
{

        std::shared_ptr<kinetic::ConnectionListener> listener(new kinetic::ConnectionListener);
        for(auto &p : cmap){

        kinetic::ConnectionOptions options;
        options.host = p.drives(0).host();
        options.port = p.drives(0).port();
        options.user_id = 1;
        options.hmac_key = "asdfasdf";
        ConnectionPointer con(new kinetic::ThreadsafeBlockingConnection(options, listener));
        connections.push_back(con);
    }
    hashcounter.resize( connections.size(), 0);
}

SimpleDistributedKineticNamespace::~SimpleDistributedKineticNamespace()
{
    printf("Hashcounts per driveID: \n");
    for(size_t i=0; i<hashcounter.size(); i++)
        printf("[%ld] %d\n",i,hashcounter[i]);
}

bool SimpleDistributedKineticNamespace::selfCheck()
{
    return true;
}

ConnectionPointer SimpleDistributedKineticNamespace::keyToCon(const std::string &key)
{
    // only has first component for directory-entry keys (only keys containing '|')
    size_t hashvalue = std::hash<std::string>()(key.substr(0,key.find_first_of("|")));
    int driveID = hashvalue % connections.size();
    hashcounter[driveID]++;
    return connections.at(driveID);
}


KineticStatus SimpleDistributedKineticNamespace::Get(const string &key, unique_ptr<KineticRecord>& record)
{
    ConnectionPointer con = keyToCon(key);
    if(!con) return KineticStatus(kinetic::StatusCode::REMOTE_REMOTE_CONNECTION_ERROR, "Could not obtain connection.");
    return con->Get(key, record);
}

KineticStatus SimpleDistributedKineticNamespace::Delete(const string &key, const string& version, WriteMode mode)
{
    ConnectionPointer con = keyToCon(key);
    if(!con) return KineticStatus(kinetic::StatusCode::REMOTE_REMOTE_CONNECTION_ERROR, "Could not obtain connection.");

    KineticStatus result = con->Delete(key, version, mode);
    if(result.ok()) capacity_estimate.portion_full -= capacity_chunksize;
    return result;
}

KineticStatus SimpleDistributedKineticNamespace::Put(const string &key, const string &current_version, WriteMode mode, const KineticRecord& record)
{
    ConnectionPointer con = keyToCon(key);
    if(!con) return KineticStatus(kinetic::StatusCode::REMOTE_REMOTE_CONNECTION_ERROR, "Could not obtain connection.");

    KineticStatus result = con->Put(key, current_version, mode, record);
    if(result.ok() && current_version.empty()) capacity_estimate.portion_full += capacity_chunksize;
    return result;
}

KineticStatus SimpleDistributedKineticNamespace::GetVersion(const string &key, unique_ptr<string>& version)
{
    ConnectionPointer con = keyToCon(key);
    if(!con) return KineticStatus(kinetic::StatusCode::REMOTE_REMOTE_CONNECTION_ERROR, "Could not obtain connection.");
    return con->GetVersion(key, version);
}

KineticStatus SimpleDistributedKineticNamespace::GetKeyRange(const string &start_key, const string &end_key, unsigned int max_results,
        unique_ptr<vector<string>> &keys)
{
    ConnectionPointer con = keyToCon(start_key);
    if(!con) return KineticStatus(kinetic::StatusCode::REMOTE_REMOTE_CONNECTION_ERROR, "Could not obtain connection.");
    const bool start_key_inclusive = false;
    const bool end_key_inclusive = false;
    const bool reverse_results = false;

    return con->GetKeyRange(start_key, start_key_inclusive, end_key, end_key_inclusive, reverse_results, max_results, keys);
}

KineticStatus SimpleDistributedKineticNamespace::Capacity(kinetic::Capacity &cap)
{
    cap = capacity_estimate;
    return KineticStatus(kinetic::StatusCode::OK, "");
}
