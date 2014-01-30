#include "kinetic_namespace.h"
#include "main.h"
#include "debug.h"

using namespace util;

static const string db_base_name = "pathmapDB_";
static const string db_version_key = db_base_name + "version";

int get_metadata(MetadataInfo *mdi)
{
    unique_ptr<KineticRecord> record;
    KineticStatus status = PRIV->kinetic->Get(mdi->getSystemPath(), record);

    if (status.notFound())
        return -ENOENT;
    if (status.notOk()){
        pok_warning("EIO");
        return -EIO;
    }

    posixok::Metadata md;
    if(! md.ParseFromString(* record->value()) )
        return -EINVAL;

    mdi->setMD(md, to_int64(record->version()));
    return 0;
}

int put_metadata(MetadataInfo *mdi)
{
    int64_t version = mdi->getCurrentVersion();
    assert(version);

    KineticRecord record(mdi->getMD().SerializeAsString(), std::to_string(version + 1), "", com::seagate::kinetic::proto::Message_Algorithm_SHA1);
    KineticStatus status = PRIV->kinetic->Put(mdi->getSystemPath(), std::to_string(version), WriteMode::REQUIRE_SAME_VERSION, record);

    /* If someone else has updated the metadata key since it has been read in, try to merge the changes. */
    if (status.versionMismatch()) {
       unique_ptr<KineticRecord> record;
       KineticStatus status = PRIV->kinetic->Get(mdi->getSystemPath(), record);

       if(status.ok()){
           posixok::Metadata md;
           if(!md.ParseFromString(* record->value()))
               return -EINVAL;

           if(mdi->mergeMD(md, to_int64(record->version())))
               return put_metadata(mdi);

           mdi->setMD(md, to_int64(record->version()));
           return -EAGAIN;
       }
    }
    if (status.notOk()){
        pok_warning("EIO");
        return -EIO;
    }

    mdi->setCurrentVersion(version + 1);
    return 0;
}

int create_metadata(MetadataInfo *mdi)
{
    std::int64_t version = mdi->getCurrentVersion();

    KineticRecord record(mdi->getMD().SerializeAsString(), std::to_string(version + 1), "", com::seagate::kinetic::proto::Message_Algorithm_SHA1);
    KineticStatus status = PRIV->kinetic->Put(mdi->getSystemPath(), "", WriteMode::REQUIRE_SAME_VERSION, record);

    if (status.versionMismatch())
        return -EEXIST;
    if (status.notOk()){
        pok_warning("EIO");
        return -EIO;
    }

    mdi->setCurrentVersion(version + 1);
    return 0;
}

int delete_metadata(MetadataInfo *mdi)
{
    KineticStatus status = PRIV->kinetic->Delete(mdi->getSystemPath(), std::to_string(mdi->getCurrentVersion()), WriteMode::REQUIRE_SAME_VERSION);

    if (status.versionMismatch())
        return -EINVAL;
    if (status.notFound())
        return -ENOENT;
    if (status.notOk()){
        pok_warning("EIO");
        return -EIO;
    }
    return 0;
}

int get_data(MetadataInfo *mdi, unsigned int blocknumber)
{
    string key = std::to_string(mdi->getMD().inode_number()) + "_" + std::to_string(blocknumber);
    unique_ptr<KineticRecord> record;
    KineticStatus status = PRIV->kinetic->Get(key, record);

    if (status.notOk() && !status.notFound()){
        pok_warning("EIO");
        return -EIO;
    }

    if (status.notFound())
        mdi->setDataInfo(blocknumber, DataInfo("",0));
    else
        mdi->setDataInfo(blocknumber, DataInfo(* record->value(), to_int64(record->version())));
    return 0;
}

int put_data(MetadataInfo *mdi, unsigned int blocknumber)
{
    DataInfo *di = mdi->getDataInfo(blocknumber);
    if(!di) return -EINVAL;
    if(!di->hasUpdates()) return 0;

    string key = std::to_string(mdi->getMD().inode_number()) + "_" + std::to_string(blocknumber);
    std::int64_t version = di->getCurrentVersion();

    KineticRecord record(di->data(), std::to_string(version + 1), "", com::seagate::kinetic::proto::Message_Algorithm_SHA1);
    KineticStatus status = PRIV->kinetic->Put(key, version?std::to_string(version):"", WriteMode::REQUIRE_SAME_VERSION, record);

    /* If someone else has updated the data block since we read it in, just write the incremental changes */
    if (status.versionMismatch()) {
        unique_ptr<KineticRecord> record;
        status = PRIV->kinetic->Get(key, record);

        if (status.ok()) {
            di->mergeDataChanges(* record->value());
            di->setCurrentVersion(to_int64(record->version()));
            return put_data(mdi, blocknumber);
        }
    }
    if (status.notOk()){
        pok_warning("EIO");
        return -EIO;
    }

    di->forgetUpdates();
    di->setCurrentVersion(version + 1);
    return 0;
}

int delete_data(MetadataInfo *mdi, unsigned int blocknumber)
{
    string key = std::to_string(mdi->getMD().inode_number()) + "_" + std::to_string(blocknumber);
    KineticStatus status = PRIV->kinetic->Delete(key, "", WriteMode::IGNORE_VERSION);

    if (status.notFound())
        return -ENOENT;
    if (status.notOk()){
        pok_warning("EIO");
        return -EIO;
    }
    return 0;
}

int put_db_entry(std::int64_t version, const posixok::db_entry &entry)
{
    string key = db_base_name + std::to_string(version);
    KineticRecord record(entry.SerializeAsString(), "", "", com::seagate::kinetic::proto::Message_Algorithm_SHA1);
    KineticStatus status = PRIV->kinetic->Put(key, "", kinetic::REQUIRE_SAME_VERSION, record);

    if (status.versionMismatch())
        return -EEXIST;
    if (status.notOk()){
        pok_warning("EIO");
        return -EIO;
    }

    /* can be stored with IGNORE_VERSION since puts serialized by the db_version_key:
     * other clients will only see the database entry we just put after updating based on the db_version_key value */
    KineticRecord empty("", std::to_string(version), "", com::seagate::kinetic::proto::Message_Algorithm_SHA1);
    status = PRIV->kinetic->Put(db_version_key, "", WriteMode::IGNORE_VERSION, empty);
    if (status.notOk())
        pok_warning("Failed updating database version key.");
    return 0;
}

int get_db_entry(std::int64_t version, posixok::db_entry &entry)
{
    string key = db_base_name + std::to_string(version);
    unique_ptr<KineticRecord> record;
    KineticStatus status = PRIV->kinetic->Get(key, record);
    if (status.notFound())
        return -ENOENT;
    if (status.notOk()){
        pok_warning("encountered status '%s' when attempting to obtain %s. Returning -EIO.",
                status.ToString().c_str(), key.c_str());
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
    if (status.notOk()){
        pok_warning("encountered status '%s' when calling getVersion on %s. Returning -EIO.",
                status.ToString().c_str(), db_version_key.c_str());
        return -EIO;
    }
    version = to_int64(keyVersion->data());
    return 0;
}
