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
#ifndef hflat_FUSUOPS_H_
#define hflat_FUSUOPS_H_

/* directory */
int hflat_readdir(const char *user_path, void *buffer, fuse_fill_dir_t filldir, off_t offset, struct fuse_file_info *fi);
int hflat_mkdir(const char *user_path, mode_t mode);
int hflat_rmdir(const char *user_path);

/* permission */
int hflat_access(const char *user_path, int mode);
int hflat_chmod(const char *user_path, mode_t mode);
int hflat_chown(const char *user_path, uid_t uid, gid_t gid);

/* attr */
int hflat_fgetattr(const char *user_path, struct stat *attr, struct fuse_file_info *fi);
int hflat_getattr(const char *user_path, struct stat *attr);
int hflat_utimens(const char *user_path, const struct timespec tv[2]);
int hflat_statfs(const char *user_path, struct statvfs *s);

/* xattr */
int hflat_setxattr(const char *user_path, const char *attr_name, const char *attr_value, size_t attr_size, int flags);
int hflat_setxattr_apple(const char *user_path, const char *attr_name, const char *attr_value, size_t attr_size, int flags, uint32_t position);
int hflat_getxattr(const char *user_path, const char *attr_name, char *attr_value, size_t attr_size);
int hflat_getxattr_apple(const char *user_path, const char *attr_name, char *attr_value, size_t attr_size, uint32_t position);
int hflat_removexattr(const char *user_path, const char *attr_name);
int hflat_listxattr(const char *user_path, char *buffer, size_t size);

/* file */
int hflat_create(const char *user_path, mode_t mode);
int hflat_fcreate(const char *user_path, mode_t mode, struct fuse_file_info *fi);
int hflat_unlink(const char *user_path);
int hflat_open(const char *user_path, struct fuse_file_info *fi);
int hflat_release(const char *user_path, struct fuse_file_info *fi);
int hflat_mknod(const char* user_path, mode_t mode, dev_t rdev);

/* data */
int hflat_read(const char* user_path, char *buf, size_t size, off_t offset, struct fuse_file_info* fi);
int hflat_write(const char* user_path, const char *buf, size_t size, off_t offset, struct fuse_file_info* fi);
int hflat_ftruncate(const char *user_path, off_t offset, struct fuse_file_info *fi);
int hflat_truncate(const char *user_path, off_t offset);

/* link */
int hflat_symlink(const char *target, const char *origin);
int hflat_hardlink(const char *target, const char *origin);
int hflat_readlink(const char *user_path, char *buffer, size_t size);

/* rename */
int hflat_rename(const char *user_path_from, const char *user_path_to);

/* sync */
int hflat_fsync(const char *user_path, int datasync, struct fuse_file_info *fi);
int hflat_fsyncdir(const char *user_path, int datasync, struct fuse_file_info *fi);
int hflat_flush(const char *user_path, struct fuse_file_info *fi);

/* main */
void hflat_destroy(void *priv);
#endif
