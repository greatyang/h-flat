#ifndef LINUX_NAMES_H_
#define LINUX_NAMES_H_
/* linux > function names have to be the original names for interposing */

#define pc_fopen    fopen
#define pc_remove   remove
#define pc_rename   rename
#define pc_chmod    chmod
#define pc_lstat    lstat
#define pc_mkdir    mkdir
#define pc_mkfifo   mkfifo
#define pc_mknod    mknod
#define pc_stat     stat
#define pc_access   access
#define pc_chown    chown
#define pc_lchown   lchown
#define pc_link     link
#define pc_symlink  symlink
#define pc_readlink readlink
#define pc_unlink   unlink
#define pc_rmdir    rmdir
#define pc_truncate truncate
#define pc_chdir    chdir
#define pc_pathconf pathconf
#define pc_creat    creat
#define pc_open     open

#endif
