#include "main.h"
#include "debug.h"
#include "fuseops.h"
#include "kinetic_helper.h"
#include <sys/param.h>


int rename_lookup(
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


static int move_directory(const char *user_path_from, const char *user_path_to,
         std::shared_ptr<MetadataInfo> &mdito,
         std::shared_ptr<MetadataInfo> &mdifrom,
         std::shared_ptr<MetadataInfo> &dir_mdito,
         std::shared_ptr<MetadataInfo> &dir_mdifrom)
{
    mdito->getMD().set_type(posixok::Metadata_InodeType_FORCE_UPDATE);
    mdito->getMD().set_force_update_version(PRIV->pmap.getSnapshotVersion() + 1);
    int err = create_metadata(mdito);
    if(err) return err;

    posixok::db_entry entry;
    entry.set_type(entry.MOVE);
    entry.set_origin(user_path_from);
    entry.set_target(user_path_to);
    err = database_op(
            std::bind(rename_lookup, user_path_from, user_path_to,
                    std::ref(dir_mdifrom), std::ref(dir_mdito), std::ref(mdifrom), std::ref(mdito)),
                    entry
                    );
    if(err) return err;

    dir_mdifrom->getMD().set_force_update_version( PRIV->pmap.getSnapshotVersion() );
    err = put_metadata(dir_mdifrom);
    if(err) return err;

    /* only touch metadata of parent directory of the new name if it is a different directory */
    if( dir_mdito->getMD().inode_number() != dir_mdifrom->getMD().inode_number() ){
       dir_mdito->getMD().set_force_update_version( PRIV->pmap.getSnapshotVersion() );
       err = put_metadata(dir_mdito);
       if(err) return err;
    }

    mdifrom->updateACtime();
    put_metadata(mdifrom);
    return delete_directory_entry(dir_mdifrom, util::path_to_filename(user_path_from));
}

static int move_hardlink(const char *user_path_from, const std::shared_ptr<MetadataInfo> &dir_mdifrom,
        const std::shared_ptr<MetadataInfo> &mdito, const std::shared_ptr<MetadataInfo> &mdifrom )
{
    mdito->getMD().set_type(posixok::Metadata_InodeType_HARDLINK_S);
    mdito->getMD().set_inode_number(mdifrom->getMD().inode_number());
    int err = create_metadata(mdito);
    if (err)
      return err;

    mdifrom->getMD().set_link_count(mdifrom->getMD().link_count() + 1);
    mdifrom->updateACtime();
    err = put_metadata(mdifrom);
    if (err)
        return err;
    return pok_unlink(user_path_from);
}

static int move_symlink(const char *user_path_from, const char *user_path_to,
        const std::shared_ptr<MetadataInfo> &mdito, const std::shared_ptr<MetadataInfo> &mdifrom )
{
    char buffer[PATH_MAX];
    int err = pok_readlink(user_path_from, buffer, PATH_MAX);
    if( err)
        return err;

    posixok::db_entry entry;
    entry.set_type(entry.SYMLINK);
    entry.set_origin(user_path_to);
    entry.set_target(buffer);

    std::string fromKey = mdifrom->getSystemPath();
    mdifrom->updateACtime();
    mdifrom->setSystemPath( mdito->getSystemPath() );
    err = database_operation(
          std::bind(create_metadata, mdifrom),
          std::bind(delete_metadata, mdifrom),
          entry);
    mdifrom->setSystemPath( fromKey );
    if(err)
        return err;
    return pok_unlink(user_path_from);
}

static int move_regular(const char *user_path_from, const std::shared_ptr<MetadataInfo> &mdito, const std::shared_ptr<MetadataInfo> &mdifrom )
{
    std::string fromKey = mdifrom->getSystemPath();
    mdifrom->setSystemPath( mdito->getSystemPath() );
    mdifrom->updateACtime();
    int err = create_metadata(mdifrom);
    mdifrom->setSystemPath( fromKey );
    if(err)
        return err;

    return pok_unlink(user_path_from);
}


/** Rename a file or directory. */
int pok_rename(const char *user_path_from, const char *user_path_to)
{
    pok_trace("Rename '%s' to '%s'", user_path_from, user_path_to);
    database_update();
    std::shared_ptr<MetadataInfo> dir_mdifrom;
    std::shared_ptr<MetadataInfo> dir_mdito;
    std::shared_ptr<MetadataInfo> mdifrom;
    std::shared_ptr<MetadataInfo> mdito;
    int err = rename_lookup(user_path_from, user_path_to, dir_mdifrom, dir_mdito, mdifrom, mdito);
    if (err) return err;

    /* Remove potentially existing directory */
    if(mdito->getCurrentVersion() != -1){
        if (S_ISDIR(mdito->getMD().mode()))
             err = pok_rmdir(user_path_to);
        else err = pok_unlink(user_path_to);
        if (err) return err;
    }

    /* Create new directory entry */
    if ((err = create_directory_entry(dir_mdito, util::path_to_filename(user_path_to))))
        return err;


    PRIV->lookup_cache.invalidate(mdifrom->getSystemPath());
    PRIV->lookup_cache.invalidate(mdito->getSystemPath());

    int mode = mdifrom->getMD().mode();

    /* Copy metadata-key to the new location & update pathmapDB if necessary. */
    if (S_ISDIR(mode))
        err = move_directory(user_path_from, user_path_to, mdito, mdifrom, dir_mdito, dir_mdifrom);

    else if (mdifrom->getMD().type() == posixok::Metadata_InodeType_HARDLINK_T)
        err = move_hardlink(user_path_from, dir_mdifrom, mdito, mdifrom);

    else if S_ISLNK(mode)
        err = move_symlink(user_path_from, user_path_to, mdito, mdifrom);

    else
        err = move_regular(user_path_from, mdito, mdifrom);

    PRIV->lookup_cache.invalidate(mdifrom->getSystemPath());
    PRIV->lookup_cache.invalidate(mdito->getSystemPath());

    if(err)
        pok_warning("Error in move operation after point of no return");
    return err;
}
