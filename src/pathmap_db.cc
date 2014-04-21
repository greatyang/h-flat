#include "pathmap_db.h"
#include <sys/param.h> /* Just for MAXSYMLINKS #define */ 
#include <thread>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <cassert>
#include "debug.h"

using posixok::MappingType;

PathMapDB::PathMapDB() :
        snapshotVersion(0), snapshot(), currentReaders(), lock()
{
}

PathMapDB::~PathMapDB()
{
}

std::int64_t PathMapDB::getSnapshotVersion() const
{
    std::lock_guard<std::mutex> locker(lock);
    return snapshotVersion;
}

bool PathMapDB::searchPathRecursive(std::string& path, std::int64_t &maxTimeStamp, const bool followReuse, const bool followSymlink) const
{
    /* Found path in snapshot */
    if (snapshot.count(path)) {
        /* Store maximum encountered timestamp */
        maxTimeStamp = std::max(maxTimeStamp, snapshot.at(path).permissionTimeStamp);

        /* There are a number of cases where we DO NOT want to remap the path even though we found it in the snapshot.
         *
         * 1) A MappingType::NONE mapping does not have a destination
         *
         * 2) A MappingType::SYMLINK mapping that is at the very end of the full path should only be followed if the mapping is requested by
         * the fuse readlink function. Otherwise every lookup of the link (e.g. 'ls' of parent directory) would go straight to the destination.
         *
         * 2) A reuse entry after a move entry should be ignored to prevent invalid remaps.
         * Example: mv a b, ln -s a l >> [ b->a, a->*a, l->a ]
         * We want calls to 'b' be mapped to 'a' not to '*a'.
         * At the same time we keep correct functionality for links: calls to 'l' are mapped to '*a'.
         *
         * */
        if ((snapshot.at(path).type == MappingType::NONE) || (!followSymlink && snapshot.at(path).type == MappingType::SYMLINK)
                || (!followReuse && snapshot.at(path).type == MappingType::REUSE)) { /*don't remap*/
        } else
            return true;
    }

    /* Remove last path component and continue if possible */
    auto pos = path.find_last_of("/");

    /* Take this opportunity to check for component-by-component POSIX name compliance. */
    if (path.size() - 1 - pos > NAME_MAX) {
        pok_debug("Last component of path '%s' has %d size.",path.c_str(),path.size()-1-pos);
        maxTimeStamp = -ENAMETOOLONG;
        return false;
    }
    if (pos == std::string::npos)
        return false;
    path.erase(pos, std::string::npos);
    return searchPathRecursive(path, maxTimeStamp, followReuse, true);
}

void PathMapDB::iointercept(std::string &path) const
{
    /* In order to support direct lookup via iointercept, handle ':' in path */
       size_t pos = path.find_first_of(':');;
       while(pos != std::string::npos){
           path[pos]='/';
           pos = path.find_first_of(':',pos);
       }
}

std::string PathMapDB::toSystemPath(const char *user_path, std::int64_t &maxTimeStamp, CallingType ctype) const
{
    int numLinksFollowed = 0;
    bool followReuse = true;
    bool followSymlink = ctype == CallingType::LOOKUP ? false : true;
    maxTimeStamp = 0;
    std::string systemPath(user_path);
    iointercept(systemPath);
    std::string temp(systemPath);


    /* Take this opportunity to check for path POSIX name compliance. */
    if (temp.size() > PATH_MAX) {
        pok_debug("path size > %d",PATH_MAX);
        maxTimeStamp = -ENAMETOOLONG;
        return systemPath;
    }
    {
        std::lock_guard<std::mutex> locker(lock);
        ++currentReaders;
    }

    while (searchPathRecursive(temp, maxTimeStamp, followReuse, followSymlink)) {
        if (snapshot.at(temp).type == MappingType::SYMLINK) {
            /* Guard against symbolic link loops */
            if (++numLinksFollowed > MAXSYMLINKS) {
                maxTimeStamp = -ELOOP;
                break;
            }
        }
        /* Don't follow reuse mapping after a move mapping. */
        followReuse = (snapshot.at(temp).type == MappingType::MOVE) ? false : true;

        /* Apply mapping to path */
        temp = systemPath.replace(0, temp.size(), snapshot.at(temp).target);
    }

    --currentReaders;
    return systemPath;
}

posixok::db_snapshot PathMapDB::serializeSnapshot()
{
    posixok::db_snapshot s;

    std::lock_guard<std::mutex> locker(lock);
    while (currentReaders.load())
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

    pok_debug("Serializing snapshot version %d.",snapshotVersion);

    for( auto e : snapshot ){
        posixok::db_snapshot_entry * re = s.add_entries();

        re->set_origin(e.first);
        re->set_target(e.second.target);
        re->set_type(e.second.type);
        re->set_permissiontimestamp(e.second.permissionTimeStamp);
    }
    s.set_snapshot_version(snapshotVersion);
    return s;
}

int PathMapDB::loadSnapshot(const posixok::db_snapshot & serialized)
{
    pok_debug("Current snapshot version is %d, loading remote snapshot version %d. ", snapshotVersion, serialized.snapshot_version());

    /* No need to update */
    if (serialized.snapshot_version() <= snapshotVersion)
        return 0;

    std::lock_guard<std::mutex> locker(lock);
    while (currentReaders.load())
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

    this->snapshotVersion = serialized.snapshot_version();
    this->snapshot.clear();
    for( auto e : serialized.entries() )
        snapshot[e.origin()] = PMEntry { e.type(), e.has_target() ? e.target() : "", e.permissiontimestamp()};

    return 0;
}

int PathMapDB::updateSnapshot(const std::list<posixok::db_entry> &entries, std::int64_t fromVersion, std::int64_t toVersion)
{
    /* sanity checks */
    assert(entries.size() == (size_t )(toVersion - fromVersion));
    assert(fromVersion <= snapshotVersion);
    pok_debug("Current snapshot version = %d, updating interval [%d , %d]", snapshotVersion, fromVersion, toVersion);

    /* No need to update */
    if (toVersion <= snapshotVersion)
        return 0;

    std::int64_t currentVersion = snapshotVersion;
    for (auto& entry : entries) {
        currentVersion++;

        if (currentVersion < fromVersion)
            continue;

        switch (entry.type()) {
        case posixok::db_entry_Type_MOVE:
            addDirectoryMove(entry.origin(), entry.target());
            break;
        case posixok::db_entry_Type_SYMLINK:
            addSoftLink(entry.origin(), entry.target());
            break;
        case posixok::db_entry_Type_NONE:
            addPermissionChange(entry.origin());
            break;
        case posixok::db_entry_Type_REMOVED:
            addUnlink(entry.origin());
            break;
        default:
            pok_warning("Invalid database entry supplied. Resetting pathmapDB");
            std::lock_guard<std::mutex> locker(lock);
            while (currentReaders.load())
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            this->snapshot.clear();
            this->snapshotVersion = 0;
            return -EINVAL;
        }
    }
    snapshotVersion = toVersion;
    return 0;
}

void PathMapDB::addSoftLink(std::string origin, std::string destination)
{
    iointercept(origin); iointercept(destination);
    std::lock_guard<std::mutex> locker(lock);
    while (currentReaders.load())
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

    snapshotVersion++;

    if (snapshot.count(origin))
        assert(snapshot[origin].type == MappingType::REUSE);

    /* Keep possibly existing reuse move mapping in order to enable lookups on the inode of the
     * symbolic link. */
    snapshot[ snapshot.count(origin) ? snapshot[origin].target : origin ] =
    {   MappingType::SYMLINK, std::string(destination), 0};
    printSnapshot();
}

void PathMapDB::addPermissionChange(std::string path)
{
    iointercept(path);
    std::lock_guard<std::mutex> locker(lock);
    while (currentReaders.load())
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

    snapshotVersion++;

    int exist = snapshot.count(path);
    snapshot[path] = exist ? PMEntry { snapshot[path].type, snapshot[path].target, snapshotVersion } :
                             PMEntry { MappingType::NONE, std::string(), snapshotVersion };

    printSnapshot();
}

/* Keep in mind that the client validated the operation against the file system at this point. This means: 
 * the path 'origin' exists and is a directory. 
 * the path 'destination' does not exist or is an empty directory. 
 * the path 'destination' does not specify a sub-directory of 'origin'
 * access permissions are validated. */
void PathMapDB::addDirectoryMove(std::string origin, std::string destination)
{
    iointercept(origin); iointercept(destination);
    std::lock_guard<std::mutex> locker(lock);
    while (currentReaders.load())
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

    snapshotVersion++;

    /* No existing mapping... the standard case. 
     * mv /a /b  [b->a, a->X]  */
    if (!snapshot.count(origin)) {
        snapshot[destination] = {MappingType::MOVE, origin, 0};
        snapshot[origin] = {MappingType::REUSE, "|reuse_"+std::to_string(snapshotVersion), 0};
    }

    /* There's an existing mapping with key==origin, update it. If existing mapping is of type REUSE a new REUSE mapping has to be generated.
     * mv /a /b [b->a, a->X1]
     * mv /b /c [c->a, a->X1]
     * mv /a /d [c->a, d->X1, a->X2]
     */
    else {
        MappingType type = snapshot[origin].type;
        assert(type == MappingType::MOVE || type == MappingType::REUSE);

        snapshot[destination] = {MappingType::MOVE, snapshot[origin].target, snapshot[origin].permissionTimeStamp};

        if(type == MappingType::REUSE)
        snapshot[origin].target = "|reuse_"+std::to_string(snapshotVersion);
        else
        snapshot.erase(origin);
    }

    /* Special case: Circular move 
     *  mv /a /b [b->a, a->X]
     *  mv /b /a [] */
    if (destination == snapshot[destination].target) {
        snapshot.erase(destination);
        snapshot.erase(origin);
    }
    printSnapshot();
}

void PathMapDB::addUnlink(std::string path)
{
    iointercept(path);
    std::lock_guard<std::mutex> locker(lock);
    while (currentReaders.load())
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

    snapshotVersion++;

    if(!snapshot.count(path))
        pok_warning("Received addUnlink request for key %s that IS NOT in the current database.", path.data());

    snapshot.erase(path);
    printSnapshot();
}

bool PathMapDB::hasMapping(std::string path)
{
    iointercept(path);
    return snapshot.count(path);
}

void PathMapDB::printSnapshot() const
{
    /* print entries in the form " key->target [type,timestamp] " */
    auto typeToString = [](MappingType t) -> std::string {
        switch(t) {
            case MappingType::MOVE: return "MOVE";
            case MappingType::REUSE: return "REUSE";
            case MappingType::SYMLINK: return "SYMLINK";
            case MappingType::NONE: return "NONE";
        }
        return "INVALID";
    };
    std::cout << "[----------------------------------------]" << std::endl;
    for (auto& element : snapshot)
        std::cout << "  " << std::setw(10) << std::left << element.first << " -> " << std::setw(10) << std::left << element.second.target << " ["
                << typeToString(element.second.type) << "," << element.second.permissionTimeStamp << "]" << std::endl;
    std::cout << "[----------------------------------------]" << std::endl;
}
