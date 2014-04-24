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
#include <sys/xattr.h>
#ifndef ENOATTR
# define ENOATTR ENODATA        /* No such attribute */
#endif

extern int fsck_directory(const char*, const std::shared_ptr<MetadataInfo>&);

/* xattr_flags:
 * XATTR_CREATE specifies a pure create, which fails if the named attribute exists already.
 * XATTR_REPLACE specifies a pure replace operation, which fails if the named attribute does not already exist.
 * By default (no flags), the extended attribute will be created if need be, or will simply replace the value if the attribute exists.
 */
int hflat_setxattr(const char *user_path, const char *attr_name, const char *attr_value, size_t attr_size, int flags)
{
    hflat_trace("Setting extended attribute for user path %s:  %s = %s", user_path, attr_name, attr_value);
    std::shared_ptr<MetadataInfo> mdi;
    int err = lookup(user_path, mdi);
    if( err) return err;

    /* (Ab)using setxattr as a possibility to start a fsck on a specific directory */
    if(std::string("fsck").compare(attr_name) == 0)
        return fsck_directory(user_path, mdi);
    if(std::string("nsck").compare(attr_name) == 0)
        return PRIV->kinetic->selfCheck() ? 0 : -EHOSTDOWN;


    hflat::Metadata_ExtendedAttribute *xattr = nullptr;

    /* Search the existing xattrs for the supplied key */
    for (int i = 0; i < mdi->getMD().xattr_size(); i++) {
        if (!mdi->getMD().xattr(i).name().compare(attr_name)) {
            xattr = mdi->getMD().mutable_xattr(i);
            break;
        }
    }
    if (xattr && (flags & XATTR_CREATE))
        return -EEXIST;
    if (!xattr && (flags & XATTR_REPLACE))
        return -ENOATTR;
    if (!xattr)
        xattr = mdi->getMD().mutable_xattr()->Add();

    /* Set & store the supplied extended attribute. */
    xattr->set_name(attr_name);
    xattr->set_value(attr_value, attr_size);

    err = put_metadata(mdi);
    if(err == -EAGAIN) return hflat_setxattr(user_path, attr_name, attr_value, attr_size, flags);
    return err;
}

int hflat_setxattr_apple(const char *user_path, const char *attr_name, const char *attr_value, size_t attr_size, int flags, uint32_t position)
{
    assert(position <= attr_size);
    return hflat_setxattr(user_path, attr_name, attr_value + position, attr_size - position, flags);
}

/** Get extended attributes */
int hflat_getxattr(const char *user_path, const char *attr_name, char *attr_value, size_t attr_size)
{
    std::shared_ptr<MetadataInfo> mdi;
    int err = lookup(user_path, mdi);
    if( err) return err;

    hflat::Metadata_ExtendedAttribute *xattr = nullptr;

    /* Search the existing xattrs for the supplied key */
    for (int i = 0; i < mdi->getMD().xattr_size(); i++) {
        if (!mdi->getMD().xattr(i).name().compare(attr_name)) {
            xattr = mdi->getMD().mutable_xattr(i);
            break;
        }
    }
    if (!xattr)
        return -ENOATTR;

    /* An empty buffer of size zero can be passed into these calls to return the current size of the named extended attribute, which can be used to
     * estimate the size of a buffer which is sufficiently large to hold the value associated with the extended attribute.
     * The interface is designed to allow guessing of initial buffer sizes, and to enlarge buffers when the return value indicates that the buffer provided was too small. */
    if (!attr_size)
        return xattr->value().size() + 1;

    if (xattr->value().size() > attr_size) {
        hflat_debug("buffer size: %d, xattr size: %d", attr_size, xattr->value().size());
        return -ERANGE;
    }
    /* Copy the value of the attribute into the supplied buffer. */
    memcpy(attr_value, xattr->value().data(), xattr->value().size());
    return xattr->value().size();
}

int hflat_getxattr_apple(const char *user_path, const char *attr_name, char *attr_value, size_t attr_size, uint32_t position)
{
    assert(position <= attr_size);
    return hflat_getxattr(user_path, attr_name, attr_value + position, attr_size - position);
}

/** Remove extended attributes */
int hflat_removexattr(const char *user_path, const char *attr_name)
{
    std::shared_ptr<MetadataInfo> mdi;
    int err = lookup(user_path, mdi);
    if( err) return err;


    /* Search the existing xattrs for the supplied key */
    for (int i = 0; i < mdi->getMD().xattr_size(); i++) {
        if (!mdi->getMD().xattr(i).name().compare(attr_name)) {
            mdi->getMD().mutable_xattr()->DeleteSubrange(i, 1);
            err = put_metadata(mdi);
            if(err == -EAGAIN) return hflat_removexattr(user_path, attr_name);
            return err;
        }
    }
    return -ENOATTR;
}

/** List extended attributes */
int hflat_listxattr(const char *user_path, char *buffer, size_t size)
{
    std::shared_ptr<MetadataInfo> mdi;
    int err = lookup(user_path, mdi);
    if( err) return err;

    size_t bytesize = 0;

    /* Iterate over existing xattrs & copy the names into the supplied buffer */
    for (int i = 0; i < mdi->getMD().xattr_size(); i++) {
        int namesize = mdi->getMD().xattr(i).name().size() + 1;
        bytesize += namesize;

        /* An empty buffer of size zero can be passed into these calls to return the current size of the list of extended attribute names,
         * which can be used to estimate the size of a buffer which is sufficiently large to hold the list of names. */
        if (size) {
            if (bytesize > size) {
                hflat_debug("buffer size: %d listsize: %d", size, bytesize);
                return -ERANGE;
            }
            strcpy(buffer, mdi->getMD().xattr(i).name().c_str());
            buffer += namesize;
        }
    }

    return (int) bytesize;
}
