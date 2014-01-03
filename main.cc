#include "main.h"
#include "debug.h"

/* permission */
extern int pok_fgetattr(const char *user_path, struct stat *attr, struct fuse_file_info *fi);
extern int pok_getattr (const char *user_path, struct stat *attr);
extern int pok_access  (const char *user_path, int mode);

/* file */
extern int pok_create(const char *user_path, mode_t mode, struct fuse_file_info *fi);
extern int pok_unlink(const char *user_path);
extern int pok_open(const char *user_path, struct fuse_file_info *fi);
extern int pok_release (const char *user_path, struct fuse_file_info *fi);

/* data */
extern int pok_read (const char* user_path, char *buf, size_t size, off_t offset, struct fuse_file_info* fi);
extern int pok_write(const char* user_path, const char *buf, size_t size, off_t offset, struct fuse_file_info* fi);

/* directory */
//extern int pok_opendir 		(const char *user_path, struct fuse_file_info *fi);
//extern int pok_releasedir 	(const char *user_path, struct fuse_file_info *fi);
extern int pok_readdir 		(const char *user_path, void *, fuse_fill_dir_t, off_t, struct fuse_file_info *fi);


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
	/* Instead of a mkfs utility, build root metadata if its not available*/
	std::unique_ptr<MetadataInfo> mdi(new MetadataInfo());
	mdi->setSystemPath("/");
	if(PRIV->nspace->getMD(mdi.get()).notOk()){
		pok_debug("Creating new file system root.");
		mdi->updateACMtime();
		mdi->pbuf()->set_id_group(fuse_get_context()->gid);
		mdi->pbuf()->set_id_user(fuse_get_context()->uid);
		mdi->pbuf()->set_mode(S_IFDIR | S_IRWXU | S_IRWXG | S_IRWXO);
		if(PRIV->nspace->putMD(mdi.get()).notOk()){
			pok_warning("Failed initializing file system root entry.");
		}
	}
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
	pok_trace("entered");
	google::protobuf::ShutdownProtobufLibrary();
	delete PRIV;
}


static struct fuse_operations pok_ops;
static void init_pok_ops(fuse_operations *ops)
{
	ops->getattr 	= pok_getattr;
	ops->fgetattr	= pok_fgetattr;
	ops->access		= pok_access;

	ops->create		= pok_create;
	ops->unlink		= pok_unlink;
	ops->open		= pok_open;
	ops->release	= pok_release;

	ops->opendir	= pok_open;
	ops->releasedir = pok_release;
	ops->readdir	= pok_readdir;

	ops->read		= pok_read;
	ops->write		= pok_write;

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
		pok_error("Exception thrown. Reason: %s \n ABORTING MOUNT.",e.what());
		return -1;
	}
	return fuse_main(argc, argv, &pok_ops, (void*)priv);
}
