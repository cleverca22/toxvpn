#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <iostream>
#include <linux/if_tun.h>
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

	if ( (fd = open("/dev/net/tun", O_RDWR)) < 0) {
		cerr << "unable to open /dev/net/tun" << endl;
	}
	memset(&ifr, 0, sizeof(ifr));

	ifr.ifr_flags = IFF_TUN;
	strncpy(ifr.ifr_name, "tox%d", IFNAMSIZ);
	
	if( (err = ioctl(fd, TUNSETIFF, (void *) &ifr)) < 0 ){
		close(fd);
		cerr << strerror(err) << err;
	}

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
	memset(&this->event,0,sizeof(this->event));
	this->event.events = EPOLLIN;
	this->event.data.ptr = this;
	epoll_ctl(epoll_handle, EPOLL_CTL_ADD, this->handle, &this->event);
}
void Tunnel::handleData(struct epoll_event &eventin, Tox *tox) {
	cout << eventin.events << endl;
	uint8_t buffer[1501];
	if (eventin.events & EPOLLIN) {
		int size = read(this->handle,buffer+1,1500);
		cout << "read" << size << endl;
		buffer[0] = 200;
		TOX_ERR_FRIEND_CUSTOM_PACKET error;
		tox_friend_send_lossy_packet(tox,this->friend_number,buffer,size+1,&error);
		cout << "error code " << error << endl;
	}
}
void Tunnel::processPacket(const uint8_t *data, size_t size) {
	char hex[size*2+1];
	memset(hex,0,size*2+1);
	to_hex(hex,data,size);
	std::cout << hex << std::endl;
	write(this->handle,data,size);
}
