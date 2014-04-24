/* direct lookup simulation via io interception
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
#include <dlfcn.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <utime.h>
#include <sys/statvfs.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/time.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/xattr.h>
#include <string>
#include "iointercept.h"

extern std::string change(const char *);


int pc_utimes (const char *path, const struct timeval times[2]){
    return (ORIG(utimes))
                (change(path).c_str(), times);
}
int pc_utime (const char *path, const struct utimbuf *buf){
    return (ORIG(utime))
                (change(path).c_str(), buf);
}
int pc_statfs (const char *path, struct statfs *buf){
    return (ORIG(statfs))
                (change(path).c_str(), buf);
}
int pc_statvfs (const char *path, struct statvfs *buf){
    return (ORIG(statvfs))
                (change(path).c_str(), buf);
}
FILE* pc_fopen (const char *path, const char *mode){
    return (ORIG(fopen))
                (change(path).c_str(), mode);
}
char * pc_realpath (const char *path, char *resolved){
    return (ORIG(realpath))
                (change(path).c_str(), resolved);
}
DIR * pc_opendir (const char *path){
    return (ORIG(opendir))
                (change(path).c_str());
}
int pc_remove(const char *path){
    return (ORIG(remove))
                (change(path).c_str());
}
int pc_rename(const char *p1, const char *p2){
    return (ORIG(rename))
                (change(p1).c_str(), change(p2).c_str());
}
int pc_chmod(const char *path, mode_t mode){
    return (ORIG(chmod))
                (change(path).c_str(), mode);
}
int pc_lstat(const char *path, struct stat *st){
    return (ORIG(lstat))
                (change(path).c_str(), st);
}
int pc_mkdir(const char *path, mode_t mode){
    return (ORIG(mkdir))
                (change(path).c_str(), mode);
}
int pc_mkfifo(const char *path, mode_t mode){
    return (ORIG(mkfifo))
                (change(path).c_str(), mode);
}
int pc_mknod(const char *path, mode_t mode, dev_t dev){
    return (ORIG(mknod))
                (change(path).c_str(), mode, dev);
}
int pc_stat(const char *path, struct stat *st){
    return (ORIG(stat))
                    (change(path).c_str(), st);
}
int pc_access(const char *path, int i){
    return (ORIG(access))
                (change(path).c_str(), i);
}
int pc_chown(const char *path, uid_t uid, gid_t gid){
    return (ORIG(chown))
                (change(path).c_str(), uid, gid);
}
int pc_lchown(const char *path, uid_t uid, gid_t gid){
    return (ORIG(lchown))
                (change(path).c_str(), uid, gid);
}
int pc_link(const char *p1, const char *p2){
    return (ORIG(link))
                (change(p1).c_str(), change(p2).c_str());
}
int pc_symlink(const char *p1, const char *p2){
    return (ORIG(symlink))
                (change(p1).c_str(), change(p2).c_str());
}
ssize_t pc_readlink(const char *path, char *buf, size_t size){
    return (ORIG(readlink))
                (change(path).c_str(), buf, size);
}
int pc_unlink(const char *path){
    return (ORIG(unlink))
                (change(path).c_str());
}
int pc_rmdir(const char *path){
    return (ORIG(rmdir))
                (change(path).c_str());
}
int pc_truncate(const char *path, off_t offset){
    return (ORIG(truncate))
                (change(path).c_str(), offset);
}
int pc_chdir(const char *path){
    return (ORIG(chdir))
               (change(path).c_str());
}
long int pc_pathconf(const char *path, int i){
    return (ORIG(pathconf))
               (change(path).c_str(), i);
}
int pc_creat(const char *path, mode_t mode){
    return (ORIG(creat))
                (change(path).c_str(), mode);
}

#ifdef __APPLE__
ssize_t pc_getxattr (const char *path, const char *name, void *value, size_t size, u_int32_t position, int options){
    return (ORIG(getxattr))
                (change(path).c_str(), name, value, size, position, options);
}
int pc_setxattr(const char *path, const char *name, void *value, size_t size, u_int32_t position, int options){
    return (ORIG(setxattr))
                (change(path).c_str(), name, value, size, position, options);
}
int pc_open(const char *path, int flags, ...){
    if(flags & O_CREAT){
        va_list vl;
        va_start(vl,flags);
        mode_t mode = va_arg(vl,int);
        va_end(vl);
        return (ORIG(open))(change(path).c_str(), flags, mode);
    }
    return (ORIG(open))(change(path).c_str(), flags);
}
DYLD_INTERPOSE(pc_access,   access);
DYLD_INTERPOSE(pc_chdir,    chdir);
DYLD_INTERPOSE(pc_chmod,    chmod);
DYLD_INTERPOSE(pc_chown,    chown);
DYLD_INTERPOSE(pc_creat,    creat);
DYLD_INTERPOSE(pc_fopen,    fopen);
DYLD_INTERPOSE(pc_getxattr, getxattr);
DYLD_INTERPOSE(pc_lchown,   lchown);
DYLD_INTERPOSE(pc_link,     link);
DYLD_INTERPOSE(pc_lstat,    lstat);
DYLD_INTERPOSE(pc_mkdir,    mkdir);
DYLD_INTERPOSE(pc_mkfifo,   mkfifo);
DYLD_INTERPOSE(pc_mknod,    mknod);
DYLD_INTERPOSE(pc_open,     open);
DYLD_INTERPOSE(pc_opendir,  opendir);
DYLD_INTERPOSE(pc_pathconf, pathconf);
DYLD_INTERPOSE(pc_readlink, readlink);
DYLD_INTERPOSE(pc_realpath, realpath);
DYLD_INTERPOSE(pc_remove,   remove);
DYLD_INTERPOSE(pc_rename,   rename);
DYLD_INTERPOSE(pc_rmdir,    rmdir);
DYLD_INTERPOSE(pc_setxattr, setxattr);
DYLD_INTERPOSE(pc_stat,     stat);
DYLD_INTERPOSE(pc_statfs,   statfs);
DYLD_INTERPOSE(pc_statvfs,  statvfs);
DYLD_INTERPOSE(pc_symlink,  symlink);
DYLD_INTERPOSE(pc_truncate, truncate);
DYLD_INTERPOSE(pc_unlink,   unlink);
DYLD_INTERPOSE(pc_utime,    utime);
DYLD_INTERPOSE(pc_utimes,   utimes);


#else
ssize_t pc_getxattr (const char *path, const char *name, void *value, size_t size){
    return (ORIG(getxattr))
                (change(path).c_str(), name, value, size);
}

int pc_setxattr(const char *path, const char *name, const void *value, size_t size, int flags){
    return (ORIG(setxattr))
                (change(path).c_str(), name, value, size, flags);
}
int pc_open(const char *path, int flags, ...){
    if(flags & O_CREAT){
        va_list vl;
        va_start(vl,flags);
        mode_t mode = va_arg(vl,int);
        va_end(vl);
        typedef int (*fp)(const char*, int, mode_t);
        return (reinterpret_cast<fp>(dlsym(RTLD_NEXT, "open")))
                (change(path).c_str(), flags, mode);
    }
    typedef int (*fp)(const char*, int);
    return (reinterpret_cast<fp>(dlsym(RTLD_NEXT, "open")))
            (change(path).c_str(), flags);
}
/* functions below have no OSX equivalent, no pc_ required*/
int open64(const char *path, int flags, ...){
    if(flags & O_CREAT){
        va_list vl;
        va_start(vl,flags);
        mode_t mode = va_arg(vl,int);
        va_end(vl);
        typedef int (*fp)(const char*, int, mode_t);
        return (reinterpret_cast<fp>(dlsym(RTLD_NEXT, "open64")))
                (change(path).c_str(), flags, mode);
    }
    typedef int (*fp)(const char*, int);
    return (reinterpret_cast<fp>(dlsym(RTLD_NEXT, "open64")))
            (change(path).c_str(), flags);
}
int openat(int dirfd, const char *path, int flags, ...){
    if(flags & O_CREAT){
        va_list vl;
        va_start(vl,flags);
        mode_t mode = va_arg(vl,int);
        va_end(vl);
        typedef int (*fp)(int, const char*, int, mode_t);
        return (reinterpret_cast<fp>(dlsym(RTLD_NEXT, "openat")))
                        (dirfd, change(path).c_str(), flags, mode);
    }
    typedef int (*fp)(int, const char*, int);
    return (reinterpret_cast<fp>(dlsym(RTLD_NEXT, "openat")))
                    (dirfd, change(path).c_str(), flags);
}
int statfs64 (const char *path, struct statfs64 *buf){
    return (ORIG(statfs64))
                (change(path).c_str(), buf);
}
int stat64 (const char *path, struct stat64 *buf){
    return (ORIG(stat64))
                (change(path).c_str(), buf);
}
int lstat64 (const char *path, struct stat64 *buf){
    return (ORIG(lstat64))
                (change(path).c_str(), buf);
}
int statvfs64 (const char *path, struct statvfs64 *buf){
    return (ORIG(statvfs64))
                (change(path).c_str(), buf);
}
int creat64 (const char *path, mode_t mode){
    return (ORIG(creat64))
                (change(path).c_str(), mode);
}
int __xmknod (int ver, const char *path, mode_t mode, dev_t *dev){
    return (ORIG(__xmknod))
                (ver, change(path).c_str(), mode, dev);
}
int __xstat (int ver, const char *path, struct stat *buf){
    return (ORIG(__xstat))
                (ver, change(path).c_str(), buf);
}
int __xstat64 (int ver, const char *path, struct stat64 *buf){
    return (ORIG(__xstat64))
                (ver, change(path).c_str(), buf);
}
int __lxstat (int ver, const char *path, struct stat *buf){
    return (ORIG(__lxstat))
                (ver, change(path).c_str(), buf);
}
int __lxstat64 (int ver, const char *path, struct stat64 *buf){
    return (ORIG(__lxstat64))
                (ver, change(path).c_str(), buf);
}
int lsetxattr(const char *path, const char *name, const void *value, size_t size, int flags){
    return (ORIG(lsetxattr))
                (change(path).c_str(), name, value, size, flags);
}
ssize_t lgetxattr (const char *path, const char *name, void *value, size_t size){
    return (ORIG(lgetxattr))
                (change(path).c_str(), name, value, size);
}
int unlinkat(int dirfd, const char *path, int flags){
    return (ORIG(unlinkat))
                (dirfd, change(path).c_str(), flags);
}
int linkat(int olddirfd, const char *oldpath, int newdirfd, const char *newpath, int flags){
    return (ORIG(linkat))
                (olddirfd, change(oldpath).c_str(), newdirfd, change(newpath).c_str(), flags);
}
int mkdirat(int dirfd, const char *path, mode_t mode){
    return (ORIG(mkdirat))
                (dirfd, change(path).c_str(), mode);
}
int mknodat(int dirfd, const char *path, mode_t mode, dev_t dev){
    return (ORIG(mknodat))
                (dirfd, change(path).c_str(), mode, dev);
}
ssize_t readlinkat(int dirfd, const char *path, char *buf, size_t bufsiz){
    return (ORIG(readlinkat))
                (dirfd, change(path).c_str(), buf, bufsiz);
}
int renameat(int olddirfd, const char *oldpath, int newdirfd, const char *newpath){
    return (ORIG(renameat))
                    (olddirfd, change(oldpath).c_str(), newdirfd, change(newpath).c_str());
}
int symlinkat(const char *oldpath, int newdirfd, const char *newpath){
    return (ORIG(symlinkat))
                       (change(oldpath).c_str(), newdirfd, change(newpath).c_str());
}
int utimensat(int dirfd, const char *path, const struct timespec times[2], int flags){
    return (ORIG(utimensat))
                (dirfd, change(path).c_str(), times, flags);
}
int mkfifoat(int dirfd, const char *path, mode_t mode){
    return (ORIG(mkfifoat))
                (dirfd, change(path).c_str(), mode);
}


#endif
