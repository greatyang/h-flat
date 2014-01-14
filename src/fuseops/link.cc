#include "main.h"
#include "debug.h"
#include "fuseops.h"
#include <uuid/uuid.h>

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



/** Create a symbolic link */
int pok_symlink (const char *target, const char *origin)
{
	/* 1) ensure that database is up-to-date, update if necessary */
	int err = update_pathmapDB();
	if(err)
		return err;

	/* 2) do a normal create (adds name to directory, creates metadata key) */
	std::unique_ptr<MetadataInfo> mdi(new MetadataInfo());
	err = create_from_mdi(origin, S_IFLNK | S_IRWXU | S_IRGRP | S_IXGRP | S_IXOTH, mdi);
	if(err)
		return err;

	/* 3) add db_entry to permanent storage. */
	posixok::db_entry entry;
	entry.set_type(posixok::db_entry_TargetType_LINK);
	entry.set_origin(origin);
	entry.set_target(target);
	NamespaceStatus status = PRIV->nspace->putDBEntry(PRIV->pmap->getSnapshotVersion()+1, entry);

	/* If 3) failed, undo the previous create. */
	if(status.notOk()){
		err = pok_unlink(origin);
		if(err){
			pok_error("Failed unlinking created metadata key after failure in database update. File System might be corrupt.");
			return err;
		}

		/* snapshot wasn't up-to-date after all. retry */
		if(status.versionMismatch())
			return pok_symlink(target, origin);

		pok_warning("Failed putting DBEntry, cannot create symlink: %s -> %s",origin,target);
		return -EINVAL;
	}

	/* 4) add to in-memory hashmap */
	PRIV->pmap->addSoftLink(origin, target);
	return 0;
}


/** Create a hard link to a file */

/* The link mechanism used for softlinks (using pathmap_db) can be used for hardlinks.
 * However: move the metadata of the hardlink to a UUID-key
 * 		    so that all names can be unlinked / moved / etc independently without affecting other names.
 */
int pok_hardlink (const char *target, const char *origin)
{
	/* ensure that database is up-to-date, update if necessary */
	int err = update_pathmapDB();
	if(err)
		return err;

	std::unique_ptr<MetadataInfo> mdi_target(new MetadataInfo());
    err = lookup(target, mdi_target);
	if (err)
		return err;
	assert(!S_ISDIR(mdi_target->pbuf()->mode()));


	/* The target of a hardlink has to be a hardlink_target.
	 * If this isn't the case, make it so. */
	if(! mdi_target->pbuf()->is_hardlink_target() ){
		uuid_t uuid;
		uuid_generate(uuid);
		char uuid_parsed[100];
		uuid_unparse(uuid, uuid_parsed);

		/* add db_entry to permanent storage. */
		posixok::db_entry entry;
		entry.set_type(posixok::db_entry_TargetType_LINK);
		entry.set_origin(target);
		entry.set_target(uuid_parsed);
		NamespaceStatus status = PRIV->nspace->putDBEntry(PRIV->pmap->getSnapshotVersion()+1, entry);

		/* retry on version missmatch */
		if(status.versionMismatch())
			return pok_hardlink(target, origin);
		if(status.notOk())
			return -EIO;

		/* add to in-memory hashmap */
		PRIV->pmap->addSoftLink(entry.origin(), entry.target());


		/* Move Metadata to entry->target. */
		status = PRIV->nspace->deleteMD(mdi_target.get());
		mdi_target->setSystemPath(entry.target());
		mdi_target->pbuf()->set_is_hardlink_target(true);
		if(status.ok())
			status = PRIV->nspace->putMD(mdi_target.get());
		if(status.notOk()){
			pok_error("Failed putting updated hardlink metadata after successful db update. FS might be corrupt.");
			return -EIO;
		}
	}
	
	/* add db_entry to permanent storage. */
	posixok::db_entry entry;
	entry.set_type(posixok::db_entry_TargetType_LINK);
	entry.set_origin(origin);
	entry.set_target(target);
	NamespaceStatus status = PRIV->nspace->putDBEntry(PRIV->pmap->getSnapshotVersion()+1, entry);

	/* retry on version missmatch */
	if(status.versionMismatch())
		return pok_hardlink(target, origin);
	if(status.notOk())
		return -EIO;

	mdi_target->pbuf()->set_link_count(mdi_target->pbuf()->link_count());
	status = PRIV->nspace->putMD(mdi_target.get());
	if(status.notOk()){
		pok_error("Failed increasing metadata link_count after successfully creating link");
		return -EIO;
	}
	return 0;
}
