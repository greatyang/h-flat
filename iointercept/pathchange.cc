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
           // printf("intial request: '%s'\n",path);
            _substitute(p, mnt.length() + 1);
           // printf("substitute: '%s'\n",p.data());
        }
    }
    return p;
}

