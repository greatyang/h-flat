#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#include "gtest/gtest.h"
#include "kinetic/kinetic.h"
#include "kinetic_namespace.h"
#include "metadata_info.h"
#include "database.pb.h"


struct mockPriv{
	 kinetic::BlockingKineticConnection *kinetic;
};
mockPriv *PRIV;

/* general utility functions */
namespace util{
	ino_t 			generate_inode_number(void);
	std::string 	generate_uuid(void);
	std::int64_t 	to_int64(const std::string &version_string);
	std::string 	path_to_filename(const std::string &path);
}

#define MAIN_H_
#include "kinetic_helper.cc"


class KineticNamespaceTest : public ::testing::Test {
    protected:
	KineticNamespaceTest(){
		kinetic::ConnectionOptions options;
		options.host = "localhost";
		options.port = 8123;
		options.user_id  = 1;
		options.hmac_key = "asdfasdf";

		kinetic::KineticConnectionFactory factory = kinetic::NewKineticConnectionFactory();
		kinetic::ConnectionHandle *kineticConnection = nullptr;
		kinetic::Status status = factory.NewConnection(options, &kineticConnection);
		if(!status.ok() || !kineticConnection){
			printf("Couldn't connect\n");
			exit(-1);
		}

		con  = std::unique_ptr<kinetic::ConnectionHandle>(kineticConnection);
		PRIV = new mockPriv();
		PRIV->kinetic = &(con->blocking());
    }

	~KineticNamespaceTest(){
		delete PRIV;
	}

    void SetUp() {
    	con->blocking().InstantSecureErase(NULL);
    }


    std::unique_ptr<kinetic::ConnectionHandle> con;
};


TEST_F(KineticNamespaceTest, MetadataCreate) {
	std::unique_ptr<MetadataInfo> mdi(new MetadataInfo());
	mdi->pbuf()->set_type(posixok::Metadata_InodeType_POSIX);
	mdi->setSystemPath("/");

	ASSERT_EQ( get_metadata(mdi.get()),  -ENOENT );
	ASSERT_EQ( create_metadata(mdi.get()), 0 );
	ASSERT_EQ( mdi->getCurrentVersion(), 1);
	mdi->setCurrentVersion(0);

	ASSERT_EQ( create_metadata(mdi.get()), -EEXIST );
	ASSERT_EQ( get_metadata(mdi.get()), 0 );
}

TEST_F(KineticNamespaceTest, MetadataDelete) {
	std::unique_ptr<MetadataInfo> mdi(new MetadataInfo());
	mdi->pbuf()->set_type(posixok::Metadata_InodeType_POSIX);
	mdi->setSystemPath("/");

	ASSERT_EQ( create_metadata(mdi.get()), 0 );
	ASSERT_EQ( mdi->getCurrentVersion(), 1);
	mdi->setCurrentVersion(3);

	ASSERT_EQ( delete_metadata(mdi.get()), -EINVAL );
	mdi->setCurrentVersion(1);
	ASSERT_EQ( delete_metadata(mdi.get()), 0 );
	ASSERT_EQ( delete_metadata(mdi.get()), -ENOENT );
}
