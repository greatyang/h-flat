#include "main.h"
#include "debug.h"
#include "fuseops.h"
#include <algorithm>

/** Create a directory */
int pok_mkdir 		(const char *user_path, mode_t mode)
{
	std::unique_ptr<MetadataInfo> mdi(new MetadataInfo());
	int err = create_from_mdi(user_path, mode | S_IFDIR, mdi);
	if(!err)
		pok_trace("Created directory @ user path: %s",user_path);
	return err;
}

/** Remove a directory */
int pok_rmdir (const char *user_path)
{
	/* Update database so that all directory entries will be counted by readdir. */
	database_update();

	struct fuse_file_info fi;
	fi.fh = 0;

	int err = pok_open(user_path, &fi);
	if(err)
		return err;
	int num_entries = pok_readdir(user_path, 0, 0, 0, &fi);
	pok_release(user_path, &fi);

	if(num_entries)
		return -ENOTEMPTY;

	return pok_unlink(user_path);
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
	if(check_access(mdi, R_OK)) // ls requires read permission
		return -EACCES;


	pok_debug("Reading directory at user path %s with %d allocated blocks and a byte size of %d",user_path,mdi->pbuf()->blocks(),mdi->pbuf()->size());

	std::unordered_map<std::string, int> ncount;
	posixok::DirectoryData data;
	posixok::DirectoryEntry e;
	std::string value;
	std::int64_t snapshotVersion = PRIV->pmap->getSnapshotVersion();

	for(unsigned int blocknum=0; blocknum < mdi->pbuf()->blocks(); blocknum++)
	{
		if(PRIV->nspace->get(mdi, blocknum, value).notOk()){
			pok_warning("Failed obtaining block #%d.", blocknum);
			continue;
		}
		if(!data.ParseFromString(value)){
			pok_warning("Failure parsing directory data for data block #%d -> data corruption. ",blocknum);
			continue;
		}

		for(int i = 0; i < data.entries_size(); i++) {
			e = data.entries(i);

			/* If the required db version of the entry is higher than the current snapshot version, the entry is invisible to this client. */
			if(e.req_version() > snapshotVersion)
				continue;
			if(e.type() == e.ADD) ncount[e.name()]++;
			else				  ncount[e.name()]--;
		}
	}

	int num_elements = 0;

	for (auto& element : ncount) {
		assert(element.second == 0 || element.second == 1);
		if(element.second == 1){
			/* We want to (ab)use this function in rmdir to check if this directory is empty. */
			if(!filldir) num_elements++;
			else
				filldir(buffer, element.first.c_str(), NULL, 0);
		}
	}
	return num_elements;
}
