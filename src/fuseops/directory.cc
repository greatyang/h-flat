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
#include "main.h"
#include "debug.h"
#include "fuseops.h"
#include "kinetic_helper.h"
#include <algorithm>

using com::seagate::kinetic::client::proto::Command_Algorithm_SHA1;

/** Create a directory */
int hflat_mkdir(const char *user_path, mode_t mode)
{
    return hflat_create(user_path, mode | S_IFDIR);
}

/** Remove a directory */
int hflat_rmdir(const char *user_path)
{
    std::shared_ptr<MetadataInfo> mdi;
    int err = lookup(user_path, mdi);
    if( err) return err;

    string keystart = std::to_string(mdi->getMD().inode_number()) + "|";
    string keyend   = std::to_string(mdi->getMD().inode_number()) + "|" + static_cast<char>(255);
    unique_ptr<vector<string>> keys(new vector<string>());
    PRIV->kinetic->GetKeyRange(keystart, keyend, 1, keys);

    hflat_debug("Found %d entries for user_path '%s' (inode number %d) ",keys->size(), user_path, mdi->getMD().inode_number());
    if (keys->size())
        return -ENOTEMPTY;

    util::database_update();
    return hflat_unlink(user_path);
}

int create_directory_entry(const std::shared_ptr<MetadataInfo> &mdi_parent, std::string filename)
{
    string direntry_key = std::to_string(mdi_parent->getMD().inode_number()) + "|" + filename;

    KineticRecord record("", std::to_string(1), "", Command_Algorithm_SHA1);
    KineticStatus status = PRIV->kinetic->Put(direntry_key, "", WriteMode::REQUIRE_SAME_VERSION, record);

    if (status.statusCode() ==  kinetic::StatusCode::REMOTE_VERSION_MISMATCH)
        return -EEXIST;
    if (!status.ok()){
        hflat_warning("Failed creating entry for file '%s' due to errno %d %s",filename.c_str(), status.statusCode(), status.message().c_str());
        return -EIO;
    }

    hflat_debug("created key %s for system path %s",direntry_key.c_str(),mdi_parent->getSystemPath().c_str());

    if(PRIV->posix == PosixMode::FULL){
        int err = put_metadata_forced(mdi_parent, [&mdi_parent](){mdi_parent->updateACMtime();});
        if (err)  hflat_warning("Failed updating parent directory time stamps after successfully adding directory entry.");
    }
    return 0;
}

int delete_directory_entry(const std::shared_ptr<MetadataInfo> &mdi_parent, std::string filename)
{
    string direntry_key = std::to_string(mdi_parent->getMD().inode_number()) + "|" + filename;

    KineticStatus status = PRIV->kinetic->Delete(direntry_key, std::to_string(1), WriteMode::REQUIRE_SAME_VERSION);
    if (!status.ok()){
        hflat_warning("Failed deleting entry for file '%s' due to errno %d %s",filename.c_str(), status.statusCode(), status.message().c_str());
        return -EIO;
    }

    hflat_debug("deleted key %s for system path %s",direntry_key.c_str(),mdi_parent->getSystemPath().c_str());

    if(PRIV->posix == PosixMode::FULL){
        int err = put_metadata_forced(mdi_parent, [&mdi_parent](){mdi_parent->updateACMtime();});
        if (err)  hflat_warning("Failed updating parent directory time stamps after successfully removing directory entry.");
    }
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
int hflat_readdir(const char *user_path, void *buffer, fuse_fill_dir_t filldir, off_t offset, struct fuse_file_info *fi)
{
    std::shared_ptr<MetadataInfo> mdi;
    int err = lookup(user_path, mdi);
    if( err) return err;

    if (check_access(mdi, R_OK)) // ls requires read permission
        return -EACCES;

    string keystart = std::to_string(mdi->getMD().inode_number()) + "|";
    string keyend   = std::to_string(mdi->getMD().inode_number()) + "|" + static_cast<char>(251);
    size_t maxsize = 100;
    unique_ptr<vector<std::string>> keys(new vector<string>());

    do {
        if (keys->size())
            keystart = keys->back();
        keys->clear();
        PRIV->kinetic->GetKeyRange(keystart, keyend, maxsize, keys);
        for (auto& element : *keys) {
            std::string filename = element.substr(element.find_first_of('|') + 1, element.length());
            filldir(buffer, filename.c_str(), NULL, 0);
        }
    } while (keys->size() == maxsize);

    return 0;
}
