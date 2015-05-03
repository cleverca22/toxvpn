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

using namespace std;

Tunnel::Tunnel(int friend_number,std::string myip, std::string peerip) {
#ifdef WIN32
	cout << "new tunnel" << friend_number << myip << peerip << endl;
	this->handle = 0;
#else
	struct ifreq ifr;
	int fd,err;
	this->friend_number = friend_number;

# ifdef __APPLE__
	if ( (fd = open("/dev/tun0", O_RDWR)) < 0) {
		cerr << "unable to open /dev/net/tun" << endl;
	}
# else
	if ( (fd = open("/dev/net/tun", O_RDWR)) < 0) {
		cerr << "unable to open /dev/net/tun" << endl;
	}
# endif
	memset(&ifr, 0, sizeof(ifr));
# ifdef __APPLE__
	strncpy(ifr.ifr_name,"tun0",IFNAMSIZ);
# else
	ifr.ifr_flags = IFF_TUN;
	strncpy(ifr.ifr_name, "tox%d", IFNAMSIZ);
	
	if( (err = ioctl(fd, TUNSETIFF, (void *) &ifr)) < 0 ){
		close(fd);
		cerr << strerror(err) << err;
	}
# endif
	puts("setting mtu...");
	// and set MTU params
	int tun_sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (tun_sock < 0) {
		printf("error while setting MTU: %s",strerror(errno));
		return;
	}
	ifr.ifr_mtu = 1200;
	err =ioctl(tun_sock, SIOCSIFMTU, &ifr);
	if (err) printf("error %d setting mtu\n",err);

	struct sockaddr_in address;
	address.sin_family = AF_INET;
	inet_aton(myip.c_str(), &address.sin_addr);
	memcpy(&ifr.ifr_addr, &address, sizeof(address));
	err = ioctl(tun_sock, SIOCSIFADDR, &ifr); 
	if (err) printf("error %d %s setting ip\n",errno,strerror(errno));

	inet_aton(peerip.c_str(), &address.sin_addr);
	memcpy(&ifr.ifr_dstaddr, &address, sizeof(address));
	err = ioctl(tun_sock, SIOCSIFDSTADDR, &ifr);
	if (err) printf("error %d %s setting dest ip\n",errno,strerror(errno));

	ifr.ifr_flags |= IFF_UP | IFF_RUNNING;
	ioctl(tun_sock, SIOCSIFFLAGS, &ifr);

	close(tun_sock);

	this->handle = fd;
	printf("tunnel interface setup on fd#%d\n",fd);
# ifdef USE_EPOLL
	memset(&this->event,0,sizeof(this->event));
	this->event.events = EPOLLIN;
	this->event.data.ptr = this;
	epoll_ctl(epoll_handle, EPOLL_CTL_ADD, this->handle, &this->event);
# endif
#endif
}
int Tunnel::populate_fdset(fd_set *readset) {
	if (handle) {
		FD_SET(this->handle,readset);
	}
	return this->handle;
}
Tunnel::~Tunnel() {
#ifdef USE_EPOLL
	epoll_ctl(epoll_handle, EPOLL_CTL_DEL, this->handle, NULL);
#endif
	close(this->handle);
}
void Tunnel::handleReadData(Tox *tox) {
#ifdef __APPLE__
# define OFFSET 5
#else
# define OFFSET 1
#endif
	uint8_t buffer[1500+OFFSET];
	int size = read(this->handle,buffer+OFFSET,1500);
	buffer[0] = 200;
#ifdef __APPLE__
	buffer[1] = 0;
	buffer[2] = 0;
	buffer[3] = 0x80;
	buffer[4] = 0;
#endif
	TOX_ERR_FRIEND_CUSTOM_PACKET error;
	/*printf("packet %d ==",size);
	for (int i=0; i<size+(OFFSET-1); i++) {
		printf(" %02x",buffer[i+1]);
	}
	printf("\n");*/
	tox_friend_send_lossy_packet(tox,this->friend_number,buffer,size+OFFSET,&error);
	switch (error) {
	case TOX_ERR_FRIEND_CUSTOM_PACKET_OK:
		break;
	case TOX_ERR_FRIEND_CUSTOM_PACKET_FRIEND_NOT_CONNECTED:
		cout << size << "byte packet dropped, friend#" << this->friend_number << "not online" << endl;
		break;
	case TOX_ERR_FRIEND_CUSTOM_PACKET_SENDQ:
		cout << size << "byte packet dropped, sendq for friend#" << this->friend_number << "full" << endl;
		break;
	default:
		cout << "TX error code " << error << endl;
	}
}
void Tunnel::processPacket(const uint8_t *data, size_t size) {
	/*printf("packet %d ==",size);
	for (int i=0; i<size; i++) {
		printf(" %02x",data[i]);
	}
	printf("\n");*/
	if (handle) {
#ifdef __APPLE__
		write(this->handle,data+4,size);
#else
		write(this->handle,data,size);
#endif
	}
}
