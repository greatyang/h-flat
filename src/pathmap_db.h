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
#ifndef PATHMAPDB_H
#define PATHMAPDB_H
#include <unordered_map>
#include <atomic>
#include <mutex>
#include <string>
#include <list>
#include "database.pb.h"

/* There are slight differences in handling path mapping depending on if a terminating symbolic link should
 * be followed (open) or not (readlink). */
enum class CallingType
{
    LOOKUP, READLINK
};

class PathMapDB final
{
    private:
        struct PMEntry
        {
            hflat::MappingType    type;
            std::string             target;
            std::int64_t            permissionTimeStamp;
        };

        /* Version & hashmap of current snapshot */
        std::int64_t snapshotVersion;
        std::unordered_map<std::string, PMEntry> snapshot;

        /* Synchronization: Ensure nobody is reading while the snapshot is being updated. */
        mutable std::atomic<int> currentReaders;
        mutable std::mutex lock;

    private:
        void iointercept(std::string &path) const;
        bool searchPathRecursive(std::string& path, std::int64_t &maxTimeStamp, const bool afterMove, const bool followSymlink) const;

    public:
        explicit PathMapDB();
        ~PathMapDB();
        PathMapDB(const PathMapDB& rhs) = delete;
        PathMapDB& operator=(const PathMapDB& rhs) = delete;

    public:
        /* Remaps supplied path according to current database snapshot. 
         * userPath -> systemPath
         * minimal required path permission timestamp value is stored in supplied integer. */
        std::string toSystemPath(const char * user_path, std::int64_t &permissionTimeStamp, CallingType ctype) const;

        /* Return current database snapshot version */
        std::int64_t getSnapshotVersion() const;

        /* Serialize the current snapshot version into a db_snapshot protobuf structure so it can be stored in remote storage. */
        hflat::db_snapshot serializeSnapshot();

        /* Load a the supplied database snapshot. Current in-memory snapshot will be overwritten. */
        int loadSnapshot(const hflat::db_snapshot & snap);

        /* Update the current database snapshot to given version using the supplied list of new entries.
         * Returns 0 on success or a negative error code */
        int updateSnapshot(const std::list<hflat::db_entry> &entries, std::int64_t fromVersion, std::int64_t toVersion);

        /* It is the responsiblity of the client to ensure that these functions are only called after
         * a successful update of the remote database (as an alternative of calling updateSnapshot()). */
        void addDirectoryMove(std::string origin, std::string destination);
        void addSoftLink(std::string origin, std::string destination);
        void addPermissionChange(std::string path);
        void addUnlink(std::string path);

        /* Used to decide if a mapping has to be removed when a directory or link is deleted. */
        bool hasMapping(std::string path);

        /* DEBUG ONLY */
        void printSnapshot() const;
    };

#endif // PATHMAPDB_H
