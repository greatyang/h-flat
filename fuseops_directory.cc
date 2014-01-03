#include "main.h"
#include "debug.h"
#include <algorithm>

/** Open directory
 *
 * Unless the 'default_permissions' mount option is given,
 * this method should check if opendir is permitted for this
 * directory. Optionally opendir may also return an arbitrary
 * filehandle in the fuse_file_info structure, which will be
 * passed to readdir, closedir and fsyncdir.
 *
 * Introduced in version 2.3
 */
/*
int pok_opendir (const char *user_path, struct fuse_file_info *fi)
{
	return 0;
}
*/

/** Release directory
 *
 * Introduced in version 2.3
 */
/*
int pok_releasedir (const char *user_path, struct fuse_file_info *fi)
{
		return 0;
}
*/

	/** Create a directory */
int pok_mkdir 		(const char *user_path, mode_t mode)
{
	struct fuse_file_info fi;
	fi.fh = fi.flags = fi.lock_owner = fi.direct_io = 0;

	mode |= S_IFDIR;
	int err = pok_create(user_path, mode, &fi);
	if(err)
		return err;
	pok_release(user_path, &fi);
	return 0;
}

/** Read directory
 *
 * This supersedes the old getdir() interface.  New applications
 * should use this.
 *
 * The filesystem may choose between two modes of operation:
 *
 * 1) The readdir implementation ignores the offset parameter, and
 * passes zero to the filler function's offset.  The filler
 * function will not return '1' (unless an error happens), so the
 * whole directory is read in a single readdir operation.  This
 * works just like the old getdir() method.
 *
 * 2) The readdir implementation keeps track of the offsets of the
 * directory entries.  It uses the offset parameter and always
 * passes non-zero offset to the filler function.  When the buffer
 * is full (or an error happens) the filler function will return
 * '1'.
 *
 * Introduced in version 2.3
 */
int pok_readdir(const char *user_path, void *buffer, fuse_fill_dir_t filldir, off_t offset, struct fuse_file_info *fi)
{
	MetadataInfo * mdi = reinterpret_cast<MetadataInfo *>(fi->fh);
	if(!mdi){
		pok_error("Read request for user path '%s' without metadata_info structure", user_path);
		return -EINVAL;
	}

	pok_debug("Reading directory at user path %s with %ld allocated blocks and a byte size of %ld",user_path,mdi->pbuf()->blocks(),mdi->pbuf()->size());

	for(unsigned int blocknum=0; blocknum < mdi->pbuf()->blocks(); blocknum++)
	{
		std::string value;
		if(PRIV->nspace->get(mdi, blocknum, &value).notOk())
		{
			pok_warning("Failed obtaining block #%d for directory at user path %s.", blocknum, user_path);
			continue;
		}

		auto delimiter = [](char c) -> bool {
			return c == '|' ? true : false;
		};
		auto e = value.end();
		auto i = value.begin();

		while(i!=e)
		{
			i = find_if_not(i, e, delimiter);
			if (i==e) break;
			auto j = std::find_if(i, e, delimiter);
			pok_trace("calling filldir with filename: %s",std::string(i,j).c_str());
			filldir(buffer, std::string(i,j).c_str(), NULL, 0);
			i=j;
		}
	}
	return 0;
}
