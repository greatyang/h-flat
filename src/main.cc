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
#include "main.h"
#include "debug.h"
#include "fuseops.h"
#include "kinetic_helper.h"
#include "simple_kinetic_namespace.h"
#include "simple_distributed_kinetic_namespace.h"
#include "distributed_kinetic_namespace.h"
#include <libconfig.h>
#include <glog/logging.h>
#include <unistd.h>


static struct fuse_operations hflat_ops;
static std::string filename;

static bool parse_configuration(
        std::vector< hflat::Partition > &clustermap, hflat::Partition &logpartition,
        int &cache_expiration_ms, PosixMode &pmode)
{
    auto cfg_to_hflat = [&](config_setting_t *partition, hflat::Partition &p) -> bool {
        if(partition)
        for(int i=0; i<config_setting_length(partition); i++){
              config_setting_t *drive = config_setting_get_elem(partition, i);
              const char *host, *status; int port;

              if( !config_setting_lookup_string(drive, "host",   &host) ||
                  !config_setting_lookup_string(drive, "status", &status) ||
                  !config_setting_lookup_int   (drive, "port",   &port) )
                  return false;

              hflat::KineticDrive *d = p.mutable_drives()->Add();
              d->set_host(host);
              d->set_port(port);

              if(strcmp(status,"GREEN") == 0)
                  d->set_status(hflat::KineticDrive_Status_GREEN);
              else if(strcmp(status,"YELLOW") == 0)
                  d->set_status(hflat::KineticDrive_Status_YELLOW);
              else if(strcmp(status,"RED") == 0)
                  d->set_status(hflat::KineticDrive_Status_RED);
              else
                  return false;

        }
        return true;
    };


    config_t cfg;
    config_init(&cfg);
    bool rtn = config_read_file(&cfg, filename.c_str());
    if  (rtn) rtn = cfg_to_hflat(config_lookup(&cfg, "log"), logpartition);

    if( config_setting_t * cmap = config_lookup(&cfg, "clustermap")) {
        for(int i = 0; rtn && i < config_setting_length(cmap); i++){
            hflat::Partition p;
            rtn = cfg_to_hflat(config_setting_get_elem(cmap, i), p);

            /* Special case: 2 drives in partition. We add a logdrive to the partition to enable the namespace to
             * differentiate between a network split and a failed drive. */
            if(p.drives_size() == 2 && logpartition.drives_size())
                p.set_logid(i % logpartition.drives_size());

            p.set_partitionid(i);
            clustermap.push_back(p);
        }
    }


    if (config_setting_t * options =  config_lookup(&cfg, "options")){
        config_setting_lookup_int(options, "cache_expiration", &cache_expiration_ms);

        const char *mode;
        if( config_setting_lookup_string(options, "posix_mode", &mode) )
            if(strcmp(mode,"RELAXED") == 0)
                pmode = PosixMode::TIMERELAXED;
    }


    if(rtn == false)
        hflat_error("Error in configuration file %s: %s:%d - %s\n", filename.c_str(),
                config_error_file(&cfg), config_error_line(&cfg), config_error_text(&cfg));
    config_destroy(&cfg);
    return rtn;
}



/**
 * Initialize filesystem
 *
 * The return value will passed in the private_data field of
 * fuse_context to all file operations and as a parameter to the
 * destroy() method.
 *
 * Introduced in version 2.3
 * Changed in version 2.6
 */
void *hflat_init(struct fuse_conn_info *conn)
{
    struct hflat_priv *priv = 0;
    std::vector< hflat::Partition > clustermap;
    hflat::Partition logpartition;
    PosixMode mode = PosixMode::FULL;
    int cache_expiration_ms = 1000;

    if(! filename.empty()){
        bool cok = parse_configuration(
                        clustermap, logpartition,
                        cache_expiration_ms,
                        mode);
        if(cok == false) REQ(EXIT_FAILURE);
    }

    try {
        if(clustermap.empty())
            priv = new hflat_priv(new SimpleKineticNamespace(), cache_expiration_ms, 1024*1024, mode);
        /* DEBUG ONLY
        else if(clustermap.size() == 1 && clustermap.at(0).drives_size() == 1){
            hflat_debug("simple namespace used");
            priv = new hflat_priv(new SimpleKineticNamespace(clustermap[0].drives(0)), cache_expiration_ms, 1024*1024, mode);
        }
        else if(std::all_of(clustermap.begin(), clustermap.end(),
                [](const hflat::Partition &p){ return p.drives_size() == 1 ? true : false;})){
            hflat_debug("simple distributed namespace used");
            priv = new hflat_priv(new SimpleDistributedKineticNamespace(clustermap), cache_expiration_ms, 1024*1024, mode);
        } */
        else
            priv = new hflat_priv(new DistributedKineticNamespace(clustermap, logpartition), cache_expiration_ms, 1024*1024, mode);
    }
    catch(std::exception& e){
        hflat_error("Exception thrown during mount operation. Reason: %s \n Check your Configuration.",e.what());
        REQ(EXIT_FAILURE);
    }
    fuse_get_context()->private_data = priv;


    /* Setup values required for inode generation. */
    if ( util::grab_inode_generation_token() )
        hflat_error("Error encountered during setup of inode number generation");

    /* Verify that root metadata is available. If it isn't, initialize it. */
    std::shared_ptr<MetadataInfo> mdi(new MetadataInfo("/"));
    int err = get_metadata(mdi);
    if (err == -ENOENT) {
        initialize_metadata(mdi, mdi, S_IFDIR | S_IRWXU | S_IRWXG | S_IRWXO);
        err = create_metadata(mdi);
    }
    if (err)
        hflat_error("Error encountered validating root metadata");

    util::database_update();
    return PRIV;
}

/**
 * Clean up filesystem
 *
 * Called on filesystem exit.
 *
 * Introduced in version 2.3
 */
void hflat_destroy(void *priv)
{
    google::protobuf::ShutdownProtobufLibrary();
    delete PRIV;
}



static void init_hflat_ops(fuse_operations *ops)
{
    ops->create = hflat_fcreate;
    ops->mknod = hflat_mknod;
    ops->unlink = hflat_unlink;
    ops->open = hflat_open;
    ops->release = hflat_release;

    ops->mkdir = hflat_mkdir;
    ops->rmdir = hflat_rmdir;
    ops->opendir = hflat_open;
    ops->releasedir = hflat_release;
    ops->readdir = hflat_readdir;

    ops->access = hflat_access;
    ops->chown = hflat_chown;
    ops->chmod = hflat_chmod;

    ops->getattr = hflat_getattr;
    ops->fgetattr = hflat_fgetattr;
    ops->utimens = hflat_utimens;
    ops->statfs = hflat_statfs;

#ifdef __APPLE__
    ops->setxattr = hflat_setxattr_apple;
    ops->getxattr = hflat_getxattr_apple;
#else
    ops->setxattr = hflat_setxattr;
    ops->getxattr = hflat_getxattr;
#endif
    ops->listxattr = hflat_listxattr;
    ops->removexattr = hflat_removexattr;

    ops->symlink = hflat_symlink;
    ops->readlink = hflat_readlink;
    ops->link = hflat_hardlink;

    ops->read = hflat_read;
    ops->write = hflat_write;
    ops->truncate = hflat_truncate;
    ops->ftruncate = hflat_ftruncate;

    ops->rename = hflat_rename;

    ops->fsync = hflat_fsync;
    ops->fsyncdir = hflat_fsyncdir;
    ops->flush = hflat_flush;

    ops->init = hflat_init;
    ops->destroy = hflat_destroy;
}

#include <sys/param.h>
int main(int argc, char *argv[])
{
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    /* google logging by kinetic-cpp-client */
    google::InitGoogleLogging(argv[0]);

    init_hflat_ops(&hflat_ops);

    /* extract cfg file path */
    for(int i=0; i<argc; i++){
        if(strncmp(argv[i],"-cfg=",5) == 0){
            filename = std::string(argv[i]+5);
            for(int j=i+1; j<argc; j++)
                argv[j-1] = argv[j];
            argc--;
        }
    }

    /* transform relative to absolute path, as fuse will switch
     * working directory to root before calling init. */
    if(filename.length() && filename[0]!='/'){
        char buf[PATH_MAX];
        getcwd(buf, PATH_MAX);
        filename.insert(0,"/");
        filename.insert(0,buf);
        hflat_trace("using configuration file %s",filename.c_str());
    }

    return fuse_main(argc, argv, &hflat_ops, nullptr);
}
