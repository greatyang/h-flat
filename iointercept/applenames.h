#ifndef APPLE_NAMES_H_
#define APPLE_NAMES_H_
/* apple > use DYLD_INTERPOSE macro for interposing */

#define DYLD_INTERPOSE(_replacment,_replacee) \
   __attribute__((used)) static struct{ const void* replacment; const void* replacee; } _interpose_##_replacee \
            __attribute__ ((section ("__DATA,__interpose"))) = { (const void*)(unsigned long)&_replacment, (const void*)(unsigned long)&_replacee };

DYLD_INTERPOSE(pc_fopen,    fopen);
DYLD_INTERPOSE(pc_remove,   remove);
DYLD_INTERPOSE(pc_rename,   rename);
DYLD_INTERPOSE(pc_chmod,    chmod);
DYLD_INTERPOSE(pc_lstat,    lstat);
DYLD_INTERPOSE(pc_mkdir,    mkdir);
DYLD_INTERPOSE(pc_mkfifo,   mkfifo);
DYLD_INTERPOSE(pc_mknod,    mknod);
DYLD_INTERPOSE(pc_stat,     stat);
DYLD_INTERPOSE(pc_access,   access);
DYLD_INTERPOSE(pc_chown,    chown);
DYLD_INTERPOSE(pc_lchown,   lchown);
DYLD_INTERPOSE(pc_link,     link);
DYLD_INTERPOSE(pc_symlink,  symlink);
DYLD_INTERPOSE(pc_readlink, readlink);
DYLD_INTERPOSE(pc_unlink,   unlink);
DYLD_INTERPOSE(pc_rmdir,    rmdir);
DYLD_INTERPOSE(pc_truncate, truncate);
DYLD_INTERPOSE(pc_chdir,    chdir);
DYLD_INTERPOSE(pc_pathconf, pathconf);
DYLD_INTERPOSE(pc_creat,    creat);
DYLD_INTERPOSE(pc_open,     open);

#endif
