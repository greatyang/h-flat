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
#include "debug.h"
#include "fuseops.h"
#include "kinetic_helper.h"
#include <sys/param.h>

using com::seagate::kinetic::client::proto::Command_Algorithm_SHA1;

static int rename_lookup(
        const char *user_path_from, const char *user_path_to,
        std::shared_ptr<MetadataInfo> &dir_mdifrom,
        std::shared_ptr<MetadataInfo> &dir_mdito,
        std::shared_ptr<MetadataInfo> &mdifrom,
        std::shared_ptr<MetadataInfo> &mdito
        )
{
    /* rename returns EINVAL when the 'from' argument is a parent directory of 'to' */
    if(strncmp(user_path_from,user_path_to,strlen(user_path_from)) == 0)
        return -EINVAL;

   /* Lookup metadata of supplied paths and their directories.
    * Except user_path_to everything has to exist, user_path_to has
    * to be deleted if it does exist. */
          int err = lookup_parent(user_path_from, dir_mdifrom);
    if (!err) err = lookup_parent(user_path_to, dir_mdito);
    if (!err) err = lookup(user_path_from, mdifrom);
    if  (err) return err;

    /* If sticky bit is set on parent directory, current user needs to be owner of parent directory OR the moved entity (or root of course). */
    if (fuse_get_context()->uid && (dir_mdifrom->getMD().mode() & S_ISVTX) && (fuse_get_context()->uid != dir_mdifrom->getMD().uid())
          && (fuse_get_context()->uid != mdifrom->getMD().uid()))
      return -EACCES;

    err = lookup(user_path_to, mdito);
    if (err && err != -ENOENT) // if path doesn't exist that's just fine
       return err;

    /* Path does exist. Make sure we're allowed to proceed. */
    /* rename returns EACCES or EPERM if the file pointed at by the 'to' argument exists, the directory containing 'to' is marked sticky,
     * and neither the containing directory nor 'to' are owned by the effective user ID */
    if (fuse_get_context()->uid && (dir_mdito->getMD().mode() & S_ISVTX) && (fuse_get_context()->uid != dir_mdito->getMD().uid())
           && (fuse_get_context()->uid != mdito->getMD().uid()))
       return -EACCES;

    /* write permissions to both directories required */
    err = check_access(dir_mdito, W_OK);
    if(!err) err = check_access(dir_mdifrom, W_OK);

    return err;
}


static int rename_regular(
    std::shared_ptr<MetadataInfo> &mdito,
    std::shared_ptr<MetadataInfo> &mdifrom )
{
    mdifrom->updateACtime();

    /* store md-key referenced by mdfrom in location pointed to be mdito */
    KineticRecord record(mdifrom->getMD().SerializeAsString(), mdito->getKeyVersion(), "", Command_Algorithm_SHA1);
    KineticStatus status = PRIV->kinetic->Put(mdito->getSystemPath(), "", WriteMode::REQUIRE_SAME_VERSION, record);
    if(!status.ok()) return -EIO;

    /* delete mdifrom */
    status = PRIV->kinetic->Delete(mdifrom->getSystemPath(), "", WriteMode::IGNORE_VERSION);
    if(!status.ok()) return -EIO;
    return 0;
}

static int rename_softlink(const char *user_path_from, const char *user_path_to,
    std::shared_ptr<MetadataInfo> &mdito,
    std::shared_ptr<MetadataInfo> &mdifrom
    )
{
    hflat::db_entry entry;
    char buffer[PATH_MAX];
    REQ_0( hflat_readlink(user_path_from, buffer, PATH_MAX) );

    entry.set_type(hflat::db_entry_Type_REMOVED);
    entry.set_origin(user_path_from);
    REQ_0( util::database_operation(entry) );

    entry.set_type(entry.SYMLINK);
    entry.set_origin(user_path_to);
    entry.set_target(buffer);
    REQ_0( util::database_operation(entry) );

    return rename_regular(mdito, mdifrom);
}

static int rename_hardlink(const char *user_path_from, std::shared_ptr<MetadataInfo> &mdito)
{
    std::shared_ptr<MetadataInfo> mdifrom;
    REQ_0( get_metadata_userpath(user_path_from, mdifrom) );
    assert(mdifrom->getMD().type() == hflat::Metadata_InodeType_HARDLINK_S);

    REQ_0(rename_regular(mdito, mdifrom));
    PRIV->lookup_cache.invalidate(mdifrom->getSystemPath());
    return 0;
}

static int rename_directory(const char *user_path_from, const char *user_path_to,
    std::shared_ptr<MetadataInfo> &mdito,
    std::shared_ptr<MetadataInfo> &mdifrom,
    std::shared_ptr<MetadataInfo> &dir_mdito,
    std::shared_ptr<MetadataInfo> &dir_mdifrom )
{
    /* Add database entry */
    hflat::db_entry entry;
    entry.set_type(entry.MOVE);
    entry.set_origin(user_path_from);
    entry.set_target(user_path_to);
    REQ_0 ( util::database_operation( entry ) );

    /* If a force_update metadata inode exist at original location (the directory had already been moved in the past), remove it. */
    std::string fu_dir =  dir_mdifrom->getSystemPath().length() > 1 ? dir_mdifrom->getSystemPath()+"/" : "/";
    std::shared_ptr<MetadataInfo> mdi_fu( new MetadataInfo( fu_dir + util::path_to_filename(user_path_from) ));
    hflat_debug("Checking path %s for existing FORCE_UPDATE inode.",mdi_fu->getSystemPath().c_str());
    if(get_metadata(mdi_fu) == 0 && mdi_fu->getMD().type() == hflat::Metadata_InodeType_FORCE_UPDATE)
        REQ_0 ( delete_metadata(mdi_fu) );

    /* Create a new force_udpate metadata key at target location.  */
    mdito->getMD().set_type(hflat::Metadata_InodeType_FORCE_UPDATE);
    mdito->getMD().set_inode_number( util::generate_inode_number() );
    REQ_0 ( create_metadata(mdito) );
    hflat_trace("successfully created FORCE_UPDATE inode for path %s",mdito->getSystemPath().data());


    /* Update ACtime for POSIX compliance */
    if(PRIV->posix == PosixMode::FULL)
        REQ_0(put_metadata_forced(mdifrom, [&mdifrom](){mdifrom->updateACtime();}) );
    return 0;
}


/** Rename a file or directory. */
int hflat_rename(const char *user_path_from, const char *user_path_to)
{
    hflat_trace("Rename '%s' to '%s'", user_path_from, user_path_to);
    std::shared_ptr<MetadataInfo> dir_mdifrom;
    std::shared_ptr<MetadataInfo> dir_mdito;
    std::shared_ptr<MetadataInfo> mdifrom;
    std::shared_ptr<MetadataInfo> mdito;
    int err = rename_lookup(user_path_from, user_path_to, dir_mdifrom, dir_mdito, mdifrom, mdito);
    if (err) return err;

    /* Remove potentially existing target if possible */
    if(mdito->getMD().inode_number()){
        if (S_ISDIR(mdito->getMD().mode()))
             err = hflat_rmdir(user_path_to);
        else err = hflat_unlink(user_path_to);
        if (err) return err;

        /* The system path set for mdito can be invalid after an unlink operation
         * iff mdito refers to a hardlinked inode... re-do lookup in this case. */
        if(mdito->getMD().type() == hflat::Metadata_InodeType_HARDLINK_T)
        	lookup(user_path_to, mdito);
    }

    /* Create new directory entry: Synchronization point (1) */
    if ((err = create_directory_entry(dir_mdito, util::path_to_filename(user_path_to)) ))
        return err;

    /* create recovery key in order to guarantee that fsck works correctly in all cases if client
     * crashes somewhere between this point and successfully finishing the rename operation. */
    std::string recovery_direntry = util::path_to_filename(user_path_to) + "|recovery|" + user_path_from;
    REQ_0( create_directory_entry (dir_mdito, recovery_direntry) );

    /* Delete old directory entry: Synchronization point (2) */
    if (( err = delete_directory_entry(dir_mdifrom, util::path_to_filename(user_path_from)) )){
        REQ_0 ( delete_directory_entry (dir_mdito, util::path_to_filename(user_path_to)) );
        REQ_0 ( delete_directory_entry (dir_mdito, recovery_direntry) );
        return err;
    }

    hflat_trace("passed serialization points");

    /* The actual move operation is type dependent */
    bool directory = S_ISDIR(mdifrom->getMD().mode());
    bool softlink  = S_ISLNK(mdifrom->getMD().mode());
    bool hardlink  = mdifrom->getMD().type() == hflat::Metadata_InodeType_HARDLINK_T;
    bool regular   = !directory && !hardlink && !softlink;

    if( directory ) REQ_0( rename_directory(user_path_from, user_path_to, mdito, mdifrom, dir_mdito, dir_mdifrom) );
    if( softlink  ) REQ_0( rename_softlink( user_path_from, user_path_to, mdito, mdifrom) );
    if( hardlink )  REQ_0( rename_hardlink( user_path_from, mdito) );
    if( regular  )  REQ_0( rename_regular(  mdito, mdifrom) );

    /* remove recovery key */
    REQ_0( delete_directory_entry (dir_mdito, recovery_direntry ));

    /* invalidate cache */
    PRIV->lookup_cache.invalidate(mdito->getSystemPath());
    PRIV->lookup_cache.invalidate(mdifrom->getSystemPath());
    return 0;
}
