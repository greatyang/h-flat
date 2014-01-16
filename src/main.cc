#include "main.h"
#include "debug.h"
#include "fuseops.h"

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
void *pok_init (struct fuse_conn_info *conn)
{
	database_update();
	return PRIV;
}

/**
 * Clean up filesystem
 *
 * Called on filesystem exit.
 *
 * Introduced in version 2.3
 */
void pok_destroy (void *priv)
{
	google::protobuf::ShutdownProtobufLibrary();
	delete PRIV;
}


static struct fuse_operations pok_ops;
static void init_pok_ops(fuse_operations *ops)
{
	ops->create		= pok_create;
	ops->unlink		= pok_unlink;
	ops->open		= pok_open;
	ops->release	= pok_release;
	ops->mknod		= pok_mknod;

	ops->mkdir		= pok_mkdir;
	ops->opendir	= pok_open;
	ops->rmdir		= pok_rmdir;
	ops->releasedir = pok_release;
	ops->readdir	= pok_readdir;

	ops->access		= pok_access;
	ops->chown		= pok_chown;
	ops->chmod		= pok_chmod;

	ops->getattr 	= pok_getattr;
	ops->fgetattr	= pok_fgetattr;
	ops->utimens	= pok_utimens;
	ops->statfs		= pok_statfs;

	#ifdef __APPLE__
	ops->setxattr	= pok_setxattr_apple;
	ops->getxattr	= pok_getxattr_apple;
	#else
	ops->setxattr 	= pok_setxattr;
	ops->getxattr	= pok_getxattr;
	#endif
	ops->listxattr	= pok_listxattr;
	ops->removexattr= pok_removexattr;

	ops->symlink    = pok_symlink;
	ops->readlink	= pok_readlink;
	ops->link		= pok_hardlink;

	ops->read		= pok_read;
	ops->write		= pok_write;
	ops->truncate 	= pok_truncate;
	ops->ftruncate 	= pok_ftruncate;

	ops->rename 	= pok_rename;

	ops->fsync 		= pok_fsync;
	ops->fsyncdir	= pok_fsyncdir;
	ops->flush		= pok_flush;

	ops->init		= pok_init;
	ops->destroy 	= pok_destroy;
}


int main(int argc, char *argv[])
{
	GOOGLE_PROTOBUF_VERIFY_VERSION;
	init_pok_ops(&pok_ops);
	struct pok_priv *priv = 0;
	try{
		 priv = new pok_priv();
	}
	catch(std::exception& e){
		pok_error("Exception thrown during mount operation. Reason: %s",e.what());
		return -1;
	}

	/* Verify that root metadata is available. If it isn't, initialize it. */
	std::unique_ptr<MetadataInfo> mdi(new MetadataInfo());
	mdi->setSystemPath("/");
	NamespaceStatus status = priv->nspace->getMD(mdi.get());

	if (status.notFound()){
		pok_trace("Initialzing root metadata.");

		mdi->updateACMtime();
		mdi->pbuf()->set_id_group(0);
		mdi->pbuf()->set_id_user(0);
		mdi->pbuf()->set_mode(S_IFDIR | S_IRWXU | S_IRWXG | S_IRWXO);
		mdi->pbuf()->set_blocks(1);
		mdi->pbuf()->set_size(0);
		mdi->pbuf()->set_path_permission_verified(0);
		mdi->pbuf()->set_data_unique_id("|");
		status = priv->nspace->putMD(mdi.get());
	}
	if (status.notOk()){
		pok_error("Error encountered when trying to validate root metadata.");
		delete priv;
		return -1;
	}

	return fuse_main(argc, argv, &pok_ops, (void*)priv);
}
