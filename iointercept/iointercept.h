#ifndef IOINTERCEPT_H
#define IOINTERCEPT_H

#ifdef __APPLE__
#define ORIG(name) &name
#define DYLD_INTERPOSE(_replacment,_replacee) \
   __attribute__((used)) static struct{ const void* replacment; const void* replacee; } _interpose_##_replacee \
            __attribute__ ((section ("__DATA,__interpose"))) = { (const void*)(unsigned long)&_replacment, (const void*)(unsigned long)&_replacee };
/* apple > use DYLD_INTERPOSE macro for interposing */

#else
#include <sys/statfs.h>
#define ORIG(name) reinterpret_cast<decltype(&name)>(dlsym(RTLD_NEXT, #name))
/* linux > function names have to be the original names for interposing */

#define pc_access       access
#define pc_chdir        chdir
#define pc_chmod        chmod
#define pc_chown        chown
#define pc_creat        creat
#define pc_fopen        fopen
#define pc_getxattr     getxattr
#define pc_lchown       lchown
#define pc_link         link
#define pc_lstat        lstat
#define pc_mkdir        mkdir
#define pc_mkfifo       mkfifo
#define pc_mknod        mknod
#define pc_open         open
#define pc_opendir      opendir
#define pc_pathconf     pathconf
#define pc_readlink     readlink
#define pc_realpath     realpath
#define pc_remove       remove
#define pc_rename       rename
#define pc_rmdir        rmdir
#define pc_setxattr     setxattr
#define pc_stat         stat
#define pc_statfs       statfs
#define pc_statvfs      statvfs
#define pc_symlink      symlink
#define pc_truncate     truncate
#define pc_unlink       unlink
#define pc_utime        utime
#define pc_utimes       utimes
#endif

#endif
