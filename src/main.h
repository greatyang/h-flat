/*
 * POSIX-o-K is either 	POSIX over Key-Value
 * or					POSIX over Kinetic
 *
 * Either way, the goal is to be POSIX compliant in a flat namespace and support file lookup without path traversal.
 */
#ifndef MAIN_H_
#define MAIN_H_

#define FUSE_USE_VERSION 26
#include <fuse/fuse.h>

#include <memory>
#include <functional>

#include "pathmap_db.h"
#include "kinetic_namespace.h"
#include "metadata_info.h"
#include "directorydata.pb.h"

/* Private file-system wide data, accessible from anywhere. */
struct pok_priv
{
	std::unique_ptr<PathMapDB> 	   pmap;	// path permission & remapping
	std::unique_ptr<FlatNamespace> nspace;	// access storage using flat namespace
//  std::unique_ptr<HouseKeeper>   ursula;  // cleans, polishes and so on
	const int blocksize;

	pok_priv():
		pmap(new PathMapDB()),
		nspace(new KineticNamespace()),
		blocksize(1024 * 1024)
	{}
};
#define PRIV ((struct pok_priv*) fuse_get_context()->private_data)



/* util > these are utility functions provided to the various path based fuse operations. */
int lookup				(const char *user_path, const std::unique_ptr<MetadataInfo> &mdi);
int lookup_parent		(const char *user_path, const std::unique_ptr<MetadataInfo> &mdi_parent);
int create_from_mdi		(const char *user_path, mode_t mode, const std::unique_ptr<MetadataInfo> &mdi);
int directory_addEntry	(const std::unique_ptr<MetadataInfo> &mdi, const posixok::DirectoryEntry &e);
std::string uuid_get	(void);
inline std::string path_to_filename(const std::string &path) { return path.substr(path.find_last_of('/')+1); }

/* util_database */
int database_update(void);
int database_operation(std::function<int ()> fsfun_do, std::function<int ()> fsfun_undo, posixok::db_entry &entry);

#endif
