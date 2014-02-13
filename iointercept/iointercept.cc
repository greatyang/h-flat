#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include <stdarg.h>
#include <string>

extern std::string change(const char *);

#ifdef __APPLE__
#define ORIG(name) &name
#else
#include "linuxnames.h"
#define ORIG(name) reinterpret_cast<decltype(&name)>(dlsym(RTLD_NEXT, #name))
#endif

// stdio
FILE* pc_fopen (const char *path, const char *mode){
    return (ORIG(fopen))
                (change(path).c_str(), mode);
}
int pc_remove(const char *path){
    return (ORIG(remove))
                (change(path).c_str());
}
int pc_rename(const char *p1, const char *p2){
    return (ORIG(rename))
                (change(p1).c_str(), change(p2).c_str());
}
// sys/stat
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
// unistd.h
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

// fcntl.h
int pc_creat(const char *path, mode_t mode){
    return (ORIG(creat))
                (change(path).c_str(), mode);
}
int pc_open(const char *path, int flags, ...){
    if(flags & O_CREAT){
        va_list vl;
        va_start(vl,flags);
        mode_t mode = va_arg(vl,int);
        va_end(vl);

#ifdef __APPLE__
        return open(change(path).c_str(), flags, mode);
#else
        typedef int (*fp)(const char*, int, mode_t);
        return (reinterpret_cast<fp>(dlsym(RTLD_NEXT, "open")))
                (change(path).c_str(), flags, mode);
#endif
    }

#ifdef __APPLE__
        return open(change(path).c_str(), flags);
#else
        typedef int (*fp)(const char*, int);
        return (reinterpret_cast<fp>(dlsym(RTLD_NEXT, "open")))
                (change(path).c_str(), flags);
#endif
}

#ifdef __APPLE__
#include "applenames.h"
#endif
