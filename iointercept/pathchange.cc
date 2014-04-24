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
#include <string>
#include <vector>
#include <sys/mount.h>
#include <unistd.h>
#include <sys/param.h>

#ifdef __linux__
#include <mntent.h>
#endif

static std::vector<std::string> mountpoints;

void getMountPoints(void)
{
    if(mountpoints.size())
        return;
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
}



/* opposed to the 'real' realpath method, there's no file system access here...
 * we build the absolute path as well as we can with the in-memory information only. */
void _realpath(std::string &path)
{
    if(path[0] != '/'){
        char buf[PATH_MAX];
        getcwd(buf, PATH_MAX);
        path.insert(0, buf + std::string("/"));
    }

    size_t pos = path.find_first_of('/',0);
    while(pos != std::string::npos && pos < path.length()-1){
        /* erase multiple '/' */
        if(path[pos+1] == '/')
            path.erase(pos, path.find_first_not_of('/',pos) - pos -1);

        /* handle '.' and '..' */
        if(path[pos+1] == '.'){
            if(path.length() <= pos+2 || path[pos+2] == '/'){
                path.erase(pos,2);
                continue;
            }
            if(path[pos+2] == '.'){
                size_t pre = path.find_last_of('/',pos-1);
                path.erase(pre + 1, pos - pre + 2);
                continue;
            }
        }
        pos = path.find_first_of('/',pos+1);
    }
}

void _substitute(std::string &path, size_t pos)
{
    while(pos != std::string::npos) {
        if(path[pos]=='/')
            path[pos]=':';
        pos = path.find_first_of('/',pos+1);
    }
}

std::string change(const char *path)
{
    getMountPoints();
    std::string p(path);
    _realpath(p);
    for(auto mnt : mountpoints){
        if(! mnt.compare(0, mnt.length(), p.data(), mnt.length())){
            printf("intial: '%s'\n",path);
            _substitute(p, mnt.length() + 1);
            printf("substitute: '%s'\n",p.data());
        }
    }
    return p;
}

