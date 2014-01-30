/*
 * POSIX-o-K is either  POSIX over Key-Value
 * or                   POSIX over Kinetic
 *
 * Either way, the goal is to be POSIX compliant in a flat namespace and support file lookup without path traversal.
 */
#ifndef MAIN_H_
#define MAIN_H_

#define FUSE_USE_VERSION 26
#include <fuse/fuse.h>
#include <unistd.h>
#include <memory>
#include <functional>

#include "pathmap_db.h"
#include "metadata_info.h"
#include "kinetic_namespace.h"

/* Private file-system wide data, accessible from anywhere. */
struct pok_priv
{
    std::unique_ptr<PathMapDB> pmap;               // path permission & remapping
    std::unique_ptr<KineticNamespace> kinetic;     // key-value api

    /* superblock like information */
    bool            multiclient;
    std::int32_t    blocksize;
    std::int64_t    inum_base;
    std::uint16_t   inum_counter;
    std::mutex      lock;

    pok_priv() :
            pmap(new PathMapDB()), kinetic(new SimpleKineticNamespace()),

            multiclient(false), blocksize(1024 * 1024), inum_base(0), inum_counter(10), lock()
    {
    }
};
#define PRIV ((struct pok_priv*) fuse_get_context()->private_data)

/* lookup > these are utility functions provided to the various path based fuse operations. */
int lookup(const char *user_path, const std::unique_ptr<MetadataInfo> &mdi, bool handle_special_inodes = true);
int lookup_parent(const char *user_path, const std::unique_ptr<MetadataInfo> &mdi_parent);

/* directory */
int create_directory_entry(const std::unique_ptr<MetadataInfo> &mdi_parent, std::string filename);
int delete_directory_entry(const std::unique_ptr<MetadataInfo> &mdi_parent, std::string filename);

/* permission */
int check_access(MetadataInfo *mdi, int mode);

/* file */
void initialize_metadata(const std::unique_ptr<MetadataInfo> &mdi, const std::unique_ptr<MetadataInfo> &mdi_parent, mode_t mode);

/* database */
int database_update(void);
int database_operation(std::function<int()> fsfun_do, std::function<int()> fsfun_undo, posixok::db_entry &entry);
int database_op(std::function<int()> verify, posixok::db_entry &entry);

/* general utility functions */
namespace util
{
    ino_t generate_inode_number(void);
    std::int64_t to_int64(const std::string &version_string);
    std::int64_t to_int64(const std::shared_ptr<const std::string> version_string);
    std::string path_to_filename(const std::string &path);
}

#endif
