#include "main.h"

using namespace std;
using namespace ToxVPN;

static void *start_routine(void *x) {
  NetworkInterface *nic = (NetworkInterface*)x;
  return nic->loop();
}
NetworkInterface::NetworkInterface(): my_tox(0), fd(0) {
  if ( (fd = open("/dev/tun0", O_RDWR)) < 0) {
    cerr << "unable to open /dev/tun0" << endl;
  }
}
void NetworkInterface::configure(string myip,Tox *my_tox) {
  int err;
  struct ifreq ifr;
  memset(&ifr, 0, sizeof(ifr));
  strncpy(ifr.ifr_name,"tun0",IFNAMSIZ);
  int tun_sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (tun_sock < 0) {
    printf("error while setting MTU: %s",strerror(errno));
    return;
  }
  ifr.ifr_mtu = 1200;
  err = ioctl(tun_sock, SIOCSIFMTU, &ifr);
  if (err) printf("error %d setting mtu\n",err);

  printf("setting ip to %s\n",myip.c_str());
  struct sockaddr_in address;
  address.sin_family = AF_INET;
  inet_aton(myip.c_str(), &address.sin_addr);
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
  this->my_tox = my_tox;
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);
  pthread_create(&reader,&attr,&start_routine,this);
  pthread_attr_destroy(&attr);

}
