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
#include "directorydata.pb.h"
#include "kinetic_namespace.h"


/* Private file-system wide data, accessible from anywhere. */
struct pok_priv
{
	std::unique_ptr<PathMapDB> 	   pmap;	// path permission & remapping
	std::unique_ptr<FlatNamespace> nspace;	// access storage using flat namespace

	const int blocksize;

	pok_priv():
		pmap(new PathMapDB()),
		nspace(new KineticNamespace()),
		blocksize(1024 * 1024)
	{}
};
#define PRIV ((struct pok_priv*) fuse_get_context()->private_data)



/* lookup > these are utility functions provided to the various path based fuse operations. */
int lookup		 (const char *user_path, const std::unique_ptr<MetadataInfo> &mdi);
int lookup_parent(const char *user_path, const std::unique_ptr<MetadataInfo> &mdi_parent);

/* directory > also contains addEntry utility functionality used by other fuse operations */
int directory_addEntry(		const std::unique_ptr<MetadataInfo> &mdi, const posixok::DirectoryEntry &e);
int pok_readdir (const char *user_path, void *, fuse_fill_dir_t, off_t, struct fuse_file_info *fi);
int pok_mkdir 	(const char *user_path, mode_t mode);

/* permission */
int pok_access  (const char *user_path, int mode);
int pok_chmod 	(const char *user_path, mode_t mode);
int pok_chown 	(const char *user_path, uid_t uid, gid_t gid);

/* attr */
int pok_fgetattr(const char *user_path, struct stat *attr, struct fuse_file_info *fi);
int pok_getattr (const char *user_path, struct stat *attr);
int pok_utimens	(const char *user_path, const struct timespec tv[2]);
int pok_statfs  (const char *user_path, struct statvfs *s);

/* file */
int pok_create	(const char *user_path, mode_t mode, struct fuse_file_info *fi);
int pok_unlink	(const char *user_path);
int pok_open	(const char *user_path, struct fuse_file_info *fi);
int pok_release (const char *user_path, struct fuse_file_info *fi);

/* data */
int pok_read 	(const char* user_path, char *buf, size_t size, off_t offset, struct fuse_file_info* fi);
int pok_write	(const char* user_path, const char *buf, size_t size, off_t offset, struct fuse_file_info* fi);
int pok_ftruncate (const char *user_path, off_t offset, struct fuse_file_info *fi);
int pok_truncate  (const char *user_path, off_t offset);

/* link */
int pok_symlink (const char *link_destination, const char *user_path);
int pok_readlink (const char *user_path, char *buffer, size_t size);

/* rename */
int pok_rename (const char *user_path_from, const char *user_path_to);

/* main */
int update_pathmapDB();
inline std::string path_to_filename(const std::string &path)
{
	return path.substr(path.find_last_of('/')+1);
}

#endif
