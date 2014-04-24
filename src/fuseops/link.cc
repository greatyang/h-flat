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

/** Read the target of a symbolic link
 *
 * The buffer should be filled with a null terminated string.  The
 * buffer size argument includes the space for the terminating
 * null character.	If the linkname is too long to fit in the
 * buffer, it should be truncated.	The return value should be 0
 * for success.
 */
int hflat_readlink(const char *user_path, char *buffer, size_t size)
{
    std::string link_destination;
    std::int64_t pathPermissionTimeStamp = 0;
    link_destination = PRIV->pmap.toSystemPath(user_path, pathPermissionTimeStamp, CallingType::READLINK);

    if (link_destination.length() >= size) {
        hflat_debug("buffer too small to fit link destination.");
        strncpy(buffer, link_destination.c_str(), size);
        buffer[size - 1] = '\0';
    } else
        strcpy(buffer, link_destination.c_str());
    return 0;
}

/** Create a symbolic link */
int hflat_symlink(const char *target, const char *origin)
{
    std::shared_ptr<MetadataInfo> mdi;
    std::shared_ptr<MetadataInfo> mdi_dir;
    int err = lookup(origin, mdi);
    if (!err)
        return -EEXIST;
    if (err != -ENOENT)
        return err;

    err = lookup_parent(origin, mdi_dir);
    if(!err) err = check_access(mdi_dir, W_OK);
    if(!err) err = create_directory_entry(mdi_dir, util::path_to_filename(origin));
    if (err) return err;

    hflat::db_entry entry;
    entry.set_type(entry.SYMLINK);
    entry.set_origin(origin);
    entry.set_target(target);

    /* do database operation... after adding the filename to directory, no other client can disallow it */
    err = util::database_operation(entry);
    if(err){
        hflat_warning("Database operation failed. Dangling directory entry!");
        return err;
    }

    /* initialize metadata and write metadata-key to drive */
    initialize_metadata(mdi, mdi_dir, S_IFLNK | S_IRWXU | S_IRGRP | S_IXGRP | S_IXOTH);
    REQ( create_metadata(mdi) );
    return err;
}

static int toHardlinkT(const std::shared_ptr<MetadataInfo> &mdi_target)
{
    assert(mdi_target->getMD().type() == hflat::Metadata_InodeType_POSIX);

    std::shared_ptr<MetadataInfo> mdi_source(new MetadataInfo( mdi_target->getSystemPath() ));
    mdi_source->setKeyVersion(mdi_target->getKeyVersion());
    mdi_source->getMD().set_type(hflat::Metadata_InodeType_HARDLINK_S);
    mdi_source->getMD().set_inode_number(mdi_target->getMD().inode_number());
    mdi_source->getMD().set_link_count(1);

    mdi_target->setSystemPath("hardlink_" + std::to_string(mdi_target->getMD().inode_number()));
    mdi_target->getMD().set_type(hflat::Metadata_InodeType_HARDLINK_T);

    /* create the forwarding to the hardlink-key. */
    if (int err = put_metadata(mdi_source))
        return err;

    /* store target metadata at hardlink-key location */
    REQ ( create_metadata(mdi_target) );
    return 0;
}

/** Create a hard link to a file */
int hflat_hardlink(const char *target, const char *origin)
{
    hflat_debug("Hardlink %s->%s", origin, target);

    std::shared_ptr<MetadataInfo> mdi_target;
    std::shared_ptr<MetadataInfo> mdi_target_dir;
    std::shared_ptr<MetadataInfo> mdi_origin;
    std::shared_ptr<MetadataInfo> mdi_origin_dir;

    int err = lookup(target, mdi_target);
    if(!err) err = lookup_parent(origin, mdi_origin_dir);
    if(!err) err = lookup_parent(target, mdi_target_dir);
    if(!err) err = check_access(mdi_origin_dir, W_OK);
    if(!err) err = check_access(mdi_target_dir, W_OK);
    if( err) return err;

    err = lookup(origin, mdi_origin);
    if (!err) return -EEXIST;
    if (err != -ENOENT) return err;

    assert(!S_ISDIR(mdi_target->getMD().mode()));

    PRIV->lookup_cache.invalidate(mdi_origin->getSystemPath());
    PRIV->lookup_cache.invalidate(mdi_target->getSystemPath());


    /* many clients try to create hardlink -> serialization over direntry */
    if ((err = create_directory_entry(mdi_origin_dir, util::path_to_filename(origin))))
        return err;

    /* If the target metadata is not already a hardlink_target, make it so. */
    if (mdi_target->getMD().type() != hflat::Metadata_InodeType_HARDLINK_T) {
        if (( err = toHardlinkT(mdi_target) )){
            REQ ( delete_directory_entry(mdi_origin_dir, util::path_to_filename(origin)) );
            if(err == -EAGAIN) return hflat_hardlink(target, origin);
            return err;
        }
    }

    mdi_target->updateACtime();
    mdi_target->getMD().set_link_count(mdi_target->getMD().link_count() + 1);
    if ((err = put_metadata(mdi_target))) {
        /* another client could have changed / deleted hardlink_t inode. */
        REQ ( delete_directory_entry(mdi_origin_dir, util::path_to_filename(origin)) );
        if(err == -EAGAIN) return hflat_hardlink(target, origin);
        return err;
    }

    mdi_origin->getMD().set_type(hflat::Metadata_InodeType_HARDLINK_S);
    mdi_origin->getMD().set_inode_number(mdi_target->getMD().inode_number());
    mdi_origin->getMD().set_link_count(1);
    REQ(create_metadata(mdi_origin));
    return 0;
}
