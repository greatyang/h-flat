#include "metadata_info.h"
#include <chrono>
#include <sys/stat.h>
#include <errno.h>



MetadataInfo::MetadataInfo() :
        systemPath(), currentVersion(0), aggregate_write()
{
}

MetadataInfo::MetadataInfo(const std::string &key) :
        systemPath(key), currentVersion(0), aggregate_write()
{
}

MetadataInfo::~MetadataInfo()
{
}

const std::string & MetadataInfo::getSystemPath()
{
    return systemPath;
}

void MetadataInfo::setSystemPath(const std::string &path)
{
    systemPath = path;
}

std::int64_t MetadataInfo::getCurrentVersion()
{
    return currentVersion;
}

void MetadataInfo::setCurrentVersion(std::int64_t version)
{
    currentVersion = version;
}


void MetadataInfo::updateACMtime()
{
    std::time_t now;
    std::time(&now);
    md_mutable.set_atime(now);
    md_mutable.set_mtime(now);
    md_mutable.set_ctime(now);
}

void MetadataInfo::updateACtime()
{
    std::time_t now;
    std::time(&now);
    md_mutable.set_atime(now);
    md_mutable.set_ctime(now);
}

bool MetadataInfo::isDirty()
{
    if(aggregate_write && aggregate_write->hasUpdates())
        return true;
    return false;
}

bool MetadataInfo::setAggregate(std::shared_ptr<DataInfo>& di)
{
    if(aggregate_write == di) return true;
    if(isDirty()) return false;
    aggregate_write = di;
    return true;
}
std::shared_ptr<DataInfo>& MetadataInfo::getAggregate()
{
    return aggregate_write;
}

/* If someone has a reasonable idea how to code this function PLEASE let me know */
bool MetadataInfo::mergeMD(const posixok::Metadata & md_update, std::int64_t version)
{
    /* type change disallows merge */
    if(md_mutable.type() != md_update.type() )
        return false;

    /* cannot merge 2 distinct updates, but 1 update is ok either direction */
     if(md_mutable.mode() !=  md_update.mode()){
         if(md_const.mode() == md_mutable.mode()) md_mutable.set_mode( md_update.mode() );
         else if(md_const.mode() != md_update.mode()) return false;
     }
     if(md_mutable.gid() !=  md_update.gid()){
         if(md_const.gid() == md_mutable.gid()) md_mutable.set_gid( md_update.gid() );
         else if(md_const.gid() != md_update.gid()) return false;
     }
     if(md_mutable.uid() !=  md_update.uid()){
         if(md_const.uid() == md_mutable.uid()) md_mutable.set_uid( md_update.uid() );
         else if(md_const.uid() != md_update.uid()) return false;
     }

     /* not sure to merge size & blocks correctly, going to treat them as unmergable for now  */
     if(md_mutable.size() !=  md_update.size()){
         if(md_const.size() == md_mutable.size()) md_mutable.set_size( md_update.size() );
         else if(md_const.size() != md_update.size()) return false;
     }
     if(md_mutable.blocks() !=  md_update.blocks()){
         if(md_const.blocks() == md_mutable.blocks()) md_mutable.set_blocks( md_update.blocks() );
         else if(md_const.blocks() != md_update.blocks()) return false;
     }

     typedef ::google::protobuf::RepeatedPtrField< ::posixok::Metadata_ReachabilityEntry > rEntries;
     auto equal = [](const rEntries &a, const rEntries &b)
     {
         auto equal = [](const posixok::Metadata_ReachabilityEntry &lhs, const posixok::Metadata_ReachabilityEntry &rhs) -> bool {
                if(lhs.uid() != rhs.uid())   return false;
                if(lhs.gid() != rhs.gid())   return false;
                if(lhs.type()!= rhs.type())  return false;
                return true;
            };
         if(a.size() != b.size()){
             return false;
         }
         for(int i=0; i<a.size(); i++){
             if(!equal(a.Get(i), b.Get(i)))
                 return false;
         }
         return true;
     };

     if(!equal( md_mutable.path_permission(), md_update.path_permission()) ){
         if(equal( md_const.path_permission(), md_mutable.path_permission()) ){
             md_mutable.mutable_path_permission()->Clear();
             md_mutable.mutable_path_permission()->CopyFrom(md_update.path_permission());
         }
         else if(!equal(md_const.path_permission(), md_update.path_permission() ))
             return false;
     }

     if(!equal( md_mutable.path_permission_children(), md_update.path_permission_children()) ){
         if(equal( md_const.path_permission_children(), md_mutable.path_permission_children()) ){
             md_mutable.mutable_path_permission_children()->Clear();
             md_mutable.mutable_path_permission_children()->CopyFrom(md_update.path_permission_children());
         }
         else if(!equal(md_const.path_permission_children(), md_update.path_permission_children() ))
             return false;
     }


     typedef ::google::protobuf::RepeatedPtrField< ::posixok::Metadata_ExtendedAttribute > xAttrs;
     auto equalX = [](const xAttrs &a, const xAttrs &b)
     {
         auto equal = [](const posixok::Metadata_ExtendedAttribute &lhs, const posixok::Metadata_ExtendedAttribute &rhs) -> bool {
                if(lhs.name().compare(rhs.name()))   return false;
                if(lhs.value().compare(rhs.value())) return false;
                  return true;
            };

         if(a.size() != b.size()){
             return false;
         }
         for(int i=0; i<a.size(); i++){
             if(!equal(a.Get(i), b.Get(i)))
                 return false;
         }
         return true;
     };

    // xattr should be treated as an incremental update as long as there's no double add or double delete, but
     // i'm going to treat it as unmergable now.
    if(!equalX( md_mutable.xattr(), md_update.xattr()) ){
        if(equalX( md_const.xattr(), md_mutable.xattr()) ){
            md_mutable.mutable_xattr()->Clear();
            md_mutable.mutable_xattr()->CopyFrom(md_update.xattr());
        }
        else if(!equalX(md_const.xattr(), md_update.xattr() ))
            return false;
    }

    /* incremental updates, apply + -  */
    int lchange = md_mutable.link_count() - md_const.link_count();
    md_mutable.set_link_count( md_update.link_count() + lchange );

    /* one-way values: the higher, the more correct. obviously.  */
    md_mutable.set_atime( std::max(  md_mutable.atime(), md_update.atime() ) );
    md_mutable.set_mtime( std::max(  md_mutable.mtime(), md_update.mtime() ) );
    md_mutable.set_ctime( std::max(  md_mutable.ctime(), md_update.ctime() ) );
    md_mutable.set_force_update_version( std::max( md_mutable.force_update_version(), md_update.force_update_version() ) );
    md_mutable.set_path_permission_verified( std::max( md_mutable.path_permission_verified(), md_update.path_permission_verified() ) );

    currentVersion = version;
    md_const = md_update;
    return true;
}

// returns false if there are local changes in an already stored md structure that might conflict with the new version
void MetadataInfo::setMD(const posixok::Metadata & md, std::int64_t version)
{
    md_mutable = md;
    md_const = md;
    currentVersion = version;
}

posixok::Metadata & MetadataInfo::getMD()
{
    return md_mutable;
}

bool MetadataInfo::computePathPermissionChildren()
{
    /* Execute permission for user / group / other */
    bool user =  md_mutable.mode() & S_IXUSR;
    bool group = md_mutable.mode() & S_IXGRP;
    bool other = md_mutable.mode() & S_IXOTH;

    std::vector<posixok::Metadata_ReachabilityEntry> v;
    posixok::Metadata_ReachabilityEntry e;
    e.set_uid(md_mutable.uid());
    e.set_gid(md_mutable.gid());
    auto addEntry = [&v,&e](posixok::Metadata_ReachabilityType type) -> void {
        e.set_type(type);
        v.push_back(e);
        assert(v.size() <= 2);
    };

    /* Check if access is restricted to a specific user and / or group */
    if (!other) {
        if (user && group)
            addEntry(posixok::Metadata_ReachabilityType_UID_OR_GID);
        if (user && !group)
            addEntry(posixok::Metadata_ReachabilityType_UID);
        if (!user && group)
            addEntry(posixok::Metadata_ReachabilityType_GID);
    }

    /* Check if a specific user and / or group is excluded. */
    if (!user)
        addEntry(posixok::Metadata_ReachabilityType_NOT_UID);
    if (!group) {
        if (!user)
            addEntry(posixok::Metadata_ReachabilityType_NOT_GID);
        if (user && other) // owner can access despite excluded group
            addEntry(posixok::Metadata_ReachabilityType_GID_REQ_UID);
    }

    auto equal = [](const posixok::Metadata_ReachabilityEntry &lhs, const posixok::Metadata_ReachabilityEntry &rhs) -> bool {
        if(lhs.uid() != rhs.uid())      return false;
        if(lhs.gid() != rhs.gid())      return false;
        if(lhs.type() != rhs.type())    return false;
        return true;
    };
    /* Remove all entries that are duplicates of entries stored in pathPermission.
     * If a restriction is already enforced by a parent directory, there's no need to enforce it again. */
    for (int i = 0; i < md_mutable.path_permission_size(); i++)
        for (auto it = v.begin(); it != v.end(); it++)
            if (equal(md_mutable.path_permission(i), *it))
                v.erase(it);


    auto ppc_contains = [this, equal](const posixok::Metadata_ReachabilityEntry &e) -> bool {
        for (int i = 0; i < md_mutable.path_permission_children_size(); i++)
            if (equal(md_mutable.path_permission_children(i), e))
                return true;
        return false;
    };
    /* Check if path_permission_children changed, if yes store in md. */
    bool changed = false;
    if(md_mutable.path_permission_children_size() != (int)v.size()) changed = true;
    for (auto it = v.begin(); it != v.end(); it++)
        if(!ppc_contains(*it)) changed = true;

    if (changed) {
        md_mutable.mutable_path_permission_children()->Clear();
        for (auto it = v.begin(); it != v.end(); it++) {
            posixok::Metadata_ReachabilityEntry *entry = md_mutable.mutable_path_permission_children()->Add();
            entry->CopyFrom(*it);
        }
    }
    return changed;
}
