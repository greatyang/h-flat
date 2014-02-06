#include "main.h"
#include "debug.h"
#include "kinetic_helper.h"


int check_access(const std::shared_ptr<MetadataInfo> &mdi, int mode)
{
    /* root does as (s)he pleases */
    if (fuse_get_context()->uid == 0)
        return 0;
    /* only test for existence of file */
    if (mode == F_OK)
        return 0;

    /* check file permissions */
    unsigned int umode = mode;
    /* test user */
    if (mdi->getMD().uid() == fuse_get_context()->uid) {
        if ((mode & mdi->getMD().mode() >> 6) == umode)
            return 0;
        else
            return -EACCES;
    }
    /* test group */
    if (mdi->getMD().gid() == fuse_get_context()->gid) {
        if ((mode & mdi->getMD().mode() >> 3) == umode)
            return 0;
        else
            return -EACCES;
    }
    /* test other */
    if ((mode & mdi->getMD().mode()) == umode)
        return 0;
    return -EACCES;

}

/**
 * Check file access permissions
 *
 * This will be called for the access() system call.  If the
 * 'default_permissions' mount option is given, this method is not
 * called.
 *
 * This method is not called under Linux kernel versions 2.4.x
 *
 * Introduced in version 2.5
 */
int pok_access(const char *user_path, int mode)
{
    /* OSX tries to verify access to root a hundred times or so... let's not go too crazy. */
    if (strlen(user_path) == 1)
        return 0;

    if (fuse_get_context()->uid == 0)
        return 0;

    std::shared_ptr<MetadataInfo> mdi;
    int err = lookup(user_path, mdi);
    if( err) return err;

    return check_access(mdi, mode);
}

static int permission_lookup(const char *user_path, std::shared_ptr<MetadataInfo> &mdi, mode_t mode, uid_t uid, gid_t gid)
{
    if ( int err = lookup(user_path, mdi) )
        return err;

    /* Only the root user can change the owner of a file.*/
    if ((uid != (uid_t) -1) && uid != mdi->getMD().uid() &&
            fuse_get_context()->uid)
      return -EPERM;

    /* You can change the group of a file only if you are a root user or if you own the file.
    *  If you own the file but are not a root user, you can change the group only to a group of which you are a member.
    *  TODO: check if owner is in group described by gid. Non-trivial, need our own group-list in file system. */
    if ((gid != (gid_t) -1) && gid != mdi->getMD().gid() &&
            fuse_get_context()->uid && fuse_get_context()->uid != mdi->getMD().uid())
      return -EPERM;

    /* Only root or owner can change file mode. */
    if ((mode != (mode_t) -1) && mode != mdi->getMD().mode() &&
            fuse_get_context()->uid && fuse_get_context()->uid != mdi->getMD().uid())
      return -EPERM;

    return 0;
}

static int do_permission_change(const char *user_path, mode_t mode, uid_t uid, gid_t gid)
{
    std::shared_ptr<MetadataInfo> mdi;
    int err = permission_lookup(user_path, mdi, mode, uid, gid);
    if( err) return err;

    if(uid != (uid_t) -1)   mdi->getMD().set_uid(uid);
    if(gid != (gid_t) -1)   mdi->getMD().set_gid(gid);
    if(mode != (mode_t) -1) mdi->getMD().set_mode(mode);
    mdi->updateACtime();

    bool db_updated = false;
    if(S_ISDIR(mdi->getMD().mode()) && mdi->computePathPermissionChildren()) {
        posixok::db_entry entry;
        entry.set_type(posixok::db_entry_TargetType_NONE);
        entry.set_origin(user_path);
        pok_debug("Set target type to %d",entry.type());

        err = database_op(std::bind(permission_lookup, user_path, std::ref(mdi), mode, uid, gid), entry);
        if(err) return err;

        mdi->getMD().set_path_permission_verified(PRIV->pmap.getSnapshotVersion());
        db_updated = true;
    }

    err = put_metadata(mdi);
    if(err && db_updated)
        pok_warning("failure applying permission change after successful database update.");
    return err;
}


/** Change the permission bits of a file */
int pok_chmod(const char *user_path, mode_t mode)
{
    return do_permission_change(user_path, mode, -1, -1);
}

/** Change the owner and group of a file */
int pok_chown(const char *user_path, uid_t uid, gid_t gid)
{
    return do_permission_change(user_path, -1, uid, gid);
}

