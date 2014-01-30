#include "main.h"
#include "debug.h"
#include "fuseops.h"
#include "kinetic_helper.h"
#include <algorithm>

/** Create a directory */
int pok_mkdir(const char *user_path, mode_t mode)
{
    return pok_create(user_path, mode | S_IFDIR);
}

/** Remove a directory */
int pok_rmdir(const char *user_path)
{
    unique_ptr<MetadataInfo> mdi(new MetadataInfo());
    if (int err = lookup(user_path, mdi))
        return err;

    string keystart = std::to_string(mdi->getMD().inode_number()) + ":";
    string keyend   = std::to_string(mdi->getMD().inode_number()) + ":" + static_cast<char>(251);
    unique_ptr<vector<string>> keys(new vector<string>());
    PRIV->kinetic->GetKeyRange(keystart, keyend, 1, keys);

    pok_debug("Found %d entries for user_path '%s' (inode number %d) ",keys->size(), user_path, mdi->getMD().inode_number());
    if (keys->size())
        return -ENOTEMPTY;

    if(int err = database_update())
        return err;
    return pok_unlink(user_path);
}

int create_directory_entry(const std::unique_ptr<MetadataInfo> &mdi_parent, std::string filename)
{
    string direntry_key = std::to_string(mdi_parent->getMD().inode_number()) + ":" + filename;

    KineticRecord record("", std::to_string(1), "", com::seagate::kinetic::proto::Message_Algorithm_SHA1);
    KineticStatus status = PRIV->kinetic->Put(direntry_key, "", WriteMode::REQUIRE_SAME_VERSION, record);

    if (status.versionMismatch())
        return -EEXIST;
    if (status.notOk()){
        pok_warning("EIO");
        return -EIO;
    }
    pok_debug("created key %s for system path %s",direntry_key.c_str(),mdi_parent->getSystemPath().c_str());
    return 0;
}

int delete_directory_entry(const std::unique_ptr<MetadataInfo> &mdi_parent, std::string filename)
{
    string direntry_key = std::to_string(mdi_parent->getMD().inode_number()) + ":" + filename;

    KineticStatus status = PRIV->kinetic->Delete(direntry_key, std::to_string(1), WriteMode::REQUIRE_SAME_VERSION);
    if (status.notOk()){
        pok_warning("EIO");
        return -EIO;
    }
    pok_debug("deleted key %s for system path %s",direntry_key.c_str(),mdi_parent->getSystemPath().c_str());
    return 0;
}

/** Read directory
 *
 * This supersedes the old getdir() interface.  New applications
 * should use this.
 *
 * The filesystem may choose between two modes of operation:
 *
 * 1) The readdir implementation ignores the offset parameter, and
 * passes zero to the filler function's offset.  The filler
 * function will not return '1' (unless an error happens), so the
 * whole directory is read in a single readdir operation.  This
 * works just like the old getdir() method.
 *
 * 2) The readdir implementation keeps track of the offsets of the
 * directory entries.  It uses the offset parameter and always
 * passes non-zero offset to the filler function.  When the buffer
 * is full (or an error happens) the filler function will return
 * '1'.
 *
 * Introduced in version 2.3
 */
int pok_readdir(const char *user_path, void *buffer, fuse_fill_dir_t filldir, off_t offset, struct fuse_file_info *fi)
{
    MetadataInfo * mdi = reinterpret_cast<MetadataInfo *>(fi->fh);
    if (!mdi) {
        pok_error("Read request for user path '%s' without metadata_info structure", user_path);
        return -EINVAL;
    }
    if (check_access(mdi, R_OK)) // ls requires read permission
        return -EACCES;

    /* A directory entry has been moved out / moved into this directory due to a directory move. */
    if (mdi->getMD().has_force_update_version() && mdi->getMD().force_update_version() > PRIV->pmap->getSnapshotVersion())
        if (int err = database_update())
            return err;

    string keystart = std::to_string(mdi->getMD().inode_number()) + ":";
    string keyend   = std::to_string(mdi->getMD().inode_number()) + ":" + static_cast<char>(251);
    size_t maxsize = 10000;
    unique_ptr<vector<std::string>> keys(new vector<string>());

    do {
        if (keys->size())
            keystart = keys->back();
        keys->clear();
        PRIV->kinetic->GetKeyRange(keystart, keyend, maxsize, keys);
        for (auto& element : *keys) {
            std::string filename = element.substr(element.find_first_of(':') + 1, element.length());
            filldir(buffer, filename.c_str(), NULL, 0);
        }
    } while (keys->size() == maxsize);

    return 0;
}
