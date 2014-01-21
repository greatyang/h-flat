#include "main.h"
#include "debug.h"
#include "fuseops.h"
#include "kinetic_helper.h"

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
	/* Setup values required for inode generation. */
	const std::string inode_base_key = "igen";
	std::string keyVersion;

	KineticStatus status = PRIV->kinetic->GetVersion(inode_base_key, &keyVersion);
	if(status.ok())
		PRIV->inum_base = util::to_int64(keyVersion);
	else if(status.notFound())
		  PRIV->inum_base = 0;
	else
		  pok_error("Error encountered obtaining inode generation numbers.");


	KineticRecord empty("",std::to_string(PRIV->inum_base+1),"",com::seagate::kinetic::proto::Message_Algorithm_SHA1);
	status = PRIV->kinetic->Put(inode_base_key, PRIV->inum_base ? std::to_string(PRIV->inum_base) : "", WriteMode::REQUIRE_SAME_VERSION, empty);
	if(status.notOk())
		  pok_error("Failed increasing inode base key.");

	/* Verify that root metadata is available. If it isn't, initialize it. */
	std::unique_ptr<MetadataInfo> mdi(new MetadataInfo());
	mdi->setSystemPath("/");
	int err = get_metadata(mdi.get());
	if (err == -ENOENT){
		  pok_trace("Initialzing root metadata.");
		  mdi->pbuf()->set_type(mdi->pbuf()->POSIX);
		  mdi->updateACMtime();
		  mdi->pbuf()->set_uid(0);
		  mdi->pbuf()->set_gid(0);
		  mdi->pbuf()->set_mode(S_IFDIR | S_IRWXU | S_IRWXG | S_IRWXO);
		  mdi->pbuf()->set_path_permission_verified(0);
		  mdi->pbuf()->set_inode_number(util::generate_inode_number());
		  err = create_metadata(mdi.get());
	}
	if(err)
		  pok_error("Error encountered validating root metadata.");

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
	ops->create		= pok_fcreate;
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
	catch(std::exception& e)
		pok_error("Exception thrown during mount operation. Reason: %s",e.what());

	return fuse_main(argc, argv, &pok_ops, (void*)priv);
}
