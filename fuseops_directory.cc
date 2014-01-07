/* If handling of directory data looks unintuitive, check directorydata.proto for an explanation */

#include "main.h"
#include "debug.h"
#include "directorydata.pb.h"
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


NamespaceStatus KineticNamespace::append( MetadataInfo *mdi, const std::string &value)
{
	std::string  data;
	const 	 int blocksize	= 1024 * 1024;
	unsigned int blocknumber = ( mdi->pbuf()->size() + value.length() ) / blocksize;

	std::string key, version, tag;
	key = mdi->pbuf()->data_unique_id() + std::to_string(blocknumber);

	// get block data
	kinetic::KineticStatus status = connection->Get(key, &data, &version, &tag);
	if(status.notAuthorized()){
		pok_warning("No authorization to read key.");
		return status;
	}

	// update block data
	data.append(value);

	// put block data with changed version number to get atomic behavior. Retry as necessary
	kinetic::KineticRecord record(data, incr(version), "", com::seagate::kinetic::proto::Message_Algorithm_SHA1);
	status = connection->Put(key, version, record);
	if(status.versionMismatch())
		return append(mdi,value);
	return status;
}
*/


int directory_addEntry(const std::unique_ptr<MetadataInfo> &mdi, const posixok::DirectoryEntry &e)
{
	/* Step 1) obtain last allocated directory data block */
	if(!mdi->pbuf()->blocks())
		mdi->pbuf()->set_blocks(1);
	std::string value;
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

int directory_removeName(const std::unique_ptr<MetadataInfo> &mdi, std::string filename , const unsigned int blocknum)
{
	/* Step 1) obtain supplied data block */
	std::string value;
	NamespaceStatus status = PRIV->nspace->get(mdi.get(), blocknum, &value);
	if(status.notOk()) return -EIO;

	posixok::DirectoryData data;
	if(!data.ParseFromString(value)){
		pok_warning("Failure parsing directory data -> data corruption. ");
		return -EINVAL;
	}

	/* Step 2) Check the directory data block for the supplied file name */
	for(int i = 0; i < data.entries_size(); i++){
		posixok::DirectoryEntry *e = data.mutable_entries(i);

		/* Found the filename in the directory data */
		if( (filename.compare(e->name()) == 0 ) &&
			(e->type() == e->ADD) )
		{
			/* Protocol buffer repeated fields can only remove the last element.
			 * To remove an arbitrary element, copy the data of the last element into
			 * the to-be-removed element, then remove the last element. */
			const posixok::DirectoryEntry &last = data.entries( data.entries_size() - 1 );
			e->CopyFrom(last);
			data.mutable_entries()->RemoveLast();

			status = PRIV->nspace->put(mdi.get(), blocknum, data.SerializeAsString(), PutModeType::ATOMIC);
			if(status.versionMismatch()) // couldn't write atomically, somebody else wrote to the same data block after we read it in
				return directory_removeName(mdi, filename, blocknum);

			/* Step 3) update directory metadata to reflect changes to directory data */
			mdi->pbuf()->set_size(mdi->pbuf()->size() - e->ByteSize());
			mdi->updateACMtime();
			status = PRIV->nspace->putMD(mdi.get());
			if(status.notOk()) return -EIO;
			return 0;
		}
	}

	/* Step 4) If the name hasn't been removed from the current data block, try others if there are any */
	if( blocknum < mdi->pbuf()->blocks() )
		return directory_removeName(mdi, filename, blocknum+1);

	return -ENOENT;
}

int directory_removeName(const std::unique_ptr<MetadataInfo> &mdi, std::string filename)
{
	return directory_removeName(mdi, filename, 0);
}

int directory_addName(const std::unique_ptr<MetadataInfo> &mdi_dir, std::string filename)
{
	posixok::DirectoryEntry entry;
	entry.set_name(filename);
	return directory_addEntry(mdi_dir,entry);
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
