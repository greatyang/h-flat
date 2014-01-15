#include "main.h"
#include "debug.h"
#include <uuid/uuid.h>

int lookup(const char *user_path, const std::unique_ptr<MetadataInfo> &mdi)
{
	std::string key, value, version;
	std::int64_t pathPermissionTimeStamp = 0;

	/* Step 1: Transform user path to system path and obtain required path permission timestamp */
	key = PRIV->pmap->toSystemPath(user_path, pathPermissionTimeStamp, CallingType::LOOKUP);
	if(pathPermissionTimeStamp < 0)
		return pathPermissionTimeStamp;

	if(key.compare(user_path))
		pok_trace("Remapped user path %s to system path %s ",user_path, key.c_str());

	/* Step 2: Get metadata from flat namespace */
	mdi->setSystemPath(key);
	NamespaceStatus getMD = PRIV->nspace->getMD(mdi.get());
	if(getMD.notFound())
		return -ENOENT;
	if(getMD.notAuthorized()){
		pok_warning("Lookup of user_path %s returned EPERM",user_path);
		return -EPERM;
	}
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



std::string uuid_get(void)
{
	uuid_t uuid;
	uuid_generate(uuid);
	char uuid_parsed[100];
	uuid_unparse(uuid, uuid_parsed);
	return std::string(uuid_parsed);
}


static void initialize_metadata(const std::unique_ptr<MetadataInfo> &mdi, const std::unique_ptr<MetadataInfo> &mdi_parent, mode_t mode)
{
	mdi->updateACMtime();
	mdi->pbuf()->set_id_group(fuse_get_context()->gid);
	mdi->pbuf()->set_id_user (fuse_get_context()->uid);
	mdi->pbuf()->set_mode(mode);
	mdi->pbuf()->set_data_unique_id(uuid_get());

	/* Inherit path permissions existing for directory */
	mdi->pbuf()->mutable_path_permission()->CopyFrom(mdi_parent->pbuf()->path_permission());
	/* Inherit logical timestamp when the path permissions have last been verified to be up-to-date */
	mdi->pbuf()->set_path_permission_verified(mdi_parent->pbuf()->path_permission_verified());

	/* Add path permissions precomputed for directory's children. */
	for(int i=0; i < mdi_parent->pbuf()->path_permission_children_size(); i++){
		posixok::Metadata::ReachabilityEntry *e = mdi->pbuf()->add_path_permission();
		e->CopyFrom( mdi_parent->pbuf()->path_permission_children(i) );
	}

	if(S_ISDIR(mode)){
		mdi->pbuf()->set_blocks(1);
		mdi->computePathPermissionChildren();
	}
}

/* Create metadata-key and add its name to parent directory. This function is used by all
 * fuseops that want to create a file (create, symlink, link)  */
int create_from_mdi(const char *user_path, mode_t mode, const std::unique_ptr<MetadataInfo> &mdi)
{
	int err = lookup(user_path, mdi);
	if(!err)
		return -EEXIST;
	if (err != -ENOENT)
		return err;

	std::unique_ptr<MetadataInfo> mdi_dir(new MetadataInfo());
	err = lookup_parent(user_path, mdi_dir);
	if(err)
		return err;

	/* File: initialize metadata and write metadata-key to drive*/
	initialize_metadata(mdi, mdi_dir, mode);
	NamespaceStatus status = PRIV->nspace->putMD(mdi.get());
	if(status.notOk()){
		pok_warning("Failed creating key '%s' due to %s",mdi->getSystemPath().c_str());
		return -EINVAL;
	}

	/* Add filename to directory */
	posixok::DirectoryEntry e;
	e.set_name( path_to_filename(mdi->getSystemPath()) );
	err = directory_addEntry( mdi_dir, e );
	if(err){
		pok_error("Failed updating parent directory of user path '%s' ",user_path);
		return err;
	}
	return 0;
}
