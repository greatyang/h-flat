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
    if(!mdi) mdi.reset(new MetadataInfo());
    mdi->setSystemPath(key);
    return get_metadata(mdi);
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
    bool cached = PRIV->lookup_cache.get(key,mdi);

    if(!cached){
        if(!mdi) mdi.reset(new MetadataInfo());
        mdi->setSystemPath(key);
        int err = get_metadata(mdi);
        if (err == -ENOENT){
            mdi->setCurrentVersion(-1);
            PRIV->lookup_cache.add(key,mdi);
        }
        if (err) return err;
    }
    if(mdi->getCurrentVersion() == -1)
        return -ENOENT;

    /* Step 3: Special inode types: Follow hardlink_source inode types in the lookup operation, update on force_update. */
    if (mdi->getMD().type() == posixok::Metadata_InodeType_HARDLINK_S) {
        if(!cached){
            pok_debug("hardlink_s found at key %s.", key.c_str());
            std::shared_ptr<MetadataInfo> mdi_source(new MetadataInfo(key));
            mdi_source->getMD().set_type( posixok::Metadata_InodeType_HARDLINK_S );
            mdi_source->getMD().set_inode_number( mdi->getMD().inode_number() );
            PRIV->lookup_cache.add(key,mdi_source);
        }
        std::string hlkey = "hardlink_" + std::to_string(mdi->getMD().inode_number());
        return lookup(hlkey.c_str(), mdi);

    }
    if (mdi->getMD().type() == posixok::Metadata_InodeType_FORCE_UPDATE) {
        pok_debug("force_metadata_update found at key %s.",key.c_str());
        std::int64_t _version = PRIV->pmap.getSnapshotVersion();
        if (int err = database_update())
            return err;
        if(PRIV->pmap.getSnapshotVersion() > _version)
            return lookup(user_path, mdi);
        return -EINVAL;
    }

    if(!cached)
         PRIV->lookup_cache.add(key,mdi);

    /* Step 4: check path permissions for staleness */
    bool stale = mdi->getMD().path_permission_verified() < pathPermissionTimeStamp;
    if (stale) {
        /* TODO: validate path permissions up the directory tree, recursively as necessary */
        pok_warning("Stale path permissions detected. Re-validation not implemented yet.");
    }
    return 0;
}


/* Lookup parent directory of supplied user path. */
int lookup_parent(const char *user_path, std::shared_ptr<MetadataInfo> &mdi_parent)
{
    std::string key(user_path);
    auto pos = key.find_last_of("/:");
    if (pos == std::string::npos)
        return -EINVAL;
    if (!pos) // root directory
        pos++;
    key.erase(pos, std::string::npos);

    if (int err = lookup(key.c_str(), mdi_parent))
        return err;

    if (!S_ISDIR(mdi_parent->getMD().mode()))
        return -ENOTDIR;
    return 0;
}
