/* file system tools
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
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/xattr.h>
#include <unistd.h>
#include <sys/mount.h>
#include <sys/param.h>
#ifdef __linux__
#include <mntent.h>
#endif

#include <string>
#include <vector>
#include <iostream>

int failure(std::string reason)
{
    std::cout << "Execution failed: " << reason << std::endl;
    return EXIT_FAILURE;
}


bool isHFMount(const char *path)
{
    std::vector<std::string> mountpoints;
    std::string hf("hflat");
#ifdef __APPLE__
    struct statfs *buf = NULL;
    int count = getmntinfo(&buf, 0);
    for (int i=0; i<count; i++){
       if(!hf.compare(0, hf.length(), buf[i].f_mntfromname, hf.length())){
           mountpoints.push_back(buf[i].f_mntonname);
       }
    }
#else
    FILE * mtab = NULL;
    struct mntent * part = NULL;
    if ( ( mtab = setmntent ("/etc/mtab", "r") ) != NULL) {
        while ( ( part = getmntent ( mtab) ) != NULL) {
            if ( ( part->mnt_fsname != NULL ) && !hf.compare(0, hf.length(), part->mnt_fsname, hf.length())){
                mountpoints.push_back(part->mnt_dir);
            }
        }
    }
    endmntent ( mtab);
#endif

    for(auto mnt : mountpoints){
      if(! mnt.compare(0, mnt.length(), path, mnt.length()))
          return true;
    }
    return false;
}

/* file system check */
int fsck(char *path)
{
    struct stat s;
    if( stat(path, &s) ) return failure("Failed to call stat on path.");
    if(!S_ISDIR(s.st_mode)) return failure("Path does not refer to a directory.");


    int err = 0;
    #ifdef __APPLE__
        err = setxattr(path, "fsck", "no value", 0, 0, XATTR_CREATE);
    #else
        err = setxattr(path, "fsck", "no value", 0, XATTR_CREATE);
    #endif
    if(err) return failure("Failure during fsck.");
    return 0;

}

/* name space check */
int nsck(char *path){
    int err = 0;
    #ifdef __APPLE__
        err = setxattr(path, "nsck", "no value", 0, 0, XATTR_CREATE);
    #else
        err = setxattr(path, "nsck", "no value", 0, XATTR_CREATE);
    #endif
    if(err) return failure("Invalid Namespace.");
    return 0;
}


int main(int argc, char *argv[])
{
    if(argc!=3 || (std::string(argv[1]).compare("-fsck") && std::string(argv[1]).compare("-nsck")) ){
        std::cout << "Usage: " << argv[0] << " [-fsck][-nsck] /path" << std::endl;
        std::cout << "\t -fsck -> file system check limited to directory identified by the path" << std::endl;
        std::cout << "\t -nsck -> namespace check on the file system identiefied by the path" << std::endl;
        return failure("Invalid arguments");
    }

    char buffer[PATH_MAX];
    char *path = realpath(argv[2], buffer);
    if(!path) return failure("Invalid path supplied.");
    if(!isHFMount(path)) return failure("Path not inside H-Flat mount.");
    if(getuid()) return failure("Not root.");

    if(std::string(argv[1]).compare("-fsck") == 0)
        return fsck(path);
    if(std::string(argv[1]).compare("-nsck") == 0)
        return nsck(path);

    return failure("Invalid.");
}
