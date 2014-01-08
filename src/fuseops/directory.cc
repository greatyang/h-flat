#include "main.h"
#include "debug.h"
#include "directorydata.pb.h"
#include <algorithm>

/* Adding an entry to a directory. To remove an entry add an entry with type SUB and let housekeeping handle the rest. */
int directory_addEntry(const std::unique_ptr<MetadataInfo> &mdi, const posixok::DirectoryEntry &e)
{
		std::string value;

	/* */
	NamespaceStatus status = PRIV->nspace->get(mdi.get(), mdi->pbuf()->blocks() - 1 , &value);
	if(status.notFound()){
		/* If we have a fresh block that's fine. */
	}
	else if(status.notOk()) return -EIO;

	posixok::DirectoryData data;
	if(!data.ParseFromString(value)){
		pok_warning("Failure parsing directory data -> data corruption. ");
		return -EINVAL;
	}

	/* Step 2) add directory entry to existing directory data block */
	posixok::DirectoryEntry *entry = data.add_entries();
	entry->CopyFrom(e);
	if(data.ByteSize() > PRIV->blocksize){
		mdi->pbuf()->set_blocks( mdi->pbuf()->blocks() + 1 );
		return directory_addEntry(mdi, e);
	}

	/* Step 3) store updated directory data block. There could be a race condition with other clients adding or removing files from the
	 * same directory. If the write fails for this reason, retry starting at step 1) */
	status = PRIV->nspace->put(mdi.get(), mdi->pbuf()->blocks() - 1, data.SerializeAsString(), PutModeType::ATOMIC);
	if(status.versionMismatch()) // couldn't write atomically, somebody else wrote to the same data block after we read it in
		return directory_addEntry(mdi, e);
	if(status.notOk()) return -EIO;

	/* Step 4) update directory metadata to reflect changes to directory data */
	mdi->pbuf()->set_size(mdi->pbuf()->size() + e.ByteSize());
	mdi->updateACMtime();
	status = PRIV->nspace->putMD(mdi.get());
	if(status.notOk()) return -EIO;
	return 0;
}


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

	std::unordered_map<std::string, int> ncount;
	std::int64_t snapshotVersion = PRIV->pmap->getSnapshotVersion();

	for(unsigned int blocknum=0; blocknum < mdi->pbuf()->blocks(); blocknum++)
	{
		std::string value;
		if(PRIV->nspace->get(mdi, blocknum, &value).notOk()){
			pok_warning("Failed obtaining block #%d.", blocknum);
			continue;
		}

		posixok::DirectoryData data;
		if(!data.ParseFromString(value)){
			pok_warning("Failure parsing directory data for data block #%d -> data corruption. ",blocknum);
			continue;
		}

		posixok::DirectoryEntry e;
		for(int i = 0; i < data.entries_size(); i++)
		{
			e = data.entries(i);

			/* If the required db version of the entry is higher than the current snapshot version,
			 * the entry is invisible to this client. */
			if(e.req_version() > snapshotVersion)
				continue;

			if(e.type() == e.ADD)
				ncount[e.name()]++;
			else
				ncount[e.name()]--;
		}
	}

	for (auto& element : ncount) {
		if(element.second == 1){
			filldir(buffer, element.first.c_str(), NULL, 0);
			continue;
		}

		/* Sanity check */
		if(element.second < 0 || element.second > 1)
			pok_warning("Counted directory entry '%s' %d times -> corrupted directory data. ", element.first.c_str(), element.second);
	}
	return 0;
}
