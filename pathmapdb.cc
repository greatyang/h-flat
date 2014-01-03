#include "pathmapdb.h"
#include <sys/param.h> /* Just for MAXSYMLINKS #define */ 
#include <thread>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <cassert>

PathMapDB::PathMapDB():
	snapshotVersion(0),
	snapshot(),
	currentReaders(),
	lock()
{
}

PathMapDB::~PathMapDB()
{
}

std::int64_t PathMapDB::getSnapshotVersion() const
{
	std::lock_guard<std::mutex> locker(lock);
	return snapshotVersion;
}

bool PathMapDB::searchPathRecursive(std::string& path, std::int64_t &maxTimeStamp, const bool afterMove, const bool followSymlink) const
{
	/* Found path in snapshot */ 
	if (snapshot.count(path))
	{
		/* Store maximum encountered timestamp */
		maxTimeStamp = std::max(maxTimeStamp,snapshot.at(path).permissionTimeStamp);
		
		/* There are a number of cases where we DO NOT want to remap the path even though we found it in the snapshot.
		 *
		 * 1) A TargetType::NONE mapping does not have a destination
		 *
		 * 2) A TargetType::LINK mapping that is at the very end of the full path should only be followed if the mapping is requested by
		 * the fuse readlink function. Otherwise every lookup of the link (e.g. 'ls' of parent directory) would go straight to the destination.
		 *
		 *
		 * 2) A reuse entry after a move entry should be ignored to prevent invalid remaps.
		 * Example: mv a b, ln -s a l >> [ b->a, a->*a, l->a ]
		 * We want calls to 'b' be mapped to 'a' not to '*a'.
		 * At the same time we keep correct functionality for links: calls to 'l' are mapped to '*a'.
		 *
		 * */
		if(		( snapshot.at(path).type == TargetType::NONE ) ||
				( !followSymlink && snapshot.at(path).type == TargetType::LINK ) ||
				( afterMove && snapshot.at(path).type == TargetType::REUSE )
		)
		{ /*don't remap*/ }
		else
			return true; 
	}
	
	/* Remove last path component and continue if possible */
	auto pos = path.find_last_of("/");
	if(pos == std::string::npos)
		return false;
	path.erase(pos,std::string::npos);
	return searchPathRecursive(path, maxTimeStamp, afterMove, true);
}

std::string PathMapDB::toSystemPath(const char *user_path, std::int64_t &maxTimeStamp, CallingType ctype) const
{
	int 		 numLinksFollowed = 0; 
	bool   		 afterMove = false;
	bool 		 followSymlink = ctype == CallingType::LOOKUP ? false : true;
	std::string  temp(user_path);
	std::string  systemPath(user_path);
	maxTimeStamp = 0;

	{  
		std::lock_guard<std::mutex> locker(lock);
		++currentReaders;
	}	
	
	while(searchPathRecursive(temp, maxTimeStamp, afterMove, followSymlink)){
		if(snapshot.at(temp).type == TargetType::LINK){
			/* Guard against symbolic link loops */
			if(++numLinksFollowed > MAXSYMLINKS){ 
				maxTimeStamp = -ELOOP;
				break;
			}
		}
		/* Remember if the target is due to a move */
		afterMove = (snapshot.at(temp).type == TargetType::MOVE) ? true : false;

		/* Apply mapping to path */ 
		temp = systemPath.replace(0,temp.size(),snapshot.at(temp).target);
	}
	
	--currentReaders;
	return systemPath;
}

std::int64_t PathMapDB::updateSnapshot()
{
	/* TODO: Obtain latest log from Remote DB */
	
	std::lock_guard<std::mutex> locker(lock);
	while(currentReaders.load())
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
		
	/* TODO: Apply log to local snapshot */ 

	return snapshotVersion;
} 


void PathMapDB::addSoftLink	(std::string origin, std::string destination)
{
	std::lock_guard<std::mutex> locker(lock);
	while(currentReaders.load())
		std::this_thread::sleep_for(std::chrono::milliseconds(1));

	snapshotVersion++;

	/* Keep possibly existing reverse move mapping in order to enable lookups on the inode of the 
	 * symbolic link. */ 
	if(snapshot.count(origin))
		assert(snapshot[origin].type == TargetType::REUSE);

	snapshot[ snapshot.count(origin) ? snapshot[origin].target : origin ] =
								{TargetType::LINK, std::string(destination), snapshotVersion};
}
	
void PathMapDB::addPermissionChange(std::string path)
{
	std::lock_guard<std::mutex> locker(lock);
	while(currentReaders.load())
		std::this_thread::sleep_for(std::chrono::milliseconds(1));

	snapshotVersion++;

	snapshot[path] = snapshot.count(path) ? 
	   PMEntry { snapshot[path].type, snapshot[path].target, snapshotVersion } : 
	   PMEntry { TargetType::NONE, std::string(), snapshotVersion };
}

/* Keep in mind that the client validated the operation against the file system at this point. This means: 
 * the path 'origin' exists and is a directory. 
 * the path 'destination' does not exist or is an empty directory. 
 * the path 'destination' does not specify a sub-directory of 'origin'
 * access permissions are validated. */ 
void PathMapDB::addDirectoryMove(std::string origin, std::string destination)
{
	std::lock_guard<std::mutex> locker(lock);
	while(currentReaders.load())
		std::this_thread::sleep_for(std::chrono::milliseconds(1));

	snapshotVersion++;
	
	/* No existing mapping... the standard case. 
	 * mv /a /b  [b->a, a->X]  */ 
	if(!snapshot.count(origin)){
		snapshot[destination] = { TargetType::MOVE, origin, 0 }; 
		snapshot[origin] 	  = { TargetType::REUSE, "/.reuse"+std::to_string(snapshotVersion), 0 }; 
	}
	
	/* There's an existing mapping with key==origin, update it 
	 * mv /a /b [b->a, a->X] 
	 * mv /b /c [c->a, a->X] */
	else{
		snapshot[destination] = { TargetType::MOVE, snapshot[origin].target, snapshot[origin].permissionTimeStamp };
		snapshot.erase(origin);	
	}
 
	/* Special case: Circular move 
	 * 	mv /a /b [b->a, a->X] 
	 *  mv /b /a [] */ 
 	if(destination == snapshot[destination].target){
		snapshot.erase(destination);
		snapshot.erase(origin);
	}
}


void PathMapDB::printSnapshot() const
{
	/* print entries in the form " key->target [type,timestamp] " */
	auto typeToString = [](TargetType t) -> std::string { 
	  switch(t){
		  case TargetType::MOVE:  return "MOVE";
		  case TargetType::REUSE: return "REUSE";
		  case TargetType::LINK:  return "LINK";
		  case TargetType::NONE:  return "NONE";
		  }
	   return "INVALID";
   };	
	std::cout << "[----------------------------------------]" << std::endl; 
	for (auto& element : snapshot) 
		std::cout << "  " << std::setw(10)  << std::left << element.first << " -> "
						  << std::setw(10)  << std::left << element.second.target 
						  << " [" << typeToString(element.second.type) << "," << element.second.permissionTimeStamp << "]" << std::endl;
	std::cout << "[----------------------------------------]" << std::endl; 
}
