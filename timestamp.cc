#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <netinet/in.h>

#include <iostream>

enum mode {
	M_UNKNOWN,
	M_SERVER,
	M_CLIENT,
};

enum timer {
	T_UNKNOWN,
	T_BINTIME,
	T_REALTIME_MICRO,
	T_REALTIME,
	T_MONOTONIC,
};

struct timer_descr {
	const char *name;
	enum timer t;
} timer_descrs[] = {
	{ "bintime",		T_BINTIME },
	{ "realtime_micro", 	T_REALTIME_MICRO },
	{ "realtime",		T_REALTIME },
	{ "monotonic",		T_MONOTONIC },
};

static bool
timestamp_sockopt(int s, enum timer t)
{
	if (t == T_BINTIME) {
		int val = 1;
		if (setsockopt(s, SOL_SOCKET, SO_BINTIME, &val, sizeof(val))
		    == -1) {
			int error = errno;
			std::cerr << "SO_BINTIME" << strerror(error) << std::endl;
			return (false);
		}
		return (true);
	}

	int val = 1;
	if (setsockopt(s, SOL_SOCKET, SO_TIMESTAMP, &val, sizeof(val)) == -1) {
		int error = errno;
		std::cerr << "SO_TIMESTAMP" << strerror(error) << std::endl;
		return (false);
	}
	switch (t) {
	case T_REALTIME_MICRO:
		val = SO_TS_REALTIME_MICRO;
		break;
	case T_REALTIME:
		val = SO_TS_REALTIME;
		break;
	case T_MONOTONIC:
		val = SO_TS_MONOTONIC;
		break;
	default:
		return (false);
	}
	if (setsockopt(s, SOL_SOCKET, SO_TS_CLOCK, &val, sizeof(val)) == -1) {
		int error = errno;
		std::cerr << "SO_TS_CLOCK" << strerror(error) << std::endl;
		return (false);
	}
	return (true);
}

static bool
do_server(const struct addrinfo *ai, enum timer timer)
{
	int error;

	int s = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
	if (s == -1) {
		error = errno;
		std::cerr << "socket: " << strerror(error) << std::endl;
		return (false);
	}
	if (bind(s, ai->ai_addr, ai->ai_addrlen) == -1) {
		error = errno;
		close(s);
		std::cerr << "bind: " << strerror(error) << std::endl;
		return (false);
	}
	if (!timestamp_sockopt(s, timer))
		return (false);
	
	return (true);
}

static bool
do_client(const struct addrinfo *ai, enum timer timer)
{
	int error;

	int s = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
	if (s == -1) {
		error = errno;
		std::cerr << "socket: " << strerror(error) << std::endl;
		return (false);
	}
	if (connect(s, ai->ai_addr, ai->ai_addrlen) == -1) {
		error = errno;
		close(s);
		std::cerr << "bind: " << strerror(error) << std::endl;
		return (false);
	}
	if (!timestamp_sockopt(s, timer))
		return (false);
	
	return (true);
}

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
	enum timer timer = T_UNKNOWN;

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
		case 't':
			for (struct timer_descr& td : timer_descrs) {
				if (strcmp(optarg, td.name) == 0) {
					timer = td.t;
					break;
				}
			}
			if (timer == T_UNKNOWN) {
				std::cerr << "Valid timer names are:" << std::endl;
				for (struct timer_descr& td : timer_descrs)
					std::cerr << "\t" << td.name << std::endl;
				exit(1);
			}
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
		struct addrinfo hints{};

		hints.ai_socktype = SOCK_DGRAM;
		hints.ai_protocol = IPPROTO_UDP;
		hints.ai_flags = AI_ADDRCONFIG;
		if (mode == M_SERVER)
			hints.ai_flags |= AI_PASSIVE;
		error = getaddrinfo(hostname, servname, &hints, &ai);
		if (error != 0)
			std::cerr << "Can't resolve address: " <<
			    gai_strerror(error) << std::endl;
	}

	int res = 1;
	switch (mode) {
	case M_SERVER:
		for (const struct addrinfo *cai = ai; cai != NULL;
		    cai = cai->ai_next) {
			if (do_server(cai, timer)) {
				res = 0;
				break;
			}
		}
		break;
	case M_CLIENT:
		for (const struct addrinfo *cai = ai; cai != NULL;
		    cai = cai->ai_next) {
			if (do_client(cai, timer)) {
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
