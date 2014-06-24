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
#include "main.h"
using com::seagate::kinetic::client::proto::Message_Algorithm_SHA1;

static int scan_direntries(const char *user_path, const std::shared_ptr<MetadataInfo> &dir, std::string &entry)
{
    string keystart = std::to_string(dir->getMD().inode_number()) + "|";
    string keyend   = std::to_string(dir->getMD().inode_number()) + "|" + static_cast<char>(254);
    size_t maxsize = 100;
    unique_ptr<vector<std::string>> keys(new vector<string>());

    do {
        if (keys->size())
            keystart = keys->back();
        keys->clear();
        PRIV->kinetic->GetKeyRange(keystart, keyend, maxsize, keys);
        for (auto& element : *keys) {
           entry = element.substr(element.find_first_of('|') + 1, element.length());
           std::string filepath = user_path + entry;
           std::shared_ptr<MetadataInfo> mdi;
           if(int err = lookup(filepath.c_str(), mdi))
               return err;
        }
    } while (keys->size() == maxsize);
    return 0;
}


/* filename:recovery:origin-path
 *      there are two potential locations direntries and md-keys could exist
 *      ensure that both exist in the OLD location, then remove recovery-entry. */
static int recover_rename(const char *dir_path, const std::shared_ptr<MetadataInfo> &target_parent_mdi, std::string &entry)
{
    std::string origin = entry.substr(entry.find_last_of('|')+1, entry.length());
    std::string target = dir_path;
    if(target.back() != '/') target+='/';
    target += entry.substr(0, entry.find_first_of('|'));

    std::shared_ptr<MetadataInfo> target_mdi;
    std::shared_ptr<MetadataInfo> origin_mdi;
    std::shared_ptr<MetadataInfo> origin_parent_mdi;

    lookup(target.c_str(), target_mdi);
    lookup(origin.c_str(), origin_mdi);
    lookup_parent(origin.c_str(), origin_parent_mdi);

    PRIV->lookup_cache.invalidate(origin);
    PRIV->lookup_cache.invalidate(target);

    /* if hardlink -> obtain HARDLINK_S md-keys before continuing */
    if( (target_mdi->getMD().inode_number() && target_mdi->getMD().type() == hflat::Metadata_InodeType_HARDLINK_T)
      ||(origin_mdi->getMD().inode_number() && origin_mdi->getMD().type() == hflat::Metadata_InodeType_HARDLINK_T)
      )
    {
        get_metadata_userpath(target.c_str(), target_mdi);
        get_metadata_userpath(origin.c_str(), origin_mdi);
    }

    /* if directory -> obtain FORCE_UPDATE md-key before continuing. */
    if( (target_mdi->getMD().inode_number() && S_ISDIR(target_mdi->getMD().mode()))
      ||(origin_mdi->getMD().inode_number() && S_ISDIR(origin_mdi->getMD().mode()))
      )
    {
        if(PRIV->pmap.hasMapping(target.c_str())){
             hflat::db_entry entry;
             entry.set_type(hflat::db_entry_Type_REMOVED);
             entry.set_origin(target);
             int err = util::database_operation(entry);
             if(err) return err;
         }
        if(PRIV->pmap.hasMapping(origin.c_str())){
             hflat::db_entry entry;
             entry.set_type(hflat::db_entry_Type_REMOVED);
             entry.set_origin(origin);
             int err = util::database_operation(entry);
             if(err) return err;
         }
        get_metadata_userpath(target.c_str(), target_mdi);
        get_metadata_userpath(origin.c_str(), origin_mdi);
    }

    /* origin md-key doesn't exist -> copy from target location */
    if(origin_mdi->getMD().inode_number() == 0){
        assert(target_mdi->getMD().inode_number());
        assert(!S_ISDIR( target_mdi->getMD().mode() ));
        KineticRecord record(target_mdi->getMD().SerializeAsString(), target_mdi->getKeyVersion(), "", Message_Algorithm_SHA1);
        KineticStatus status = PRIV->kinetic->Put(origin_mdi->getSystemPath(), "", WriteMode::REQUIRE_SAME_VERSION, record);
        if(!status.ok()) return -EIO;
    }

    /* delete target metadata key if it exists */
    if(target_mdi->getMD().inode_number()){
        KineticStatus status = PRIV->kinetic->Delete(target_mdi->getSystemPath(), "", WriteMode::IGNORE_VERSION);
        if(!status.ok()) return -EIO;
    }

    /* origin direntry might not exist -> create */
    int err = create_directory_entry(origin_parent_mdi, util::path_to_filename(origin) );
    if(err && err != -EEXIST) return err;

    /* delete recovery direntry */
    err = delete_directory_entry(target_parent_mdi, entry);
    if(err) return err;
    return 0;
}

int fsck_directory(const char* user_path, const std::shared_ptr<MetadataInfo> &mdi)
{
    assert(S_ISDIR(mdi->getMD().mode()));
    assert(fuse_get_context()->uid == 0);
    std::string entry;

    while ( int err = scan_direntries(user_path, mdi, entry) )
    {
        if(err != -ENOENT) return err;

        /* entry is a recovery-entry */
        if(entry.find_first_of('|') != std::string::npos){
            err = recover_rename(user_path, mdi, entry);
            if(err) return err;
            continue;
        }

        /* standard behavior for resolving dangling direntries: just delete it (and possibly existing database entries)*/
        std::string filepath = user_path + entry;
        if(PRIV->pmap.hasMapping(filepath.c_str())){
             hflat::db_entry entry;
             entry.set_type(hflat::db_entry_Type_REMOVED);
             entry.set_origin(filepath);
             err = util::database_operation(entry);
             if(err) return err;
         }
         err = delete_directory_entry(mdi, entry.c_str());
         if(err) return err;
    }
    return 0;
}
