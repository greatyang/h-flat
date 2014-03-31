#include "main.h"
#include "debug.h"
#include "fuseops.h"
#include "kinetic_helper.h"
#include "simple_kinetic_namespace.h"
#include "distributed_kinetic_namespace.h"


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
    if (err) pok_error("Error encountered validating root metadata");

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

int main(int argc, char *argv[])
{
    GOOGLE_PROTOBUF_VERIFY_VERSION;
    init_pok_ops(&pok_ops);
    struct pok_priv *priv = 0;
    try {
        // SimpleKineticNamespace *simple = new SimpleKineticNamespace();

        std::vector< posixok::Partition > clustermap;
        int port = 8123;

        for(int j=0; j<2; j++){
            posixok::Partition p;
            for(int i=0; i<3; i++){
                posixok::KineticDrive *d = p.mutable_drives()->Add();
                d->set_host("localhost");
                d->set_port(port++);
                d->set_status(posixok::KineticDrive_Status_GREEN);
            }
            clustermap.push_back(p);
        }

        DistributedKineticNamespace *distributed = new DistributedKineticNamespace(clustermap);
        priv = new pok_priv(distributed);
    }
    catch(std::exception& e){
        pok_error("Exception thrown during mount operation. Reason: %s",e.what());
        exit(EXIT_FAILURE);
    }
    return fuse_main(argc, argv, &pok_ops, (void*)priv);
}
