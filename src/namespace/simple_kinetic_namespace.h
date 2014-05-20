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
#ifndef SIMPLE_KINETIC_NAMESPACE_H_
#define SIMPLE_KINETIC_NAMESPACE_H_
#include "kinetic_namespace.h"
#include "replication.pb.h"

/* Simple one-drive implementation. */
class SimpleKineticNamespace final : public KineticNamespace
{
private:
    kinetic::ConnectionOptions        options;
    kinetic::Capacity                 capacity_estimate;
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
    bool          selfCheck();
public:
    explicit SimpleKineticNamespace(const hflat::KineticDrive &d);
    explicit SimpleKineticNamespace();
    ~SimpleKineticNamespace();
};


#endif /* SIMPLE_KINETIC_NAMESPACE_H_ */
