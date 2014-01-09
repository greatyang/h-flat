#ifndef POK_FUSUOPS_H_
#define POK_FUSUOPS_H_

/* directory */
int pok_readdir (const char *user_path, void *, fuse_fill_dir_t, off_t, struct fuse_file_info *fi);
int pok_mkdir 	(const char *user_path, mode_t mode);

/* permission */
int pok_access  (const char *user_path, int mode);
int pok_chmod 	(const char *user_path, mode_t mode);
int pok_chown 	(const char *user_path, uid_t uid, gid_t gid);

/* attr */
int pok_fgetattr(const char *user_path, struct stat *attr, struct fuse_file_info *fi);
int pok_getattr (const char *user_path, struct stat *attr);
int pok_utimens	(const char *user_path, const struct timespec tv[2]);
int pok_statfs  (const char *user_path, struct statvfs *s);

/* xattr */
int pok_setxattr (const char *user_path, const char *attr_name, const char *attr_value, size_t attr_size, int flags);
int pok_setxattr_apple (const char *user_path, const char *attr_name, const char *attr_value, size_t attr_size, int flags, uint32_t position);
int pok_getxattr (const char *user_path, const char *attr_name, char *attr_value, size_t attr_size);
int pok_getxattr_apple (const char *user_path, const char *attr_name, char *attr_value, size_t attr_size, uint32_t position);
int pok_removexattr (const char *user_path, const char *attr_name);int pok_listxattr (const char *user_path, char *buffer, size_t size);

/* file */
int pok_create	(const char *user_path, mode_t mode, struct fuse_file_info *fi);
int pok_unlink	(const char *user_path);
int pok_open	(const char *user_path, struct fuse_file_info *fi);
int pok_release (const char *user_path, struct fuse_file_info *fi);

/* data */
int pok_read 	(const char* user_path, char *buf, size_t size, off_t offset, struct fuse_file_info* fi);
int pok_write	(const char* user_path, const char *buf, size_t size, off_t offset, struct fuse_file_info* fi);
int pok_ftruncate (const char *user_path, off_t offset, struct fuse_file_info *fi);
int pok_truncate  (const char *user_path, off_t offset);

/* link */
int pok_symlink (const char *link_destination, const char *user_path);
int pok_readlink (const char *user_path, char *buffer, size_t size);

/* rename */
int pok_rename (const char *user_path_from, const char *user_path_to);

/* sync */
int pok_fsync	(const char *user_path, int datasync, struct fuse_file_info *fi);
int pok_fsyncdir(const char *user_path, int datasync, struct fuse_file_info *fi);
int pok_flush 	(const char *user_path, struct fuse_file_info *fi);

#endif
