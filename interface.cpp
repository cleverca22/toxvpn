#include "main.h"

using namespace std;
using namespace ToxVPN;

void *NetworkInterface::loop() {
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
    } else {
      printf("select == %d\n",r);
      printf("select error fd:%d r:%d errno:%d %s\n",fd,r,errno,strerror(errno));
    }
  }
  return 0;
}
static const uint8_t required[] = { 0x00, 0x00, 0x08, 0x00, 0x45 };
void dump_packet(uint8_t *buffer, ssize_t size) {
  for (int i=0; i<size; i++) {
    printf("%02x ",buffer[i]);
  }
  printf("\n");
}
void NetworkInterface::handleReadData() {
  uint8_t readbuffer[1500];
  ssize_t size = read(fd,readbuffer,1500);
  if (size < 0) {
    printf("unable to read from tun %d, %s\n",fd,strerror(errno));
    exit(-2);
    return;
  }
  for (unsigned int i=0; i<sizeof(required); i++) {
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
void NetworkInterface::forwardPacket(Route route, uint8_t *readbuffer, ssize_t size) {
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
void NetworkInterface::addPeerRoute(struct in_addr peer, int friend_number) {
  Route x;
  x.network = peer;
  inet_pton(AF_INET,"255.255.255.255",&x.mask);
  x.maskbits = 32;
  x.friend_number = friend_number;
  routes.push_back(x);
  systemRouteSingle(interfaceIndex,peer,"10.123.123.123");
}
void NetworkInterface::setPeerIp(struct in_addr peer, int friend_number) {
  // TODO, flag as online, remove previous ip route
  addPeerRoute(peer,friend_number);
}
void NetworkInterface::removePeer(int friend_number) {
  // TODO, remove routes in-app and in-kernel
}
bool NetworkInterface::findRoute(Route *route,struct in_addr peer) {
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
void NetworkInterface::processPacket(const uint8_t *data, size_t size, int friend_number) {
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
