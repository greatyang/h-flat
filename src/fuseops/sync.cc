#include "main.h"
#include "kinetic_helper.h"
#include "debug.h"

/** Synchronize file contents
 *
 * If the datasync parameter is non-zero, then only the user data
 * should be flushed, not the meta data.
 *
 * Changed in version 2.2
 */
int hflat_fsync(const char *user_path, int datasync, struct fuse_file_info *fi)
{
    std::shared_ptr<MetadataInfo> mdi;
    int err = lookup(user_path, mdi);
    if( err) return err;

    std::shared_ptr<DataInfo> di = mdi->getDirtyData();
    if(! di || ! di->hasUpdates() )
        return 0;

    /* Data and metadata is flushed as a unit, synchronized over the metadata key. Different flush rules for O_APPEND and regular */
    int blocknum = util::to_int64(di->getKey().substr(di->getKey().find_first_of('_',0)+1,di->getKey().size()));
    size_t newsize = std::max((std::uint64_t) PRIV->blocksize * blocknum + di->data().size(), (std::uint64_t) mdi->getMD().size());
    if(mdi->getMD().size() < newsize || PRIV->posix == PosixMode::FULL){
       mdi->getMD().set_size(newsize);
       mdi->getMD().set_blocks((newsize / PRIV->blocksize) + 1);
       mdi->updateACMtime();
       err = put_metadata(mdi);
       /* O_APPEND -> can't allow non-serialized data changes*/
       if(err && fi && (fi->flags & O_APPEND)){
           PRIV->data_cache.invalidate(di->getKey());
           return err;
       }
       if(err == -EAGAIN) return hflat_fsync(user_path, datasync, fi);
       if(err) hflat_error("Impossible Error. ");
    }
   /* write data key */
   return put_data(di);
}

/** Synchronize directory contents
 *
 * If the datasync parameter is non-zero, then only the user data
 * should be flushed, not the meta data
 *
 * Introduced in version 2.3
 */
int hflat_fsyncdir(const char *user_path, int datasync, struct fuse_file_info *fi)
{
    /* No action required in this file system, directories are never dirty. */
    return 0;
}

/** Possibly flush cached data
 *
 * BIG NOTE: This is not equivalent to fsync().  It's not a
 * request to sync dirty data.
 *
 * Flush is called on each close() of a file descriptor.  So if a
 * filesystem wants to return write errors in close() and the file
 * has cached dirty data, this is a good place to write back data
 * and return any errors.  Since many applications ignore close()
 * errors this is not always useful.
 *
 * NOTE: The flush() method may be called more than once for each
 * open().	This happens if more than one file descriptor refers
 * to an opened file due to dup(), dup2() or fork() calls.	It is
 * not possible to determine if a flush is final, so each flush
 * should be treated equally.  Multiple write-flush sequences are
 * relatively rare, so this shouldn't be a problem.
 *
 * Filesystems shouldn't assume that flush will always be called
 * after some writes, or that if will be called at all.
 *
 * Changed in version 2.2
 */
int hflat_flush(const char *user_path, struct fuse_file_info *fi)
{
    /* This seems like too much of a random place to do much of anything. */
    return 0;
}
