#include "metadata_info.h"
#include <chrono>

posixok::Metadata * MetadataInfo::pbuf(){
	return &md;
}

const std::string & MetadataInfo::getSystemPath(){
	return systemPath;
}

const std::string & MetadataInfo::getCurrentVersion(){
	return currentVersion;
}

void MetadataInfo::setSystemPath(const std::string &path){
	systemPath=path;
}

void MetadataInfo::setCurrentVersion(const std::string &version){
	currentVersion=version;
}

void MetadataInfo::updateACMtime(){
	std::time_t now;
	std::time(&now);
	md.set_atime(now);
	md.set_mtime(now);
	md.set_ctime(now);
}

void MetadataInfo::updateACtime(){
	std::time_t now;
	std::time(&now);
	md.set_atime(now);
	md.set_ctime(now);
}

