#include "kinetic_namespace.h"
#include <exception>


SimpleKineticNamespace::SimpleKineticNamespace(kinetic::ConnectionOptions options)
{
	connect(options);
}

SimpleKineticNamespace::SimpleKineticNamespace()
{
	kinetic::ConnectionOptions options;
	options.host = "localhost";
	options.port = 8123;
	options.user_id  = 1;
	options.hmac_key = "asdfasdf";

	connect(options);
}

SimpleKineticNamespace::~SimpleKineticNamespace()
{
}

void SimpleKineticNamespace::connect(kinetic::ConnectionOptions options)
{
	kinetic::KineticConnectionFactory factory = kinetic::NewKineticConnectionFactory();

	kinetic::ConnectionHandle *kineticConnection = nullptr;
	kinetic::Status status = factory.NewConnection(options, &kineticConnection);
	if(!status.ok() || !kineticConnection){
		throw std::runtime_error(status.ToString());
	}
	con = std::unique_ptr<kinetic::ConnectionHandle>(kineticConnection);
}

KineticStatus SimpleKineticNamespace::Get(const std::string &key, std::unique_ptr<KineticRecord>* record)
{
	return con->blocking().Get(key, record);
}

KineticStatus SimpleKineticNamespace::Delete(const std::string &key, const std::string& version, WriteMode mode)
{
	return con->blocking().Delete(key, version, mode);
}

KineticStatus SimpleKineticNamespace::Put(const std::string &key, const std::string &current_version, WriteMode mode, const KineticRecord& record)
{
	return con->blocking().Put(key, current_version, mode, record);
}

KineticStatus SimpleKineticNamespace::GetVersion (const std::string &key, std::string* version)
{
	return con->blocking().GetVersion(key, version);
}


KineticStatus SimpleKineticNamespace::GetKeyRange(const std::string &start_key, const std::string &end_key, unsigned int max_results, std::vector<std::string> *keys)
{
	const bool start_key_inclusive 	= false;
	const bool end_key_inclusive 	= false;
	const bool reverse_results   	= false;
	return con->blocking().GetKeyRange(start_key, start_key_inclusive, end_key, end_key_inclusive, reverse_results, max_results, keys);
}

KineticStatus SimpleKineticNamespace::Capacity(kinetic::Capacity &cap)
{
	kinetic::DriveLog log;
	KineticStatus status =  con->blocking().GetLog(&log);
	if(status.ok()){
		cap.remaining_bytes = log.capacity.remaining_bytes;
		cap.total_bytes		= log.capacity.total_bytes;
	}
	return status;
}
