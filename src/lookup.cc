#include "main.h"
#include "debug.h"
#include "kinetic_helper.h"

int lookup(const char *user_path, const std::unique_ptr<MetadataInfo> &mdi, bool handle_special_inodes)
{
    std::string key, value, version;
    std::int64_t pathPermissionTimeStamp = 0;

    /* Step 1: Transform user path to system path and obtain required path permission timestamp */
    key = PRIV->pmap->toSystemPath(user_path, pathPermissionTimeStamp, CallingType::LOOKUP);
    if (pathPermissionTimeStamp < 0)
        return pathPermissionTimeStamp;

    if (key.compare(user_path))
        pok_trace("Remapped user path %s to system path %s ", user_path, key.c_str());

    /* Step 2: Get metadata from flat namespace */
    mdi->setSystemPath(key);
    if (int err = get_metadata(mdi.get()))
        return err;

    /* Step 3: Special inode types: Follow hardlink_source inode types in the lookup operation, update on force_update. */
    if (handle_special_inodes) {
        if (mdi->getMD().type() == posixok::Metadata_InodeType_HARDLINK_S) {
            mdi->setSystemPath("hardlink_" + std::to_string(mdi->getMD().inode_number()));
            if (int err = get_metadata(mdi.get()))
                return err;
        }
        if (mdi->getMD().type() == posixok::Metadata_InodeType_FORCE_UPDATE) {
            if (int err = database_update())
                return err;
            return lookup(user_path, mdi, false); // only one special inode possible
        }
    }

    /* Step 4: check path permissions for staleness */
    bool stale = mdi->getMD().path_permission_verified() < pathPermissionTimeStamp;
    if (stale) {
        /* TODO: validate path permissions up the directory tree, recursively as necessary */
        pok_warning("Stale path permissions detected. Re-validation not implemented yet.");
    }
    return 0;
}

/* Lookup parent directory of supplied user path. */
int lookup_parent(const char *user_path, const std::unique_ptr<MetadataInfo> &mdi_parent)
{
    std::string key(user_path);
    auto pos = key.find_last_of("/");
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
