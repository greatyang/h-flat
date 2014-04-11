# Lookup Strategies & Namespaces

File systems traditionally implement a component-based lookup strategy, reflecting the hierachical nature of the file system namespace. In order to read a file, every directory in the path has to be read as well. If the required metadata is not already cached, this leads to I/O for every single path component. Storage systems that operate in a flat namespace (key-value, object storage), in contrast, use only a single component; this obviously changes lookup performance characteristics completely: It is no longer cache dependant (except for direct hits) and it is no longer file set dependant (doesn't matter how many other files there are or where a file is located in the directory tree). 

The interface offered by flat-namespace storage systems is usually a get-put type of interface that is much less feature rich than the POSIX file system interface. This file system keeps a flat namespace internally while exporting a POSIX interface. While there are positive performance implications as discussed, especially for big systems, this is not a straightforward process. Some file system functionality is inherrently hierachical and can't be directly mapped to a flat namespace. The following section will discuss these cases. 


## Hierachical Functionality in a Flat Namespace

In a component based lookup approach, access permissions for every component of a file path are evaluated independently. Thus, by the time the permissions of a file are checked, it is already verified that the current user has the required access permissions for every directory in the path. With a direct lookup approach, the complete permission set of the file path must be stored with each file. 

The bigger issue, however, are file system operations that can affect many file at once. These are: 

+ changes to directory permissions: when the access permissions of a directory change, it can change the path permissions of all files located somewhere below that directory.
+ directory rename / move operations: when a directory is rename or moved, the paths to all files located below that directory change. 
+ links: a single link to a directory can create an additional valid path to many files

Updating all potentially affected metadata is clearly not a practical solution. Instead, we *remember* these operations in a way that is available during the lookup process. 

Using the knowledge that such an operation occurred can now be used to behave as expected by the user without having touched any of the actual metadata. For example, consider renaming a directory */a* to */b* and trying to access */b/file* afterwards. Due to the knowledge of the move operation the path is internally remapped to */a/file*, resulting in the correct response. The same path-substitution logic can be used for links. For changes to path permissions, the knowledge of the operation allows to detect if the path-permissions stored for a specfic file are potentially stale (older than a change in the file's path) and need to be re-verified. 


### Multiple Clients: Local Knowledge & Effects on File System Semantics
A scenario with multiple independently acting file system clients adds a layer of complexity: The above described *knowledge* about exectued hierachical file system operations has to be available at every client. Querying a remote service for this information is not practical as it is required for every lookup operation. It is not possible, however, to guarantee synchronous updates for an arbitrary number of file system clients. 

Executed operations are stored in system-level files on the regular file system (invisible to a user); each file system client can use this information to update its local knowledge about executed hierachical operations. Local knowledge is therefore updated asynchronously. 

This slightly changes file system semantics: If client **A** moves a directory, this will not be immediately visible to client **B**. Instead, client **B** will be able to keep using the old path of the directory until it updates the database. The same delayed update semantics apply to the other two hierachical file system operations. 

# Architecture

The file system code is decoupled from handling the actual kinetic drives, as shown in the following diagram.  
![Image](../../wiki/kinetic-namespace.png?raw=true)

Conceptually, the file system operates on a single kinetic namespace that it assumes is 100% reliable. How this namespace is sharded to multiple drives and reliability & redundancy issues are handled is the task of a kinetic namespace implementation. Currently there exist two implementations: The simple kinetic namespace forwards all requests to a single drive or simulator instance. The distributed kinetic namespace implements namespace sharding and replication.

# Code Overview 
**src**
+ *pathmapdb* the path-remapping logic used to support hierachical file system functionality  
+ *main* initializing & mounting the file system  
+ *metadata_info* in-memory representation of per-file metadata  
+ *data_info* in-memory data structure to store file data  
+ *util / lookup* utility functions used by other code
+ *lru_cache* a header only lru cache implementation with item expiration    

**src/fuseops**  
Contains the implementations of fuse functions, sorted by category. data.cc, e.g., contains data related functions: read / write / truncate.  src/fuseops.h lists all implemented functions.

**src/generated**  
Contains files that are automatically genearted by google protobuf. protobuf source files are located in src/proto

**src/proto**  
Protobuf definitions for file system data structures that are serialized in order to be stored in permanent storage.
+ metadata.proto > per file metadata, normal inode metadata + extended attributes + path permissions
+ database.proto > record hierachical file system operations such as directory moves
+ replication.proto > storing partition state for cluster management

**src/namespace** 
The namespace interface and simple & distributed implementations. Also contains some helper functions.

