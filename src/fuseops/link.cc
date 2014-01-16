#include "main.h"
#include "debug.h"
#include "fuseops.h"

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
		pok_debug("buffer too small to fit link destination.");
		strncpy(buffer, link_destination.c_str(), size);
		buffer[size-1] = '\0';
	}
	else
		strcpy(buffer, link_destination.c_str());
	return 0;
}

static int symlink_fsdo(const char *origin)
{
	std::unique_ptr<MetadataInfo> mdi(new MetadataInfo());
	return create_from_mdi(origin, S_IFLNK | S_IRWXU | S_IRGRP | S_IXGRP | S_IXOTH, mdi);
}

/** Create a symbolic link */
int pok_symlink (const char *target, const char *origin)
{
	posixok::db_entry entry;
	entry.set_type(entry.SYMLINK);
	entry.set_origin(origin);
	entry.set_target(target);

	return database_operation(
			std::bind(symlink_fsdo, origin),
			std::bind(pok_unlink,origin),
			entry);
}


static int hardlink_fsdo(MetadataInfo *mdi, const char *user_path_origin)
{
	std::unique_ptr<MetadataInfo> mdi_dir(new MetadataInfo());
	int err = lookup_parent(user_path_origin, mdi_dir);
	if(err)
		return err;

	mdi->pbuf()->set_link_count( mdi->pbuf()->link_count() + 1 );
	NamespaceStatus status = PRIV->nspace->putMD(mdi);
	if(status.notOk())
		return -EIO;

	posixok::DirectoryEntry de;
	de.set_name(path_to_filename(user_path_origin));
	err = directory_addEntry(mdi_dir, de);

	/* Failed adding directory entry -> UNDO linkcount increase */
	if(err){
		mdi->pbuf()->set_link_count( mdi->pbuf()->link_count() - 1 );
		NamespaceStatus status = PRIV->nspace->putMD(mdi);
		if(status.notOk())
			kill_compound_fail();
	}
	return err;
}

static int hardlink_fsundo(MetadataInfo *mdi, const char *user_path_origin)
{
	std::unique_ptr<MetadataInfo> mdi_dir(new MetadataInfo());
	int err = lookup_parent(user_path_origin, mdi_dir);
	if(err)
		return err;

	mdi->pbuf()->set_link_count( mdi->pbuf()->link_count() -1 );
	NamespaceStatus status = PRIV->nspace->putMD(mdi);
	if(status.notOk())
		return -EIO;

	posixok::DirectoryEntry de;
	de.set_name(path_to_filename(user_path_origin));
	de.set_type(de.SUB);
	return directory_addEntry(mdi_dir, de);
}


static int md_key_move (std::string keyFrom, std::string keyTo)
{
	/* get md key */
	std::unique_ptr<MetadataInfo> mdi(new MetadataInfo(keyFrom, ""));
	NamespaceStatus status = PRIV->nspace->getMD(mdi.get());
	if(status.notFound())
		return -ENOENT;
	if(status.notValid())
		return -EINVAL;
	if(status.notAuthorized())
		return -EPERM;

	/* remove md key from original location */
	status = PRIV->nspace->deleteMD(mdi.get());
	if(status.notOk())
		return -EIO;

	/* store md key at new location */
	mdi->setSystemPath(keyTo);
	if(keyTo.compare(0,8,"hardlink_",0,8) == 0)
		mdi->pbuf()->set_is_hardlink_target(true);
	status = PRIV->nspace->putMD(mdi.get());

	/* try to recover if encountering a compound failure. */
	if(status.notOk()){
		mdi->setSystemPath(keyFrom);
		if(keyFrom.compare(0,8,"hardlink_",0,8) != 0)
			mdi->pbuf()->set_is_hardlink_target(false);
		NamespaceStatus status = PRIV->nspace->putMD(mdi.get());
		if(status.notOk())
			kill_compound_fail();
		return -EIO;
	}
	return 0;
}



/*  Please note: Hardlinks could be alternatively implemented without using the database:
 *		indirection-metadata at the name-locations (possible because no directories are hardlinks)
 *		which is followed inside the file system would do the same trick.
 *		Although with 1 additional drive access each lookup.
 */
/** Create a hard link to a file */
int pok_hardlink (const char *target, const char *origin)
{
	std::unique_ptr<MetadataInfo> mdi(new MetadataInfo());
	int err = lookup(target, mdi);
	if (err)
		return err;
	assert(!S_ISDIR(mdi->pbuf()->mode()));

	pok_trace("Requested hardlink from user path %s to file at user_path %s.", origin, target);

	/* If the target metadata is not already a hardlink_target, make it so. */
	if(! mdi->pbuf()->is_hardlink_target() ){
		posixok::db_entry entry;
		entry.set_type(entry.HARDLINK);
		entry.set_origin(target);
		entry.set_target("hardlink_"+uuid_get());

		int err = database_operation(
				std::bind(md_key_move, mdi->getSystemPath(), entry.target()),
				std::bind(md_key_move, entry.target(), mdi->getSystemPath()),
				entry);
		if(err)
			return err;

		mdi->setSystemPath(entry.target());
		mdi->pbuf()->set_is_hardlink_target(true);
	}

	posixok::db_entry entry;
	entry.set_type(entry.HARDLINK);
	entry.set_origin(origin);
	entry.set_target(mdi->getSystemPath());

	return database_operation(
			std::bind(hardlink_fsdo,   mdi.get(), origin),
			std::bind(hardlink_fsundo, mdi.get(), origin),
			entry);
}
