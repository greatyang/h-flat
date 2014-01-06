#include "main.h"
#include "debug.h"

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
		pok_warning("buffer too small to fit link destination.");
		strncpy(buffer, link_destination.c_str(), size);
		buffer[size-1] = '\0';
	}
	else
		strcpy(buffer, link_destination.c_str());
	return 0;
}

static int update_pathmapDB()
{
	std::int64_t dbVersion;
	NamespaceStatus status = PRIV->nspace->getDBVersion(dbVersion);
	if(status.notOk()){
		pok_warning("Cannot access database.");
		return -EINVAL;
	}
	std::int64_t snapshotVersion = PRIV->pmap->getSnapshotVersion();

	/* Sanity */
	assert(dbVersion >= snapshotVersion);

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

/** Create a symbolic link */
int pok_symlink (const char *link_destination, const char *user_path)
{
	/* 1) ensure that database is up-to-date, update if necessary */
	int err = update_pathmapDB();
	if(err)
		return err;

	/* 2) do a normal create (adds name to directory, creates metadata key) */
	struct fuse_file_info fi;
	fi.fh = fi.flags = fi.lock_owner = fi.direct_io = 0;
	err = pok_create(user_path, S_IFLNK | S_IRWXU | S_IRGRP | S_IXGRP | S_IXOTH, &fi);
	if(err)
		return err;
	pok_release(user_path, &fi);

	/* 3) add db_entry to permanent storage. */
	posixok::db_entry entry;
	entry.set_type(posixok::db_entry_TargetType_LINK);
	entry.set_origin(user_path);
	entry.set_target(link_destination);
	NamespaceStatus status = PRIV->nspace->putDBEntry(PRIV->pmap->getSnapshotVersion()+1, entry);

	/* If 3) failed, undo the previous create. */
	if(status.notOk()){
		pok_unlink(user_path);

		/* snapshot wasn't up-to-date after all. retry */
		if(status.versionMismatch())
			return pok_symlink(link_destination, user_path);

		pok_warning("Failed putting DBEntry, cannot create symlink: %s -> %s",user_path,link_destination);
		return -EINVAL;
	}

	/* 4) add to in-memory hashmap */
	PRIV->pmap->addSoftLink(user_path, link_destination);
	return 0;
}
