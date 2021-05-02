# Set you prefererred CFLAGS/compiler compiler here.
# Our github runner provides gcc-10 by default.
CC ?= cc
CFLAGS ?= -g -Wall -O2
CXX ?= c++
CXXFLAGS ?= -g -Wall -O2
CARGO ?= cargo
RUSTFLAGS ?= -g

# this target should build all executables for all tests
all:
	@echo "Please set a concrete build command here"
	false

# C example:
#all:
#	$(CC) $(CFLAGS) -o memfs memfs.c `pkg-config fuse --cflags --libs`

# C++ example:
#all:
#	$(CXX) $(CXXFLAGS) -o memfs memfs.cpp `pkg-config fuse --cflags --libs`

# Rust example:
#all:
#	$(CARGO) build --release

# Usually there is no need to modify this
check: all
	$(MAKE) -C tests check
