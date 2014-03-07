#include "main.h"
#include "debug.h"
#include "kinetic_helper.h"

#include <execinfo.h>

/* This version of the lookup function bypasses the lookup cache and all verification / manipulation of read in metadata, making
 * it in essence a user-path based metadata_get */
int get_metadata_userpath(const char *user_path, std::shared_ptr<MetadataInfo> &mdi)
{
    std::string key, value, version;
    std::int64_t pathPermissionTimeStamp = 0;

    /* Step 1: Transform user path to system path and obtain required path permission timestamp */
    key = PRIV->pmap.toSystemPath(user_path, pathPermissionTimeStamp, CallingType::LOOKUP);
    if (pathPermissionTimeStamp < 0)
        return pathPermissionTimeStamp;

    /* Step 2: Get metadata from flat namespace */
    mdi.reset(new MetadataInfo(key));
    return get_metadata(mdi);
}


/* For full functionality, all group checks should do in_group checks instead of straight
 * comparisons, compare chown. */
static int check_path_permissions(const google::protobuf::RepeatedPtrField<posixok::Metadata_ReachabilityEntry > &path_permissions){


    for(int i=0; i < path_permissions.size(); i++){
        const posixok::Metadata_ReachabilityEntry &e = path_permissions.Get(i);

        switch(e.type()){

        case posixok::Metadata_ReachabilityType_UID:
            if(fuse_get_context()->uid != e.uid())
                return -EACCES;
            break;
        case posixok::Metadata_ReachabilityType_GID:
            if(fuse_get_context()->gid != e.gid())
                return -EACCES;
            break;
        case posixok::Metadata_ReachabilityType_UID_OR_GID:
            if(fuse_get_context()->uid != e.uid())
                if(fuse_get_context()->gid != e.gid())
                    return -EACCES;
            break;
        case posixok::Metadata_ReachabilityType_NOT_GID:
            if(fuse_get_context()->gid == e.gid())
               return -EACCES;
            break;
        case posixok::Metadata_ReachabilityType_NOT_UID:
            if(fuse_get_context()->uid == e.uid())
                return -EACCES;
            break;
        case posixok::Metadata_ReachabilityType_GID_REQ_UID:
            if(fuse_get_context()->gid == e.gid())
                if(fuse_get_context()->uid != e.uid())
                    return -EACCES;
            break;
        }
    }
    return 0;
}


int lookup(const char *user_path, std::shared_ptr<MetadataInfo> &mdi)
{
    std::string key, value, version;
    std::int64_t pathPermissionTimeStamp = 0;

    /* Step 1: Transform user path to system path and obtain required path permission timestamp */
    key = PRIV->pmap.toSystemPath(user_path, pathPermissionTimeStamp, CallingType::LOOKUP);
    if (pathPermissionTimeStamp < 0)
        return pathPermissionTimeStamp;

    /* Step 2: Get metadata from flat namespace */
    bool cached = PRIV->lookup_cache.get(key, mdi);

    if(!cached){
        mdi.reset(new MetadataInfo(key));
        int err = get_metadata(mdi);
        if (err == -ENOENT){
            PRIV->lookup_cache.add(key,mdi);
        }
        if (err) return err;
    }
    if(mdi->getMD().inode_number() == 0)
        return -ENOENT;

    /* Step 3: Special inode types: Follow hardlink_source inode types in the lookup operation, update on force_update. */
    if (mdi->getMD().type() == posixok::Metadata_InodeType_HARDLINK_S) {
        if(!cached){
            pok_debug("type HARDLINK_S for user_path %s",user_path);
            std::shared_ptr<MetadataInfo> mdi_source(new MetadataInfo(key));
            mdi_source->getMD().set_type( posixok::Metadata_InodeType_HARDLINK_S );
            mdi_source->getMD().set_inode_number( mdi->getMD().inode_number() );
            PRIV->lookup_cache.add(key,mdi_source);
        }
        std::string hlkey = "hardlink_" + std::to_string(mdi->getMD().inode_number());
        return lookup(hlkey.c_str(), mdi);

    }
    if (mdi->getMD().type() == posixok::Metadata_InodeType_FORCE_UPDATE) {
        if (int err = util::database_update()){
            pok_debug("encountered force_update inode in regular lookup and couldn't update database");
            return err;
        }
        return lookup(user_path, mdi);
    }

    if(!cached) PRIV->lookup_cache.add(key,mdi);

    /* Step 4: check path permissions, update (recursively as necessary) in case of staleness */
    bool stale = mdi->getMD().path_permission_verified() < pathPermissionTimeStamp;
    if (stale) {
        pok_debug("stale path permission for path %s (%d vs required %d )",user_path, mdi->getMD().path_permission_verified(), pathPermissionTimeStamp);
        std::shared_ptr<MetadataInfo> mdi_parent;
        if (int err = lookup_parent(user_path, mdi_parent)){
            if(err != -EACCES) return err;
        }
        inherit_path_permissions(mdi, mdi_parent);
        int err = put_metadata(mdi);
        if (err == -EAGAIN ) return lookup(user_path, mdi);
        if (err) return err;
    }
    return check_path_permissions(mdi->getMD().path_permission());
}


/* Lookup parent directory of supplied user path. */
int lookup_parent(const char *user_path, std::shared_ptr<MetadataInfo> &mdi_parent)
{
    std::string path(user_path);
    auto pos = path.find_last_of("/:");
    if (pos == std::string::npos)
        return -EINVAL;
    if (!pos) // root directory
        pos++;
    path.erase(pos, std::string::npos);

    if (int err = lookup(path.c_str(), mdi_parent))
        return err;

    if (!S_ISDIR(mdi_parent->getMD().mode()))
        return -ENOTDIR;

    return check_path_permissions(mdi_parent->getMD().path_permission_children());
}
