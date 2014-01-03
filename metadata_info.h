/* In-memory metadata representation. */

#ifndef METADATA_INFO_H_
#define METADATA_INFO_H_


#include "metadata.pb.h"

class MetadataInfo final{

private:
	posixok::Metadata md;
	std::string  systemPath;		// key in flat namespace where metadata is stored
	std::string  currentVersion;	// current version of key in flat namespace

public:
	explicit MetadataInfo(const std::string &systemPath, const std::string &currentVersion):
	md(),systemPath(systemPath), currentVersion(currentVersion){};
	explicit MetadataInfo():
	md(),systemPath(), currentVersion(){};
	~MetadataInfo(){};

public:
	posixok::Metadata * pbuf(); 								// direct access to protobuf structure

	void updateACMtime();
	void updateACtime();
	const std::string & getSystemPath();
	const std::string & getCurrentVersion();
	void setSystemPath(const std::string &systemPath);
	void setCurrentVersion(const std::string &currentVersion);
};




#endif /* METADATA_INFO_H_ */
