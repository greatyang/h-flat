#include "main.h"
#include "debug.h"

/** Read the target of a symbolic link
 *
 * The buffer should be filled with a null terminated string.  The
 * buffer size argument includes the space for the terminating
 * null character.	If the linkname is too long to fit in the
 * buffer, it should be truncated.	The return value should be 0
 * for success.
 */
int pok_readlink (const char *user_path, char *buffer, size_t size)
{
	std::string link_destination;
	std::int64_t pathPermissionTimeStamp = 0;
	link_destination = PRIV->pmap->toSystemPath(user_path, pathPermissionTimeStamp, CallingType::READLINK);

	if(link_destination.length() >= size){
		pok_warning("buffer too small to fit link destination.");
		strncpy(buffer, link_destination.c_str(), size);
		buffer[size-1] = '\0';
	}
	else
		strcpy(buffer, link_destination.c_str());
	return 0;
}

/** Create a symbolic link */
int pok_symlink (const char *link_destination, const char *user_path)
{

	struct fuse_file_info fi;
	fi.fh = fi.flags = fi.lock_owner = fi.direct_io = 0;

	int err = pok_create(user_path, S_IFLNK | S_IRWXU | S_IRGRP | S_IXGRP | S_IXOTH, &fi);
	if(err)
		return err;
	pok_release(user_path, &fi);

	/* TODO: Add DB update to KV store before updating local hashmap. */

	PRIV->pmap->addSoftLink(user_path, link_destination);
	return 0;
}
