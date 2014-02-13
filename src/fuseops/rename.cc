#include "main.h"
#include "debug.h"
#include "fuseops.h"
#include "kinetic_helper.h"
#include <sys/param.h>


static int rename_lookup(
        const char *user_path_from, const char *user_path_to,
        std::shared_ptr<MetadataInfo> &dir_mdifrom,
        std::shared_ptr<MetadataInfo> &dir_mdito,
        std::shared_ptr<MetadataInfo> &mdifrom,
        std::shared_ptr<MetadataInfo> &mdito
        )
{
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

static int rename_updateDB(const char *user_path_from, const char *user_path_to,
    std::shared_ptr<MetadataInfo> &mdito,
    std::shared_ptr<MetadataInfo> &mdifrom,
    std::shared_ptr<MetadataInfo> &dir_mdito,
    std::shared_ptr<MetadataInfo> &dir_mdifrom)
{
    posixok::db_entry entry;
    if(S_ISDIR(mdifrom->getMD().mode())){
      entry.set_type(entry.MOVE);
      entry.set_origin(user_path_from);
      entry.set_target(user_path_to);
    }
    else{
        assert(PRIV->pmap.hasMapping(user_path_from));
        entry.set_type(posixok::db_entry_TargetType_REMOVED);
        entry.set_origin(user_path_from);
        int err = util::database_operation([](){return 0;},entry);
        if (err) return err;

        char buffer[PATH_MAX];
        err = pok_readlink(user_path_from, buffer, PATH_MAX);
        if( err) return err;
        entry.set_type(entry.SYMLINK);
        entry.set_origin(user_path_to);
        entry.set_target(buffer);
    }
    return util::database_operation( std::bind(rename_lookup, user_path_from, user_path_to,
               std::ref(dir_mdifrom), std::ref(dir_mdito), std::ref(mdifrom), std::ref(mdito)),
               entry );
}

static int rename_moveMDKey(
    std::shared_ptr<MetadataInfo> &mdito,
    std::shared_ptr<MetadataInfo> &mdifrom )
{
    mdifrom->updateACtime();
    KineticRecord record(mdifrom->getMD().SerializeAsString(), std::to_string(mdifrom->getCurrentVersion() + 1), "",
            com::seagate::kinetic::proto::Message_Algorithm_SHA1);
    KineticStatus status = PRIV->kinetic->Put(mdito->getSystemPath(), "", WriteMode::REQUIRE_SAME_VERSION, record);
    if(!status.ok()) return -EIO;
    status = PRIV->kinetic->Delete(mdifrom->getSystemPath(), "", WriteMode::IGNORE_VERSION);
    if(!status.ok()) return -EIO;
    return 0;
}

static int rename_moveHardlink(const char *user_path_from, const char *user_path_to)
{
    std::shared_ptr<MetadataInfo> mdito;
    std::shared_ptr<MetadataInfo> mdifrom;

    int err = get_metadata_userpath(user_path_from, mdifrom);
    assert(mdifrom->getMD().type() == posixok::Metadata_InodeType_HARDLINK_S);

    err = lookup(user_path_to, mdito);
    assert(err == -ENOENT);

    err = rename_moveMDKey(mdito,mdifrom);

    PRIV->lookup_cache.invalidate(mdifrom->getSystemPath());
    PRIV->lookup_cache.invalidate(mdito->getSystemPath());
    return err;
}

static int rename_changeDirMD(const char *user_path_from, const char *user_path_to,
    std::shared_ptr<MetadataInfo> &mdito,
    std::shared_ptr<MetadataInfo> &mdifrom,
    std::shared_ptr<MetadataInfo> &dir_mdito,
    std::shared_ptr<MetadataInfo> &dir_mdifrom )
{
    /* If a force_update metadata inode exist at original location (the directory had already been moved in the past), remove it.
     * Create a new force_udpate metadata key at target location. */
    std::shared_ptr<MetadataInfo> mdi_fu(new MetadataInfo());
    mdi_fu->setSystemPath(dir_mdifrom->getSystemPath()+"/"+util::path_to_filename(user_path_from));
    int err = get_metadata(mdi_fu);
    if(!err && mdi_fu->getMD().type() == posixok::Metadata_InodeType_FORCE_UPDATE){
        err = delete_metadata(mdi_fu);
        if(err) return err;
    }
    mdi_fu->setSystemPath(dir_mdito->getSystemPath()+"/"+util::path_to_filename(user_path_to));
    mdi_fu->getMD().set_type(posixok::Metadata_InodeType_FORCE_UPDATE);
    mdi_fu->getMD().set_force_update_version(PRIV->pmap.getSnapshotVersion()+1 );
    err = create_metadata(mdi_fu);
    if(err) return err;

    /* Set force_update_version property in parent directories */
    dir_mdifrom->getMD().set_force_update_version( PRIV->pmap.getSnapshotVersion()+1 );
    err = put_metadata(dir_mdifrom);
    if(err) return err;
    if(dir_mdifrom != dir_mdito){
        dir_mdito->getMD().set_force_update_version( PRIV->pmap.getSnapshotVersion()+1 );
        err = put_metadata(dir_mdito);
        if(err) return err;
    }

    /* Update ACtime for POSIX compliance */
    mdifrom->updateACtime();
    return put_metadata(mdifrom);
}


/** Rename a file or directory. */
int pok_rename(const char *user_path_from, const char *user_path_to)
{
    pok_trace("Rename '%s' to '%s'", user_path_from, user_path_to);
    util::database_update();
    std::shared_ptr<MetadataInfo> dir_mdifrom;
    std::shared_ptr<MetadataInfo> dir_mdito;
    std::shared_ptr<MetadataInfo> mdifrom;
    std::shared_ptr<MetadataInfo> mdito;
    int err = rename_lookup(user_path_from, user_path_to, dir_mdifrom, dir_mdito, mdifrom, mdito);
    if (err) return err;

    /* rename returns EINVAL when the 'from' argument is a parent directory of 'to' */
    if(strncmp(user_path_from,user_path_to,strlen(user_path_from)) == 0)
        return -EINVAL;

    /* Remove potentially existing target if possible */
    if(mdito->getCurrentVersion() != -1){
        if (S_ISDIR(mdito->getMD().mode()))
             err = pok_rmdir(user_path_to);
        else err = pok_unlink(user_path_to);
        if (err) return err;
    }

    /* Create new directory entry: Synchronization point.  */
    if ((err = create_directory_entry(dir_mdito, util::path_to_filename(user_path_to))))
        return err;

    /* update database for directory and symlink moves, obtain the HARDLINK_S metadata for hardlink moves */
    if(S_ISDIR(mdifrom->getMD().mode()) || S_ISLNK(mdifrom->getMD().mode()))
       err = rename_updateDB(user_path_from, user_path_to, mdito, mdifrom, dir_mdito, dir_mdifrom);

    /* do the actual file system work */
    if(S_ISDIR(mdifrom->getMD().mode())){
        if(!err) err = rename_changeDirMD(user_path_from, user_path_to, mdito, mdifrom, dir_mdito, dir_mdifrom);
    } else if (mdifrom->getMD().type() == posixok::Metadata_InodeType_HARDLINK_T){
        if(!err) err = rename_moveHardlink(user_path_from, user_path_to);
    } else
        if(!err) err = rename_moveMDKey(mdito, mdifrom);


    /* delete old directory entry */
    if(!err) err = delete_directory_entry(dir_mdifrom, util::path_to_filename(user_path_from));

    /* invalidate cache */
    PRIV->lookup_cache.invalidate(mdifrom->getSystemPath());
    PRIV->lookup_cache.invalidate(mdito->getSystemPath());

    if(err)  pok_warning("Error in move operation after serialization point. Shouldn't have happened. Really really.");
    return err;
}
