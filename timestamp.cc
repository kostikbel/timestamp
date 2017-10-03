#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

#include <iostream>

static bool
do_server(const struct addrinfo *ai)
{
	return (false);
}

static bool
do_client(const struct addrinfo *ai)
{
	return (false);
}

enum mode {
	M_UNKNOWN,
	M_SERVER,
	M_CLIENT
};

static void
usage()
{
	std::cerr << "Usage: timestamp -c|-s [-h address] [-p port] [-t timer]\n";
	exit(1);
}

int
main(int argc, char *argv[])
{
	const char *hostname = NULL, *servname = NULL;
	struct addrinfo *ai = NULL;
	int c, error;
	enum mode mode = M_UNKNOWN;

	while ((c = getopt(argc, argv, "ch:t:sp:")) != -1) {
		switch (c) {
		case 'c':
			mode = M_CLIENT;
			break;
		case 's':
			mode = M_SERVER;
			break;
		case 'h':
			hostname = optarg;
			break;
		case 'p':
			servname = optarg;
			break;
		case '?':
		case ':':
		default:
			usage();
		}
	}

	if (mode == M_UNKNOWN)
		usage();

	if (hostname != NULL || servname != NULL) {
		error = getaddrinfo(hostname, servname, NULL, &ai);
		if (error != 0)
			std::cerr << "Can't resolve address: " <<
			    gai_strerror(error) << std::endl;
	}

	int res = 1;
	switch (mode) {
	case M_SERVER:
		for (const struct addrinfo *cai = ai; cai != NULL;
		    cai = cai->ai_next) {
			if (do_server(cai)) {
				res = 0;
				break;
			}
		}
		break;
	case M_CLIENT:
		for (const struct addrinfo *cai = ai; cai != NULL;
		    cai = cai->ai_next) {
			if (do_client(cai)) {
				res = 0;
				break;
			}
		}
		break;
	default:
		break;
	}
	return (res);
}
