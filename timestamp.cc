/*
 * Copyright (c) 2017 Mellanox Technologies
 * This software was developed by Konstantin Belousov <konstantinb@mellanox.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <netinet/in.h>

#include <iostream>
#include <string>
#include <thread>

enum mode {
	M_UNKNOWN,
	M_SERVER,
	M_CLIENT,
};

enum timer {
	T_UNKNOWN =		10,
	T_BINTIME =		11,
	T_REALTIME_MICRO =	12,
	T_REALTIME =		13,
	T_MONOTONIC =		14,
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

struct ts {
	int timer;
	struct sock_timestamp_info sti;
	union {
		struct timespec t_s;
		struct timeval t_v;
		struct bintime t_b;
	};
};

struct packet {
	uint32_t id;
	struct ts clnt_snd;
	struct ts srv_rcv;
	struct ts srv_snd;
	struct ts clnt_rcv;
};

static const char *
timer_name(int t)
{
	for (struct timer_descr& td : timer_descrs) {
		if (t == td.t)
			return (td.name);
	}
	return (NULL);
}

static std::string
decode_sti_flags(const struct sock_timestamp_info& sti)
{
	std::string res;

	if ((sti.st_info_flags & ST_INFO_HW) != 0)
		res += "HW";
	if ((sti.st_info_flags & ST_INFO_HW_HPREC) != 0)
		res += ",PREC";
	return res;
}

static std::ostream& operator<<
(std::ostream& stream, const struct ts& ts)
{
	stream << "<" << decode_sti_flags(ts.sti) << "> ";
	const char *t_name = timer_name(ts.timer);
	if (t_name == NULL) {
		stream << "Unknown (" << ts.timer << ")";
	} else {
		stream << t_name << "\t";
		switch (ts.timer) {
		case T_BINTIME:
			stream << ts.t_b.sec << "\t" << ts.t_b.frac;
			break;
		case T_REALTIME_MICRO:
		case T_REALTIME:
			stream << ts.t_v.tv_sec << "\t" << ts.t_v.tv_usec;
			break;
		case T_MONOTONIC:
			stream << ts.t_s.tv_sec << "\t" << ts.t_s.tv_nsec;
			break;
			break;
		default:
			break;
		}
	}
	return (stream);
}

static std::ostream& operator<<
(std::ostream& stream, const struct packet& p)
{
	stream << "Packet " << p.id << ":" << std::endl;
	stream << "\tclient sent :\t" << p.clnt_snd << std::endl;
	stream << "\tserver recvd:\t" << p.srv_rcv << std::endl;
	stream << "\tserver sent :\t" << p.srv_snd << std::endl;
	stream << "\tclient recvd:\t" << p.clnt_rcv << std::endl;
	return (stream);
}

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
setup_server(const struct addrinfo *ai, enum timer timer, int& s)
{
	int error;

	s = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
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
setup_client(const struct addrinfo *ai, enum timer timer, int& s)
{
	int error;

	s = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
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

static int
recv_packet(int s, struct packet *p, struct sockaddr *sa, struct ts *ts)
{
	struct msghdr m{};
	struct iovec v[1];
	char control_buf[1024];

	v[0].iov_base = p;
	v[0].iov_len = sizeof(*p);
	m.msg_name = sa;
	m.msg_namelen = sa->sa_len;
	m.msg_iov = v;
	m.msg_iovlen = nitems(v);
	m.msg_control = control_buf;
	m.msg_controllen = sizeof(control_buf);
	int error = recvmsg(s, &m, 0);
	if (error == -1)
		return (error);
	if ((m.msg_flags & MSG_TRUNC) != 0) {
		std::cerr << "truncated packet" << std::endl;
		return (-2);
	}
	if ((m.msg_flags & MSG_CTRUNC) != 0) {
		std::cerr << "truncated control" << std::endl;
		return (-2);
	}

	memset(&ts->sti, 0, sizeof(ts->sti));
	bool stamped = false;
	for (struct cmsghdr *c = CMSG_FIRSTHDR(&m); c != NULL;
	     c = CMSG_NXTHDR(&m, c)) {
		if (c->cmsg_level != SOL_SOCKET)
			continue;
		switch (c->cmsg_type) {
		case SCM_BINTIME:
			ts->timer = T_BINTIME;
			memcpy(&ts->t_b, CMSG_DATA(c), sizeof(ts->t_b));
			stamped = true;
			break;
		case SCM_REALTIME:
			ts->timer = T_REALTIME;
			memcpy(&ts->t_s, CMSG_DATA(c), sizeof(ts->t_s));
			stamped = true;
			break;
		case SCM_TIMESTAMP:
			ts->timer = T_REALTIME_MICRO;
			memcpy(&ts->t_v, CMSG_DATA(c), sizeof(ts->t_v));
			stamped = true;
			break;
		case SCM_MONOTONIC:
			ts->timer = T_MONOTONIC;
			memcpy(&ts->t_s, CMSG_DATA(c), sizeof(ts->t_s));
			stamped = true;
			break;
		case SCM_TIME_INFO:
			ts->sti = *(struct sock_timestamp_info *)CMSG_DATA(c);
			break;
		default:
			break;
		}
	}
	if (!stamped) {
		std::cerr << "no timestamp in control data" << std::endl;
		return (-2);
	}
	return (0);
}

static int
send_packet(int s, struct sockaddr *sa, socklen_t sa_len, enum timer timer,
    struct packet *p, struct ts *ts)
{
	struct timeval tv;
	int error = gettimeofday(&tv, NULL);
	if (error == -1)
		return (-1);

	switch (timer) {
	case T_BINTIME:
		ts->timer = T_BINTIME;
		timeval2bintime(&tv, &ts->t_b);
		break;
	case T_REALTIME_MICRO:
		ts->timer = T_REALTIME_MICRO;
		ts->t_v = tv;
		break;
	case T_REALTIME:
		ts->timer = T_REALTIME;
		TIMEVAL_TO_TIMESPEC(&tv, &ts->t_s);
		break;
	case T_MONOTONIC:
		// XXX
		ts->timer = T_MONOTONIC;
		TIMEVAL_TO_TIMESPEC(&tv, &ts->t_s);
		break;
	default:
		break;
	}

	error = sendto(s, p, sizeof(*p), 0, sa, sa_len);
	return (error);
}

static void
server_loop_step(int s, enum timer timer)
{
	struct packet p;
	struct sockaddr *sa;
	char sa_buf[SOCK_MAXADDRLEN];

	bzero(sa_buf, sizeof(sa_buf));
	sa = (struct sockaddr *)sa_buf;
	sa->sa_len = sizeof(sa_buf);
	int error = recv_packet(s, &p, sa, &p.srv_rcv);
	if (error == -1) {
		error = errno;
		std::cerr << "recv_packet: " << strerror(error) << std::endl;
		return;
	} else if (error == -2) {
		return;
	}

	error = send_packet(s, sa, sa->sa_len, timer, &p, &p.srv_snd);
	if (error == -1) {
		error = errno;
		std::cerr << "send_packet: " << strerror(error) << std::endl;
		return;
	}
}

static uint32_t packet_id;

static void
client_send_loop_step(int s, enum timer timer)
{
	struct packet p{};

	p.id = ++packet_id;
	int error = send_packet(s, NULL, 0, timer, &p, &p.clnt_snd);
	if (error == -1) {
		error = errno;
		std::cerr << "send_packet: " << strerror(error) << std::endl;
		return;
	}
}

static void
client_receive_loop_step(int s)
{
	struct packet p;
	struct sockaddr *sa;
	char sa_buf[SOCK_MAXADDRLEN];

	bzero(sa_buf, sizeof(sa_buf));
	sa = (struct sockaddr *)sa_buf;
	sa->sa_len = sizeof(sa_buf);
	int error = recv_packet(s, &p, sa, &p.clnt_rcv);
	if (error == -1) {
		error = errno;
		std::cerr << "recv_packet: " << strerror(error) << std::endl;
		return;
	} else if (error == -2) {
		return;
	}

	std::cout << p;
}

static void
server_loop(int s, enum timer timer, int count)
{
	for (int i = 0; count == -1 || i < count; i++)
		server_loop_step(s, timer);
}

static void
client_send_loop(int s, enum timer timer, int delay, int count)
{
	for (int i = 0; count == -1 || i < count; i++) {
		client_send_loop_step(s, timer);
		if (delay != 0) {
			struct timespec ts;
			ts.tv_sec = delay / 1000;
			ts.tv_nsec = (delay % 1000) * 1000000;
			nanosleep(&ts, NULL);
		}
	}
}

static void
client_receive_loop(int s, int count)
{
	for (int i = 0; count == -1 || i < count; i++)
		client_receive_loop_step(s);
}

static void
client_loop(int s, enum timer timer, int delay, int count)
{
	std::thread thread(client_send_loop, s, timer, delay, count);
	client_receive_loop(s, count);
	thread.join();
}

static void
usage()
{
	std::cerr << "Usage: timestamp -c|-s -t timer [-h address] "
	    "[-p port] [-d delay(ms)] [-a packet count]\n";
	exit(1);
}

int
main(int argc, char *argv[])
{
	const char *hostname = NULL, *servname = NULL;
	struct addrinfo *ai = NULL;
	int c, count = -1, delay = 0, error, s;
	enum mode mode = M_UNKNOWN;
	enum timer timer = T_UNKNOWN;

	while ((c = getopt(argc, argv, "a:cd:h:t:sp:")) != -1) {
		switch (c) {
		case 'a':
			count = atoi(optarg);
			break;
		case 'd':
			delay = atoi(optarg);
			break;
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
				std::cerr << "Valid timer names are:" <<
				    std::endl;
				for (struct timer_descr& td : timer_descrs)
					std::cerr << "\t" << td.name <<
					    std::endl;
				exit(1);
			}
			break;
		case '?':
		case ':':
		default:
			usage();
		}
	}

	if (mode == M_UNKNOWN || timer == T_UNKNOWN)
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
	const struct addrinfo *cai = ai;
	switch (mode) {
	case M_SERVER:
		for (; cai != NULL; cai = cai->ai_next) {
			if (setup_server(cai, timer, s)) {
				res = 0;
				break;
			}
		}
		break;
	case M_CLIENT:
		for (; cai != NULL; cai = cai->ai_next) {
			if (setup_client(cai, timer, s)) {
				res = 0;
				break;
			}
		}
		break;
	default:
		break;
	}
	if (res != 0) {
		std::cerr << "cannot select address" << std::endl;
		return (res);
	}

	switch (mode) {
	case M_SERVER:
		server_loop(s, timer, count);
		break;
	case M_CLIENT:
		client_loop(s, timer, delay, count);
		break;
	default:
		break;
	}
}
