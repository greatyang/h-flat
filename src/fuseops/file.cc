#include "main.h"
#include "debug.h"

static int unlink_fsdo(const char *user_path)
{
	/* Lookup metadata of supplied path and its directory. */
	std::unique_ptr<MetadataInfo> mdi(new MetadataInfo());
	int err = lookup(user_path, mdi);
	if (err)
		return err;

	std::unique_ptr<MetadataInfo> mdi_dir(new MetadataInfo());
	err = lookup_parent(user_path, mdi_dir);
	if (err)
		return err;

	/* Unlink Metadata Key. If other names for the key continue to exist just decrease the link-counter. */
	mdi->pbuf()->set_link_count( mdi->pbuf()->link_count() - 1);
	NamespaceStatus status = NamespaceStatus::makeInvalid();
	if(mdi->pbuf()->link_count()){
		assert(mdi->pbuf()->is_hardlink_target());
		status = PRIV->nspace->putMD(mdi.get());
	}
	else
		status = PRIV->nspace->deleteMD(mdi.get());
	if(status.notOk()){
		pok_warning("Failed putting / deleting Metadata Key");
		return -EIO;
	}

	/* Remove associated directory entry. */
	posixok::DirectoryEntry e;
	e.set_name(path_to_filename(user_path));
	e.set_type(e.SUB);
	err = directory_addEntry(mdi_dir, e);

	/* PARTIAL UNDO */
	if(err){
		mdi->pbuf()->set_link_count( mdi->pbuf()->link_count() + 1);
		status = PRIV->nspace->putMD(mdi.get());
		if(status.notOk())
			kill_compound_fail();
	}
	return err;
}

static int unlink_fsundo(const char *user_path, MetadataInfo *mdi)
{
	assert(mdi);

	std::unique_ptr<MetadataInfo> mdi_dir(new MetadataInfo());
	int err = lookup_parent(user_path, mdi_dir);
	if (err)
		return err;

	mdi->pbuf()->set_link_count( mdi->pbuf()->link_count() + 1);
	NamespaceStatus status = PRIV->nspace->putMD(mdi);
	if(status.notOk())
		return -EIO;

	/* Add associated directory entry. */
	posixok::DirectoryEntry e;
	e.set_name(path_to_filename(user_path));
	e.set_type(e.ADD);
	return directory_addEntry(mdi_dir, e);
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
	int err = create_from_mdi(user_path, mode, mdi);
	if(err)
		return err;

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
