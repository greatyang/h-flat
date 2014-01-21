#include <errno.h>
#include <fcntl.h>
#include <string.h>

#include "gtest/gtest.h"
#include "pathmap_db.h"

class PathmapDBTest: public ::testing::Test
{
protected:
    PathmapDBTest()
    {
    }

    void SetUp()
    {
        db.reset(new PathMapDB());
    }

    // void TearDown() {  }
    std::unique_ptr<PathMapDB> db;

};

/* File System: []
 * Database:    [] */
TEST_F(PathmapDBTest, InitAndSanity)
{
    std::string t1("/a/b/c/d/file");
    std::int64_t pathPermissionTimeStamp;
    std::string t1b = db->toSystemPath(t1.c_str(), pathPermissionTimeStamp, CallingType::LOOKUP);

    ASSERT_EQ(db->getSnapshotVersion(), 0);
    ASSERT_EQ(db->getSnapshotVersion(), 0);
    ASSERT_EQ(pathPermissionTimeStamp, 0);
    ASSERT_EQ(t1, t1b);
}

/* Assume initially existing directories /x and /x/a */
TEST_F(PathmapDBTest, CircularRename)
{
    std::string userPath, systemPath;
    std::int64_t pathPermissionTimeStamp;

    /* move /x/a /x/b */
    db->addDirectoryMove("/x/a", "/x/b");
    ASSERT_EQ(db->getSnapshotVersion(), 1);
    systemPath = db->toSystemPath("/x/b", pathPermissionTimeStamp, CallingType::LOOKUP);
    ASSERT_EQ(pathPermissionTimeStamp, 0);
    ASSERT_EQ(systemPath, "/x/a");
    /*******************
     * Database:	/x/b->/x/a | MOVE
     * 			/x/a->/0a  | REUSE
     *******************/

    /* move /x/b /x/c */
    db->addDirectoryMove("/x/b", "/x/c");
    ASSERT_EQ(db->getSnapshotVersion(), 2);
    systemPath = db->toSystemPath("/x/b", pathPermissionTimeStamp, CallingType::LOOKUP);
    ASSERT_EQ(pathPermissionTimeStamp, 0);
    ASSERT_EQ(systemPath, "/x/b");
    systemPath = db->toSystemPath("/x/c", pathPermissionTimeStamp, CallingType::LOOKUP);
    ASSERT_EQ(pathPermissionTimeStamp, 0);
    ASSERT_EQ(systemPath, "/x/a");
    /*******************
     * Database:	/x/c->/x/a | MOVE
     * 			/x/a->/0a  | REUSE
     *******************/

    /* move /x/c /x/a */
    db->addDirectoryMove("/x/c", "/x/a");
    ASSERT_EQ(db->getSnapshotVersion(), 3);
    systemPath = db->toSystemPath("/x/a", pathPermissionTimeStamp, CallingType::LOOKUP);
    ASSERT_EQ(pathPermissionTimeStamp, 0);
    ASSERT_EQ(systemPath, "/x/a");
    systemPath = db->toSystemPath("/x/b", pathPermissionTimeStamp, CallingType::LOOKUP);
    ASSERT_EQ(pathPermissionTimeStamp, 0);
    ASSERT_EQ(systemPath, "/x/b");
    systemPath = db->toSystemPath("/x/c", pathPermissionTimeStamp, CallingType::LOOKUP);
    ASSERT_EQ(pathPermissionTimeStamp, 0);
    ASSERT_EQ(systemPath, "/x/c");
    /*******************
     * Database:	[]
     *******************/
}

TEST_F(PathmapDBTest, Reuse)
{
    std::string userPath, systemPath;
    std::int64_t pathPermissionTimeStamp;

    db->addDirectoryMove("/a", "/b");
    /* b->a, a->reuse */

    systemPath = db->toSystemPath("/a", pathPermissionTimeStamp, CallingType::LOOKUP);
    db->addDirectoryMove("/a", "/c");
    ASSERT_EQ(systemPath, db->toSystemPath("/c", pathPermissionTimeStamp, CallingType::LOOKUP));
    ASSERT_NE(db->toSystemPath("/a", pathPermissionTimeStamp, CallingType::LOOKUP), "/a");
}
