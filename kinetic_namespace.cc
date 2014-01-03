#include "debug.h"
#include "kinetic_namespace.h"

#include "kinetic/kinetic_connection_factory.h"
#include "kinetic/connection_options.h"
#include "kinetic/status.h"
#include "protobufutil/message_stream.h"

#include <exception>


KineticNamespace::KineticNamespace()
{
	kinetic::ConnectionOptions options;
	options.host = std::string("localhost");
	options.port = 8123;
	options.user_id = 1;
	options.hmac_key = "asdfasdf";

	palominolabs::protobufutil::MessageStreamFactory message_stream_factory(NULL, value_factory);
	kinetic::KineticConnectionFactory kinetic_connection_factory(hmac_provider,
			message_stream_factory);

	kinetic::KineticConnection *kineticConnection;
	kinetic::Status con = kinetic_connection_factory.NewConnection(options, &kineticConnection);
	if(!con.ok()){
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
std::string incr(const std::string &version)
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
