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
#include "kinetic_helper.h"
#include <sys/types.h>
#include <sys/param.h>
#include "fuseops.h"

/** Get file attributes.
 *
 * Similar to stat().  The 'st_dev' and 'st_blksize' fields are
 * ignored. The 'st_ino' field is ignored except if the 'use_ino'
 * mount option is given.
 */
int hflat_getattr(const char *user_path, struct stat *attr)
{
    std::shared_ptr<MetadataInfo> mdi;
    int err = lookup(user_path, mdi);
    if( err) return err;

    if(mdi->getDirtyData() && mdi->getDirtyData()->hasUpdates())
        hflat_fsync(user_path, 0, nullptr);

    attr->st_ino    = mdi->getMD().inode_number();
    attr->st_atime  = mdi->getMD().atime();
    attr->st_mtime  = mdi->getMD().mtime();
    attr->st_ctime  = mdi->getMD().ctime();
    attr->st_uid    = mdi->getMD().uid();
    attr->st_gid    = mdi->getMD().gid();
    attr->st_mode   = mdi->getMD().mode();
    attr->st_nlink  = mdi->getMD().link_count();
    attr->st_size   = mdi->getMD().size();
    attr->st_blocks = mdi->getMD().blocks();
    attr->st_blksize= PRIV->blocksize;
    return 0;
}


/**
 * Get attributes from an open file
 *
 * This method is called instead of the getattr() method if the
 * file information is available.
 *
 * Currently this is only called after the create() method if that
 * is implemented (see above).  Later it may be called for
 * invocations of fstat() too.
 *
 * Introduced in version 2.5
 */
int hflat_fgetattr(const char *user_path, struct stat *attr, struct fuse_file_info *fi)
{
   return hflat_getattr(user_path, attr);
}

/**
 * Change the access and modification times of a file with
 * nanosecond resolution
 *
 * Introduced in version 2.6
 */
int hflat_utimens(const char *user_path, const struct timespec tv[2])
{
    std::shared_ptr<MetadataInfo> mdi;
    int err = lookup(user_path, mdi);
    if( err) return err;

    mdi->getMD().set_atime(tv[0].tv_sec);
    mdi->getMD().set_mtime(tv[1].tv_sec);
    err = put_metadata(mdi);
    if(err == -EAGAIN) return hflat_utimens(user_path, tv);
    return err;
}

/** Get file system statistics
 *
 * The 'f_frsize', 'f_favail', 'f_fsid' and 'f_flag' fields are ignored
 *
 * Replaced 'struct statfs' parameter with 'struct statvfs' in
 * version 2.5
 */
int hflat_statfs(const char *user_path, struct statvfs *s)
{
    kinetic::Capacity cap;
    KineticStatus status = PRIV->kinetic->Capacity(cap);
    if (!status.ok())
        return -EIO;

    s->f_frsize = PRIV->blocksize; /* Minimal allocated block size */
    s->f_bsize  = PRIV->blocksize; /* Preferred file system block size for I/O requests */
    s->f_blocks = (fsblkcnt_t) (  cap.nominal_capacity_in_bytes / PRIV->blocksize ); /* Blocks on FS in units of f_frsize */
    s->f_bavail = (fsblkcnt_t) ( ( cap.nominal_capacity_in_bytes - cap.nominal_capacity_in_bytes * cap.portion_full)  / PRIV->blocksize); /* Free blocks */
    s->f_bfree  = s->f_bavail;

    s->f_namemax = NAME_MAX; /* Max file name length */
    s->f_files   = static_cast<fsfilcnt_t>(PRIV->inum_base) + PRIV->inum_counter; /* Total inodes */
    s->f_ffree   = std::numeric_limits<std::uint16_t>::max(); /* Free inodes */
    return 0;
}
