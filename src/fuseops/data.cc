#include "main.h"
#include "debug.h"
#include "kinetic_helper.h"
#include "fuseops.h"

enum class rw {READ, WRITE};
static int do_rw(char *buf, size_t size, off_t offset, const std::shared_ptr<MetadataInfo> &mdi, std::shared_ptr<DataInfo> &di, rw mode)
{
    int blocknum     = offset / PRIV->blocksize;
    int inblockstart = offset - blocknum * PRIV->blocksize;
    int inblocksize  = size > (size_t) PRIV->blocksize - inblockstart ? PRIV->blocksize - inblockstart : size;
    std::string key  = std::to_string(mdi->getMD().inode_number()) + "_" + std::to_string(blocknum);

    if(PRIV->data_cache.get(key, di) == false){
        if (int err = get_data(key, di))
            return err;
        PRIV->data_cache.add(key, di);
    }

    if(mode == rw::WRITE)  di->updateData(buf, inblockstart, inblocksize);
    if(mode == rw::READ)   memcpy(buf, di->data().data() + inblockstart, inblocksize);

    return inblocksize;
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
    std::shared_ptr<MetadataInfo> mdi;
    int err = lookup(user_path, mdi);
    if( err) return err;

    if (offset + size > mdi->getMD().size()) {
        pok_warning("Attempting to read beyond EOF for user path %s", user_path);
        return 0;
    }
    std::shared_ptr<DataInfo> di;
    return do_rw(buf,size,offset,mdi,di,rw::READ);
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
    pok_debug("writing %d bytes at offset %d for user path %s, O_APPEND: %d", size, offset, user_path, fi->flags & O_APPEND);

    std::shared_ptr<MetadataInfo> mdi;
    int err = lookup(user_path, mdi);
    if( err) return err;

    std::shared_ptr<DataInfo> di;
    size = do_rw(const_cast<char*>(buf), size, offset, mdi, di, rw::WRITE);
    if( size <= 0 ) return size;

    /* set updated datainfo structure in mdi, flush existing dirty data if required */
    if(! mdi->setDirtyData(di) ){
        err = pok_fsync(user_path, 0, fi);
        if(err) return err;
        mdi->setDirtyData(di);
    }

    /* check if the write should be immediately flushed or aggregated */
    if( (fi->flags & O_APPEND) || ((offset+size) % PRIV->blocksize == 0) ){
        err = pok_fsync(user_path,0,fi);
        if(err == -EAGAIN) return pok_write(user_path, buf, size, offset, fi);
        if(err) return err;
    }

    return size;
}

/** Change the size of a file */
int pok_truncate(const char *user_path, off_t offset)
{
    pok_debug("truncate for user path %s to size %ld",user_path,offset);
    std::shared_ptr<MetadataInfo> mdi;
    int err = lookup(user_path, mdi);
    if( err) return err;

    if (check_access(mdi, W_OK))
       return -EACCES;

    if (offset > std::numeric_limits<std::uint32_t>::max())
       return -EFBIG;

    mdi->getMD().set_size(offset);
    mdi->updateACMtime();
    return put_metadata(mdi);

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
    return  pok_truncate(user_path, offset);
}

