#include "main.h"
#include "debug.h"
#include "fuseops.h"
#include <sys/param.h>

static int move_directory_entry(const char *user_path_from, const char *user_path_to)
{

	/* Lookup metadata of supplied paths and their directories.
	 * Except user_path_to everything has to exist, user_path_to has
	 * to be deleted if it does exist.
	 */
	std::unique_ptr<MetadataInfo> dir_mdifrom(new MetadataInfo());
	int err = lookup_parent(user_path_from, dir_mdifrom);
	if (err)
		return err;

	std::unique_ptr<MetadataInfo> dir_mdito(new MetadataInfo());
	err = lookup_parent(user_path_to, dir_mdito);
	if (err)
		return err;

	std::int64_t req_snapshotVersion = PRIV->pmap->getSnapshotVersion()+1;

	/* remove old directory entry */
	posixok::DirectoryEntry e;
	e.set_name(path_to_filename(user_path_from));
	e.set_type(e.SUB);
	e.set_req_version(req_snapshotVersion);
	err = directory_addEntry(dir_mdifrom, e);
	if (err)
		return err;

	/* add new directory entry */
	e.set_name(path_to_filename(user_path_to));
	e.set_type(e.ADD);
	e.set_req_version(req_snapshotVersion);
	err = directory_addEntry(dir_mdito, e);

	return err;
}


/** Rename a file or directory. */
int pok_rename (const char *user_path_from, const char *user_path_to)
{
	pok_trace("Rename '%s' to '%s'", user_path_from, user_path_to);

	/* Lookup to-be-moved metadata in order to decide what to do. */
	std::unique_ptr<MetadataInfo> mdi_from(new MetadataInfo());
	int err = lookup(user_path_from, mdi_from);
	if (err)
		return err;

	/* unlink target name if it exists */
	int unlinked = err = pok_unlink(user_path_to);
	if (unlinked && unlinked != -ENOENT)
		return unlinked;

	/* Moving a directory. */
	if(S_ISDIR(mdi_from->pbuf()->mode())){
		posixok::db_entry entry;
		entry.set_type(entry.MOVE);
		entry.set_origin(user_path_from);
		entry.set_target(user_path_to);

		err = database_operation(
				std::bind(move_directory_entry, user_path_from, user_path_to),
				std::bind(move_directory_entry, user_path_to, user_path_from),
				entry);
		if(!err || unlinked != -ENOENT)
			return err;
		kill_compound_fail(); // we have unlinked something but failed with the move operation
	}

	/* For non-directorys there's 2 steps:
	 *   a) create new softlink / hardlink / regular file
	 *   b) unlink old name */
	if(S_ISLNK(mdi_from->pbuf()->mode())) {
		char buffer[PATH_MAX];
		err = pok_readlink(user_path_from, buffer, PATH_MAX);
		if(!err) err = pok_symlink (buffer, user_path_to);
	}
	else if(mdi_from->pbuf()->is_hardlink_target())	{
		err = pok_hardlink (mdi_from->getSystemPath().c_str(), user_path_to);
	}
	else {
		err = create_from_mdi(user_path_to, mdi_from->pbuf()->mode(), mdi_from);
	}

	if(err && unlinked == -ENOENT)
		kill_compound_fail(); // we have unlinked something but failed with the move operation

	/* unlink old name link */
	err = pok_unlink  (user_path_from);
	if (err)
		kill_compound_fail(); // we have created something but failed with the move operation

	return 0;
}
