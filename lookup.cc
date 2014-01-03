#include "main.h"
#include "debug.h"


int lookup(const char *user_path, const std::unique_ptr<MetadataInfo> &mdi)
{
	std::string key, value, version;
	std::int64_t pathPermissionTimeStamp = 0;

	/* Step 1: Transform user path to system path and obtain required path permission timestamp */
	key = PRIV->pmap->toSystemPath(user_path, pathPermissionTimeStamp, CallingType::LOOKUP);
	if(pathPermissionTimeStamp < 0)
		return pathPermissionTimeStamp;

	/* Step 2: Get metadata from flat namespace */
	mdi->setSystemPath(key);
	NamespaceStatus getMD = PRIV->nspace->getMD(mdi.get());
	if(getMD.notFound())
		return -ENOENT;
	if(getMD.notAuthorized())
		return -EPERM;
	if(getMD.notValid())
		return -EINVAL;

	/* Step 3: check path permissions for staleness */
	bool stale = mdi->pbuf()->path_permission_verified() < pathPermissionTimeStamp;
	if(stale){
		/* TODO: validate path permissions up the directory tree, recursively as necessary */
		pok_warning("Stale path permissions detected. Re-validation not implemented yet.");
	}
	return 0;
}


/* Path permissions are already verified. The systemPath, however, needs to be computed fresh(!). */
int lookup_parent(const char *user_path, const std::unique_ptr<MetadataInfo> &mdi_parent)
{
	std::string key(user_path);
	auto pos =  key.find_last_of("/");
	if(pos == std::string::npos)
		return -EINVAL;
	if(!pos) // root directory
		pos++;
	key.erase(pos,std::string::npos);

	return lookup(key.c_str(), mdi_parent);
}
