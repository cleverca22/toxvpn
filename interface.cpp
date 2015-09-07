#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <iostream>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <net/if.h>
#include <linux/if_tun.h>

#include "interface.h"
#include "route.h"

using namespace std;
using namespace ToxVPN;

static void *start_routine(void *x) {
	Interface *nic = (Interface*)x;
	nic->loop();
}
Interface::Interface(string myip, Tox *my_tox): my_tox(my_tox) {
	fd = 0;
	int err;
	struct ifreq ifr;
	memset(&ifr, 0, sizeof(ifr));
#ifdef __APPLE__
	if ( (fd = open("/dev/tun0", O_RDWR)) < 0) {
		cerr << "unable to open /dev/tun0" << endl;
	}
#else
	if ( (fd = open("/dev/net/tun", O_RDWR)) < 0) {
		cerr << "unable to open /dev/net/tun" << endl;
	}
#endif
#ifdef __APPLE__
	strncpy(ifr.ifr_name,"tun0",IFNAMSIZ);
#else
	ifr.ifr_flags = IFF_TUN;
	strncpy(ifr.ifr_name, "tox_master%d", IFNAMSIZ);
	
	if( (err = ioctl(fd, TUNSETIFF, (void *) &ifr)) < 0 ){
		close(fd);
		cerr << strerror(err) << err;
	}
#endif
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

	inet_aton("10.123.123.123", &address.sin_addr);
	memcpy(&ifr.ifr_dstaddr, &address, sizeof(address));
	err = ioctl(tun_sock, SIOCSIFDSTADDR, &ifr);
	if (err) printf("error %d %s setting dest ip\n",errno,strerror(errno));

	ifr.ifr_flags |= IFF_UP | IFF_RUNNING;
	ioctl(tun_sock, SIOCSIFFLAGS, &ifr);

	close(tun_sock);

	interfaceIndex = if_nametoindex(ifr.ifr_name);
	printf("%d %s\n",interfaceIndex,ifr.ifr_name);
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);
	pthread_create(&reader,&attr,&start_routine,this);
	pthread_attr_destroy(&attr);
}
void *Interface::loop() {
	fd_set readset;
	struct timeval timeout;
	int r;
	while (true) {
		FD_ZERO(&readset);
		FD_SET(fd,&readset);
		timeout.tv_sec = 60;
		timeout.tv_usec = 0;
		r = select(fd+1, &readset, NULL, NULL, &timeout);
		if (r > 0) {
			if (FD_ISSET(fd,&readset)) handleReadData();
		} else if (r == 0) {
			puts("select == 0");
		} else {
			printf("select == %d\n",r);
			printf("select error fd:%d r:%d errno:%d %s\n",fd,r,errno,strerror(errno));
		}
	}
	return 0;
}
static const uint8_t required[] = { 0x00, 0x00, 0x08, 0x00, 0x45 };
void dump_packet(uint8_t *buffer, int size) {
	for (int i=0; i<size; i++) {
		printf("%02x ",buffer[i]);
	}
	printf("\n");
}
void Interface::handleReadData() {
	uint8_t readbuffer[1500];
	int size = read(fd,readbuffer,1500);
	for (int i=0; i<sizeof(required); i++) {
		if (readbuffer[i] != required[i]) {
			puts("unsupported packet, dropping");
			dump_packet(readbuffer,size);
		}
	}
	struct in_addr *dest = (struct in_addr*) (readbuffer + 20);
	//printf("read %d bytes on master interface for %s\n",size,inet_ntoa(*dest));
	//dump_packet(readbuffer,size);
#ifdef __APPLE__
# define OFFSET 5
#else
# define OFFSET 1
#endif
	Route route;
	if (findRoute(&route, *dest)) {
		forwardPacket(route,readbuffer,size);
	} else {
		printf("no route found for %s\n",inet_ntoa(*dest));
	}
}
void Interface::forwardPacket(Route route, uint8_t *readbuffer, int size) {
	uint8_t buffer[1500+OFFSET];
	buffer[0] = 200;
#ifdef __APPLE__
	buffer[1] = 0;
	buffer[2] = 0;
	buffer[3] = 0x08;
	buffer[4] = 0;
#endif
	memcpy(buffer+OFFSET,readbuffer,size);
	TOX_ERR_FRIEND_CUSTOM_PACKET error;
	tox_friend_send_lossy_packet(my_tox,route.friend_number,buffer,size+OFFSET,&error);
	switch (error) {
	case TOX_ERR_FRIEND_CUSTOM_PACKET_OK:
		break;
	case TOX_ERR_FRIEND_CUSTOM_PACKET_FRIEND_NOT_CONNECTED:
		cout << size << "byte packet dropped, friend#" << route.friend_number << "not online" << endl;
		break;
	case TOX_ERR_FRIEND_CUSTOM_PACKET_SENDQ:
		cout << size << "byte packet dropped, sendq for friend#" << route.friend_number << "full" << endl;
		break;
	default:
		cout << "TX error code " << error << endl;
	}
}
void Interface::addPeerRoute(struct in_addr peer, int friend_number) {
	Route x;
	x.network = peer;
	inet_aton("255.255.255.255",&x.mask);
	x.maskbits = 32;
	x.friend_number = friend_number;
	routes.push_back(x);
	systemRouteSingle(interfaceIndex,peer,"10.123.123.123");
}
void Interface::setPeerIp(struct in_addr peer, int friend_number) {
	// TODO, flag as online, remove previous ip route
	addPeerRoute(peer,friend_number);
}
void Interface::removePeer(int friend_number) {
	// TODO, remove routes in-app and in-kernel
}
bool Interface::findRoute(Route *route,struct in_addr peer) {
	std::list<Route>::const_iterator i;
	for (i=routes.begin(); i!=routes.end(); ++i) {
		Route r = *i;
		string network1(inet_ntoa(r.network));
		string mask1(inet_ntoa(r.mask));
		uint32_t network = (uint32_t)r.network.s_addr;
		uint32_t mask = (uint32_t)r.mask.s_addr;
		//printf("test %08x\n",(network & mask));
		//printf("%s %s %d\n",network1.c_str(),mask1.c_str(),r.friend_number);
		if ((network&mask) == (peer.s_addr&mask)) {
			*route = r;
			return true;
		}
	}
	return false;
}
void Interface::processPacket(const uint8_t *data, size_t size, int friend_number) {
	/*printf("packet %d ==",size);
	for (int i=0; i<size; i++) {
		printf(" %02x",data[i]);
	}
	printf("\n");*/
	if (fd) {
#ifdef __APPLE__
		write(fd,data+4,size);
#else
		write(fd,data,size);
#endif
	}
}
