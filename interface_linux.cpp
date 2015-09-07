#include "interface.h"
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/if_tun.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <iostream>
#include <fcntl.h>
#include <unistd.h>

using namespace std;
using namespace ToxVPN;

NetworkInterface::NetworkInterface(string myip, Tox *my_tox): my_tox(my_tox) {
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
}
