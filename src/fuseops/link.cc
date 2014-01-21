#include "main.h"
#include "debug.h"
#include "fuseops.h"
#include "kinetic_helper.h"

/** Read the target of a symbolic link
 *
 * The buffer should be filled with a null terminated string.  The
 * buffer size argument includes the space for the terminating
 * null character.	If the linkname is too long to fit in the
 * buffer, it should be truncated.	The return value should be 0
 * for success.
 */
int pok_readlink(const char *user_path, char *buffer, size_t size)
{
    std::string link_destination;
    std::int64_t pathPermissionTimeStamp = 0;
    link_destination = PRIV->pmap->toSystemPath(user_path, pathPermissionTimeStamp, CallingType::READLINK);

    if (link_destination.length() >= size) {
        pok_debug("buffer too small to fit link destination.");
        strncpy(buffer, link_destination.c_str(), size);
        buffer[size - 1] = '\0';
    } else
        strcpy(buffer, link_destination.c_str());
    return 0;
}

/** Create a symbolic link */
int pok_symlink(const char *target, const char *origin)
{
    posixok::db_entry entry;
    entry.set_type(entry.SYMLINK);
    entry.set_origin(origin);
    entry.set_target(target);

    return database_operation(std::bind(pok_create, origin, S_IFLNK | S_IRWXU | S_IRGRP | S_IXGRP | S_IXOTH), std::bind(pok_unlink, origin), entry);
}

/** Create a hard link to a file */
int pok_hardlink(const char *target, const char *origin)
{
    std::unique_ptr<MetadataInfo> mdi_target(new MetadataInfo());
    if (int err = lookup(target, mdi_target))
        return err;
    assert(!S_ISDIR(mdi_target->pbuf()->mode()));

    pok_debug("Hardlink %s->%s", origin, target);

    std::unique_ptr<MetadataInfo> mdi_origin(new MetadataInfo());
    int err = lookup(origin, mdi_origin);
    if (!err)
        return -EEXIST;
    if (err != -ENOENT)
        return err;

    std::unique_ptr<MetadataInfo> mdi_origin_dir(new MetadataInfo());
    err = lookup_parent(origin, mdi_origin_dir);
    if (err)
        return err;

    /* If the target metadata is not already a hardlink_target, make it so. */
    if (mdi_target->pbuf()->type() != posixok::Metadata_InodeType_HARDLINK_T) {
        assert(mdi_target->pbuf()->type() == posixok::Metadata_InodeType_POSIX);

        std::unique_ptr<MetadataInfo> mdi_source(new MetadataInfo());
        mdi_source->pbuf()->set_type(posixok::Metadata_InodeType_HARDLINK_S);
        mdi_source->pbuf()->set_inode_number(mdi_target->pbuf()->inode_number());
        mdi_source->setCurrentVersion(mdi_target->getCurrentVersion());
        mdi_source->setSystemPath(mdi_target->getSystemPath());

        mdi_target->setSystemPath("hardlink_" + std::to_string(mdi_target->pbuf()->inode_number()));
        mdi_target->pbuf()->set_type(posixok::Metadata_InodeType_HARDLINK_T);

        /* store target metadata at hardlink-key location */
        if (int err = create_metadata(mdi_target.get()))
            return err;

        /* create the forwarding to the hardlink-key.
         * if this should fail, there's a garbage uuid key to be cleaned up. */
        if (int err = put_metadata(mdi_source.get()))
            return err;

    }

    if ((err = create_directory_entry(mdi_origin_dir, util::path_to_filename(origin))))
        return err;

    mdi_target->pbuf()->set_link_count(mdi_target->pbuf()->link_count() + 1);
    if ((err = put_metadata(mdi_target.get()))) {
        pok_warning("Failed increasing link count of metadata after creating directory entry");
        return err;
    }

    mdi_origin->pbuf()->set_type(posixok::Metadata_InodeType_HARDLINK_S);
    mdi_origin->pbuf()->set_inode_number(mdi_target->pbuf()->inode_number());
    if ((err = create_metadata(mdi_origin.get())))
        pok_warning("Failed creating metadata key after increasing target link count and creating directory entry.");
    return err;
}
