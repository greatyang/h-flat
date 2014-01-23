#include "main.h"
#include "debug.h"
#include "fuseops.h"
#include "kinetic_helper.h"
#include "database.pb.h"

int database_update(void)
{
    std::int64_t dbVersion;
    std::int64_t snapshotVersion = PRIV->pmap->getSnapshotVersion();

    if (int err = get_db_version(dbVersion))
        return err;

    /* Sanity */
    assert(dbVersion >= snapshotVersion);

    /* Nothing to do. */
    if (dbVersion == snapshotVersion)
        return 0;

    /* Update */
    std::list<posixok::db_entry> entries;
    posixok::db_entry entry;
    for (std::int64_t v = snapshotVersion + 1; v <= dbVersion; v++) {
        if (int err = get_db_entry(v, entry))
            return err;
        entries.push_back(entry);
    }
    return PRIV->pmap->updateSnapshot(entries, snapshotVersion, dbVersion);
}

/* If put_db_entry fails due to an out-of-date snapshot, the file system operation needs to be re-verified using the supplied function with
 * the updated snaphshot before put_db_entry can be retried. */
int database_op(std::function<int()> verify, posixok::db_entry &entry)
{
    std::int64_t snapshotVersion = PRIV->pmap->getSnapshotVersion();

    int err = put_db_entry(snapshotVersion + 1, entry);
    if(!err){
        std::list<posixok::db_entry> entries;
        entries.push_back(entry);
        PRIV->pmap->updateSnapshot(entries, snapshotVersion, snapshotVersion + 1);
        return 0;
    }
    if (err != -EEXIST)
        return err;

    err = database_update();
    if(err) return err;

    if (PRIV->pmap->getSnapshotVersion() <= snapshotVersion)
        return -EINVAL;

    err = verify();
    if(err) return err;

    return database_op(verify, entry);
}

int database_operation(std::function<int()> fsfun_do, std::function<int()> fsfun_undo, posixok::db_entry &entry)
{
    /* If we expect other clients to modify the database, update it before starting the operation. */
    if (PRIV->multiclient) {
        int err = database_update();
        if (err)
            return err;
    }

    /* Execute supplied file system function & store which snapshotVersion is current.*/
    std::int64_t snapshotVersion = PRIV->pmap->getSnapshotVersion();
    int err = fsfun_do();
    if (err)
        return err;

    /* Add supplied database entry to on-drive database. */
    err = put_db_entry(snapshotVersion + 1, entry);
    if (!err) {
        std::list<posixok::db_entry> entries;
        entries.push_back(entry);
        PRIV->pmap->updateSnapshot(entries, snapshotVersion, snapshotVersion + 1);
        return 0;
    }

    pok_debug("Putting DB entry #%d failed, undoing fs operation.",snapshotVersion+1);

    /* Adding database entry failed, undo file system function. */
    if (fsfun_undo()) {
        pok_error("Unrecoverable File System Error. \n "
                "Failed undoing a file system operation after a database update failure. \n"
                "Killing myself now. Goodbye.");
        pok_destroy(PRIV);
        exit(EXIT_FAILURE);
    }

    /* Try to update the local database snapshot. Retry if snapshot version changed, otherwise
     * there appears to be a problem with the database, we can't execute the desired operation. */
    database_update();
    if (PRIV->pmap->getSnapshotVersion() > snapshotVersion)
        database_operation(fsfun_do, fsfun_undo, entry);

    pok_warning("EIO");
    return -EIO;
}
