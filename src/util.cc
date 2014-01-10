#include "main.h"
#include "debug.h"

int lookup(const char *user_path, const std::unique_ptr<MetadataInfo> &mdi)
{
	std::string key, value, version;
	std::int64_t pathPermissionTimeStamp = 0;

	/* Step 1: Transform user path to system path and obtain required path permission timestamp */
	key = PRIV->pmap->toSystemPath(user_path, pathPermissionTimeStamp, CallingType::LOOKUP);
	if(pathPermissionTimeStamp < 0)
		return pathPermissionTimeStamp;

	/* Step 2: Get metadata from flat namespace */
	mdi->setSystemPath(key);
	NamespaceStatus getMD = PRIV->nspace->getMD(mdi.get());
	if(getMD.notFound())
		return -ENOENT;
	if(getMD.notAuthorized())
		return -EPERM;
	if(getMD.notValid())
		return -EINVAL;

	/* Step 3: check path permissions for staleness */
	bool stale = mdi->pbuf()->path_permission_verified() < pathPermissionTimeStamp;
	if(stale){
		/* TODO: validate path permissions up the directory tree, recursively as necessary */
		pok_warning("Stale path permissions detected. Re-validation not implemented yet.");
	}
	return 0;
}


/* Path permissions are already verified. The systemPath, however, needs to be computed fresh(!). */
int lookup_parent(const char *user_path, const std::unique_ptr<MetadataInfo> &mdi_parent)
{
	std::string key(user_path);
	auto pos =  key.find_last_of("/");
	if(pos == std::string::npos)
		return -EINVAL;
	if(!pos) // root directory
		pos++;
	key.erase(pos,std::string::npos);

	return lookup(key.c_str(), mdi_parent);
}


/* Adding an entry to a directory. To remove an entry add an entry with type SUB and let housekeeping handle the rest. */
int directory_addEntry(const std::unique_ptr<MetadataInfo> &mdi, const posixok::DirectoryEntry &e)
{
	std::string value;
	NamespaceStatus status = PRIV->nspace->get(mdi.get(), mdi->pbuf()->blocks() - 1 , value);
	if(status.notFound()){
		// If we have a fresh block that's fine.
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


int update_pathmapDB()
{
	std::int64_t dbVersion;
	NamespaceStatus status = PRIV->nspace->getDBVersion(dbVersion);
	if(status.notOk()){
		pok_warning("Cannot access database.");
		return -EINVAL;
	}
	std::int64_t snapshotVersion = PRIV->pmap->getSnapshotVersion();

	/* Sanity */
	//assert(dbVersion >= snapshotVersion);
	if(dbVersion<snapshotVersion){
		pok_error("Read in dbVersion: %d, local snapshotVersion %d. ");
		return -EINVAL;
	}

	/* Nothing to do. */
	if(dbVersion == snapshotVersion)
		return 0;

	/* Update */
	std::list<posixok::db_entry> entries;
	posixok::db_entry entry;
	for(std::int64_t v = snapshotVersion+1; v <= dbVersion; v++){
		status = PRIV->nspace->getDBEntry(v, entry);
		if(status.notOk()){
				pok_warning("Cannot access database.");
				return -EINVAL;
			}
		entries.push_back(entry);
	}
	return PRIV->pmap->updateSnapshot(entries, snapshotVersion, dbVersion);
}
