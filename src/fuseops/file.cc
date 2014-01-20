#include "main.h"
#include "debug.h"
#include "kinetic_helper.h"

using namespace util;

static int unlink_fsdo(const char *user_path)
{
	std::unique_ptr<MetadataInfo> mdi(new MetadataInfo());
	std::unique_ptr<MetadataInfo> mdi_dir(new MetadataInfo());
	int err;

	/* Lookup metadata of supplied path and its directory. */
			 err = lookup(user_path, mdi);
	if(!err) err = lookup_parent(user_path, mdi_dir);
	if( err) return err;

	/* Removing a file requires write + execute permission on directory. */
	if(check_access(mdi_dir.get(), W_OK | X_OK))
		return -EACCES;
	/* If sticky bit is set on directory, current user needs to be owner of directory OR file (or root of course). */
	if(  fuse_get_context()->uid &&	(mdi_dir->pbuf()->mode() & S_ISVTX) &&
		(fuse_get_context()->uid != mdi_dir->pbuf()->uid()) &&
		(fuse_get_context()->uid != mdi->pbuf()->uid()) )
		return -EACCES;

	/* Unlink Metadata Key. If other names for the key continue to exist just decrease the link-counter. */
	mdi->pbuf()->set_link_count( mdi->pbuf()->link_count() - 1);
	pok_debug("link count after unlink: %d",mdi->pbuf()->link_count());
	if(mdi->pbuf()->link_count())
		err = put_metadata(mdi.get());
	else
		err = delete_metadata(mdi.get());
	if(err)
		return err;

	/* If we got ourselves a hardlink, make sure to delete the corresponding HARDLINK_S forwarding key */
	if(mdi->pbuf()->type() == posixok::Metadata_InodeType_HARDLINK_T){
		pok_debug("Detected Hardlink Inode");
		std::unique_ptr<MetadataInfo> mdi_source(new MetadataInfo());
			err = lookup(user_path, mdi_source, false);
		if(!err){
			assert(mdi_source->pbuf()->type() == posixok::Metadata_InodeType_HARDLINK_S);
			err = delete_metadata(mdi_source.get());
		}
		if(err)
			pok_warning("Failed deleting internal hardlink source metadata key after successfully unlinking hardlink target key");
	}

	/* Remove associated directory entry. */
	err = delete_directory_entry(mdi_dir, path_to_filename(user_path));
	if(err)
		pok_warning("Failed deleting directory entry after successfully unlinking inode.");
	return err;
}

static int unlink_fsundo(const char *user_path, MetadataInfo *mdi)
{
	assert(mdi);
	std::unique_ptr<MetadataInfo> mdi_dir(new MetadataInfo());
	if( int err = lookup_parent(user_path, mdi_dir) )
		return err;

	mdi->pbuf()->set_link_count( mdi->pbuf()->link_count() + 1);
	if (int err = put_metadata(mdi))
		return err;

	return create_directory_entry(mdi_dir, path_to_filename(user_path));
}

/** Remove a file */
int pok_unlink(const char *user_path)
{
	/* TODO: Hand over all data keys to Housekeeping after a sucessfull unlink operation. */
	pok_trace("Attempting to remove file with user path: %s",user_path);

	if(PRIV->pmap->hasMapping(user_path)){
		/* Remove associated path mappings. The following sequence for example should not leave a mapping behind.
			 * 	mkdir a   mv a b 	rmdir b */
		posixok::db_entry entry;
		entry.set_type(posixok::db_entry_TargetType_REMOVED);
		entry.set_origin(user_path);

		/* Lookup metadata of supplied path in case the operation needs to be undone. */
		std::unique_ptr<MetadataInfo> mdi(new MetadataInfo());
		int err = lookup(user_path, mdi);
		if (err)
			return err;

		return database_operation(
				std::bind(unlink_fsdo,  user_path),
				std::bind(unlink_fsundo,user_path, mdi.get()),
				entry);
	}
	return unlink_fsdo(user_path);
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
	pok_trace("Attempting to open file with user path: %s",user_path);
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

	int access = fi->flags & O_ACCMODE;
	if(access == O_RDONLY) err = check_access(mdi.get(), R_OK);
	if(access == O_WRONLY) err = check_access(mdi.get(), W_OK);
	if(access == O_RDWR)   err = check_access(mdi.get(), R_OK | W_OK);
	if(err)
		return err;


	// while this isn't pretty, it's probably the best way to use fi->fh
	fi->fh = reinterpret_cast<std::uint64_t>(mdi.release());
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

static void initialize_metadata(const std::unique_ptr<MetadataInfo> &mdi, const std::unique_ptr<MetadataInfo> &mdi_parent, mode_t mode)
{
	mdi->updateACMtime();
	mdi->pbuf()->set_type(mdi->pbuf()->POSIX);
	mdi->pbuf()->set_gid(fuse_get_context()->gid);
	mdi->pbuf()->set_uid(fuse_get_context()->uid);
	mdi->pbuf()->set_mode(mode);
	mdi->pbuf()->set_inode_number(generate_inode_number());

	if(!mdi->pbuf()->has_data_unique_id()) // TODO: remove this once the code requiring such nonsence is gone
		mdi->pbuf()->set_data_unique_id(generate_uuid());


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
int pok_fcreate(const char *user_path, mode_t mode, struct fuse_file_info *fi)
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

	/* Add filename to directory */
	err = create_directory_entry(mdi_dir, path_to_filename(user_path));
	if(err)
		return err;

	/* initialize metadata and write metadata-key to drive*/
	initialize_metadata(mdi, mdi_dir, mode);
	err = create_metadata(mdi.get());
	if(err){
		pok_warning("Failed creating metadata key after successfully creating directory-entry key. \n"
					"Dangling directory entry!");
		return err;
	}

	// while this isn't pretty, it's probably the best way to use fi->fh
	fi->fh = reinterpret_cast<std::uint64_t>(mdi.release());
	pok_trace("Successfully created user path %s.",user_path);
	return 0;
}


int pok_create(const char *user_path, mode_t mode)
{
	struct fuse_file_info fi;
	fi.fh = 0;
	if( int err = pok_fcreate(user_path, mode, &fi))
		return err;
	pok_release(user_path, &fi);
	return 0;
}

/** Create a file node
 *
 * This is called for creation of all non-directory, non-symlink
 * nodes.  If the filesystem defines a create() method, then for
 * regular files that will be called instead.
 */
int pok_mknod(const char* user_path, mode_t mode, dev_t rdev)
{
	return pok_create(user_path, mode);
}
