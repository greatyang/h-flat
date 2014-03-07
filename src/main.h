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
#include "simple_kinetic_namespace.h"
#include "lru_cache.h"

enum class PosixMode
{
    FULL, TIMERELAXED
};

/* Private file-system wide data, accessible from anywhere. */
struct pok_priv
{
    std::unique_ptr<KineticNamespace> kinetic;
    LRUcache<std::string, std::shared_ptr<MetadataInfo>> lookup_cache;
    LRUcache<std::string, std::shared_ptr<DataInfo>>     data_cache;
    PathMapDB pmap;

    /* superblock like information */
    std::int32_t    blocksize;
    PosixMode       posix;

    /* inode generation */
    std::int64_t    inum_base;
    std::uint16_t   inum_counter;
    std::mutex      lock;

    pok_priv() :
            kinetic(new SimpleKineticNamespace()),
            lookup_cache(1000, 10, std::mem_fn(&MetadataInfo::getSystemPath),
                    [](const std::shared_ptr<MetadataInfo> &mdi){
                        if(mdi->getDirtyData() && mdi->getDirtyData()->hasUpdates())
                            return true;
                        return false;
            }),
            data_cache(1000, 10, std::mem_fn(&DataInfo::getKey), std::mem_fn(&DataInfo::hasUpdates)),
            pmap(),
            blocksize(1024 * 1024),
            posix(PosixMode::FULL),
            inum_base(0),
            inum_counter(0),
            lock()
    {}
};
#define PRIV ((struct pok_priv*) fuse_get_context()->private_data)

/* lookup > these are utility functions provided to the various path based fuse operations. */
int lookup(const char *user_path, std::shared_ptr<MetadataInfo> &mdi);
int lookup_parent(const char *user_path, std::shared_ptr<MetadataInfo> &mdi_parent);
int get_metadata_userpath(const char *user_path, std::shared_ptr<MetadataInfo> &mdi);

/* directory */
int create_directory_entry(const std::shared_ptr<MetadataInfo> &mdi_parent, std::string filename);
int delete_directory_entry(const std::shared_ptr<MetadataInfo> &mdi_parent, std::string filename);

/* permission */
int check_access(const std::shared_ptr<MetadataInfo> &mdi, int mode);

/* file */
void initialize_metadata(const std::shared_ptr<MetadataInfo> &mdi, const std::shared_ptr<MetadataInfo> &mdi_parent, mode_t mode);
void inherit_path_permissions(const std::shared_ptr<MetadataInfo> &mdi, const std::shared_ptr<MetadataInfo> &mdi_parent);


/* general utility functions */
namespace util
{
    int grab_inode_generation_token(void);
    ino_t generate_inode_number(void);
    std::int64_t to_int64(const std::string &version_string);
    std::int64_t to_int64(const std::shared_ptr<const std::string> version_string);
    std::string path_to_filename(const std::string &path);
    int database_update(void);
    int database_operation(posixok::db_entry &entry);
}

#endif
