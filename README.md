POSIX-o-K
=========
POSIX-over-Kinetic. The goal is to be POSIX compliant in a flat namespace while supporting file lookup without path traversal.


## Dependencies
+ [Kinetic-C-Client](https://github.com/Seagate/Kinetic-C-Client)
+ **OSX** [libosxfuse](http://osxfuse.github.io)
+ **Linux** uuid-dev libssl-dev


## Initial Setup
See *Environment Setup* section of the makefile. Path to Kinetic-C-Client directory needs to be set correctly. 

## Debugging
For easy debugging run the file system single threaded & in the foreground (see mount target in Makefile)