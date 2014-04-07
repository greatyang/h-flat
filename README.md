POSIX-o-K
=========
POSIX-over-Kinetic. The goal is to be POSIX compliant in a flat namespace while supporting file lookup without path traversal.

# Table of Contents
  * [Concepts](#Concepts)
    * [Sub-Project - IOintercept](#IOintercept)
    * [Sub-Project - tools](#tools)
  * [Architecture](#Architecture)
  * [Getting Started](#Dependencies)


#Concepts 

## Sub-Projects
### IOintercept 
The goal of the iointercept library is to enable (at least partial) direct-lookup functionality with operating systems not supporting it natively. 
Which, at the moment, is every operating system.

![Image](wiki/iopath.png?raw=true)

The above diagram shows the path an I/O request takes in a UNIX system until it hits the drive.
The component-based lookup logic is implemented in the Virtual File System. Because a request will only reach a file system after it passes the Virtual File System,
a lookup strategy without path traversal can't be implemented solely in the file system. 
To prevent the VFS from generating multiple file system requests for a single request by the user, the iointercept library intercepts path based file system library calls 
(e.g. open, chmod, etc.) and modifies the file path to hide path components from the VFS. The library has to be preloaded (using LD_PRELOAD on Linux and DYLD_INSERT_LIBRARIES on OSX) to become active. 

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

# Architecture 

# Getting Started  <a id="started"></a>
### Dependencies
+ [CMake](http://www.cmake.org) is used to build the project
+ **OSX** [libosxfuse](http://osxfuse.github.io) 
+ **Linux** *libssl-dev* and *uuid-dev* packages

### Initial Setup
+ Install any missing dependencies
+ Clone the git repository 
+ Create a build directory. If you want you can use the cloned git repository as your build directory, but using a separate directory is recommended in order to cleanly separate sources and generated files. 
+ From your build directory call `cmake "path/to/cloned-git"`, if you're using the cloned git repository as your build directory this would be `cmake .` 
+ Run `make`

+ Note that the *iointercept* and *tools* sub-projects have independent cmake files, if you whish to build them repeat the above process. 


### Configure & Execute
To mount run the executable given the mountpoint as a parameter. Some flags interesting for debugging: 

+ -s single threaded mode
+ -f foreground mode: sends all debug to std out 
+ -d debug mode: fuse lists internal function calls in addition to pok_debug output
+ -o mount options, note that *allow_other*, *use_ino*, and *attr_timeout=0* fuse mount options are required for POSIX compliant behavior, 

Example: `./POSIX-o-K -s -f -o allow_other,use_ino,attr_timeout=0 /mountpoint` 
