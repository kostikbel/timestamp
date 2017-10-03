CC=/usr/local/opt/gcc-7.2.0/bin/gcc
CXX=/usr/local/opt/gcc-7.2.0/bin/g++

all:	timestamp

timestamp:	timestamp.cc
	$(CXX) -Wall -Wextra -std=c++17 -g -O -o timestamp timestamp.cc

clean:	.PHONY
clean:
	rm -f timestamp timestamp.core
