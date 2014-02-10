#include <errno.h>
#include <fcntl.h>
#include <string.h>

#include "gtest/gtest.h"
#include "metadata_info.h"

class mdiTest: public ::testing::Test
{
protected:
    mdiTest()
    {
    }

    void SetUp()
    {
        md.Clear();
        mdi.setMD(md, 0);
    }
    MetadataInfo mdi;
    posixok::Metadata md;
    // void TearDown() {  }
};


TEST_F(mdiTest, MergeMaxValue)
{
    md.set_atime(100);
    EXPECT_TRUE( mdi.mergeMD(md, 1));
    EXPECT_EQ( mdi.getMD().atime(), md.atime());

    md.set_atime(200);
    mdi.getMD().set_atime(300);
    EXPECT_TRUE( mdi.mergeMD(md, 1));
    EXPECT_EQ( mdi.getMD().atime(), (std::uint32_t) 300);

    md.set_atime(500);
    mdi.getMD().set_atime(400);
    EXPECT_TRUE( mdi.mergeMD(md, 1));
    EXPECT_EQ( mdi.getMD().atime(), md.atime());
}

TEST_F(mdiTest, MergeMode)
{
    // no changes
    mdi.setMD(md, 1);
    ASSERT_TRUE( mdi.mergeMD(md, 1));

    // only md changed
    mdi.setMD(md, 1);
    md.set_mode(11);
    ASSERT_TRUE( mdi.mergeMD(md, 1));
    ASSERT_EQ( mdi.getMD().mode(), md.mode());

    // only mdi changed
    mdi.setMD(md, 1);
    mdi.getMD().set_mode(2);
    ASSERT_TRUE( mdi.mergeMD(md, 1));
    ASSERT_EQ( mdi.getMD().mode(), (std::uint32_t) 2);

    // md & mdi changed to same value
    mdi.setMD(md, 1);
    md.set_mode(3);
    mdi.getMD().set_mode(3);
    ASSERT_TRUE( mdi.mergeMD(md, 1));
    ASSERT_EQ( mdi.getMD().mode(), md.mode());

    // md & mdi changed to differenet values
    mdi.setMD(md, 1);
    md.set_mode(4);
    mdi.getMD().set_mode(5);
    ASSERT_FALSE( mdi.mergeMD(md, 1));
}

TEST_F(mdiTest, MergeReachability)
{
    posixok::Metadata_ReachabilityEntry * e;

    // no changes
    e =  md.mutable_path_permission()->Add();
    e->set_gid(10);
    e->set_type(posixok::Metadata_ReachabilityType_GID);
    mdi.setMD(md, 1);
    ASSERT_TRUE( mdi.mergeMD(md, 1));

    // only md changed
    mdi.setMD(md, 1);
    e =  md.mutable_path_permission()->Add();
    e->set_gid(11);
    e->set_type(posixok::Metadata_ReachabilityType_GID);
    ASSERT_TRUE( mdi.mergeMD(md, 1));

    // only mdi changed
    mdi.setMD(md, 1);
    e =  mdi.getMD().mutable_path_permission()->Add();
    e->set_gid(12);
    e->set_type(posixok::Metadata_ReachabilityType_GID);
    ASSERT_TRUE( mdi.mergeMD(md, 1));

    // md & mdi changed to same value
    mdi.setMD(md, 1);
    e = md.mutable_path_permission()->Add();
    e->set_uid(42);
    e->set_type(posixok::Metadata_ReachabilityType_UID);
    e = mdi.getMD().mutable_path_permission()->Add();
    e->set_uid(42);
    e->set_type(posixok::Metadata_ReachabilityType_UID);
    ASSERT_TRUE( mdi.mergeMD(md, 1));

    // md & mdi changed to differenet values
    mdi.setMD(md, 1);
    e = md.mutable_path_permission()->Add();
    e->set_uid(13);
    e->set_type(posixok::Metadata_ReachabilityType_UID);
    e = mdi.getMD().mutable_path_permission()->Add();
    e->set_uid(14);
    e->set_type(posixok::Metadata_ReachabilityType_UID);
    ASSERT_FALSE( mdi.mergeMD(md, 1));
}


TEST_F(mdiTest, MergeXattr)
{
    posixok::Metadata_ExtendedAttribute * x;

    // no changes
    x =  md.mutable_xattr()->Add();
    x->set_name("a");
    x->set_value("value");
    mdi.setMD(md, 1);
    ASSERT_TRUE( mdi.mergeMD(md, 1));

    // only md changed
    mdi.setMD(md, 1);
    x =  md.mutable_xattr()->Add();
    x->set_name("b");
    x->set_value("value");
    ASSERT_TRUE( mdi.mergeMD(md, 1));

    // only mdi changed
    mdi.setMD(md, 1);
    x =  mdi.getMD().mutable_xattr()->Add();
    x->set_name("c");
    x->set_value("value");
    ASSERT_TRUE( mdi.mergeMD(md, 1));

    // md & mdi changed to same value
    mdi.setMD(md, 1);
    x =  mdi.getMD().mutable_xattr()->Add();
    x->set_name("d");
    x->set_value("value");
    x =  md.mutable_xattr()->Add();
    x->set_name("d");
    x->set_value("value");
    ASSERT_TRUE( mdi.mergeMD(md, 1));

    // md & mdi changed to different values
    mdi.setMD(md, 1);
    x =  mdi.getMD().mutable_xattr()->Add();
    x->set_name("e");
    x->set_value("value");
    x =  md.mutable_xattr()->Add();
    x->set_name("f");
    x->set_value("value");
    ASSERT_FALSE( mdi.mergeMD(md, 1));
}

TEST_F(mdiTest, PPChildren)
{

    mdi.computePathPermissionChildren();
}









