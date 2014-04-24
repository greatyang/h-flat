/* h-flat file system: Hierarchical Functionality in a Flat Namespace
 * Copyright (c) 2014 Seagate
 * Written by Paul Hermann Lensing
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "kinetic_namespace.h"
#include "main.h"
#include "debug.h"

using namespace util;
using kinetic::StatusCode;
using com::seagate::kinetic::client::proto::Message_Algorithm_SHA1;

static const string db_base_name = "pathmapDB_";
static const string db_version_key = db_base_name + "version";

int get_metadata(const std::shared_ptr<MetadataInfo> &mdi)
{
    hflat_debug("GET key %s",mdi->getSystemPath().c_str());
    unique_ptr<KineticRecord> record;
    KineticStatus status = PRIV->kinetic->Get(mdi->getSystemPath(), record);

    if(!status.ok()){
        if(status.statusCode() ==  StatusCode::REMOTE_NOT_FOUND)
            return -ENOENT;
        hflat_warning("status == %s",status.message().c_str());
        return -EIO;
    }

    hflat::Metadata md;
    if(! md.ParseFromString( *record->value()) )
        return -EINVAL;
    mdi->setMD(md, *record->version() );
    return 0;
}


int put_metadata(const std::shared_ptr<MetadataInfo> &mdi)
{
    hflat_debug("PUT key %s",mdi->getSystemPath().c_str());
    std::string new_version  = util::generate_uuid();

    KineticRecord record(mdi->getMD().SerializeAsString(), new_version, "", Message_Algorithm_SHA1);
    KineticStatus status = PRIV->kinetic->Put(mdi->getSystemPath(), mdi->getKeyVersion(), WriteMode::REQUIRE_SAME_VERSION, record);

    if(!status.ok()){
        PRIV->lookup_cache.invalidate(mdi->getSystemPath());
        if (status.statusCode() ==  StatusCode::REMOTE_VERSION_MISMATCH) return -EAGAIN;
        if (status.statusCode() ==  StatusCode::REMOTE_NOT_FOUND) return -ENOENT;
        hflat_warning("status == %s",status.message().c_str());
        return -EIO;
    }

    hflat_debug("PUT key %s",mdi->getSystemPath().c_str());
    mdi->setKeyVersion(new_version);
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
    hflat_debug("CREATE key %s",mdi->getSystemPath().c_str());
    std::string new_version = util::generate_uuid();

    KineticRecord record(mdi->getMD().SerializeAsString(),  new_version, "", Message_Algorithm_SHA1);
    KineticStatus status = PRIV->kinetic->Put(mdi->getSystemPath(), "", WriteMode::REQUIRE_SAME_VERSION, record);

    if (status.statusCode() ==  StatusCode::REMOTE_VERSION_MISMATCH)
        return -EEXIST;
    if (!status.ok()){
        hflat_warning("status == %s",status.message().c_str());
        return -EIO;
    }
    mdi->setKeyVersion(new_version);
    return 0;
}

int delete_metadata(const std::shared_ptr<MetadataInfo> &mdi)
{
    hflat_debug("DELETE key %s",mdi->getSystemPath().c_str());
    PRIV->lookup_cache.invalidate(mdi->getSystemPath());
    KineticStatus status = PRIV->kinetic->Delete(mdi->getSystemPath(), mdi->getKeyVersion(), WriteMode::REQUIRE_SAME_VERSION);

    if (status.statusCode() ==  StatusCode::REMOTE_VERSION_MISMATCH)
        return -EAGAIN;
    if (status.statusCode() ==  StatusCode::REMOTE_NOT_FOUND)
        return -ENOENT;
    if (!status.ok()){
        hflat_warning("status == %s",status.message().c_str());
        return -EIO;
    }
    return 0;
}

int get_data(const std::string &key, std::shared_ptr<DataInfo> &di)
{
    hflat_debug("GET DATA %s",key.c_str());
    unique_ptr<KineticRecord> record;
    KineticStatus status = PRIV->kinetic->Get(key, record);

    if (!status.ok() && status.statusCode() !=  StatusCode::REMOTE_NOT_FOUND){
        hflat_warning("status == %s",status.message().c_str());
        return -EIO;
    }

    if (status.statusCode() ==  StatusCode::REMOTE_NOT_FOUND)
        di.reset(new DataInfo(key, std::string(""), std::string("")));
    else
        di.reset(new DataInfo(key, *record->version(), *record->value()));
    return 0;
}

int put_data(const std::shared_ptr<DataInfo> &di)
{
    hflat_debug("PUT DATA %s",di->getKey().c_str());
    std::string new_version = util::generate_uuid();

    KineticRecord record(di->data(), new_version, "", Message_Algorithm_SHA1);
    KineticStatus status = PRIV->kinetic->Put(di->getKey(), di->getKeyVersion(), kinetic::REQUIRE_SAME_VERSION, record);

    /* If someone else has updated the data block since we read it in, just write the incremental changes */
    if (status.statusCode() ==  StatusCode::REMOTE_VERSION_MISMATCH) {
        unique_ptr<KineticRecord> record;
        status = PRIV->kinetic->Get(di->getKey(), record);

        if (status.ok()) {
            di->mergeDataChanges(*record->value());
            di->setKeyVersion(*record->version());
            return put_data(di);
        }
    }
    if (!status.ok()){
        hflat_warning("status == %s",status.message().c_str());
        return -EIO;
    }

    di->forgetUpdates();
    di->setKeyVersion(new_version);
    return 0;
}

int delete_data(const std::shared_ptr<DataInfo> &di)
{
    hflat_debug("DELETE DATA %s",di->getKey().c_str());
    KineticStatus status = PRIV->kinetic->Delete(di->getKey(), "", WriteMode::IGNORE_VERSION);
    PRIV->data_cache.invalidate(di->getKey());
    if (status.statusCode() == StatusCode::REMOTE_NOT_FOUND)
        return -ENOENT;
    if (!status.ok())
        return -EIO;
    return 0;
}

int put_db_entry(std::int64_t version, const hflat::db_entry &entry)
{
    string key = db_base_name + std::to_string(version);
    KineticRecord record(entry.SerializeAsString(), std::to_string(version), "", Message_Algorithm_SHA1);
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
        hflat_warning("Failed updating database version key.");
    return 0;
}

int get_db_entry(std::int64_t version, hflat::db_entry &entry)
{
    string key = db_base_name + std::to_string(version);
    unique_ptr<KineticRecord> record;
    KineticStatus status = PRIV->kinetic->Get(key, record);
    if (status.statusCode() ==  StatusCode::REMOTE_NOT_FOUND)
        return -ENOENT;
    if (!status.ok()){
        hflat_warning("encountered status '%s' when attempting to obtain %s. Returning -EIO.",
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
        hflat_warning("encountered status '%s' when calling getVersion on %s. Returning -EIO.",
                status.message().c_str(), db_version_key.c_str());
        return -EIO;
    }
    version = to_int64(keyVersion->data());
    return 0;
}


int put_db_snapshot (const hflat::db_snapshot &s)
{
    string key = db_base_name + "SNAPSHOT";
    KineticRecord record(s.SerializeAsString(), "", "", Message_Algorithm_SHA1);
    KineticStatus status = PRIV->kinetic->Put(key, "", kinetic::IGNORE_VERSION, record);

    if (!status.ok())
        return -EIO;

    hflat_trace("Stored serialized snapshot for database version %ld",s.snapshot_version());
    return 0;

}
int get_db_snapshot (hflat::db_snapshot &s)
{
    string key = db_base_name + "SNAPSHOT";
    unique_ptr<KineticRecord> record;
    KineticStatus status = PRIV->kinetic->Get(key, record);

    if (!status.ok())
        return -EIO;

    if(!s.ParseFromString(*record->value()))
        return -EINVAL;

    hflat_trace("Loaded serialized snapshot for database version %ld",s.snapshot_version());
    return 0;
}
