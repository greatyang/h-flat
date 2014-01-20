#include "main.h"
#include "debug.h"
#include <uuid/uuid.h>
#include <stdint.h>

namespace util{

std::int64_t to_int64(const std::string &version_string)
{
	std::uint64_t version;
	try{
		version = std::stoll(version_string);
	}
	catch(std::exception& e){
		pok_warning("Illegal version string '%s'. Setting version to 0.",version_string.c_str());
		version = 0;
	}
	return version;
}

std::string generate_uuid(void)
{
	uuid_t uuid;
	uuid_generate(uuid);
	char uuid_parsed[100];
	uuid_unparse(uuid, uuid_parsed);
	return std::string(uuid_parsed);
}

ino_t generate_inode_number(void)
{
	std::uint16_t counter;
	{
		std::lock_guard<std::mutex> locker(PRIV->lock);
		counter = ++ PRIV->inum_counter;

		if(!counter)						// counter wrapped, need new inum_base value
			pok_error("Not Implemented.");  // TODO: implement

	}
	return (ino_t) (PRIV->inum_base + counter);
}

std::string path_to_filename(const std::string &path)
{
	return path.substr(path.find_last_of('/')+1);
}

}
