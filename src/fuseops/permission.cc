#include "main.h"
#include "debug.h"
#include <unistd.h>

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
int pok_access (const char *user_path, int mode)
{
	/* OSX tries to verify access to root a hundred times or so... let's not go too crazy. */
	if(strlen(user_path)==1)
		return 0;
	if(fuse_get_context()->uid == 0)
		return 0;

	std::unique_ptr<MetadataInfo> mdi(new MetadataInfo());
	int err = lookup(user_path, mdi);
	if (err)
		return err;

	/* only test for existence of file */
	if(mode == F_OK)
		return 0;

	unsigned int umode = mode;
	/* test other */
	if ( (mode & mdi->pbuf()->mode()) == umode )
		return 0;
	/* test group */
	if(mdi->pbuf()->id_group() == fuse_get_context()->gid){
		pok_trace("checking group...");
		if ( (mode & mdi->pbuf()->mode() >> 3) == umode )
			return 0;
	}
	/* test user */
	if(mdi->pbuf()->id_user() == fuse_get_context()->uid){
		pok_trace("checking user...");
		if ( (mode & mdi->pbuf()->mode() >> 6) == umode )
			return 0;
	}
	pok_trace("Access for user_path '%s' NOT GRANTED",user_path);
	return -EACCES;
}


static int do_permission_change(const std::unique_ptr<MetadataInfo> &mdi, mode_t mode, uid_t uid, gid_t gid)
{
	mdi->pbuf()->set_id_group(gid);
	mdi->pbuf()->set_id_user(uid);
	mdi->pbuf()->set_mode(mode);
	mdi->updateACtime();
	NamespaceStatus status = PRIV->nspace->putMD(mdi.get());
	if(status.notOk())
		return -EIO;
	return 0;
}

static int do_permission_change_lookup(const char *user_path, mode_t mode, uid_t uid, gid_t gid)
{
	std::unique_ptr<MetadataInfo> mdi(new MetadataInfo());
	int err = lookup(user_path, mdi);
	if (err)
		return err;
	return do_permission_change(mdi, mode, uid, gid);
}

/** Change the permission bits of a file */
int pok_chmod (const char *user_path, mode_t mode)
{
	std::unique_ptr<MetadataInfo> mdi(new MetadataInfo());
	int err = lookup(user_path, mdi);
	if (err)
		return err;

	pok_trace("Changing mode for user_path %s from %d to %d",user_path, mdi->pbuf()->mode(), mode);

	/* POSIX: return EPERM if not root or owner. */
	if(fuse_get_context()->uid) 	if(fuse_get_context()->uid != mdi->pbuf()->id_user()) return -EPERM;

	if(S_ISDIR(mode) && mdi->computePathPermissionChildren()){
		posixok::db_entry entry;
		entry.set_type(entry.NONE);
		entry.set_origin(user_path);

		mode_t old_mode = mdi->pbuf()->mode();
		err = database_operation(
				std::bind(do_permission_change_lookup, user_path, mode, 	 mdi->pbuf()->id_user(), mdi->pbuf()->id_group()),
				std::bind(do_permission_change_lookup, user_path, old_mode, mdi->pbuf()->id_user(), mdi->pbuf()->id_group()),
				entry);
		return err;
	}
	return do_permission_change(mdi, mode,  mdi->pbuf()->id_user(), mdi->pbuf()->id_group());
}

/** Change the owner and group of a file */
int pok_chown (const char *user_path, uid_t uid, gid_t gid)
{
	std::unique_ptr<MetadataInfo> mdi(new MetadataInfo());
	int err = lookup(user_path, mdi);
	if (err)
		return err;

	pok_trace("Changing owner / group for user_path %s from %d:%d to %d:%d   fuse_context: %d:%d ",user_path,
			mdi->pbuf()->id_user(), mdi->pbuf()->id_group(), uid, gid,
			fuse_get_context()->uid,fuse_get_context()->gid);

	if(uid != (uid_t)-1)
		if(fuse_get_context()->uid)
			return -EPERM;
	if(gid != (gid_t)-1)
		if(fuse_get_context()->uid && fuse_get_context()->uid != fuse_get_context()->uid)
			return -EPERM;

	if(gid	== (gid_t)-1) gid = mdi->pbuf()->id_group();
	if(uid  == (uid_t)-1) uid = mdi->pbuf()->id_user();

	if(S_ISDIR(mdi->pbuf()->mode()) && mdi->computePathPermissionChildren()){
		posixok::db_entry entry;
		entry.set_type(entry.NONE);
		entry.set_origin(user_path);

		uid_t old_uid = mdi->pbuf()->id_user();
		gid_t old_gid = mdi->pbuf()->id_group();
		err = database_operation(
				std::bind(do_permission_change_lookup, user_path, mdi->pbuf()->mode(), uid, gid),
				std::bind(do_permission_change_lookup, user_path, mdi->pbuf()->mode(), old_uid, old_gid),
				entry);
		return err;
	}
	return  do_permission_change(mdi, mdi->pbuf()->mode(), uid, gid);
}

