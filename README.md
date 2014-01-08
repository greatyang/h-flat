POSIX-o-K
=========
POSIX-over-Kinetic. The goal is to be POSIX compliant in a flat namespace while supporting file lookup without path traversal.

## Dependencies
+ [CMake](http://www.cmake.org)
+ **OSX** [libosxfuse](http://osxfuse.github.io)
+ **Linux** *uuid-dev* & *libssl-dev* packages might not be installed, use your favorite package manager to install them 

## Initial Setup
+ Install any missing dependencies
+ Run `cmake .` 

  Tip: Alternatively build out-of-source to stop polluting the sources with cmake generated files. Simply create a build directory anywhere you like and call `cmake <path/to/POSIX-o-K>` from there. 
+ Run `make`

## Debugging
For easy debugging run the file system single threaded & in the foreground `./POSIX-o-K -s -f /mountpoint`