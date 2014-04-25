# Kinetic Drives

Seagate Kinetic drives provide a low-level key-value interface (get/put/delete). A main feature of Kinetic is that it can provide test-and-set behavior using versioned keys: A put or delete operation will only succeed if the remote key version equals the key version that is supplied by the client. Using this feature, most of the the concurrency issues of a distributed / parallel file system that are traditionally handled by separate metadata servers can instead be implemented by directly using the drives. As a result, a serverless design consisting only of clients and kinetic drives becomes feasible. 

![Image](../../wiki/distributed-fs.png?raw=true)

The file system code is decoupled from handling the actual kinetic drives. It operates on a single kinetic namespace, which supplies a global key-value namespace. All implementation logic is encapsulated by a kinetic namespace implementation: The number of connected drives, how the global namespace is mapped to the individual drives, how (or even if) redundancy & reliability issues are handled. Currently there are two kinetic namespace implementations: The simple kinetic namespace forwards all requests to a single drive or simulator instance. The distributed kinetic namespace implements namespace sharding and replication. It should be noted, however, that the focus of this project lies in the file system part, not in providing a well-rounded distributed key-value store implementation on top of the basic kinetic api. As such, more advanced features such as changing the number of partitions in the cluster for an existing system are not supported.  

![Image](../../wiki/kinetic-namespace.png?raw=true)


# Lookup Strategies & Namespaces

File systems traditionally implement a component-based lookup strategy, reflecting the hierachical nature of the file system namespace. In order to read a file, every directory in the path has to be read as well. If the required metadata is not already cached, this leads to I/O for every single path component. 

Storage systems that operate in a flat namespace (key-value, object storage), in contrast, use only a single component. This changes lookup performance characteristics fundamentally: Lookup is no longer cache dependant (except for direct hits) and it is no longer file set dependant (doesn't matter how many other files there are or where a file is located in the directory tree). Both of these characteristics gain importance as a file system grows: While for small systems caching can mask the effect of component based lookup almost completely, metadata performance can become the defining performance characteristic in big file systems. 

The interface offered by flat-namespace storage systems is usually a get-put type of interface that is much less feature rich than the POSIX file system interface. The h-flat file system attempts to provide full POSIX functionality while retaining the performance and scalability characteristics of a key-value storage system. However, some file system functionality is inherrently hierachical and can't be directly mapped to a flat namespace. The following section will discuss these cases. 

## Hierachical Functionality in a Flat Namespace

In a component based lookup approach, access permissions for every component of a file path are evaluated independently. Thus, by the time the permissions of a file are checked, it is already verified that the current user has the required access permissions for every directory in the path. With a direct lookup approach, the complete permission set of the file path must be stored with each file. 

The bigger issue, however, are file system operations that can affect many file at once. These are: 

+ changes to directory permissions: when the access permissions of a directory change, it can change the path permissions of all files located somewhere below that directory
+ directory rename / move operations: when a directory is rename or moved, the paths to all files located below that directory change
+ links: a single link to a directory can create an additional valid path to many files

Updating all potentially affected metadata is clearly not a practical solution. Instead, we *remember* these operations in a way that is available during the lookup process. 

Using the knowledge that such an operation occurred can now be used to behave as expected by the user without having touched any of the actual metadata. For example, consider renaming a directory */a* to */b* and trying to access */b/file* afterwards. Due to the knowledge of the move operation the path is internally remapped to */a/file*, resulting in the correct response. The same path-substitution logic can be used for links. For changes to path permissions, the knowledge of the operation allows to detect if the path-permissions stored for a specfic file are potentially stale (older than a change in the file's path) and need to be re-verified. 


### Multiple Clients: Local Knowledge & Effects on File System Semantics
A scenario with multiple independently acting file system clients adds a layer of complexity: The above described *knowledge* about exectued hierachical file system operations has to be available at every client. Querying a remote service for this information is not practical as it is required for every lookup operation. It is not possible, however, to guarantee synchronous updates for an arbitrary number of file system clients. This leads to an architecture where the local knowledge of a client might not be equivalent on all clients. The following diagram shows three clients, each with a different snapshot of the global database storing hierachical changes to the file system. 

![Image](../../wiki/multi-client.png?raw=true)

All database entries are stored on the Kinetic drives,  which can be accessed as usual by all clients in order to update their local snapshot. When a client executes a hierarchical file system operation it creates a new database entry 

Executed operations are stored in system-level files on the regular file system (invisible to a user); each file system client can use this information to update its local knowledge about executed hierachical operations. Local knowledge is therefore updated asynchronously. 

This slightly changes file system semantics: If client **A** moves a directory, this will not be immediately visible to client **B**. Instead, client **B** will be able to keep using the old path of the directory until it updates the database. The same delayed update semantics apply to the other two hierachical file system operations. 


![Image](../../wiki/h-flat.png?raw=true)

# Code Overview 

**src/fuseops**  
Contains the implementations of fuse functions, sorted by category. data.cc, e.g., contains data related functions: read / write / truncate. All implemented functions are listed in src/fuseops.h

**src/proto**  
Protobuf definitions for file system data structures that are serialized at some point to be stored in a key-value pair. Compiled protbuf definition are placed in **src/generated**  
+ metadata.proto > per file metadata, normal inode metadata + extended attributes + path permissions
+ database.proto > record hierachical file system operations such as directory moves
+ replication.proto > storing partition state for cluster management

**src/namespace** 
The namespace interface and implementations as well as helper functions for common namespace accesses. 

**src**
File system initialization & mounting, the path-remapping logic used to support hierachical file system functionality, in memory representations of data & metadata, the cache implemenatation and general utility functions
