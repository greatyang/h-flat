#include "main.h"
#include "debug.h"
#include "fuseops.h"
#include "kinetic_helper.h"
#include "simple_kinetic_namespace.h"
#include "distributed_kinetic_namespace.h"
#include <libconfig.h>

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

static struct fuse_operations hflat_ops;
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


bool parse_configuration(char *file,
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
    bool rtn = config_read_file(&cfg, file);
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
        hflat_error("Error in configuration file %s: %s:%d - %s\n", file,
                config_error_file(&cfg), config_error_line(&cfg), config_error_text(&cfg));
    config_destroy(&cfg);
    return rtn;
}


int main(int argc, char *argv[])
{
    GOOGLE_PROTOBUF_VERIFY_VERSION;
    struct hflat_priv *priv = 0;

    std::vector< hflat::Partition > clustermap;
    hflat::Partition logpartition;
    int cache_expiration_ms = 1000;
    PosixMode mode = PosixMode::FULL;

    init_hflat_ops(&hflat_ops);

    for(int i=0; i<argc; i++)
        if(strncmp(argv[i],"-cfg=",5) == 0){
            if( false == parse_configuration(argv[i]+5,
                    clustermap, logpartition,
                    cache_expiration_ms,
                    mode))
                return(EXIT_FAILURE);
            else{
                for(int j=i+1; j<argc; j++)
                    argv[j-1] = argv[j];
                argc--;
            }
        }

    try {
        if(clustermap.empty())
            priv = new hflat_priv(new SimpleKineticNamespace(), cache_expiration_ms, 1024*1024, mode);
        else if(clustermap.size() == 1 && clustermap.at(0).drives_size() == 1){
            hflat_debug("simple namespace used");
            priv = new hflat_priv(new SimpleKineticNamespace(clustermap[0].drives(0)), cache_expiration_ms, 1024*1024, mode);
        }
        else
            priv = new hflat_priv(new DistributedKineticNamespace(clustermap, logpartition), cache_expiration_ms, 1024*1024, mode);
    }
    catch(std::exception& e){
        hflat_error("Exception thrown during mount operation. Reason: %s \n Check your Configuration.",e.what());
        return(EXIT_FAILURE);
    }
    hflat_debug("Read cache expiration time set to %d milliseconds. Posix mode is %s", cache_expiration_ms, mode == PosixMode::FULL ? "FULL" : "RELAXED" );
    return fuse_main(argc, argv, &hflat_ops, (void*)priv);
}
