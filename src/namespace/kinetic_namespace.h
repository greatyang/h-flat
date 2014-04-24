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
using std::unique_ptr;
using std::vector;
using std::string;

class KineticNamespace
{
public:
    virtual KineticStatus Get(const string &key, unique_ptr<KineticRecord>& record) = 0;
    virtual KineticStatus Delete(const string &key, const string& version, WriteMode mode) = 0;
    virtual KineticStatus Put(const string &key, const string &current_version, WriteMode mode, const KineticRecord& record) = 0;
    virtual KineticStatus GetVersion(const string &key, unique_ptr<string>& version) = 0;
    virtual KineticStatus GetKeyRange(const string &start_key, const string &end_key, unsigned int max_results, unique_ptr<vector<string>> &keys) = 0;
    virtual KineticStatus Capacity(kinetic::Capacity &cap) = 0;

    /* Waiting for P2P operation support
     *     virtual KineticStatus DeleteKeyRangeAsync(const string &start_key, const string &end_key); */
    virtual bool selfCheck() = 0;
    virtual ~KineticNamespace(){};
};

#endif /* KINETIC_NAMESPACE_H_ */
