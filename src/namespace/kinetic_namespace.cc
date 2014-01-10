#include "kinetic_namespace.h"
#include "debug.h"
#include "kinetic/kinetic_connection_factory.h"

#include <exception>
#include <chrono>
#include <thread>


using palominolabs::protobufutil::MessageStreamFactory;

KineticNamespace::KineticNamespace()
{
	kinetic::ConnectionOptions con_options;
	con_options.host = "localhost";
	con_options.port = 8123;
	con_options.user_id  = 1;
	con_options.hmac_key = "asdfasdf";

	kinetic::KineticConnectionFactory factory = kinetic::NewKineticConnectionFactory();

	kinetic::ConnectionHandle *kineticConnection = nullptr;
	kinetic::Status status = factory.NewConnection(con_options, &kineticConnection);
	if(!status.ok() || !kineticConnection){
		pok_debug("Unable to connect, Error: %s",status.ToString().c_str());
		throw std::runtime_error(status.ToString());
	}
	con = std::unique_ptr<kinetic::ConnectionHandle>(kineticConnection);
}

KineticNamespace::~KineticNamespace()
{
}


NamespaceStatus KineticNamespace::getMD(MetadataInfo *mdi)
{
	kinetic::KineticRecord *record = nullptr;
	kinetic::KineticStatus  status = con->blocking().Get(mdi->getSystemPath(), &record);

	if(status.ok()){
		if(!mdi->pbuf()->ParseFromString(record->value()))
			status = NamespaceStatus::makeInvalid();
		else
			mdi->setCurrentVersion(record->version());
	}
	if(record)
		delete record;
	return status;
}

NamespaceStatus KineticNamespace::KineticNamespace::putMD(MetadataInfo *mdi)
{
	kinetic::KineticRecord record(mdi->pbuf()->SerializeAsString(), "", "", com::seagate::kinetic::proto::Message_Algorithm_SHA1);
	kinetic::KineticStatus status = con->blocking().Put(mdi->getSystemPath(), "", true, record);
	return status;
}


NamespaceStatus KineticNamespace::deleteMD( MetadataInfo *mdi )
{
	return con->blocking().Delete(mdi->getSystemPath(),"",true);
}


NamespaceStatus KineticNamespace::get( MetadataInfo *mdi, unsigned int blocknumber, std::string &value)
{
	std::string key = mdi->pbuf()->data_unique_id() + std::to_string(blocknumber);
	kinetic::KineticRecord *record = nullptr;
	kinetic::KineticStatus  status = con->blocking().Get(key, &record);

	if(status.ok()){
		mdi->trackDataVersion(blocknumber, record->version());
		value = record->value();
	}
	return status;
}


/* Convert passed version string to integer, increment, and pass it back as a string */
static std::string incr(const std::string &version)
{
	if(version.empty())
		return std::to_string(1);

	std::uint64_t iversion;
	try{
		iversion = std::stoll(version);
	}
	catch(std::exception& e){
		pok_warning("Exception thrown during conversion of string '%s' to int, resetting key-version to 0. Exception Reason: %s \n .",version.c_str(), e.what());
		iversion = 0;
	}
	iversion++;
	return std::to_string(iversion);
}


NamespaceStatus KineticNamespace::put( MetadataInfo *mdi, unsigned int blocknumber, const std::string &value, const PutModeType type)
{
	std::string key = mdi->pbuf()->data_unique_id() + std::to_string(blocknumber);
	std::string curVersion = "";
	std::string newVersion = "";
	if(type == PutModeType::ATOMIC){
		curVersion = mdi->getDataVersion(blocknumber);
		newVersion = incr(curVersion);
	}

	kinetic::KineticRecord record(value, newVersion, "", com::seagate::kinetic::proto::Message_Algorithm_SHA1);
	kinetic::KineticStatus status = con->blocking().Put(key, curVersion, type == PutModeType::POSIX ? true : false, record);

	if((type == PutModeType::ATOMIC) && status.ok())
		mdi->trackDataVersion(blocknumber, newVersion);
	return status;
}


NamespaceStatus KineticNamespace::free(MetadataInfo *mdi, unsigned int blocknumber)
{
	std::string key = mdi->pbuf()->data_unique_id() + std::to_string(blocknumber);
	kinetic::KineticStatus status = con->blocking().Delete(key,"",true);
	return status;
}

void KineticNamespace::fixDBVersionMissmatch( std::int64_t version )
{
	/* Get the value and see what's what ... if it still hasn't been updated, assume the other client died. If it has been
	 * updated beyond the current version (this client might have lost network connection for a while), accept the situation.
	 */
	std::int64_t stored_version;
	kinetic::KineticStatus status = getDBVersion(stored_version);
	if(status.notOk()){
		pok_warning("Failed to set 'pathmapDB_version' key after successfully putting pathmdb entry.");
		return;
	}

	if(stored_version > version)
		return;

	kinetic::KineticRecord vrecord("", std::to_string(version), "", com::seagate::kinetic::proto::Message_Algorithm_SHA1);
	status = con->blocking().Put(db_versionname, std::to_string(stored_version), false , vrecord);

	if(status.versionMismatch()){
		pok_debug("Encountered version missmatch in fixDBVersionMissmatch. Retrying due to possible race condition. ");
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		return fixDBVersionMissmatch(version);
	}

	if(status.notOk())
		pok_warning("Failed to set 'pathmapDB_version' key after successfully putting pathmdb entry.");
}

NamespaceStatus KineticNamespace::putDBEntry( std::int64_t version, const posixok::db_entry &entry )
{
	std::string key = db_basename + std::to_string(version);
	kinetic::KineticRecord record(entry.SerializeAsString(), "", "", com::seagate::kinetic::proto::Message_Algorithm_SHA1);
	kinetic::KineticStatus status = con->blocking().Put(key, "", false, record);

	if(status.notOk())
		return status;

	/* At this point the update has been successfully completed. We should update the pathmapDB_version record to reflect this change.  */
	kinetic::KineticRecord vrecord("", std::to_string(version), "", com::seagate::kinetic::proto::Message_Algorithm_SHA1);
	status = con->blocking().Put(db_versionname, version > 1 ? std::to_string(version-1) : "", false, vrecord);

	/* A version missmatch can occur if a previous putDBEntry call (possibly from another client) has not updated the pathmapDB_version key.
	 * This could be due to
	 * 	a) a simple race condition
	 * 	b) an error that occurred after the previous entry was put (e.g. loss of network connection) */
	if(status.versionMismatch())
		fixDBVersionMissmatch( version );

	return NamespaceStatus::makeOk();
}

NamespaceStatus KineticNamespace::getDBEntry( std::int64_t version, posixok::db_entry &entry)
{
	std::string key = db_basename + std::to_string(version);
	kinetic::KineticRecord *record = nullptr;
	kinetic::KineticStatus  status = con->blocking().Get(key, &record);

	if(status.ok())
		if(!entry.ParseFromString(record->value()))
			status = NamespaceStatus::makeInvalid();

	if(record) delete record;
	return status;
}

NamespaceStatus KineticNamespace::getDBVersion (std::int64_t &version)
{
	std::string keyVersion;
	kinetic::KineticStatus status = con->blocking().GetVersion(db_versionname, &keyVersion);

	if(status.notFound()){
		version = 0;
		return NamespaceStatus::makeOk();
	}
	if(status.notOk())
		return status;

	try{
		version = std::stoll(keyVersion);
	}
	catch(std::exception& e){
		pok_warning("Exception thrown during conversion of string '%s' to int. Exception Reason: %s \n .",keyVersion.c_str(), e.what());
		return NamespaceStatus::makeInternalError(e.what());
	}
	return status;
}
