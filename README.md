POSIX-o-K
=========
POSIX-over-Kinetic. The goal is to be POSIX compliant in a flat namespace while supporting file lookup without path traversal.

## Dependencies
+ [CMake](http://www.cmake.org)
+ **OSX** [libosxfuse](http://osxfuse.github.io)
+ **Linux** *libssl-dev* package might not be installed, use your favorite package manager to install them 

## Initial Setup
+ Install any missing dependencies
+ Run `cmake .` 
 + Tip: Alternatively build out-of-source to stop polluting the sources with cmake generated files. Simply create a build directory anywhere you like and call `cmake <path/to/POSIX-o-K>` from there. 
+ Run `make`

## Running
At the moment the file system expects a LC2 simulator instance with default values to be running on localhost. Mounting will not be successful if this is not the case.
To mount run the executable given the mountpoint as a parameter. Some flags interesting for debugging: 

+ -s single threaded mode
+ -f foreground mode: sends all debug to std out 
+ -d debug mode: fuse lists internal function calls in addition to pok_debug output
+ -o mount options, note that *allow_other*, *use_ino*, and *attr_timeout=0* fuse mount options are required for POSIX compliant behavior, 

Example: `./POSIX-o-K -s -f -o allow_other,use_ino /mountpoint` 

## Direct Lookup
To achieve (partial) direct-lookup functionality with operating systems not supporting it natively (which, at the moment, is everybody), use iointercept library.  

This library implements path-based file system library functions (e.g. open, chmod, etc.). The file path is modified so that all path-components below a supplied mountpoint appear as a single path component to the operating system. 

E.g. /a/mountpoint/path/to/file -> /a/mountpoint/path:to:file

It is intended to be preloaded (using LD_PRELOAD on Linux and DYLD_INSERT_LIBRARIES on OSX) in order to overload the standard implementations. 
User -> {intercept} -> libc -> kernel -> fuse -> POK