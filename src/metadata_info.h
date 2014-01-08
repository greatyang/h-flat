/* In-memory metadata representation. */

#ifndef METADATA_INFO_H_
#define METADATA_INFO_H_


#include "metadata.pb.h"

class MetadataInfo final{

private:
	posixok::Metadata md;
	std::string  systemPath;		// key in flat namespace where metadata is stored
	std::string  currentVersion;	// current version of metadata key in flat namespace

	std::map<int, std::string> dataVersion; // keep track of key-versions of data keys that have been read in // map[blocknum] == keyVersion

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

	void 		trackDataVersion(int blockNumber, const std::string &keyVersion);
	std::string getDataVersion  (int blockNumber);

	/* return 'true' if changed, 'false' if unchanged. */
	bool computePathPermissionChildren();

	const std::string & getSystemPath();
	const std::string & getCurrentVersion();
	void setSystemPath(const std::string &systemPath);
	void setCurrentVersion(const std::string &currentVersion);
};




#endif /* METADATA_INFO_H_ */
