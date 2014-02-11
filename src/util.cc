#include "main.h"
#include "debug.h"
#include <stdint.h>
#include "kinetic_helper.h"
#include "database.pb.h"


namespace util
{

std::int64_t to_int64(const std::string &version_string)
{
    std::uint64_t version;
    try {
        version = std::stoll(version_string);
    } catch (std::exception& e) {
        pok_warning("Illegal version string '%s'. Setting version to 0.", version_string.c_str());
        version = 0;
    }
    return version;
}

std::int64_t to_int64(const std::shared_ptr<const std::string> version_string)
{
    return to_int64(version_string->data());
}


int grab_inode_generation_token(void)
{
    const std::string inode_base_key = "inode_generation";

    std::unique_ptr<std::string> keyVersion;
    KineticStatus status = PRIV->kinetic->GetVersion(inode_base_key, keyVersion);
    if (status.ok())
        PRIV->inum_base = util::to_int64(keyVersion->data());
    else if (status.statusCode() == kinetic::StatusCode::REMOTE_NOT_FOUND)
        PRIV->inum_base = 0;
    else
        return -EIO;

    KineticRecord empty("", std::to_string(PRIV->inum_base + UINT16_MAX), "", com::seagate::kinetic::proto::Message_Algorithm_SHA1);
    status = PRIV->kinetic->Put(inode_base_key, PRIV->inum_base ? std::to_string(PRIV->inum_base) : "", WriteMode::REQUIRE_SAME_VERSION, empty);
    if (status.statusCode() ==  kinetic::StatusCode::REMOTE_VERSION_MISMATCH)
        return grab_inode_generation_token();
    if (!status.ok())
        return -EIO;
    return 0;
}

ino_t generate_inode_number(void)
{
    std::lock_guard<std::mutex> locker(PRIV->lock);
    PRIV->inum_counter++;
    if (PRIV->inum_counter == UINT16_MAX){
       if( grab_inode_generation_token() )
           pok_error(" Error encountered when attempting to refresh inode generation token. "
                     " Cannot generate inode numbers. Quitting. ");
       PRIV->inum_counter = 0;
    }
    return (ino_t) (PRIV->inum_base +  PRIV->inum_counter);
}

std::string path_to_filename(const std::string &path)
{
    return path.substr(path.find_last_of("/:") + 1);
}


int database_update(void)
{
    std::int64_t dbVersion;
    std::int64_t snapshotVersion = PRIV->pmap.getSnapshotVersion();

    if (int err = get_db_version(dbVersion))
        return err;
    assert(dbVersion >= snapshotVersion);

    if (dbVersion == snapshotVersion)
        return -EALREADY;

    /* Update */
    std::list<posixok::db_entry> entries;
    posixok::db_entry entry;
    for (std::int64_t v = snapshotVersion + 1; v <= dbVersion; v++) {
        if (int err = get_db_entry(v, entry))
            return err;
        entries.push_back(entry);
    }
    return PRIV->pmap.updateSnapshot(entries, snapshotVersion, dbVersion);
}

/* If put_db_entry fails due to an out-of-date snapshot, the file system operation needs to be re-verified using the supplied function with
 * the updated snaphshot before put_db_entry can be retried. */
int database_operation(std::function<int()> verify, posixok::db_entry &entry)
{
    std::int64_t snapshotVersion = PRIV->pmap.getSnapshotVersion();

    int err = put_db_entry(snapshotVersion + 1, entry);
    if(!err){
        std::list<posixok::db_entry> entries;
        entries.push_back(entry);
        PRIV->pmap.updateSnapshot(entries, snapshotVersion, snapshotVersion + 1);
        return 0;
    }
    if (  err != -EEXIST) return err;
    if (( err = database_update() )) return err;
    if (( err = verify()          )) return err;
    return database_operation(verify, entry);
}


}
