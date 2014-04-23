#include "main.h"
#include "debug.h"
#include "kinetic_helper.h"
#include "fuseops.h"
#include <thread>

using namespace util;

int unlink_force_update(const char *user_path)
{
    std::shared_ptr<MetadataInfo> mdi_fu;
    int err = get_metadata_userpath(user_path, mdi_fu);
    if( err == -ENOENT) return 0;
    hflat_warning("returning %d",err);
    if( err) return err;

    hflat_debug("Found force_update entry.");
    assert(mdi_fu->getMD().type() == hflat::Metadata_InodeType_FORCE_UPDATE);
    return delete_metadata(mdi_fu);
}

/* lookup & verify access permissions */
int unlink_lookup(const char *user_path, std::shared_ptr<MetadataInfo> &mdi, std::shared_ptr<MetadataInfo> &mdi_dir )
{
    if( int err = lookup(user_path, mdi) )
        return err;
    if( int err = lookup_parent(user_path, mdi_dir) )
        return err;

    if (check_access(mdi_dir, W_OK | X_OK))
        return -EACCES;
    // If sticky bit is set on directory, current user needs to be owner of directory OR file (or root of course).
    if (fuse_get_context()->uid && (mdi_dir->getMD().mode() & S_ISVTX) && (fuse_get_context()->uid != mdi_dir->getMD().uid())
            && (fuse_get_context()->uid != mdi->getMD().uid()))
        return -EACCES;
    return 0;
};

/* Need to serialize hardlink unlinks over HARDLINK_S key in addition to metadata key
 * in case multiple clients try to unlink the same hardlink name in parallel (only one may succeed). */
static int unlink_hardlink(const char *user_path, const std::shared_ptr<MetadataInfo> &mdiT)
{
    std::shared_ptr<MetadataInfo> mdiS;
    int err = get_metadata_userpath(user_path, mdiS);
    if( err) return err;
    assert(mdiS->getMD().type() == hflat::Metadata_InodeType_HARDLINK_S);

    /* set HARDLINK_S link count to 0 */
    if(mdiS->getMD().link_count() == 0) return -EINVAL;
    mdiS->getMD().set_link_count(0);
    err = put_metadata(mdiS);
    if(err) return err;

    /* unlink HARDLINK_T metadata key */
    mdiT->getMD().set_link_count( mdiT->getMD().link_count() - 1 );
    if(mdiT->getMD().link_count() == 0) err = delete_metadata(mdiT);
    else{
        mdiT->updateACtime();
        err = put_metadata(mdiT);
    }

    /* If unlinking HARDLINK_T metadata key was unsuccessful, reset HARDLINK_S link count to 1. Otherwise delete HARDLINK_S key.
     * Since HARDLINK_S key was used for serialization, no other client will interfere writing to it.  */
    if(err){
        mdiS->getMD().set_link_count(1);
        REQ( put_metadata(mdiS) );
        return err;
    }
    REQ( delete_metadata(mdiS) );
    return 0;
}

/** Remove a file */
int hflat_unlink(const char *user_path)
{
    std::shared_ptr<MetadataInfo> mdi;
    std::shared_ptr<MetadataInfo> mdi_dir;
    int err = unlink_lookup(user_path, mdi, mdi_dir);
    if( err) return err;

    bool hardlink  = mdi->getMD().type() == hflat::Metadata_InodeType_HARDLINK_T;
    bool directory = S_ISDIR(mdi->getMD().mode());

    if(hardlink)  err = unlink_hardlink(user_path, mdi);
    else          err = delete_metadata(mdi);
    if( err == -EAGAIN) return hflat_unlink(user_path);
    if( err) return err;

   /* Remove associated path mapping if it exists. */
    if(PRIV->pmap.hasMapping(user_path)){
        hflat::db_entry entry;
        entry.set_type(hflat::db_entry_Type_REMOVED);
        entry.set_origin(user_path);
        REQ( database_operation(entry) );
    }

    /* remove force_update that (might) exist for the current path when unlinking a directory that has been moved. */
    if (directory) REQ(unlink_force_update(user_path));

    /* remove directory entry */
    REQ( delete_directory_entry(mdi_dir, path_to_filename(user_path)) );

    /* start a background thread to delete now unused data blocks */
    auto datadelete = [](int size, int ino, struct hflat_priv *priv){
        while(size > 0){
            std::string key = std::to_string(ino) + "_" + std::to_string(size / priv->blocksize);
            priv->kinetic->Delete(key, "", WriteMode::IGNORE_VERSION);
            size -= priv->blocksize;
        }
    };
    if(!hardlink || mdi->getMD().link_count() == 0){
        std::thread t(std::bind(datadelete,mdi->getMD().size(), mdi->getMD().inode_number(), PRIV));
        t.detach();
    }


    /* Look for a potential unused reuse mapping.
     * The following sequence should not leave a mapping behind.
     *      mkdir a -> mv a b -> rmdir b.
     * In contrast, the following sequence should leave the reuse mapping intact:
     *      mkdir a -> mv a b -> mkdir a -> rmdir b
     *
     * To check if the name exists and remove the mapping if it doesn't in a way
     * that is unproblematic considering concurrency issues, use existing create &
     * unlink functionality.
     */

    const char * reuse = mdi->getSystemPath().c_str();
    if(PRIV->pmap.hasMapping(reuse)){
        if(hflat_create (reuse, S_IFREG) == 0)
            hflat_unlink(reuse);
    }
    return 0;
}

/** File open operation
 *
 * No creation (O_CREAT, O_EXCL) and by default also no
 * truncation (O_TRUNC) flags will be passed to open(). If an
 * application specifies O_TRUNC, fuse first calls truncate()
 * and then open(). Only if 'atomic_o_trunc' has been
 * specified and kernel version is 2.6.24 or later, O_TRUNC is
 * passed on to open.
 *
 * Unless the 'default_permissions' mount option is given,
 * open should check if the operation is permitted for the
 * given flags. Optionally open may also return an arbitrary
 * filehandle in the fuse_file_info structure, which will be
 * passed to all file operations.
 *
 * Changed in version 2.2
 */
int hflat_open(const char *user_path, struct fuse_file_info *fi)
{
    std::shared_ptr<MetadataInfo> mdi;
    int err = lookup(user_path, mdi);
    if( err) return err;

    int access = fi->flags & O_ACCMODE;
    if (access == O_RDONLY)
        err = check_access(mdi, R_OK);
    if (access == O_WRONLY)
        err = check_access(mdi, W_OK);
    if (access == O_RDWR)
        err = check_access(mdi, R_OK | W_OK);
    if (err)
        return err;
    return 0;
}

/** Release an open file
 *
 * Release is called when there are no more references to an open
 * file: all file descriptors are closed and all memory mappings
 * are unmapped.
 *
 * For every open() call there will be exactly one release() call
 * with the same flags and file descriptor.	 It is possible to
 * have a file opened more than once, in which case only the last
 * release will mean, that no more reads/writes will happen on the
 * file.  The return value of release is ignored.
 *
 * Changed in version 2.2
 */
int hflat_release(const char *user_path, struct fuse_file_info *fi)
{
    return hflat_fsync(user_path, 0, fi);
}

void inherit_path_permissions(const std::shared_ptr<MetadataInfo> &mdi, const std::shared_ptr<MetadataInfo> &mdi_parent)
{
    /* Inherit path permissions existing for directory */
     mdi->getMD().mutable_path_permission()->CopyFrom(mdi_parent->getMD().path_permission());
     mdi->getMD().set_path_permission_verified(mdi_parent->getMD().path_permission_verified());

     /* Add path permissions precomputed for directory's children. */
     for (int i = 0; i < mdi_parent->getMD().path_permission_children_size(); i++) {
         hflat::Metadata::ReachabilityEntry *e = mdi->getMD().add_path_permission();
         e->CopyFrom(mdi_parent->getMD().path_permission_children(i));
     }
     if (S_ISDIR(mdi->getMD().mode())) mdi->computePathPermissionChildren();
}

void initialize_metadata(const std::shared_ptr<MetadataInfo> &mdi, const std::shared_ptr<MetadataInfo> &mdi_parent, mode_t mode)
{
    mdi->updateACMtime();
    mdi->getMD().set_type(hflat::Metadata_InodeType_POSIX);
    mdi->getMD().set_gid(fuse_get_context()->gid);
    mdi->getMD().set_uid(fuse_get_context()->uid);
    mdi->getMD().set_mode(mode);
    mdi->getMD().set_inode_number(generate_inode_number());
    inherit_path_permissions(mdi,mdi_parent);
}

int hflat_create(const char *user_path, mode_t mode)
{
    std::shared_ptr<MetadataInfo> mdi;
    int err = lookup(user_path, mdi);
    if (!err)
        return -EEXIST;
    if (err != -ENOENT)
        return err;

    std::shared_ptr<MetadataInfo> mdi_dir;
    err = lookup_parent(user_path, mdi_dir);
    if (err == -ENOENT && strchr(user_path,':')) return -ENOTDIR; // fuse will infinity-loop on enoent with iointercept
    if (err) return err;

    /* create requires write access to parent directory */
    err = check_access(mdi_dir, W_OK);
    if(err) return err;

    /* Add filename to directory */
    err = create_directory_entry(mdi_dir, path_to_filename(user_path));
    if (err) return err;

    /* initialize metadata and write metadata-key to drive*/
    initialize_metadata(mdi, mdi_dir, mode);
    REQ ( create_metadata(mdi) );
    return 0;
}

/**
 * Create and open a file
 *
 * If the file does not exist, first create it with the specified
 * mode, and then open it.
 *
 * If this method is not implemented or under Linux kernel
 * versions earlier than 2.6.15, the mknod() and open() methods
 * will be called instead.
 *
 * Introduced in version 2.5
 */
int hflat_fcreate(const char *user_path, mode_t mode, struct fuse_file_info *fi)
{
    return hflat_create(user_path, mode);
}


/** Create a file node
 *
 * This is called for creation of all non-directory, non-symlink
 * nodes.  If the filesystem defines a create() method, then for
 * regular files that will be called instead.
 */
int hflat_mknod(const char* user_path, mode_t mode, dev_t rdev)
{
    return hflat_create(user_path, mode);
}
