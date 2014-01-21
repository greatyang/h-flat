#include "main.h"
#include "debug.h"
#include "kinetic_helper.h"
#include <sys/types.h>
#include <sys/param.h>

static void fillattr(struct stat *attr, MetadataInfo *mdi)
{
    attr->st_ino    = mdi->pbuf()->inode_number();
    attr->st_atime  = mdi->pbuf()->atime();
    attr->st_mtime  = mdi->pbuf()->mtime();
    attr->st_ctime  = mdi->pbuf()->ctime();
    attr->st_uid    = mdi->pbuf()->uid();
    attr->st_gid    = mdi->pbuf()->gid();
    attr->st_mode   = mdi->pbuf()->mode();
    attr->st_nlink  = mdi->pbuf()->link_count();
    attr->st_size   = mdi->pbuf()->size();
    attr->st_blocks = mdi->pbuf()->blocks();
    attr->st_blksize= PRIV->blocksize;
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
int pok_fgetattr(const char *user_path, struct stat *attr, struct fuse_file_info *fi)
{
    MetadataInfo * mdi = reinterpret_cast<MetadataInfo *>(fi->fh);
    if (!mdi) {
        pok_warning("Read request for user path '%s' without metadata_info structure", user_path);
        return -EINVAL;
    }
    fillattr(attr, mdi);
    return 0;
}

/** Get file attributes.
 *
 * Similar to stat().  The 'st_dev' and 'st_blksize' fields are
 * ignored. The 'st_ino' field is ignored except if the 'use_ino'
 * mount option is given.
 */
int pok_getattr(const char *user_path, struct stat *attr)
{
    std::unique_ptr<MetadataInfo> mdi(new MetadataInfo());
    if (int err = lookup(user_path, mdi))
        return err;

    fillattr(attr, mdi.get());
    return 0;
}

/**
 * Change the access and modification times of a file with
 * nanosecond resolution
 *
 * Introduced in version 2.6
 */
int pok_utimens(const char *user_path, const struct timespec tv[2])
{
    std::unique_ptr<MetadataInfo> mdi(new MetadataInfo());
    if (int err = lookup(user_path, mdi))
        return err;

    mdi->pbuf()->set_atime(tv[0].tv_sec);
    mdi->pbuf()->set_mtime(tv[1].tv_sec);
    return put_metadata(mdi.get());
}

/** Get file system statistics
 *
 * The 'f_frsize', 'f_favail', 'f_fsid' and 'f_flag' fields are ignored
 *
 * Replaced 'struct statfs' parameter with 'struct statvfs' in
 * version 2.5
 */
int pok_statfs(const char *user_path, struct statvfs *s)
{
    kinetic::Capacity cap;
    KineticStatus status = PRIV->kinetic->Capacity(cap);
    if (status.notOk())
        return -EIO;

    s->f_bsize  = PRIV->blocksize; /* File system block size */
    s->f_blocks = (fsblkcnt_t) cap.total_bytes; /* Blocks on FS in units of f_frsize */
    s->f_bavail = (fsblkcnt_t) cap.remaining_bytes; /* Free blocks */
    s->f_bfree  = (fsblkcnt_t) cap.remaining_bytes; /* Blocks available to non-root */

    s->f_namemax = NAME_MAX; /* Max file name length */

    s->f_files = UINT16_MAX; /* Total inodes */
    s->f_ffree = UINT16_MAX; /* Free inodes */

    return 0;
}
