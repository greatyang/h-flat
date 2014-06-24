# Table of Contents
  * [Core Concepts](#the-file-system)
  * [Getting Started](#getting-started)
    * [Initial Setup](#initial-setup)
    * [Mounting the File System](#mount)
    * [Configuration](#configuration)
  * [Sub-Projects](#sub-projects)  
    * [IO Interception Library](#iointercept)
    * [File System Tools](#file-system-tools)
  * [Testing](#testing)

# The File System 
*H-flat* stands for *Hierarchical Functionality in a Flat Namespace*. The core concept, accordingly, is to provide standard hierarchical file system semantics (POSIX) while using a flat namespace internally; to combine the performance and scalability of a key-value storage system with a file system interface. 

To achieve key-value performance characteristics the full path of a file is considered the file's key. Using this key the file metadata can be retrieved without accessing the individual directories of the path. Some detail about the implications of skipping directory traversal as well as the file system architecture can be found [here](README_ARCH.md).


# Getting Started
You might want to get the [kinetic simulator](https://github.com/Seagate/kinetic-java). Especially if you don't happen to have a bunch of kinetic drives lying around. 

### Dependencies
+ **OSX** [libosxfuse](http://osxfuse.github.io),
 + Tip: make your life easier by using [homebrew](http://brew.sh) to install dependencies 
+ **Linux** *libfuse-dev* 
+ A git client, a c++ compiler and [CMake](http://www.cmake.org). Other dependencies should be resolved automatically during the build process. 

### Initial Setup
+ Install any missing dependencies
+ Clone the git repository 
+ Create a build directory. If you want you can use the cloned git repository as your build directory, but using a separate directory is recommended in order to cleanly separate sources and generated files. 
+ From your build directory call `cmake /path/to/cloned-git`, if you're using the cloned git repository as your build directory this would be `cmake .` 
+ Run `make`

### Mount
To mount run the executable given the mountpoint as a parameter. Use -o to specify mount options. Note that *allow_other*, *use_ino*, and *attr_timeout=0* fuse mount options are **required** for POSIX compliant behavior

Some flags interesting for debugging: 
+ -s single threaded mode
+ -f foreground mode: sends all debug to std out 
+ -d debug mode: list internal fuse function calls in addition to file system debug output

Example: `./hflat -s -f -o allow_other,use_ino,attr_timeout=0 /mountpoint` 


### Configuration

See [example.cfg](example.cfg) in sources for an example configuration. Use *-cfg=filename* as a mount option. This mount option can be used at any place in the parameter list, including as a last parameter beyond the mount point. 

Example: `./hflat -o allow_other,use_ino,attr_timeout=0 /mountpoint -cfg=example.cfg` 

#### Cluster Configuration
If no kinetic cluster configuration is supplied at mount time, the file system attempts to connect to a single kinetic simulator instance running on localhost with standard parameters. 

The cluster configuration defines the **clustermap** and **log** variables using the following scheme:

     Drive      = {host;port;status;}
     Partition  = Drive+
     clustermap = Partition+
     log        = Partition 
     
There are three valid values for status: 
+ GREEN : The kinetic drive is in the read & write quorums, it is up-to-date on all keys
+ YELLOW: The drive might have missed key updates, it is in the write quorum but not the read quorum
+ RED   : The drive is currently broken / unreachable. 

Keys are sharded across all existing partitions of the clustermap. Keys written to a partition are replicated among all drives of the partition. Typical configurations would therefore be 1 drive per partition for no redundancy and 3 drives per partition for triple redundancy. The optional log partition is used to increase rebuild speed for temporarily unavailable drives in the clustermap. 

#### Client Configuration
##### Read Cache
The file system client implements a read-only cache to speed multiple requests to the same metadata / data. An auto-expiration time can be specified in milliseconds using the **cache_expiration** variable. A longer expiration time generally improves performance while a shorter expiration time improves agility: While stale cache items are detected on write & automatically resolved, multiple clients working on shared files can experience an additional delay until changes to a file become visible for read-only operations such as stat.

It follows that different read-cache strategies are optimal depending on the way clients use the file system. In case of a single client or clients working in a completely non-overlapping manner it can be disabled (set to 0) for optimal performance. A setting of 1000 millisecond combines performance and agility when many clients perform operations on shared files and directories. If sharing files / directories is done only infrequently, a higher cache timeout value can be chosen. 

*Default value: 1000* 

##### POSIX Compliance
If **posix_mode** is set to *FULL*, ctime and mtime attributes are always updated according to POSIX specification. If set to *RELAXED*, ctime and mtime updates are skipped for performance reasons in certain scenarios. This allows, for example, file creation without writing to the directory (which could be a bottleneck in case of concurrent created in a distributed setting). 

*Default value: FULL*



## Sub-Projects
### IOintercept 
The goal of the iointercept library is to enable (at least partial) direct-lookup functionality with operating systems not supporting it natively. Which, at the moment, is every operating system.

![Image](../../wiki/iopath.png?raw=true)

The above diagram shows the path an I/O request takes in a UNIX system until it hits the drive. The component-based lookup logic is implemented in the Virtual File System. Changes to the VFS itself require a kernel patch and thus introduce a high barrier for usage as well as limited portability. 

The second option is to introduce a change *before* the request hits the VFS layer. This is the approach taken by the iointercept library. Path based glibc file system library calls (e.g. open, chmod, etc.) are intercepted and the file paths are modified in a way to hide all path components below the file system mount point from the VFS. Library call interception is achieved using the library preloading method (LD_PRELOAD on Linux and DYLD_INSERT_LIBRARIES on OSX). 

Example Request: ` /a/mountpoint/path/to/file -> /a/mountpoint/path:to:file`

As a result, the Virtual File System sees all files of the file system as direct children of the file system mountpoint, disabling component-based VFS functionality and enabling lookup without path traversal. 


### File System Tools
The file system tool suite located in the *tools* directory enables file system and name space checks. 

#####File System Check 
Repairs invalid file system state that might occur when a file system client crashes or is disconnected in the middle of a multi-step metadata operation. Every invalid file system state can be detected by dangling directory entries (entries that have no metadata). fsck takes a directory path as a parameter, as walking the whole directory tree is unnecessary and for big systems also impractical. This operation can be done online. 

Example: `./tools -fsck /path/to/directory/with/dangling/direntries` 

#####Name Space Check 
Execute a self-check on the underlying key-value namespace. In the case of a distributed kinetic namespace, the connection to all kinetic drives listed in the cluster definition will be checked. Should connection to a drive that is marked as inaccesible be succesfull, it is re-integrated into the existing cluster. This operation can be done online. 

Example: `./tools -nsck /path/to/mountpoint` 


# Testing
The following tools can be (and have been) used to test file system functionality. 
+ [POSIX Test Suite](http://www.tuxera.com/community/posix-test-suite/)
+ [File System Exerciser](http://codemonkey.org.uk/projects/fsx/)

