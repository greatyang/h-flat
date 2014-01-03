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
#include <errno.h>
#include <memory>

#include "pathmapdb.h"
#include "metadata_info.h"
#include "kinetic_namespace.h"


/* Private file-system wide data, accessible from anywhere. */
struct pok_priv
{
	std::unique_ptr<PathMapDB> 	   pmap;	// path permission & remapping
	std::unique_ptr<FlatNamespace> nspace;	// access storage using flat namespace

	pok_priv():
		pmap(new PathMapDB()),
		nspace(new KineticNamespace())
	{}
};
#define PRIV ((struct pok_priv*) fuse_get_context()->private_data)


/* internal.cpp */
int lookup		 (const char *user_path, const std::unique_ptr<MetadataInfo> &mdi);
int lookup_parent(const char *user_path, const std::unique_ptr<MetadataInfo> &mdi_parent);
std::string generate_unique_id();
#endif
