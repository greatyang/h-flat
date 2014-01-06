#include "debug.h"
#include "kinetic_namespace.h"

#include "kinetic/kinetic_connection_factory.h"
#include "kinetic/connection_options.h"
#include "kinetic/status.h"

#include "protobufutil/message_stream.h"

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

    palominolabs::protobufutil::MessageStreamFactory * message_stream_factory = new palominolabs::protobufutil::MessageStreamFactory(NULL, value_factory);
    con_factory = std::unique_ptr<kinetic::KineticConnectionFactory>(new kinetic::KineticConnectionFactory(hmac_provider, message_stream_factory));

	kinetic::KineticConnection *kineticConnection = nullptr;
	kinetic::Status con = con_factory->NewConnection(con_options, &kineticConnection);
	if(!con.ok() || !kineticConnection){
		pok_debug("Unable to connect, Error: %s",con.ToString().c_str());
		throw std::runtime_error(con.ToString());
	}
	connection = std::unique_ptr<kinetic::KineticConnection>(kineticConnection);
}

KineticNamespace::~KineticNamespace()
{
}


NamespaceStatus KineticNamespace::getMD( MetadataInfo *mdi)
{
	std::string value, version, tag;
	kinetic::KineticStatus status = connection->Get(mdi->getSystemPath(), &value, &version, &tag);

	if(status.ok())
		if(!mdi->pbuf()->ParseFromString(value))
			return NamespaceStatus::makeInvalid();

	mdi->setCurrentVersion(version);
	return status;
}

NamespaceStatus KineticNamespace::KineticNamespace::putMD(MetadataInfo *mdi)
{
	kinetic::KineticRecord record(mdi->pbuf()->SerializeAsString(), mdi->getCurrentVersion(), "", com::seagate::kinetic::proto::Message_Algorithm_SHA1);
	kinetic::KineticStatus status = connection->Put(mdi->getSystemPath(), mdi->getCurrentVersion(), record);

	return status;
}


NamespaceStatus KineticNamespace::deleteMD( MetadataInfo *mdi )
{
	return NamespaceStatus::makeInternalError("No blocking delete implemented in Kinetic-C-Client at the moment.");
}


NamespaceStatus KineticNamespace::get( MetadataInfo *mdi, unsigned int blocknumber, std::string *value)
{
	std::string key, version, tag;
	key = mdi->pbuf()->data_unique_id() + std::to_string(blocknumber);

	kinetic::KineticStatus status = connection->Get(key, value, &version, &tag);
	return status;
}

NamespaceStatus KineticNamespace::put( MetadataInfo *mdi, unsigned int blocknumber, const std::string &value)
{
	std::string key = mdi->pbuf()->data_unique_id() + std::to_string(blocknumber);

	kinetic::KineticRecord record(value, mdi->getCurrentVersion(), "", com::seagate::kinetic::proto::Message_Algorithm_SHA1);
	kinetic::KineticStatus status = connection->Put(key, mdi->getCurrentVersion(), record);
	return status;
}

/* Convert passed version string to integer, increment, and pass it back as a string */
static std::string incr(const std::string &version)
{
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

NamespaceStatus KineticNamespace::append( MetadataInfo *mdi, const std::string &value)
{
	std::string  data;
	const 	 int blocksize	= 1024 * 1024;
	unsigned int blocknumber = ( mdi->pbuf()->size() + value.length() ) / blocksize;

	std::string key, version, tag;
	key = mdi->pbuf()->data_unique_id() + std::to_string(blocknumber);

	/* get block data */
	kinetic::KineticStatus status = connection->Get(key, &data, &version, &tag);
	if(status.notAuthorized()){
		pok_warning("No authorization to read key.");
		return status;
	}

	/* update block data */
	data.append(value);

	/* put block data with changed version number to get atomic behavior. Retry as necessary */
	kinetic::KineticRecord record(data, incr(version), "", com::seagate::kinetic::proto::Message_Algorithm_SHA1);
	status = connection->Put(key, version, record);
	if(status.versionMismatch())
		return append(mdi,value);
	return status;
}


NamespaceStatus KineticNamespace::free( MetadataInfo *mdi, unsigned int blocknumber)
{
	return NamespaceStatus::makeInternalError("No blocking delete implemented in Kinetic-C-Client at the moment.");
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

	kinetic::KineticRecord record(std::to_string(version), std::to_string(version), "", com::seagate::kinetic::proto::Message_Algorithm_SHA1);
	status = connection->Put("pathmaDB_version", std::to_string(stored_version), record);

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
	std::string key = "pathmapDB_" + std::to_string(version);
	kinetic::KineticRecord record(entry.SerializeAsString(), "", "", com::seagate::kinetic::proto::Message_Algorithm_SHA1);
	kinetic::KineticStatus status = connection->Put(key, "", record);

	if(status.notOk())
		return status;

	/* At this point the update has been successfully completed. We should update the pathmapDB_version record to reflect this change.  */
	kinetic::KineticRecord vrecord(std::to_string(version), std::to_string(version), "", com::seagate::kinetic::proto::Message_Algorithm_SHA1);
	status = connection->Put("pathmaDB_version", version > 1 ? std::to_string(version-1) : "", vrecord);

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
	std::string key = "pathmapDB_" + std::to_string(version);
	std::string value, keyversion, tag;
	kinetic::KineticStatus status = connection->Get(key, &value, &keyversion, &tag);

	if(status.ok())
		if(!entry.ParseFromString(value))
			return NamespaceStatus::makeInvalid();
	return status;
}


NamespaceStatus KineticNamespace::getDBVersion (std::int64_t &version)
{
	std::string value, keyversion, tag;
	kinetic::KineticStatus status = connection->Get("pathmapDB_version", &value, &keyversion, &tag);

	if(status.notFound()){
		version = 0;
		return NamespaceStatus::makeOk();
	}
	if(status.notOk())
		return status;

	try{
		version = std::stoll(value);
	}
	catch(std::exception& e){
		pok_warning("Exception thrown during conversion of string '%s' to int, resetting key-version to 0. Exception Reason: %s \n .",value.c_str(), e.what());
		return NamespaceStatus::makeInternalError(e.what());
	}
	return status;
}
