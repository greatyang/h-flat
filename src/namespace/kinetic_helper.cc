#include "kinetic_namespace.h"
#include "main.h"
#include "debug.h"

using namespace util;
using kinetic::StatusCode;
using com::seagate::kinetic::proto::Message_Algorithm_SHA1;

static const string db_base_name = "pathmapDB_";
static const string db_version_key = db_base_name + "version";

int get_metadata(const std::shared_ptr<MetadataInfo> &mdi)
{
    pok_debug("GET key %s",mdi->getSystemPath().c_str());
    unique_ptr<KineticRecord> record;
    KineticStatus status = PRIV->kinetic->Get(mdi->getSystemPath(), record);

    if(!status.ok()){
        if(status.statusCode() ==  StatusCode::REMOTE_NOT_FOUND) return -ENOENT;
        pok_warning("status == %s",status.message().c_str());
        return -EIO;
    }

    posixok::Metadata md;
    if(! md.ParseFromString( *record->value()) )
        return -EINVAL;
    VectorClock version( *record->version() );

    mdi->setMD(md, version);
    return 0;
}


int put_metadata(const std::shared_ptr<MetadataInfo> &mdi)
{
    VectorClock   version = mdi->getKeyVersion();

    KineticRecord record(mdi->getMD().SerializeAsString(), (++version).serialize(), "", Message_Algorithm_SHA1);
    KineticStatus status = PRIV->kinetic->Put(mdi->getSystemPath(), mdi->getKeyVersion().serialize(), WriteMode::REQUIRE_SAME_VERSION, record);

    if(!status.ok()){
        PRIV->lookup_cache.invalidate(mdi->getSystemPath());
        if (status.statusCode() ==  StatusCode::REMOTE_VERSION_MISMATCH) return -EAGAIN;
        if (status.statusCode() ==  StatusCode::REMOTE_NOT_FOUND) return -ENOENT;
        pok_warning("status == %s",status.message().c_str());
        return -EIO;
    }

    pok_debug("PUT key %s",mdi->getSystemPath().c_str());
    mdi->setKeyVersion(version);
    return 0;
}

int put_metadata_forced(const std::shared_ptr<MetadataInfo> &mdi, std::function<void()> md_update)
{
    md_update();

    int err = put_metadata(mdi);
    if(err != -EAGAIN)
        return err;
    err = get_metadata(mdi);
    if(err)
        return err;
    return put_metadata_forced(mdi, md_update);
}


int create_metadata(const std::shared_ptr<MetadataInfo> &mdi)
{
    pok_debug("CREATE key %s",mdi->getSystemPath().c_str());
    VectorClock version;

    KineticRecord record(mdi->getMD().SerializeAsString(),  (++version).serialize(), "", Message_Algorithm_SHA1);
    KineticStatus status = PRIV->kinetic->Put(mdi->getSystemPath(), "", WriteMode::REQUIRE_SAME_VERSION, record);

    if (status.statusCode() ==  StatusCode::REMOTE_VERSION_MISMATCH)
        return -EEXIST;
    if (!status.ok()){
        pok_warning("status == %s",status.message().c_str());
        return -EIO;
    }
    mdi->setKeyVersion(version);
    return 0;
}

int delete_metadata(const std::shared_ptr<MetadataInfo> &mdi)
{
    KineticStatus status = PRIV->kinetic->Delete(mdi->getSystemPath(), mdi->getKeyVersion().serialize(), WriteMode::REQUIRE_SAME_VERSION);
    PRIV->lookup_cache.invalidate(mdi->getSystemPath());

    if (status.statusCode() ==  StatusCode::REMOTE_VERSION_MISMATCH)
        return -EAGAIN;
    if (status.statusCode() ==  StatusCode::REMOTE_NOT_FOUND)
        return -ENOENT;
    if (!status.ok()){
        pok_warning("status == %s",status.message().c_str());
        return -EIO;
    }
    return 0;
}

int get_data(const std::string &key, std::shared_ptr<DataInfo> &di)
{
    pok_debug("GET DATA %s",key.c_str());
    unique_ptr<KineticRecord> record;
    KineticStatus status = PRIV->kinetic->Get(key, record);

    if (!status.ok() && status.statusCode() !=  StatusCode::REMOTE_NOT_FOUND){
        pok_warning("status == %s",status.message().c_str());
        return -EIO;
    }

    if (status.statusCode() ==  StatusCode::REMOTE_NOT_FOUND)
        di.reset(new DataInfo(key, VectorClock(), std::string("")));
    else
        di.reset(new DataInfo(key, VectorClock(*record->version()), *record->value()));
    return 0;
}

int put_data(const std::shared_ptr<DataInfo> &di)
{
    pok_debug("PUT DATA %s",di->getKey().c_str());
    VectorClock   version = di->getKeyVersion();
    KineticRecord record(di->data(), (++version).serialize(), "", Message_Algorithm_SHA1);
    KineticStatus status = PRIV->kinetic->Put(di->getKey(), di->getKeyVersion().serialize(), kinetic::REQUIRE_SAME_VERSION, record);

    /* If someone else has updated the data block since we read it in, just write the incremental changes */
    if (status.statusCode() ==  StatusCode::REMOTE_VERSION_MISMATCH) {
        unique_ptr<KineticRecord> record;
        status = PRIV->kinetic->Get(di->getKey(), record);

        if (status.ok()) {
            di->mergeDataChanges(*record->value());
            di->setKeyVersion(VectorClock(*record->version()));
            return put_data(di);
        }
    }
    if (!status.ok()){
        pok_warning("status == %s",status.message().c_str());
        return -EIO;
    }

    di->forgetUpdates();
    di->setKeyVersion(version);
    return 0;
}

int delete_data(const std::shared_ptr<DataInfo> &di)
{
    KineticStatus status = PRIV->kinetic->Delete(di->getKey(), "", WriteMode::IGNORE_VERSION);
    if (status.statusCode() == StatusCode::REMOTE_NOT_FOUND)
        return -ENOENT;
    if (!status.ok())
        return -EIO;
    return 0;
}

int put_db_entry(std::int64_t version, const posixok::db_entry &entry)
{
    string key = db_base_name + std::to_string(version);
    KineticRecord record(entry.SerializeAsString(), "", "", Message_Algorithm_SHA1);
    KineticStatus status = PRIV->kinetic->Put(key, "", kinetic::REQUIRE_SAME_VERSION, record);

    if (status.statusCode() ==  StatusCode::REMOTE_VERSION_MISMATCH)
        return -EEXIST;
    if (!status.ok())
        return -EIO;

    /* can be stored with IGNORE_VERSION since puts serialized by the db_version_key:
     * other clients will only see the database entry we just put after updating based on the db_version_key value */
    KineticRecord empty("", std::to_string(version), "", Message_Algorithm_SHA1);
    status = PRIV->kinetic->Put(db_version_key, "", WriteMode::IGNORE_VERSION, empty);
    if (!status.ok())
        pok_warning("Failed updating database version key.");
    return 0;
}

int get_db_entry(std::int64_t version, posixok::db_entry &entry)
{
    string key = db_base_name + std::to_string(version);
    unique_ptr<KineticRecord> record;
    KineticStatus status = PRIV->kinetic->Get(key, record);
    if (status.statusCode() ==  StatusCode::REMOTE_NOT_FOUND)
        return -ENOENT;
    if (!status.ok()){
        pok_warning("encountered status '%s' when attempting to obtain %s. Returning -EIO.",
                status.message().c_str(), key.c_str());
        return -EIO;
    }

    if (!entry.ParseFromString(* record->value()))
        return -EINVAL;
    return 0;
}

int get_db_version(std::int64_t &version)
{
    unique_ptr<string> keyVersion;
    KineticStatus status = PRIV->kinetic->GetVersion(db_version_key, keyVersion);
    if (status.statusCode() ==  StatusCode::REMOTE_NOT_FOUND){
        version = 0;
        return 0;
    }
    if (!status.ok()){
        pok_warning("encountered status '%s' when calling getVersion on %s. Returning -EIO.",
                status.message().c_str(), db_version_key.c_str());
        return -EIO;
    }
    version = to_int64(keyVersion->data());
    return 0;
}
