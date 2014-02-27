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

    /* Root can do what (s)he wants. */
    if(fuse_get_context()->uid == 0)
        return 0;

    /* Non-root can't change the owner of a file.*/
    if ((uid != (uid_t) -1) && (uid != mdi->getMD().uid()))
        return -EPERM;

    /* Non-root owner can change group to a group of which he is a member.
    *  TODO: group member-check. not possible in fuse with system groups */
    if ((gid != (gid_t) -1) && (gid != mdi->getMD().gid()))
        if( (fuse_get_context()->uid != mdi->getMD().uid()) )
            return -EPERM;

    /* Owner can change file mode. */
    if ((mode != (mode_t) -1) && (mode != mdi->getMD().mode()))
        if(fuse_get_context()->uid != mdi->getMD().uid())
            return -EPERM;
    return 0;
}

static int do_permission_change(const char *user_path, mode_t mode, uid_t uid, gid_t gid)
{
    std::shared_ptr<MetadataInfo> mdi;
    int err = permission_lookup(user_path, mdi, mode, uid, gid);
    if( err) return err;

    /* when non-super-user calls chmod successfully, if the group ID of the file is not the effective group ID and if the file is a regular file, set-gid is cleared */
    if(fuse_get_context()->uid && (mode != (mode_t)-1) && fuse_get_context()->gid != mdi->getMD().gid() && S_ISREG(mode)){
       mode &= ~S_ISGID;
    }

    /* when non-super-user calls chown successfully, set-uid and set-gid bits are removed, except when both uid and gid are equal to -1.*/
    if(fuse_get_context()->uid && (uid != (uid_t) -1 || gid != (gid_t) -1)){
        assert(mode == (mode_t)-1);
        mode = mdi->getMD().mode();
        mode &= ~S_ISUID;
        mode &= ~S_ISGID;
    }

    if(uid != (uid_t) -1)   mdi->getMD().set_uid(uid);
    if(gid != (gid_t) -1)   mdi->getMD().set_gid(gid);
    if(mode != (mode_t) -1) mdi->getMD().set_mode(mode);
    mdi->updateACtime();

    err = put_metadata(mdi);
    if(err == -EAGAIN)
       return do_permission_change(user_path, mode, uid, gid);
    if(err) return err;

    /* note in database if path permissions changed */
    if(S_ISDIR(mdi->getMD().mode()) && mdi->computePathPermissionChildren())
    {
        posixok::db_entry entry;
        entry.set_type(posixok::db_entry_TargetType_NONE);
        entry.set_origin(user_path);

        /* the permission change already happened, no need to verify that it is permitted in database_operation */
        REQ( util::database_operation([](){return 0;}, entry) );
        std::int64_t snapshot_version = PRIV->pmap.getSnapshotVersion();
        err = put_metadata_forced(mdi, [&mdi, &snapshot_version](){ mdi->getMD().set_path_permission_verified(snapshot_version);});
        assert(!err || err == -ENOENT);
    }
    return 0;
}


/** Change the permission bits of a file */
int pok_chmod(const char *user_path, mode_t mode)
{
    return  do_permission_change(user_path, mode, -1, -1);
}

/** Change the owner and group of a file */
int pok_chown(const char *user_path, uid_t uid, gid_t gid)
{
    return do_permission_change(user_path, -1, uid, gid);
}

