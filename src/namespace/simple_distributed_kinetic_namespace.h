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
#ifndef SIMPLE_DISTRIBUTED_KINETIC_NAMESPACE_H_
#define SIMPLE_DISTRIBUTED_KINETIC_NAMESPACE_H_
#include <random>
#include <vector>
#include "replication.pb.h"
#include "kinetic_namespace.h"

/* Aggregates a number of kinetic drives into a single namespace.
 * Doesn't use any kind or replication or redundancy strategy, see distributed_kinetic_namespace for a real world implementation. */
class SimpleDistributedKineticNamespace final : public KineticNamespace
{
private:
    std::vector< std::shared_ptr<kinetic::BlockingKineticConnection> > connections;
    std::vector< int > hashcounter;
    std::default_random_engine        random_generator;
    kinetic::Capacity                 capacity_estimate;
    float                             capacity_chunksize;

private:
    std::shared_ptr<kinetic::BlockingKineticConnection> keyToCon(const std::string &key);

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

public:
    explicit SimpleDistributedKineticNamespace(const std::vector< hflat::Partition > &clustermap);
    ~SimpleDistributedKineticNamespace();
};


#endif /* SIMPLE_DISTRIBUTED_KINETIC_NAMESPACE_H_ */
