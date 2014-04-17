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
void *pok_init(struct fuse_conn_info *conn)
{
    /* Setup values required for inode generation. */
    if ( util::grab_inode_generation_token() )
        pok_error("Error encountered during setup of inode number generation");

    /* Verify that root metadata is available. If it isn't, initialize it. */
    std::shared_ptr<MetadataInfo> mdi(new MetadataInfo("/"));
    int err = get_metadata(mdi);
    if (err == -ENOENT) {
        initialize_metadata(mdi, mdi, S_IFDIR | S_IRWXU | S_IRWXG | S_IRWXO);
        err = create_metadata(mdi);
    }
    if (err)
        pok_error("Error encountered validating root metadata");

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
void pok_destroy(void *priv)
{
    google::protobuf::ShutdownProtobufLibrary();
    delete PRIV;
}

static struct fuse_operations pok_ops;
static void init_pok_ops(fuse_operations *ops)
{
    ops->create = pok_fcreate;
    ops->mknod = pok_mknod;
    ops->unlink = pok_unlink;
    ops->open = pok_open;
    ops->release = pok_release;

    ops->mkdir = pok_mkdir;
    ops->rmdir = pok_rmdir;
    ops->opendir = pok_open;
    ops->releasedir = pok_release;
    ops->readdir = pok_readdir;

    ops->access = pok_access;
    ops->chown = pok_chown;
    ops->chmod = pok_chmod;

    ops->getattr = pok_getattr;
    ops->fgetattr = pok_fgetattr;
    ops->utimens = pok_utimens;
    ops->statfs = pok_statfs;

#ifdef __APPLE__
    ops->setxattr = pok_setxattr_apple;
    ops->getxattr = pok_getxattr_apple;
#else
    ops->setxattr = pok_setxattr;
    ops->getxattr = pok_getxattr;
#endif
    ops->listxattr = pok_listxattr;
    ops->removexattr = pok_removexattr;

    ops->symlink = pok_symlink;
    ops->readlink = pok_readlink;
    ops->link = pok_hardlink;

    ops->read = pok_read;
    ops->write = pok_write;
    ops->truncate = pok_truncate;
    ops->ftruncate = pok_ftruncate;

    ops->rename = pok_rename;

    ops->fsync = pok_fsync;
    ops->fsyncdir = pok_fsyncdir;
    ops->flush = pok_flush;

    ops->init = pok_init;
    ops->destroy = pok_destroy;
}


bool parse_configuration(char *file,
        std::vector< posixok::Partition > &clustermap, posixok::Partition &logpartition,
        int &cache_expiration_ms, PosixMode &pmode)
{
    auto cfg_to_posixok = [&](config_setting_t *partition, posixok::Partition &p) -> bool {
        if(partition)
        for(int i=0; i<config_setting_length(partition); i++){
              config_setting_t *drive = config_setting_get_elem(partition, i);
              const char *host, *status; int port;

              if( !config_setting_lookup_string(drive, "host",   &host) ||
                  !config_setting_lookup_string(drive, "status", &status) ||
                  !config_setting_lookup_int   (drive, "port",   &port) )
                  return false;

              posixok::KineticDrive *d = p.mutable_drives()->Add();
              d->set_host(host);
              d->set_port(port);

              if(strcmp(status,"GREEN") == 0)
                  d->set_status(posixok::KineticDrive_Status_GREEN);
              else if(strcmp(status,"YELLOW") == 0)
                  d->set_status(posixok::KineticDrive_Status_YELLOW);
              else if(strcmp(status,"RED") == 0)
                  d->set_status(posixok::KineticDrive_Status_RED);
              else
                  return false;

        }
        return true;
    };


    config_t cfg;
    config_init(&cfg);
    bool rtn = config_read_file(&cfg, file);
    if  (rtn) rtn = cfg_to_posixok(config_lookup(&cfg, "log"), logpartition);

    if( config_setting_t * cmap = config_lookup(&cfg, "clustermap")) {
        for(int i = 0; rtn && i < config_setting_length(cmap); i++){
            posixok::Partition p;
            rtn = cfg_to_posixok(config_setting_get_elem(cmap, i), p);

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
        pok_error("Error in configuration file %s: %s:%d - %s\n", file,
                config_error_file(&cfg), config_error_line(&cfg), config_error_text(&cfg));
    config_destroy(&cfg);
    return rtn;
}


int main(int argc, char *argv[])
{
    GOOGLE_PROTOBUF_VERIFY_VERSION;
    struct pok_priv *priv = 0;

    std::vector< posixok::Partition > clustermap;
    posixok::Partition logpartition;
    int cache_expiration_ms = 1000;
    PosixMode mode = PosixMode::FULL;

    init_pok_ops(&pok_ops);

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
            priv = new pok_priv(new SimpleKineticNamespace(), cache_expiration_ms, 1024*1024, mode);
        else if(clustermap.size() == 1 && clustermap.at(0).drives_size() == 1){
            pok_debug("simple namespace used");
            priv = new pok_priv(new SimpleKineticNamespace(clustermap[0].drives(0)), cache_expiration_ms, 1024*1024, mode);
        }
        else
            priv = new pok_priv(new DistributedKineticNamespace(clustermap, logpartition), cache_expiration_ms, 1024*1024, mode);
    }
    catch(std::exception& e){
        pok_error("Exception thrown during mount operation. Reason: %s \n Check your Configuration.",e.what());
        return(EXIT_FAILURE);
    }
    pok_debug("Read cache expiration time set to %d milliseconds. Posix mode is %s", cache_expiration_ms, mode == PosixMode::FULL ? "FULL" : "RELAXED" );
    return fuse_main(argc, argv, &pok_ops, (void*)priv);
}
