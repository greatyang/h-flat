#include "main.h"
#include "debug.h"
#include "kinetic_helper.h"
#include <unistd.h>
#include <vector>

static int readwrite(char *buf, size_t size, off_t offset, const std::shared_ptr<MetadataInfo> &mdi, const std::unique_ptr<std::vector<std::shared_ptr<DataInfo>>> &datakeys)
{
    /* data request might span multiple blocks */
    int blocksize = PRIV->blocksize;
    int sizeleft = size;
    int blocknum = offset / blocksize;

    while (sizeleft) {
        int inblockstart = offset > blocknum * blocksize ? (offset - blocknum * blocksize) : 0;
        int inblocksize = sizeleft > blocksize - inblockstart ? blocksize - inblockstart : sizeleft;

        std::shared_ptr<DataInfo> data;
        string key = std::to_string(mdi->getMD().inode_number()) + "_" + std::to_string(blocknum);
        if(! PRIV->data_cache.get(key, data)){
            if (int err = get_data(key, data))
             return err;
            if(! PRIV->data_cache.add(key, data))
                pok_debug("Failed to add key %s to data cache",key.c_str());
        }
        if (datakeys){
            pok_trace("update datainfo");
            data->updateData(buf, inblockstart, inblocksize);
            datakeys->push_back(data);
        }
        else{
            pok_trace("read from datainfo");
            memcpy(buf + size - sizeleft, data->data().data() + inblockstart, inblocksize);
        }

        sizeleft -= inblocksize;
        blocknum++;
    }
    return 0;
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

    pok_debug("Read request of %d bytes for user path %s at offset %d. ", size, user_path, offset);
    err = readwrite(buf, size, offset, mdi, nullptr);
    if(err) return err;
    return size;
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
    std::shared_ptr<MetadataInfo> mdi;
    int err = lookup(user_path, mdi);
    if( err) return err;

    pok_debug("writing %d bytes at offset %d for user path %s", size, offset, user_path);

    size_t newsize = std::max((std::uint64_t) offset + size, (std::uint64_t) mdi->getMD().size());
    mdi->getMD().set_size(newsize);
    mdi->getMD().set_blocks((newsize / PRIV->blocksize) + 1);
    mdi->updateACMtime();

    std::unique_ptr<std::vector<std::shared_ptr<DataInfo>>> datakeys(new std::vector<std::shared_ptr<DataInfo>>());
    err = readwrite(const_cast<char*>(buf), size, offset, mdi, datakeys);
    if( err) return err;

    /* Check if the write should be delayed for write aggregation:
     *       write targets only a single data block and doesn't fill it completely */
    if(datakeys->size() == 1 && ((offset+size) % PRIV->blocksize)){
       if(mdi->setAggregate(datakeys->at(0)))
           return size;
    }

    /* write metadata key */
    err = put_metadata(mdi);
    if(err == -EAGAIN) return pok_write(user_path, buf, size, offset, fi);
    if(err) return err;

    /* write data keys */
    for (size_t i=0; i<datakeys->size(); i++){
        if(!err) err = put_data(datakeys->at(i));
    }
    if(mdi->isDirty())
        if(!err) err = put_data(mdi->getAggregate());

    if(err)  pok_warning("Failed data put after successfully updating metadata");
    return size;
}

/** Change the size of a file */
int pok_truncate(const char *user_path, off_t offset)
{
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

