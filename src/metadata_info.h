/* In-memory metadata representation. */

#ifndef METADATA_INFO_H_
#define METADATA_INFO_H_


#include "metadata.pb.h"

class MetadataInfo final{

private:
	posixok::Metadata md;
	std::string  systemPath;		// key in flat namespace where metadata is stored
	std::string  currentVersion;	// current version of metadata key in flat namespace

	std::map<int, std::string> dataVersion; // keep track of key-versions of data keys that have been read in
											// map[blocknum] == keyVersion

public:
	explicit MetadataInfo(const std::string &systemPath, const std::string &currentVersion):
	md(),systemPath(systemPath), currentVersion(currentVersion){};
	explicit MetadataInfo():
	md(),systemPath(), currentVersion(){};
	~MetadataInfo(){};

public:
	// initialize md values based on parent
	void initialize(const std::unique_ptr<MetadataInfo> &mdi_parent, mode_t mode);

	// direct access to protobuf structure
	posixok::Metadata * pbuf();

	// convenience functions to update timestamps in protobuf
	void updateACMtime();
	void updateACtime();

	// contains all the path permission computation
	// returns 'true' if changed, 'false' if unchanged.
	bool computePathPermissionChildren();


	// get & set
	void 		trackDataVersion(int blockNumber, const std::string &keyVersion);
	std::string getDataVersion  (int blockNumber);
	const std::string & getSystemPath();
	const std::string & getCurrentVersion();
	void setSystemPath(const std::string &systemPath);
	void setCurrentVersion(const std::string &currentVersion);
};




#endif /* METADATA_INFO_H_ */
