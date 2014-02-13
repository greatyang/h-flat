#include "main.h"
#include "debug.h"
#include "kinetic_helper.h"
#include "fuseops.h"

using namespace util;

int unlink_hardlink_source(const char *user_path)
{
    std::shared_ptr<MetadataInfo> mdi_source;
    int err = get_metadata_userpath(user_path, mdi_source);
    if( err) return err;

    assert(mdi_source->getMD().type() == posixok::Metadata_InodeType_HARDLINK_S);
    return delete_metadata(mdi_source);
}

int unlink_force_update(const char *user_path)
{
    std::shared_ptr<MetadataInfo> mdi_fu;
    int err = get_metadata_userpath(user_path, mdi_fu);
    if( err == -ENOENT) return 0;
    if( err) return err;

    pok_debug("Found force_update entry.");
    assert(mdi_fu->getMD().type() == posixok::Metadata_InodeType_FORCE_UPDATE);
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

/* TODO: Hand over all data keys to Housekeeping after a successfull unlink operation. */
/** Remove a file */
int pok_unlink(const char *user_path)
{
    std::shared_ptr<MetadataInfo> mdi;
    std::shared_ptr<MetadataInfo> mdi_dir;
    int err = unlink_lookup(user_path, mdi, mdi_dir);
    if( err) return err;

    if (posixok::Metadata_InodeType_HARDLINK_T == mdi->getMD().type())
          err = unlink_hardlink_source(user_path);
    if( err) return err;

    /* Unlink Metadata Key */
    if (mdi->getMD().link_count() > 1){
        mdi->getMD().set_link_count( mdi->getMD().link_count() - 1 );
        mdi->updateACtime();
        err = put_metadata(mdi);
    }
    else{
        err = delete_metadata(mdi);
    }
    if( err) return err;

    /* Remove associated path mappings. The following sequence for example should not leave a mapping behind.
    *  mkdir a   mv a b   rmdir b */
    if(PRIV->pmap.hasMapping(user_path)){
        posixok::db_entry entry;
        entry.set_type(posixok::db_entry_TargetType_REMOVED);
        entry.set_origin(user_path);
        err = database_operation(std::bind(unlink_lookup, user_path, std::ref(mdi), std::ref(mdi_dir)),
                entry);
    }
    if (!err && S_ISDIR(mdi->getMD().mode()))
        err = unlink_force_update(user_path);
    if(!err)
        err = delete_directory_entry(mdi_dir, path_to_filename(user_path));

    if (err) pok_warning("Failure in the middle of an unlink operation");
    return err;

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
int pok_open(const char *user_path, struct fuse_file_info *fi)
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
int pok_release(const char *user_path, struct fuse_file_info *fi)
{
    return pok_fsync(user_path, 0, fi);
}

void inherit_path_permissions(const std::shared_ptr<MetadataInfo> &mdi, const std::shared_ptr<MetadataInfo> &mdi_parent)
{
    /* Inherit path permissions existing for directory */
     mdi->getMD().mutable_path_permission()->CopyFrom(mdi_parent->getMD().path_permission());
     mdi->getMD().set_path_permission_verified(mdi_parent->getMD().path_permission_verified());

     /* Add path permissions precomputed for directory's children. */
     for (int i = 0; i < mdi_parent->getMD().path_permission_children_size(); i++) {
         posixok::Metadata::ReachabilityEntry *e = mdi->getMD().add_path_permission();
         e->CopyFrom(mdi_parent->getMD().path_permission_children(i));
     }
     if (S_ISDIR(mdi->getMD().mode())) mdi->computePathPermissionChildren();
}

void initialize_metadata(const std::shared_ptr<MetadataInfo> &mdi, const std::shared_ptr<MetadataInfo> &mdi_parent, mode_t mode)
{
    mdi->updateACMtime();
    mdi->getMD().set_type(posixok::Metadata_InodeType_POSIX);
    mdi->getMD().set_gid(fuse_get_context()->gid);
    mdi->getMD().set_uid(fuse_get_context()->uid);
    mdi->getMD().set_mode(mode);
    mdi->getMD().set_inode_number(generate_inode_number());
    inherit_path_permissions(mdi,mdi_parent);
}

int pok_create(const char *user_path, mode_t mode)
{
    std::shared_ptr<MetadataInfo> mdi;
    int err = lookup(user_path, mdi);
    if (!err)
        return -EEXIST;
    if (err != -ENOENT)
        return err;

    std::shared_ptr<MetadataInfo> mdi_dir(new MetadataInfo());
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
    err = create_metadata(mdi);
    if (err) {
        pok_warning("Failed creating metadata key after successfully creating directory-entry key. \n"
                "Dangling directory entry!");
        return err;
    }
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
int pok_fcreate(const char *user_path, mode_t mode, struct fuse_file_info *fi)
{
    return pok_create(user_path, mode);
}


/** Create a file node
 *
 * This is called for creation of all non-directory, non-symlink
 * nodes.  If the filesystem defines a create() method, then for
 * regular files that will be called instead.
 */
int pok_mknod(const char* user_path, mode_t mode, dev_t rdev)
{
    return pok_create(user_path, mode);
}
