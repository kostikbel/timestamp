#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

#include <iostream>

enum mode {
	M_SERVER,
	M_CLIENT
};

static void
usage()
{
	std::cerr << "Usage: timestamp [-cs] [-h address] [-p port] [-t timer]\n";
	exit(1);
}

int
main(int argc, char *argv[])
{
	int c;
	enum mode mode;

	while ((c = getopt(argc, argv, "ch:t:sp:")) != -1) {
		switch (c) {
		case 'c':
			mode = M_CLIENT;
			break;
		case 's':
			mode = M_SERVER;
			break;
		case 'h':
			// XXX
			break;
		case 'p':
			// XXX
			break;
		case '?':
		case ':':
		default:
			usage();
		}
	}

	switch (mode) {
	case M_SERVER:
		break;
	case M_CLIENT:
		break;
	}
}
