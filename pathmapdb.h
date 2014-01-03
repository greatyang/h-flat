#ifndef PATHMAPDB_H
#define PATHMAPDB_H

#include <unordered_map>
#include <atomic>
#include <mutex>
#include <string>

/* There are slight differences in handling path mapping depending on where the call originates. */
enum class CallingType { LOOKUP, READLINK };

class PathMapDB final
{
private:
	enum class TargetType { MOVE, REUSE, LINK, NONE };
	struct PMEntry{
		TargetType   type; 
		std::string  target; 
		std::int64_t permissionTimeStamp;
	};

	/* Version & hashmap of current snapshot */ 
	std::int64_t snapshotVersion;
	std::unordered_map<std::string, PMEntry> snapshot;
	
	/* Synchronization: Ensure nobody is reading while the snapshot is being updated. */ 
	mutable std::atomic<int> currentReaders; 
	mutable std::mutex lock; 
	
private:
	bool searchPathRecursive(std::string& path, std::int64_t &maxTimeStamp, const bool afterMove, const bool followSymlink) const;

public:
	explicit PathMapDB();
	~PathMapDB();
	PathMapDB(const PathMapDB& rhs) = delete;
	PathMapDB& operator=(const PathMapDB& rhs) = delete;
	
public: 
	/* Remaps supplied path according to current database snapshot. 
	 * userPath -> systemPath
	 * minimal required path permission timestamp value is stored in supplied integer. */
	std::string toSystemPath(const char * user_path, std::int64_t &permissionTimeStamp, CallingType ctype) const;


	/* Return current database snapshot version */ 
	std::int64_t getSnapshotVersion() const;
	
	/* Update the current database snapshot to newest version. 
	 * Returns new database snapshot version on success or a negative
	   error code */
	std::int64_t updateSnapshot(); 
	
	/* It is the responsiblity of the client to ensure that these functions are only called after
	 * a successful update of the remote database (as an alternative of calling updateSnapshot()). */
	void addDirectoryMove	(std::string origin, std::string destination);
	void addSoftLink	 	(std::string origin, std::string destination);
	void addPermissionChange(std::string path);

	/* DEBUG ONLY */ 
	void printSnapshot() const; 
};

#endif // PATHMAPDB_H
