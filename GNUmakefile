CP?=/usr/local/opt/gcc-7.2.0
CC=$(CP)/bin/gcc
CXX=$(CP)/bin/g++

all:	timestamp

timestamp:	timestamp.cc
	$(CXX) -Wall -Wextra -std=c++17 -Wl,-rpath,$(CP)/lib -g -O -o \
	    timestamp timestamp.cc -lpthread

.PHONY:	clean
clean:
	rm -f timestamp timestamp.core
