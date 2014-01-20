#include "main.h"
#include "debug.h"
#include "kinetic_helper.h"
#include <sys/xattr.h>
#ifndef ENOATTR
# define ENOATTR ENODATA        /* No such attribute */
#endif


/* xattr_flags:
 * XATTR_CREATE specifies a pure create, which fails if the named attribute exists already.
 * XATTR_REPLACE specifies a pure replace operation, which fails if the named attribute does not already exist.
 * By default (no flags), the extended attribute will be created if need be, or will simply replace the value if the attribute exists.
 */
int pok_setxattr (const char *user_path, const char *attr_name, const char *attr_value, size_t attr_size, int flags)
{
	pok_trace("Setting extended attribute for user path %s:  %s = %s",user_path,attr_name,attr_value);

	std::unique_ptr<MetadataInfo> mdi(new MetadataInfo());
	int err = lookup(user_path, mdi);
	if(err)
		return err;

	posixok::Metadata_ExtendedAttribute *xattr = nullptr;

	/* Search the existing xattrs for the supplied key */
	for(int i=0; i < mdi->pbuf()->xattr_size(); i++) {
		if(! mdi->pbuf()->xattr(i).name().compare(attr_name) ) {
			xattr = mdi->pbuf()->mutable_xattr(i);
			break;
		}
	}
	if( xattr && (flags & XATTR_CREATE))
		return -EEXIST;
	if(!xattr && (flags & XATTR_REPLACE))
		return -ENOATTR;
	if(!xattr)
		xattr = mdi->pbuf()->mutable_xattr()->Add();

	/* Set & store the supplied extended attribute. */
	xattr->set_name(attr_name);
	xattr->set_value(attr_value, attr_size);

	return put_metadata(mdi.get());
}

int pok_setxattr_apple (const char *user_path, const char *attr_name, const char *attr_value, size_t attr_size, int flags, uint32_t position){
	assert(position <= attr_size);
	return pok_setxattr(user_path, attr_name, attr_value + position, attr_size - position, flags);
}

/** Get extended attributes */
int pok_getxattr (const char *user_path, const char *attr_name, char *attr_value, size_t attr_size)
{
	std::unique_ptr<MetadataInfo> mdi(new MetadataInfo());
	int err = lookup(user_path, mdi);
	if(err)
		return err;

	posixok::Metadata_ExtendedAttribute *xattr = nullptr;

	/* Search the existing xattrs for the supplied key */
	for(int i=0; i < mdi->pbuf()->xattr_size(); i++) {
		if(! mdi->pbuf()->xattr(i).name().compare(attr_name) ) {
			xattr = mdi->pbuf()->mutable_xattr(i);
			break;
		}
	}
	if(!xattr)
		return -ENOATTR;

	/* An empty buffer of size zero can be passed into these calls to return the current size of the named extended attribute, which can be used to
	 * estimate the size of a buffer which is sufficiently large to hold the value associated with the extended attribute.
	 * The interface is designed to allow guessing of initial buffer sizes, and to enlarge buffers when the return value indicates that the buffer provided was too small. */
	if(!attr_size)
		return xattr->value().size() + 1;

	if(xattr->value().size() > attr_size){
		pok_debug("buffer size: %d, xattr size: %d",attr_size, xattr->value().size());
		return -ERANGE;
	}
	/* Copy the value of the attribute into the supplied buffer. */
	memcpy(attr_value, xattr->value().data(), xattr->value().size());
	return xattr->value().size();
}

int pok_getxattr_apple (const char *user_path, const char *attr_name, char *attr_value, size_t attr_size, uint32_t position)
{
	assert(position <= attr_size);
	return pok_getxattr(user_path, attr_name, attr_value + position, attr_size - position);
}



/** Remove extended attributes */
int pok_removexattr (const char *user_path, const char *attr_name)
{
	std::unique_ptr<MetadataInfo> mdi(new MetadataInfo());
	int err = lookup(user_path, mdi);
	if(err)
		return err;

	/* Search the existing xattrs for the supplied key */
	for(int i=0; i < mdi->pbuf()->xattr_size(); i++) {
		if(! mdi->pbuf()->xattr(i).name().compare(attr_name) ) {
			mdi->pbuf()->mutable_xattr()->DeleteSubrange(i,1);
			return put_metadata(mdi.get());
		}
	}
	return -ENOATTR;
}

/** List extended attributes */
int pok_listxattr (const char *user_path, char *buffer, size_t size)
{
	std::unique_ptr<MetadataInfo> mdi(new MetadataInfo());
	int err = lookup(user_path, mdi);
	if(err)
		return err;

	size_t bytesize = 0;

	/* Iterate over existing xattrs & copy the names into the supplied buffer */
	for(int i=0; i < mdi->pbuf()->xattr_size(); i++) {
		int namesize = mdi->pbuf()->xattr(i).name().size() + 1;
		bytesize += namesize;

		/* An empty buffer of size zero can be passed into these calls to return the current size of the list of extended attribute names,
		 * which can be used to estimate the size of a buffer which is sufficiently large to hold the list of names. */
		if(size){
			if(bytesize > size){
				pok_debug("buffer size: %d listsize: %d",size, bytesize);
				return -ERANGE;
			}
			strcpy(buffer, mdi->pbuf()->xattr(i).name().c_str());
			buffer += namesize;
		}
	}

	return (int)bytesize;
}
