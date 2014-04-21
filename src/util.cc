#include "main.h"
#include "debug.h"
#include "kinetic_helper.h"
#include "database.pb.h"
#include <stdint.h>
#include <uuid/uuid.h>
#include <future>

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

std::string generate_uuid()
{
    uuid_t uuid;
    uuid_generate(uuid);
    return std::string(reinterpret_cast<const char *>(uuid), sizeof(uuid_t));
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
    if (PRIV->inum_counter == std::numeric_limits<std::uint16_t>::max()){
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

/* Update the local database snapshot from remotely stored db_entries. Returns -EALREADY if
 * the local snapshot is already at the newest version. */
int database_update(void)
{
    std::int64_t dbVersion;
    std::int64_t snapshotVersion = PRIV->pmap.getSnapshotVersion();
    posixok::db_entry entry;

    if (int err = get_db_version(dbVersion))
        return err;
    assert(dbVersion >= snapshotVersion);

    if (dbVersion == snapshotVersion){
        /* dbVersion could be outdated... make sure before failing. */
        if (get_db_entry(dbVersion+1, entry))
           return -EALREADY;
        dbVersion+=1;
    }

    /* See if it makes sense to get a full snapshot instead of an incremental update. */
    if(dbVersion - snapshotVersion > 42){
        posixok::db_snapshot s;
        if(get_db_snapshot(s) == 0){
            PRIV->pmap.loadSnapshot(s);
            snapshotVersion = PRIV->pmap.getSnapshotVersion();
        }
    }

    /* Update using single db_entries. */
    std::list<posixok::db_entry> entries;
    for (std::int64_t v = snapshotVersion + 1; v <= dbVersion; v++) {
        if (int err = get_db_entry(v, entry))
            return err;
        entries.push_back(entry);
    }
    return PRIV->pmap.updateSnapshot(entries, snapshotVersion, dbVersion);
}

int database_operation(posixok::db_entry &entry)
{
    std::int64_t snapshotVersion = PRIV->pmap.getSnapshotVersion();

    int err = put_db_entry(snapshotVersion + 1, entry);
    if(!err){
        std::list<posixok::db_entry> entries;
        entries.push_back(entry);
        PRIV->pmap.updateSnapshot(entries, snapshotVersion, snapshotVersion + 1);
        if(snapshotVersion % 42 == 0){
            posixok::db_snapshot s = PRIV->pmap.serializeSnapshot();
            put_db_snapshot(s);
        }
        return 0;
    }
    if (  err != -EEXIST){
        pok_error("put_db_entry returned error code %d",err);
        return err;
    }
    if (( err = database_update() ))
        return err;
    return database_operation(entry);
}

}

