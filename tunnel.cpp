#ifdef WIN32
# include <winsock2.h>
#else
# include <arpa/inet.h>
# include <net/if.h>
# include <netinet/in.h>
# include <sys/ioctl.h>
# include <sys/socket.h>
# ifndef __APPLE__
#  include <linux/if_tun.h>
# endif
#endif

#include <errno.h>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <tox/tox.h>
#include <unistd.h>
#include <stdio.h>

#include "tunnel.h"
#include "main.h"
#include "interface.h"

using namespace std;

Tunnel::Tunnel(int friend_number,std::string myip, std::string peerip, Interface *interface) {
#ifdef WIN32
	cout << "new tunnel" << friend_number << myip << peerip << endl;
	this->handle = 0;
#else
	struct ifreq ifr;
	int err;
	this->friend_number = friend_number;

	struct in_addr peerBinary;
	inet_aton(peerip.c_str(), &peerBinary);
	interface->addPeerRoute(peerBinary,friend_number);
#endif
}
Tunnel::~Tunnel() {
}
