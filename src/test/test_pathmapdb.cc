#define CATCH_CONFIG_MAIN 
#include "catch.hpp"
#include "../pathmap_db.cc"

/* File System: []
 * Database:    [] */ 
TEST_CASE( "init+sanity", "[pathmapdb]" ) {
   std::unique_ptr<PathMapDB> db(new PathMapDB);
   
   std::string  t1("/a/b/c/d/file");
   std::int64_t pathPermissionTimeStamp;
   std::string t1b = db->toSystemPath(t1.c_str(), pathPermissionTimeStamp, CallingType::LOOKUP);


   REQUIRE(db->getSnapshotVersion() == 0);
   REQUIRE(pathPermissionTimeStamp == 0);
   REQUIRE(t1 == t1b);

   //REQUIRE(t1.compare(t1b) == 0);
}

/* Assume initially existing directories /x and /x/a */ 
TEST_CASE( "rename-circular", "[pathmapdb]" ) {
   std::unique_ptr<PathMapDB> db(new PathMapDB);
 
   std::string userPath, systemPath;
   std::int64_t pathPermissionTimeStamp;

   /* move /x/a /x/b */
   db->addDirectoryMove("/x/a", "/x/b");
   REQUIRE(db->getSnapshotVersion() == 1);
   systemPath = db->toSystemPath("/x/b", pathPermissionTimeStamp, CallingType::LOOKUP);
   REQUIRE(pathPermissionTimeStamp == 0);
   REQUIRE(systemPath == "/x/a");
   /*******************
	* Database:	/x/b->/x/a | MOVE
	* 			/x/a->/0a  | REUSE
	*******************/
   //db->printSnapshot();

   /* move /x/b /x/c */
   db->addDirectoryMove("/x/b", "/x/c");
   REQUIRE(db->getSnapshotVersion() == 2);
   systemPath = db->toSystemPath("/x/b", pathPermissionTimeStamp, CallingType::LOOKUP);
   REQUIRE(pathPermissionTimeStamp == 0);
   REQUIRE(systemPath == "/x/b");
   systemPath = db->toSystemPath("/x/c",pathPermissionTimeStamp, CallingType::LOOKUP);
   REQUIRE(pathPermissionTimeStamp == 0);
   REQUIRE(systemPath == "/x/a");
	/*******************
	* Database:	/x/c->/x/a | MOVE
	* 			/x/a->/0a  | REUSE
	*******************/
   //db->printSnapshot();

   /* move /x/c /x/a */
   db->addDirectoryMove("/x/c", "/x/a");
   REQUIRE(db->getSnapshotVersion() == 3);
   systemPath = db->toSystemPath("/x/a",pathPermissionTimeStamp, CallingType::LOOKUP);
   REQUIRE(pathPermissionTimeStamp == 0);
   REQUIRE(systemPath == "/x/a");
   systemPath = db->toSystemPath("/x/b",pathPermissionTimeStamp, CallingType::LOOKUP);
   REQUIRE(pathPermissionTimeStamp == 0);
   REQUIRE(systemPath == "/x/b");
   systemPath = db->toSystemPath("/x/c",pathPermissionTimeStamp, CallingType::LOOKUP);
   REQUIRE(pathPermissionTimeStamp == 0);
   REQUIRE(systemPath == "/x/c");
  /*******************
   * Database:	[]
   *******************/
   //db->printSnapshot();

}

TEST_CASE( "rename-reuse", "[pathmapdb]" ) {
   std::unique_ptr<PathMapDB> db(new PathMapDB);

   std::string userPath, systemPath;
   std::int64_t pathPermissionTimeStamp;


   db->addDirectoryMove("/a", "/b");
   /* b->a, a->reuse */

   systemPath = db->toSystemPath("/a", pathPermissionTimeStamp, CallingType::LOOKUP);
   db->addDirectoryMove("/a", "/c");
   REQUIRE(systemPath == db->toSystemPath("/c", pathPermissionTimeStamp, CallingType::LOOKUP));
   REQUIRE(db->toSystemPath("/a", pathPermissionTimeStamp, CallingType::LOOKUP) != "/a");
}
