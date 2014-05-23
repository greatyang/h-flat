/* h-flat file system: Hierarchical Functionality in a Flat Namespace
 * Copyright (c) 2014 Seagate
 * Written by Paul Hermann Lensing
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
#include "lru_cache.h"

enum class PosixMode { FULL, TIMERELAXED };
/* Private file-system wide data, accessible from anywhere. */
struct hflat_priv
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

    hflat_priv(KineticNamespace *kn, int cache_expiration_ms, int block_size_bytes, PosixMode mode):
            kinetic(kn),
            lookup_cache(cache_expiration_ms, 5000,
                    std::mem_fn(&MetadataInfo::getSystemPath),
                    [](const std::shared_ptr<MetadataInfo> &mdi){
                       std::shared_ptr<DataInfo> di = mdi->getDirtyData();
                       if(! di || ! di->hasUpdates() )
                            return false;
                       return true;
            }),
            data_cache(cache_expiration_ms, 500,
                    std::mem_fn(&DataInfo::getKey),
                    std::mem_fn(&DataInfo::hasUpdates)
            ),
            pmap(),
            blocksize(block_size_bytes),
            posix(mode),   // POSIX conform updating of directory time stamps costs performance
            inum_base(0),
            inum_counter(0),
            lock()
    {}
};
#define PRIV ((struct hflat_priv*) fuse_get_context()->private_data)


/* these are utility functions provided to the various fuse operations */

/* lookup */
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

namespace util
{
    int grab_inode_generation_token(void);
    ino_t generate_inode_number(void);
    std::string generate_uuid(void);
    std::int64_t to_int64(const std::string &version_string);
    std::int64_t to_int64(const std::shared_ptr<const std::string> version_string);
    std::string path_to_filename(const std::string &path);
    int database_update(void);
    int database_operation(hflat::db_entry &entry);
}

#endif
