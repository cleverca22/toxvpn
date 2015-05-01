#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <iostream>

#ifndef __APPLE__
#include <linux/if_tun.h>
#endif

#include <net/if.h>
#include <netinet/in.h>
#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <string.h>
#include <tox/tox.h>
#include <unistd.h>

#include "tunnel.h"
#include "main.h"

using namespace std;

Tunnel::Tunnel(int friend_number,std::string myip, std::string peerip) {
	struct ifreq ifr;
	int fd,err;
	this->friend_number = friend_number;

#ifdef __APPLE__
	if ( (fd = open("/dev/tun0", O_RDWR)) < 0) {
		cerr << "unable to open /dev/net/tun" << endl;
	}
#else
	if ( (fd = open("/dev/net/tun", O_RDWR)) < 0) {
		cerr << "unable to open /dev/net/tun" << endl;
	}
#endif
	memset(&ifr, 0, sizeof(ifr));
#ifndef __APPLE__
	ifr.ifr_flags = IFF_TUN;
	strncpy(ifr.ifr_name, "tox%d", IFNAMSIZ);
	
	if( (err = ioctl(fd, TUNSETIFF, (void *) &ifr)) < 0 ){
		close(fd);
		cerr << strerror(err) << err;
	}
#endif
	// and set MTU params
	int tun_sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (tun_sock < 0) {
		cerr << strerror(errno) << "while setting MTU" << endl;
		return;
	}
	ifr.ifr_mtu = 1200;
	ioctl(tun_sock, SIOCSIFMTU, &ifr);

	struct sockaddr_in address;
	address.sin_family = AF_INET;
	inet_aton(myip.c_str(), &address.sin_addr);
	memcpy(&ifr.ifr_addr, &address, sizeof(address));
	ioctl(tun_sock, SIOCSIFADDR, &ifr); 

	inet_aton(peerip.c_str(), &address.sin_addr);
	memcpy(&ifr.ifr_dstaddr, &address, sizeof(address));
	ioctl(tun_sock, SIOCSIFDSTADDR, &ifr);

	ifr.ifr_flags |= IFF_UP | IFF_RUNNING;
	ioctl(tun_sock, SIOCSIFFLAGS, &ifr);

	close(tun_sock);

	this->handle = fd;
#ifdef USE_EPOLL
	memset(&this->event,0,sizeof(this->event));
	this->event.events = EPOLLIN;
	this->event.data.ptr = this;
	epoll_ctl(epoll_handle, EPOLL_CTL_ADD, this->handle, &this->event);
#endif
}
int Tunnel::populate_fdset(fd_set *readset) {
	FD_SET(this->handle,readset);
	return this->handle;
}
Tunnel::~Tunnel() {
#ifdef USE_EPOLL
	epoll_ctl(epoll_handle, EPOLL_CTL_DEL, this->handle, NULL);
#endif
	close(this->handle);
}
void Tunnel::handleReadData(Tox *tox) {
	uint8_t buffer[1501];
	int size = read(this->handle,buffer+1,1500);
	buffer[0] = 200;
	TOX_ERR_FRIEND_CUSTOM_PACKET error;
	tox_friend_send_lossy_packet(tox,this->friend_number,buffer,size+1,&error);
	if (error != TOX_ERR_FRIEND_CUSTOM_PACKET_OK) cout << "error code " << error << endl;
}
void Tunnel::processPacket(const uint8_t *data, size_t size) {
	write(this->handle,data,size);
}
