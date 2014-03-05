/* Fsck command line utility. The functional implementation is done inside the file system itself. */
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


bool isPokMount(const char *path)
{
    std::vector<std::string> mountpoints;
    std::string pok("POSIX-o-K");
#ifdef __APPLE__
    struct statfs *buf = NULL;
    int count = getmntinfo(&buf, 0);
    for (int i=0; i<count; i++){
       if(!pok.compare(0, pok.length(), buf[i].f_mntfromname, pok.length())){
           mountpoints.push_back(buf[i].f_mntonname);
       }
    }
#else
    FILE * mtab = NULL;
    struct mntent * part = NULL;
    if ( ( mtab = setmntent ("/etc/mtab", "r") ) != NULL) {
        while ( ( part = getmntent ( mtab) ) != NULL) {
            if ( ( part->mnt_fsname != NULL ) && !pok.compare(0, pok.length(), part->mnt_fsname, pok.length())){
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


int main(int argc, char *argv[])
{
    /* A path must be supplied, the supplied path must refer to a directory in a POSIX-o-K mount*/
    if(argc<2) return failure("No path supplied.");
    if(argc>2) return failure("Too many arguments.");

    char buffer[PATH_MAX];
    char *path = realpath(argv[1], buffer);
    if(!path) return failure("Invalid path supplied.");
    if(!isPokMount(path)) return failure("Path not inside POSIX-o-K mount.");
    if(getuid()) return failure("fsck must be run as root.");

    struct stat s;
    if( stat(path, &s) ) return failure("Failed to call stat on path.");
    if(!S_ISDIR(s.st_mode)) return failure("Path does not refer to a directory.");

    if(setxattr(path, "fsck", "", 0, 0, XATTR_CREATE)) return failure("Failure during fsck.");
    return 0;
}
