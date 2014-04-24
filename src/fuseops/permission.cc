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
int hflat_access(const char *user_path, int mode)
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

    /* Non-owner isn't actually allowed to change anything. */
    if(fuse_get_context()->uid != mdi->getMD().uid())
        return -EPERM;

    /* Owner can change group only to a group of which he is a member.
     * TODO: group member-check -> Not possible in fuse with system groups.
     * We alternatively enforce that the owner can only change the group to his currently active group. */
    if ((gid != (gid_t) -1) && (gid != fuse_get_context()->gid))
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
        hflat::db_entry entry;
        entry.set_type(hflat::db_entry_Type_NONE);
        entry.set_origin(user_path);

        REQ( util::database_operation(entry) );
        std::int64_t snapshot_version = PRIV->pmap.getSnapshotVersion();
        err = put_metadata_forced(mdi, [&mdi, &snapshot_version](){ mdi->getMD().set_path_permission_verified(snapshot_version);});
        assert(!err || err == -ENOENT);
    }
    return 0;
}


/** Change the permission bits of a file */
int hflat_chmod(const char *user_path, mode_t mode)
{
    return  do_permission_change(user_path, mode, -1, -1);
}

/** Change the owner and group of a file */
int hflat_chown(const char *user_path, uid_t uid, gid_t gid)
{
    return do_permission_change(user_path, -1, uid, gid);
}

