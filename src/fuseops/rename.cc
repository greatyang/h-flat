#include "main.h"
#include "debug.h"
#include "fuseops.h"

/** Rename a file or directory. */
int pok_rename (const char *user_path_from, const char *user_path_to)
{
	pok_trace("Rename '%s' to '%s'", user_path_from, user_path_to);

	/* Lookup metadata of supplied paths and their directories.
	 * Except user_path_to everything has to exist, user_path_to has
	 * to be deleted if it does exist.
	 */
	std::unique_ptr<MetadataInfo> dir_mdifrom(new MetadataInfo());
	int err = lookup_parent(user_path_from, dir_mdifrom);
	if (err)
		return err;

	std::unique_ptr<MetadataInfo> mdifrom(new MetadataInfo());
	err = lookup(user_path_from, mdifrom);
	if (err)
		return err;

	std::unique_ptr<MetadataInfo> dir_mdito(new MetadataInfo());
	err = lookup_parent(user_path_to, dir_mdito);
	if (err)
		return err;

	err = pok_unlink(user_path_to);
	if (err && err != -ENOENT)
		return err;


	std::int64_t req_snapshotVersion = 0;
	err = 0;

	/* Different handling of rename for directories / everything else */
	if(S_ISDIR(mdifrom->pbuf()->mode())){

		/* 1) ensure that database is up-to-date, update if necessary */
		int err = update_pathmapDB();
		if(err)
			return err;

		/* 2) add db_entry to permanent storage. */
		posixok::db_entry entry;
		entry.set_type(entry.MOVE);
		entry.set_origin(user_path_from);
		entry.set_target(user_path_to);
		NamespaceStatus status = PRIV->nspace->putDBEntry(PRIV->pmap->getSnapshotVersion()+1, entry);

		/* 3) Retry everything (including lookup) if somebody overwrote since our db-update. */
		if(status.versionMismatch())
				return pok_rename(user_path_from, user_path_to);
		if(status.notOk())
			return -EIO;

		PRIV->pmap->addDirectoryMove(user_path_from, user_path_to);
		req_snapshotVersion = PRIV->pmap->getSnapshotVersion();
	}
	else{
		/* Move the metadata-key: delete old location, create at new location.
		   TODO: Handle moving hardlinks */
		if(mdifrom->pbuf()->is_hardlink_target())
			return -ENOSYS;
		NamespaceStatus status = PRIV->nspace->deleteMD( mdifrom.get() );
		if(status.notOk())
			return -EIO;
		std::string keyname = dir_mdito->getSystemPath() + (dir_mdito->getSystemPath().size() == 1 ? "" : "/") + path_to_filename(user_path_to);
		mdifrom->setSystemPath(keyname);
		status = PRIV->nspace->putMD( mdifrom.get() );
		if(status.notOk())
			err = -EIO;
	}

	/* For both directory and file moves we need to update the parent directories */
	posixok::DirectoryEntry e;
	e.set_name(path_to_filename(user_path_from));
	e.set_type(e.SUB);
	e.set_req_version(req_snapshotVersion);
	if(!err) err = directory_addEntry(dir_mdifrom, e);

	e.set_name(path_to_filename(user_path_to));
	e.set_type(e.ADD);
	e.set_req_version(req_snapshotVersion);
	if(!err) err = directory_addEntry(dir_mdito, e);

	if(err)
		pok_error("Unrecoverable error in rename operation. File system might be corrupt.");
	return err;
}
