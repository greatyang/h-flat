#include "main.h"
#include "debug.h"
#include <algorithm>


static int readwrite (char *buf, size_t size, off_t offset, MetadataInfo * mdi, bool write)
{
	/* We might have to split the operation across multiple blocks */
	const int blocksize	= 1024 * 1024;
	int sizeleft 		= size;
	int blocknum    	= offset / blocksize;

	while(sizeleft){
		int inblockstart 	=  offset > blocknum * blocksize ? (offset - blocknum * blocksize ) : 0;
		int	inblocksize		=  sizeleft > blocksize-inblockstart ? blocksize-inblockstart : sizeleft;

		std::string value;
		NamespaceStatus getD = PRIV->nspace->get(mdi, blocknum, &value);

		/* Should never happen, user shouldn't have been able to open the file. */
		if(getD.notAuthorized() || getD.notValid()){
			pok_error("Failure reading data block %s for key = %s due to %s",blocknum, mdi->getSystemPath().c_str(), getD.ToString().c_str());
			return -EINVAL;
		}

		/* Perfectly fine if file has holes, just fill buffer with zeros in this case. */
		if(getD.notFound()){
			value.resize(blocksize);
			value.replace(0,blocksize,blocksize,'0');
		}

		if(write){
			value.replace(inblockstart, inblocksize, buf);
			PRIV->nspace->put(mdi, blocknum, value);
		}
		else{
			memcpy(buf+size-sizeleft, value.data() + inblockstart, inblocksize);
		}

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
int pok_read (const char* user_path, char *buf, size_t size, off_t offset, struct fuse_file_info* fi)
{
	MetadataInfo * mdi = reinterpret_cast<MetadataInfo *>(fi->fh);
	if(!mdi){
		pok_error("Read request for user path '%s' without metadata_info structure", user_path);
		return -EINVAL;
	}
	if(offset+size > mdi->pbuf()->size()){
		pok_trace("Attempting to read beyond EOF for user path %s",user_path);
		return 0;
	}
	pok_trace("reading from user path %s",user_path);
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
	MetadataInfo * mdi = reinterpret_cast<MetadataInfo *>(fi->fh);
	if(!mdi){
		pok_error("Write request for user path '%s' without metadata_info structure", user_path);
		return -EINVAL;
	}
	pok_trace("writing %d bytes at offset %d to user path %s",size, offset, user_path);
	int rtn = readwrite(const_cast<char*>(buf), size, offset, mdi, true);

	/* update metadata on success */
	if(rtn>0){
		size_t newsize = std::max(offset+size,(std::uint64_t)mdi->pbuf()->size());
		mdi->pbuf()->set_size(newsize);
		mdi->pbuf()->set_blocks(newsize / 1024*1024 + 1);
		mdi->updateACMtime();

		NamespaceStatus status = PRIV->nspace->putMD(mdi);
		if(status.notOk()){
			pok_warning("Failed putting metadata key '%s' due to %s",mdi->getSystemPath().c_str(), status.ToString().c_str());
			return -EINVAL;
		}
	}
	return rtn;
}
