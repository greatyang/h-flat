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

	pok_trace("Checking access for user_path '%s'",user_path);
	std::unique_ptr<MetadataInfo> mdi(new MetadataInfo());
	int err = lookup(user_path, mdi);
	if (err){
		pok_debug("lookup returned error code %d",err);
		return err;
	}

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
	return -EACCES;
}


/** Change the permission bits of a file */
int pok_chmod (const char *user_path, mode_t mode)
{
	std::unique_ptr<MetadataInfo> mdi(new MetadataInfo());
	int err = lookup(user_path, mdi);
	if (err){
		pok_debug("lookup returned error code %d",err);
		return err;
		}

	mdi->pbuf()->set_mode(mode);
	mdi->updateACtime();

	NamespaceStatus status = PRIV->nspace->putMD(mdi.get());
	if(status.notOk())
		return -EIO;
	return 0;
}

/** Change the owner and group of a file */
int pok_chown (const char *user_path, uid_t uid, gid_t gid)
{
	std::unique_ptr<MetadataInfo> mdi(new MetadataInfo());
	int err = lookup(user_path, mdi);
	if (err){
		pok_debug("lookup returned error code %d",err);
		return err;
	}

	mdi->pbuf()->set_id_group(gid);
	mdi->pbuf()->set_id_user(uid);
	mdi->updateACtime();

	NamespaceStatus status = PRIV->nspace->putMD(mdi.get());
	if(status.notOk())
		return -EIO;
	return 0;
}

