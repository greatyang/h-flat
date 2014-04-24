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
#include "kinetic_helper.h"
#include "fuseops.h"
#include <algorithm>

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
    if(mode == rw::READ){
        /* After a truncate operation that increases size a client may legally read data that was never written.
         * This data should be set to 0. */
        memset(buf, 0, size);
        int copysize = std::min( (int)inblocksize, (int)di->data().size() - inblockstart );
        if( copysize > 0)
            di->data().copy(buf, copysize, inblockstart);
    }
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
int hflat_read(const char* user_path, char *buf, size_t size, off_t offset, struct fuse_file_info* fi)
{
    hflat_debug("reading %d bytes at offset %d for user path %s", size, offset, user_path);

    std::shared_ptr<MetadataInfo> mdi;
    int err = lookup(user_path, mdi);
    if( err) return err;

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
int hflat_write(const char* user_path, const char *buf, size_t size, off_t offset, struct fuse_file_info* fi)
{
    hflat_debug("writing %d bytes at offset %d for user path %s, O_APPEND: %d", size, offset, user_path, fi->flags & O_APPEND);

    std::shared_ptr<MetadataInfo> mdi;
    int err = lookup(user_path, mdi);
    if( err) return err;

    std::shared_ptr<DataInfo> di;
    size = do_rw(const_cast<char*>(buf), size, offset, mdi, di, rw::WRITE);
    if( size <= 0 ) return size;

    /* set updated datainfo structure in mdi, flush existing dirty data if required */
    if(! mdi->setDirtyData(di) ){
        err = hflat_fsync(user_path, 0, fi);
        if(err){
            hflat_warning("Failed flushing dirty metadata&data kept for write aggregation.");
            return err;
        }
        mdi->setDirtyData(di);
    }

    /* check if the write should be immediately flushed or aggregated */
    if( (fi->flags & O_APPEND) || ((offset+size) % PRIV->blocksize == 0) ){
        err = hflat_fsync(user_path,0,fi);
        if(err == -EAGAIN) return hflat_write(user_path, buf, size, offset, fi);
        if(err) return err;
    }

    return size;
}

/** Change the size of a file */
int hflat_truncate(const char *user_path, off_t offset)
{
    hflat_debug("truncate for user path %s to size %ld",user_path,offset);
    std::shared_ptr<MetadataInfo> mdi;
    int err = lookup(user_path, mdi);
    if(!err) err = hflat_fsync(user_path, 0, nullptr);
    if( err) return err;

    if (check_access(mdi, W_OK))
       return -EACCES;

    if (offset > std::numeric_limits<std::uint32_t>::max())
       return -EFBIG;

    off_t size = mdi->getMD().size();

    /* update metadata */
    mdi->getMD().set_size(offset);
    mdi->updateACMtime();
    err = put_metadata(mdi);
    if(err == -EAGAIN) return hflat_truncate(user_path, offset);
    if(err) return err;

    /* truncate last valid data block */
    std::shared_ptr<DataInfo> di;
    if(offset < size){
        std::string key = std::to_string(mdi->getMD().inode_number()) + "_" + std::to_string(offset/PRIV->blocksize);
        if(PRIV->data_cache.get(key, di) == false){
            if (int err = get_data(key, di)) return err;
            PRIV->data_cache.add(key, di);
        }
        di->truncate(offset % PRIV->blocksize);
        put_data(di);
    }

    /* delete all data blocks existing past the specified offset */
    for(int i=offset/PRIV->blocksize+1; i<=size/PRIV->blocksize; i++){
        std::string key = std::to_string(mdi->getMD().inode_number()) + "_" + std::to_string(i);
        if(PRIV->data_cache.get(key, di))  PRIV->data_cache.invalidate(key);
        else di.reset(new DataInfo(key, std::string(""), std::string("")));
        delete_data(di);
    }
    return 0;
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
int hflat_ftruncate(const char *user_path, off_t offset, struct fuse_file_info *fi)
{
    return  hflat_truncate(user_path, offset);
}

