#include "main.h"
#include "debug.h"
#include <sys/types.h>

void fillattr(struct stat *attr, MetadataInfo * mdi)
{
	attr->st_atime = mdi->pbuf()->atime();
	attr->st_mtime = mdi->pbuf()->mtime();
	attr->st_ctime = mdi->pbuf()->ctime();
	attr->st_uid   = mdi->pbuf()->id_user();
	attr->st_gid   = mdi->pbuf()->id_group();
	attr->st_mode  = mdi->pbuf()->mode();
	attr->st_nlink = mdi->pbuf()->link_count();
	attr->st_size  = mdi->pbuf()->size();
	attr->st_blocks= mdi->pbuf()->blocks();
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
	pok_debug("Getting attribtues of user path %s",user_path);
	MetadataInfo * mdi = reinterpret_cast<MetadataInfo *>(fi->fh);
	if(!mdi){
		pok_error("Read request for user path '%s' without metadata_info structure", user_path);
		return -EINVAL;
	}
	fillattr(attr,mdi);
	return 0;
}

/** Get file attributes.
 *
 * Similar to stat().  The 'st_dev' and 'st_blksize' fields are
 * ignored.	 The 'st_ino' field is ignored except if the 'use_ino'
 * mount option is given.
 */
int pok_getattr(const char *user_path, struct stat *attr)
{
	pok_debug("Getting attribtues of user path %s",user_path);
	std::unique_ptr<MetadataInfo> mdi(new MetadataInfo());
	int err = lookup(user_path, mdi);
	if (err){
		pok_debug("lookup returned error code %d",err);
		return err;
	}
	fillattr(attr,mdi.get());
	return 0;
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
int pok_access (const char *user_path, int mode)
{
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

	pok_debug("no access granted");
	return -EACCES;
}



