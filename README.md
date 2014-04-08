# Table of Contents
  * [Core Concepts](#concepts)
    * [Problem Cases](#problem-cases) 
  * [Architecture](#architecture)
  * [Sub-Projects](#sub-projects)  
    * [IO Interception Library](#iointercept)
    * [File System Tools](#file-system-tools)
  * [Getting Started](#getting-started)
  * [Testing](#testing)

# Concepts 

File systems implement a component-based lookup strategy, reflecting the hierachical nature of the file system namespace. In order to read a file, every directory in the path has to be read as well. If the required metadata is not already cached, this leads to I/O for every single path component. Storage systems that operate in a flat namespace (key-value, object storage), in contrast, have only a single component; this drastically changes lookup performance. 

The main concept behind POSIX-over-Kinetic is to provide standard hierachical file system semantics (POSIX) while using a flat namepsace internally; to combine the performance and scalability of a key-value storage system with a file system interface. To this purpose, the full path of the file is considered the files key / its object id. Using this key the file metadata can be retrieved without accessing the individual directories of the path.

In a component based lookup approach, access permissions for every component of a file path are evaluated independently. Thus, by the time the permissions of a file are checked, it is already verified that the current user has the required access permissions for every directory in the path. With a direct lookup approach, the complete permission set of the file path must be stored with each file. 

### Problem Cases
Skipping component traversal during the lookup process introduces new challenges: Some file system functionality is inherrently hierachical and can't be implemented in a straightforward manner in a flat namespace. 

+ changes to directory permissions: when the access permissions of a directory change, it can change the path permissions of all files located somewhere below that directory.
+ directory rename / move operations: when a directory is rename or moved, the paths to all files located below that directory change. 
+ links: a single link to a directory can create an additional valid path to many files

The three problem cases above have in common that a single file system operation can affect an arbitrary number of files. Updating all potentially affected metadata is clearly not a practical solution. Instead, we *remember* these operations in a way that is available during the lookup process. 

Using the knowledge that such an operation occurred can now be used to behave as expected by the user without having touched any of the actual metadata. For example, consider renaming a directory */a* to */b* and trying to access */b/file* afterwards. Due to the knowledge of the move operation the path is internally remapped to */a/file*, resulting in the correct response. The same path-substitution logic can be used for links. For changes to path permissions, the knowledge of the operation allows to detect if the path-permissions stored for a specfic file are potentially stale (older than a change in the file's path) and need to be re-verified. 




# Architecture 
This section intends to give a brief architecture overview to help making sense of the code. 



## Sub-Projects
### IOintercept 
The goal of the iointercept library is to enable (at least partial) direct-lookup functionality with operating systems not supporting it natively. Which, at the moment, is every operating system.

![Image](../../wiki/iopath.png?raw=true)

The above diagram shows the path an I/O request takes in a UNIX system until it hits the drive. The component-based lookup logic is implemented in the Virtual File System. Because a request will only reach a file system after it passes the Virtual File System, a lookup strategy without path traversal can't be implemented solely in the file system. To prevent the VFS from generating multiple file system requests for a single request by the user, the iointercept library intercepts path based file system library calls (e.g. open, chmod, etc.) and modifies the file path to hide path components from the VFS. The library has to be preloaded (using LD_PRELOAD on Linux and DYLD_INSERT_LIBRARIES on OSX) to become active. 

Example Request: */a/mountpoint/path/to/file* 
Changed by iontercept to a request to */a/mountpoint/path:to:file*

In effect, the Virtual File System sees all files as direct children of the file system mountpoint, disabling any component-based VFS functionality. 


### File System Tools
The file system tool suite located in the *tools* directory enables file system and name space checks. 

#####File System Check - fsck 
Repair invalid file system state that might occur when a file system client crashes or is disconnected in the middle of a multi-step metadata operation. 
Every invalid file system state can be detected by dangling directory entries (entries that have no metadata). fsck takes a directory path as a parameter, as walking
the whole directory tree is unnecessary and for big systems also inpractical. If a dangling directory entry exists after a crash, simply execute fsck with the affected directory as a parameter.
This operation can be done online. 

#####Name Space Check - nsck 
Execute a self-check on the underlying key-value namespace. In the case of a distributed kinetic namespace, the connection to all kinetic drives listed in the cluster definition will be checked. 
Should connection to a drive that is marked as inaccesible be succesfull, it is re-integrated into the existing cluster. This operation can be done online. 

# Getting Started  <a id="started"></a>
### Dependencies
+ [CMake](http://www.cmake.org) is used to build the project
+ **OSX** [libosxfuse](http://osxfuse.github.io) 
+ **Linux** *libssl-dev* and *uuid-dev* packages

### Initial Setup
+ Install any missing dependencies
+ Clone the git repository 
+ Create a build directory. If you want you can use the cloned git repository as your build directory, but using a separate directory is recommended in order to cleanly separate sources and generated files. 
+ From your build directory call `cmake /path/to/cloned-git`, if you're using the cloned git repository as your build directory this would be `cmake .` 
+ Run `make`

+ Note that the *iointercept* and *tools* sub-projects have independent cmake files, if you whish to build them repeat the above process. 


### Configure & Execute
To mount run the executable given the mountpoint as a parameter. Some flags interesting for debugging: 

+ -s single threaded mode
+ -f foreground mode: sends all debug to std out 
+ -d debug mode: fuse lists internal function calls in addition to pok_debug output
+ -o mount options, note that *allow_other*, *use_ino*, and *attr_timeout=0* fuse mount options are required for POSIX compliant behavior, 

Example: `./POSIX-o-K -s -f -o allow_other,use_ino,attr_timeout=0 /mountpoint` 

# Testing
File system functionality has been tested with the following tools: 
+ [POSIX Test Suite](http://www.tuxera.com/community/posix-test-suite/)
+ [File System Exerciser](http://codemonkey.org.uk/projects/fsx/)


