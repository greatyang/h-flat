#include "main.h"
#include "debug.h"
#include "fuseops.h"
#include "kinetic_helper.h"
#include <sys/param.h>


static int move_directory(const char *user_path_from, const char *user_path_to,
        const std::unique_ptr<MetadataInfo> &mdito,
        const std::unique_ptr<MetadataInfo> &dir_mdito,
        const std::unique_ptr<MetadataInfo> &dir_mdifrom)
{
    posixok::db_entry entry;
    entry.set_type(entry.MOVE);
    entry.set_origin(user_path_from);
    entry.set_target(user_path_to);

    mdito->getMD().set_type(posixok::Metadata_InodeType_FORCE_UPDATE);
    mdito->getMD().set_force_update_version(PRIV->pmap->getSnapshotVersion() + 1);
    int err = database_operation(
            std::bind(create_metadata, mdito.get()),
            std::bind(delete_metadata, mdito.get()),
            entry);
    if(err)
        return err;

    dir_mdifrom->getMD().set_force_update_version( PRIV->pmap->getSnapshotVersion() );
    err = put_metadata(dir_mdifrom.get());
    if(err)
        return err;

    /* only touch metadata of parent directory of the new name if it is a different directory */
    if( dir_mdito->getMD().inode_number() != dir_mdifrom->getMD().inode_number() ){
        dir_mdito->getMD().set_force_update_version( PRIV->pmap->getSnapshotVersion() );
        err = put_metadata(dir_mdito.get());
           if(err)
               return err;
    }
    return delete_directory_entry(dir_mdifrom, util::path_to_filename(user_path_from));
}

static int move_hardlink(const char *user_path_from, const std::unique_ptr<MetadataInfo> &dir_mdifrom,
        const std::unique_ptr<MetadataInfo> &mdito, const std::unique_ptr<MetadataInfo> &mdifrom )
{
    mdito->getMD().set_type(posixok::Metadata_InodeType_HARDLINK_S);
    mdito->getMD().set_inode_number(mdifrom->getMD().inode_number());
    int err = create_metadata(mdito.get());
    if (err)
      return err;

    mdifrom->getMD().set_link_count(mdifrom->getMD().link_count() + 1);
    err = put_metadata(mdifrom.get());
    if (err)
        return err;
    return pok_unlink(user_path_from);
}

static int move_symlink(const char *user_path_from, const char *user_path_to,
        const std::unique_ptr<MetadataInfo> &mdito, const std::unique_ptr<MetadataInfo> &mdifrom )
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
    mdifrom->setSystemPath( mdito->getSystemPath() );
    err = database_operation(
          std::bind(create_metadata, mdifrom.get()),
          std::bind(delete_metadata, mdifrom.get()),
          entry);
    mdifrom->setSystemPath( fromKey );
    if(err)
        return err;
    return pok_unlink(user_path_from);
}

static int move_regular(const char *user_path_from, const std::unique_ptr<MetadataInfo> &mdito, const std::unique_ptr<MetadataInfo> &mdifrom )
{
    std::string fromKey = mdifrom->getSystemPath();
    mdifrom->setSystemPath( mdito->getSystemPath() );
    int err = create_metadata(mdifrom.get());
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

    std::unique_ptr<MetadataInfo> dir_mdifrom(new MetadataInfo());
    std::unique_ptr<MetadataInfo> dir_mdito(new MetadataInfo());
    std::unique_ptr<MetadataInfo> mdifrom(new MetadataInfo());
    std::unique_ptr<MetadataInfo> mdito(new MetadataInfo());
    int err = 0;

    /* Lookup metadata of supplied paths and their directories.
     * Except user_path_to everything has to exist, user_path_to has
     * to be deleted if it does exist. */
    if (!err)
        lookup_parent(user_path_from, dir_mdifrom);
    if (!err)
        err = lookup_parent(user_path_to, dir_mdito);
    if (!err)
        err = lookup(user_path_from, mdifrom);
    if (err)
        return err;

    /* If sticky bit is set on parent directory, current user needs to be owner of parent directory OR the moved entity (or root of course). */
   if (fuse_get_context()->uid && (dir_mdifrom->getMD().mode() & S_ISVTX) && (fuse_get_context()->uid != dir_mdifrom->getMD().uid())
           && (fuse_get_context()->uid != mdifrom->getMD().uid()))
       return -EACCES;

    err = lookup(user_path_to, mdito);
    if (err && err != -ENOENT) // if path doesn't exist that's just fine
        return err;
    /* Path does exist. Make sure we're allowed to proceed. */
    else if (!err) {
        /* rename returns EACCES or EPERM if the file pointed at by the 'to' argument exists, the directory containing 'to' is marked sticky,
         * and neither the containing directory nor 'to' are owned by the effective user ID */
        if (fuse_get_context()->uid && (dir_mdito->getMD().mode() & S_ISVTX) && (fuse_get_context()->uid != dir_mdito->getMD().uid())
                && (fuse_get_context()->uid != mdito->getMD().uid()))
            return -EACCES;

        if (S_ISDIR(mdito->getMD().mode()))
            err = pok_rmdir(user_path_to);
        else
            err = pok_unlink(user_path_to);
        if (err)
            return err;
    }

    /* Create new directory entry */
    if ((err = create_directory_entry(dir_mdito, util::path_to_filename(user_path_to))))
        return err;

    int mode = mdifrom->getMD().mode();

    /* Copy metadata-key to the new location & update pathmapDB if necessary. */
    if (S_ISDIR(mode))
        err = move_directory(user_path_from, user_path_to, mdito, dir_mdito, dir_mdifrom);

    else if (mdifrom->getMD().type() == posixok::Metadata_InodeType_HARDLINK_T)
        err = move_hardlink(user_path_from, dir_mdifrom, mdito, mdifrom);

    else if S_ISLNK(mode)
        err = move_symlink(user_path_from, user_path_to, mdito, mdifrom);

    else
        err = move_regular(user_path_from, mdito, mdifrom);


    if(err)
        pok_warning("Error in move operation after point of no return");
    return err;

}
