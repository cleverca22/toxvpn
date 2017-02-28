#include "main.h"
#include "interface.h"

using namespace std;
using namespace ToxVPN;

static void *start_routine(void *x) {
  NetworkInterface *nic = (NetworkInterface*)x;
  return nic->loop();
}

NetworkInterface::NetworkInterface(): my_tox(0) {
  fd = 0;
  if ( (fd = open("/dev/net/tun", O_RDWR)) < 0) {
    cerr << "unable to open /dev/net/tun" << endl;
  }
}

void NetworkInterface::configure(string ip_in,Tox *tox_in) {
  int err;
  struct ifreq ifr;
  memset(&ifr, 0, sizeof(ifr));
  ifr.ifr_flags = IFF_TUN;
  strncpy(ifr.ifr_name, "tox_master%d", IFNAMSIZ);

  if( (err = ioctl(fd, TUNSETIFF, (void *) &ifr)) < 0 ){
    if (errno == EPERM) {
      cerr << "no permission to create tun device" << endl;
      exit(-1);
    }
    cerr << strerror(errno) << err << endl;
    close(fd);
  }
  // and set MTU params
  int tun_sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (tun_sock < 0) {
    printf("error while setting MTU: %s",strerror(errno));
    return;
  }
  ifr.ifr_mtu = 1200;
  err = ioctl(tun_sock, SIOCSIFMTU, &ifr);
  if (err) printf("error %d setting mtu\n",err);

  printf("setting ip to %s\n",ip_in.c_str());
  struct sockaddr_in address;
  address.sin_family = AF_INET;
  inet_aton(ip_in.c_str(), &address.sin_addr);
  memcpy(&ifr.ifr_addr, &address, sizeof(address));
  err = ioctl(tun_sock, SIOCSIFADDR, &ifr);
  if (err) printf("error %d %s setting ip\n",errno,strerror(errno));

  inet_aton("10.123.123.123", &address.sin_addr);
  memcpy(&ifr.ifr_dstaddr, &address, sizeof(address));
  err = ioctl(tun_sock, SIOCSIFDSTADDR, &ifr);
  if (err) printf("error setting dest ip: %s\n",strerror(errno));

  ifr.ifr_flags |= IFF_UP | IFF_RUNNING;
  ioctl(tun_sock, SIOCSIFFLAGS, &ifr);

  close(tun_sock);

  interfaceIndex = if_nametoindex(ifr.ifr_name);
  my_tox = tox_in;
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);
  pthread_create(&reader,&attr,&start_routine,this);
  pthread_attr_destroy(&attr);
}
