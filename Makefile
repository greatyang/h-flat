#################################
# Environment Setup 			
KINETIC_CLIENT := $(HOME)/git/Kinetic-C-Client
MOUNT_DIR	   := /scratch

#################################
# Yes, the Kinetic-C-Client is weird... there really shouldn't be outside dependencies in include/kinetic
PUBLIC	:= $(KINETIC_CLIENT)/include
MAIN	:= $(KINETIC_CLIENT)/src/main
GEN		:= $(KINETIC_CLIENT)/src/main/generated
PROTO 	:= $(KINETIC_CLIENT)/vendor/src/protobufutil/include
PROTOV  := $(KINETIC_CLIENT)/vendor/src/protobufutil/vendor/include
GOOGLE  := $(KINETIC_CLIENT)/vendor/src/protobufutil/vendor/include/google
GMOCK 	:= $(KINETIC_CLIENT)/vendor/src/protobufutil/vendor/src/gmock/fused-src

KINETIC_INCLUDES := -I$(GMOCK) -I$(GOOGLE) -I$(PROTOV) -I$(PROTO) -I$(GEN) -I$(MAIN) -I$(PUBLIC) 

LPROTO	:= $(KINETIC_CLIENT)/vendor/src/protobufutil
LVENDOR := $(KINETIC_CLIENT)/vendor/src/protobufutil/vendor/lib

KINETIC_LIBS := -L$(KINETIC_CLIENT) -L$(LPROTO) -L$(LVENDOR) -lkinetic_client -lprotobufutil -lglog -lgflags -lssl -lcrypto -lprotobuf
#################################

DIR 	  := $(PWD) 
SRC       := $(wildcard *.cc)
OBJ       := $(patsubst %.cc,%.o,$(SRC))
TARGET    := POSIX-o-K


TESTDIR   := $(PWD)/tests/
TESTSRC   := $(foreach sdir,$(TESTDIR),$(wildcard $(sdir)*.cc))
TESTOBJ   := $(patsubst $(PWD)/%.cc,%.o,$(TESTSRC))
TESTTARGET:= test-pok

PROTOSRC  := $(wildcard *.proto)

CPPFLAGS  		:= -std=c++11 -O2 -g -Wfatal-errors -Wall -Wextra -Wno-unknown-warning-option -Wno-unused-parameter -Wno-unused-local-typedefs -DGTEST_USE_OWN_TR1_TUPLE=1 -D__STDC_FORMAT_MACROS $(KINETIC_INCLUDES) 
LIBS			:= $(KINETIC_LIBS)

OS := $(shell uname)
ifeq ($(OS),Darwin)
	CPPFLAGS  	+= -D_FILE_OFFSET_BITS=64 -DFUSE_USE_VERSION=26 -I/usr/local/include/osxfuse
	LIBS  		+= -losxfuse
endif
ifeq ($(OS),Linux)
	CPPFLAGS  	+= `pkg-config fuse --cflags`
	LIBS  		+= -ldl -luuid `pkg-config fuse --libs`
endif

all:  test proto $(OBJ)
	  $(CXX)  $(CPPFLAGS) $(OBJ) $(LIBS) -o $(TARGET)

test: clean $(TESTOBJ)
	  $(CXX) $(CPPFLAGS) $(TESTOBJ) $(LIBS) -o $(TESTTARGET)

clean:
	rm -f $(OBJ) $(TESTOBJ) $(TARGET) $(TESTTARGET)

proto: 
	protoc -I=./ --cpp_out=./ $(PROTOSRC) 
	
# baby steps. single threaded & forground mode
mount: all
	./$(TARGET) -f -s $(MOUNT_DIR)
