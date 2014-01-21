#include "main.h"
#include "debug.h"
#include "kinetic_helper.h"
#include <unistd.h>

static int readwrite(char *buf, size_t size, off_t offset, MetadataInfo * mdi, bool write)
{
    /* might have to split the operation across multiple blocks */
    int blocksize = PRIV->blocksize;
    int sizeleft = size;
    int blocknum = offset / blocksize;

    while (sizeleft) {
        int inblockstart = offset > blocknum * blocksize ? (offset - blocknum * blocksize) : 0;
        int inblocksize = sizeleft > blocksize - inblockstart ? blocksize - inblockstart : sizeleft;

        if (!mdi->hasDataInfo(blocknum))
            if (int err = get_data(mdi, blocknum))
                return err;

        DataInfo *di = mdi->getDataInfo(blocknum);
        if (write) {
            di->updateData(buf, inblockstart, inblocksize);
            put_data(mdi, blocknum);
        } else
            memcpy(buf + size - sizeleft, di->data().data() + inblockstart, inblocksize);

        sizeleft -= inblocksize;
        blocknum++;
    }
    return size;

}

/** Read data from an open file
 *
 * Read should return exactly the number of bytes requested except
 * on EOF or error, otherwise the rest of the data will be
 * substituted with zeroes.	 An exception to this is when the
 * 'direct_io' mount option is specified, in which case the return
 * value of the read system call will reflect the return value of
 * this operation.
 *
 * Changed in version 2.2
 */
int pok_read(const char* user_path, char *buf, size_t size, off_t offset, struct fuse_file_info* fi)
{
    MetadataInfo * mdi = reinterpret_cast<MetadataInfo *>(fi->fh);
    if (!mdi) {
        pok_warning("Read request for user path '%s' without metadata_info structure", user_path);
        return -EINVAL;
    }
    if (offset + size > mdi->pbuf()->size()) {
        pok_trace("Attempting to read beyond EOF for user path %s", user_path);
        return 0;
    }
    return readwrite(buf, size, offset, mdi, false);
}

/** Write data to an open file
 *
 * Write should return exactly the number of bytes requested
 * except on error.	 An exception to this is when the 'direct_io'
 * mount option is specified (see read operation).
 *
 * Changed in version 2.2
 */
int pok_write(const char* user_path, const char *buf, size_t size, off_t offset, struct fuse_file_info* fi)
{
    pok_debug("Write request of %d bytes for user path %s. ", size, user_path);
    MetadataInfo * mdi = reinterpret_cast<MetadataInfo *>(fi->fh);
    if (!mdi) {
        pok_warning("Write request for user path '%s' without metadata_info structure", user_path);
        return -EINVAL;
    }
    int rtn = readwrite(const_cast<char*>(buf), size, offset, mdi, true);

    /* update metadata on success */
    if (rtn > 0) {
        size_t newsize = std::max((std::uint64_t) offset + size, (std::uint64_t) mdi->pbuf()->size());
        mdi->pbuf()->set_size(newsize);
        mdi->pbuf()->set_blocks((newsize / PRIV->blocksize) + 1);
        mdi->updateACMtime();
        if ( int err = put_metadata(mdi) )
           return err;
    }
    return rtn;
}

static int truncate(MetadataInfo *mdi, off_t offset)
{
    if (check_access(mdi, W_OK))
        return -EACCES;

    if (offset > std::numeric_limits<std::uint32_t>::max())
        return -EFBIG;

    mdi->pbuf()->set_size(offset);
    mdi->updateACMtime();
    return put_metadata(mdi);
}

/** Change the size of a file */
int pok_truncate(const char *user_path, off_t offset)
{
    std::unique_ptr<MetadataInfo> mdi(new MetadataInfo());
    if (int err = lookup(user_path, mdi))
        return err;
    return truncate(mdi.get(), offset);
}

/**
 * Change the size of an open file
 *
 * This method is called instead of the truncate() method if the
 * truncation was invoked from an ftruncate() system call.
 *
 * If this method is not implemented or under Linux kernel
 * versions earlier than 2.6.15, the truncate() method will be
 * called instead.
 *
 * Introduced in version 2.5
 */
int pok_ftruncate(const char *user_path, off_t offset, struct fuse_file_info *fi)
{
    MetadataInfo * mdi = reinterpret_cast<MetadataInfo *>(fi->fh);
    if (!mdi) {
        pok_warning("Write request for user path '%s' without metadata_info structure", user_path);
        return -EINVAL;
    }
    return truncate(mdi, offset);
}

