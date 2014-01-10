#include "main.h"
#include "debug.h"
#include <uuid/uuid.h>

/** Remove a file */
int pok_unlink(const char *user_path)
{
	pok_trace("Attempting to remove file with user path: %s",user_path);

	/* Lookup metadata of supplied path and its directory. */
	std::unique_ptr<MetadataInfo> mdi(new MetadataInfo());
	int err = lookup(user_path, mdi);
	if (err)
		return err;

	std::unique_ptr<MetadataInfo> mdi_dir(new MetadataInfo());
	err = lookup_parent(user_path, mdi_dir);
	if (err)
		return err;

	/* Delete Metadata Key*/
	NamespaceStatus status = PRIV->nspace->deleteMD(mdi.get());
	if(status.notOk()){
		pok_warning("Failed deleting Metadata Key");
		return -EIO;
	}

	/* Remove associated directory entry. */
	posixok::DirectoryEntry e;
	e.set_name(path_to_filename(user_path));
	e.set_type(e.SUB);
	err = directory_addEntry(mdi_dir, e);
	if(err)
		pok_error("Unrecoverable error in unlink operation. File system might be corrupt.");

	/* TODO: Hand over all data keys to Housekeeping. */
	return err;
}

/** File open operation
 *
 * No creation (O_CREAT, O_EXCL) and by default also no
 * truncation (O_TRUNC) flags will be passed to open(). If an
 * application specifies O_TRUNC, fuse first calls truncate()
 * and then open(). Only if 'atomic_o_trunc' has been
 * specified and kernel version is 2.6.24 or later, O_TRUNC is
 * passed on to open.
 *
 * Unless the 'default_permissions' mount option is given,
 * open should check if the operation is permitted for the
 * given flags. Optionally open may also return an arbitrary
 * filehandle in the fuse_file_info structure, which will be
 * passed to all file operations.
 *
 * Changed in version 2.2
 */
int pok_open(const char *user_path, struct fuse_file_info *fi)
{
	pok_debug("Attempting to open file with user path: %s",user_path);
	if(fi->fh){
		// This could be perfectly legal, I am not sure how fuse works... if it is
		// we need to reference count metadata_info structures.
		pok_error("File handle supplied when attempting to open user path %s",user_path);
		return -EINVAL;
	}

	std::unique_ptr<MetadataInfo> mdi(new MetadataInfo());
	int err = lookup(user_path, mdi);
	if (err)
		return err;

	// while this isn't pretty, it's probably the best way to use fi->fh
	fi->fh = reinterpret_cast<std::uint64_t>(mdi.release());
	return 0;
}



static void initialize_metadata(std::unique_ptr<MetadataInfo> &mdi, const std::unique_ptr<MetadataInfo> &mdi_parent, mode_t mode)
{
	uuid_t uuid;
	uuid_generate(uuid);
	char uuid_parsed[100];
	uuid_unparse(uuid, uuid_parsed);

	mdi->updateACMtime();
	mdi->pbuf()->set_id_group(fuse_get_context()->gid);
	mdi->pbuf()->set_id_user (fuse_get_context()->uid);
	mdi->pbuf()->set_mode(mode);
	mdi->pbuf()->set_data_unique_id(uuid_parsed);

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

/**
 * Create and open a file
 *
 * If the file does not exist, first create it with the specified
 * mode, and then open it.
 *
 * If this method is not implemented or under Linux kernel
 * versions earlier than 2.6.15, the mknod() and open() methods
 * will be called instead.
 *
 * Introduced in version 2.5
 */
int pok_create(const char *user_path, mode_t mode, struct fuse_file_info *fi)
{
	pok_trace("Attempting to create file with user path: %s",user_path);
	if(fi->fh){
		// This could be perfectly legal, I am not sure how fuse works... if it is
		// we need to reference count metadata_info structures.
		pok_error("File handle supplied when attempting to create user path %s",user_path);
		return -EINVAL;
	}
	std::unique_ptr<MetadataInfo> mdi(new MetadataInfo());
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

	// while this isn't pretty, it's probably the best way to use fi->fh
	fi->fh = reinterpret_cast<std::uint64_t>(mdi.release());
	pok_trace("Successfully created user path %s.",user_path);
	return 0;
}

/** Release an open file
 *
 * Release is called when there are no more references to an open
 * file: all file descriptors are closed and all memory mappings
 * are unmapped.
 *
 * For every open() call there will be exactly one release() call
 * with the same flags and file descriptor.	 It is possible to
 * have a file opened more than once, in which case only the last
 * release will mean, that no more reads/writes will happen on the
 * file.  The return value of release is ignored.
 *
 * Changed in version 2.2
 */
int pok_release (const char *user_path, struct fuse_file_info *fi)
{
	MetadataInfo * mdi = reinterpret_cast<MetadataInfo *>(fi->fh);
	if(mdi){
		pok_trace("Releasing in-memory metadata information for system path: %s (user path: %s)",mdi->getSystemPath().c_str(),user_path);
		delete(mdi);
		fi->fh = 0;
	}
	return 0;
}
