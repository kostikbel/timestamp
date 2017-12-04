This is a test program to check the SO_TIMESTAMP functionality on
FreeBSD.  It includes both client and server and depends on
functionality which was included into FreeBSD HEAD by
https://svnweb.freebsd.org/base?view=revision&revision=325507.

Usage:
```
	timestamp -s options -- server mode
	timestamp -c options -- client mode
```
Options:
```
	-t timestampt format, use -t help to see possible values
	-h address to listen to (server mode), to sends packets to (client mode)
	-p port
	-a count -- process specified number of packets
	-d ms -- client mode, delay between sends
```
Client sends packets to server, server bounces the received packets
back to the client.  In each packet there are four slots for
timestamps:
* client send
* server receive
* server send
* client receive

Corresponding process fills the slot.  After reception, client prints
all the slots, together with flags indicating hardware and
high-precision stamp.