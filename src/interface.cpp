#include <errno.h>
#include <iostream>
#include <linux/if_tun.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <unistd.h>

#include "interface.h"
#include "main.h"
#include "route.h"

using namespace std;
using namespace ToxVPN;

typedef struct {
  uint16_t hardware_type;
  uint16_t protocol_type;
  uint8_t hw_size;
  uint8_t protocol_size;
  uint16_t opcode;
  uint8_t src_mac[6];
  struct in_addr src_ip;
  uint8_t dst_mac[6];
  struct in_addr dst_ip;
} __attribute__((__packed__)) arp_header;

typedef struct {
  uint8_t dest[6];
  uint8_t src[6];
  uint16_t type;
  uint8_t next[0];
} __attribute__((__packed__)) ethernet_header;

typedef struct {
  struct tun_pi pi;
  ethernet_header eth_hdr;
  arp_header arp_hdr;
} arp_reply_packet;

void NetworkInterface::send_arp_reply(const uint8_t *macsrc, struct in_addr src, struct in_addr dst, const uint8_t *pubkey) {
  arp_reply_packet pkt;
  pkt.pi.flags = 0;
  pkt.pi.proto = htons(0x0806);
  pubkey_to_mac(pubkey, pkt.eth_hdr.src);
  memcpy(pkt.eth_hdr.dest, macsrc, 6);
  pkt.eth_hdr.type = htons(0x0806);
  pkt.arp_hdr.hardware_type = htons(1);
  pkt.arp_hdr.protocol_type = htons(0x800);
  pkt.arp_hdr.hw_size = 6;
  pkt.arp_hdr.protocol_size = 4;
  pkt.arp_hdr.opcode = htons(2);
  pubkey_to_mac(pubkey, pkt.arp_hdr.src_mac);
  pkt.arp_hdr.src_ip = dst;
  memcpy(pkt.arp_hdr.dst_mac, macsrc, 6);
  pkt.arp_hdr.dst_ip = src;
  send_pi_packet_to_kernel((uint8_t*)&pkt, sizeof(pkt));
}

void NetworkInterface::process_arp_request(const uint8_t *macsrc, struct in_addr src, struct in_addr dst) {
  Route route;
  if (findRoute(&route, dst)) {
    if (route.netmode == MODE_TUN) {
      // remote peer is in TUN mode, generate an ARP reply locally
      send_arp_reply(macsrc, src, dst, route.pubkey);
    } else {
      fprintf(stderr, "peer isnt in TUN mode\n");
    }
  } else {
    fprintf(stderr, "no route for arp %s\n", inet_ntoa(dst));
  }
}

void* NetworkInterface::loop() {
    fd_set readset;
    struct timeval timeout;
    int r;
    while(true) {
        FD_ZERO(&readset);
        FD_SET(fd, &readset);
        timeout.tv_sec = 60;
        timeout.tv_usec = 0;
        r = select(fd + 1, &readset, nullptr, nullptr, &timeout);
        if(r > 0) {
            if(FD_ISSET(fd, &readset))
                handleReadData();
        } else if(r == 0) {
        } else {
            printf("select == %d\n", r);
            printf("select error fd:%d r:%d errno:%d %s\n", fd, r, errno,
                   strerror(errno));
        }
    }
    return nullptr;
}
#ifndef __APPLE__
static const uint8_t required[] = {0x00, 0x00, 0x08, 0x00, 0x45};
#endif
void dump_packet(uint8_t* buffer, ssize_t size) {
    for(int i = 0; i < size; i++) {
        printf("%02x ", buffer[i]);
    }
    printf("\n");
}
void NetworkInterface::handleReadData() {
  uint8_t readbuffer[1500];
  ssize_t size_ = read(fd, readbuffer, 1500);
  if(size_ < 0) {
      printf("unable to read from tun %d, %s\n", fd, strerror(errno));
      exit(-2);
      return;
  }
  uint32_t size = (uint32_t)size_;

  if (netmode == MODE_TAP) {
    struct tun_pi *pi = (struct tun_pi*)readbuffer;
    ethernet_header *eth_header = (ethernet_header*)(readbuffer + 4);
    uint8_t *ip_header = &eth_header->next[0];
    if (ntohs(pi->proto) == 0x86dd) { // IPv6, TODO
      return;
    } else if (ntohs(pi->proto) == 0x800) { // IPv4
      //printf("flags: 0x%x, proto: 0x%x\n", pi->flags, pi->proto);
      //dump_packet(ip_header, size - 4 - 14);
      struct in_addr *src = (struct in_addr*) (ip_header + 12);
      struct in_addr *dest = (struct in_addr*) (ip_header + 16);
      char src_str[16], dst_str[16];
      strncpy(src_str, inet_ntoa(*src), 16);
      strncpy(dst_str, inet_ntoa(*dest), 16);
      //printf("%ld bytes for %s -> %s\n", size, src_str, dst_str);
      if (mac_is_multicast(eth_header->dest)) {
        //printf("mcast to %s\n", dst_str);
        broadcastPacket(readbuffer, size);
      } else {
        Route route;
        if (findRoute(&route, *dest)) {
          forwardPacket(route, readbuffer, size);
        } else {
          printf("no route found for %s\n", dst_str);
        }
      }
    } else if (ntohs(pi->proto) == 0x0806) { // ARP
      //dump_packet(ip_header, size - 4 - 14);
      const arp_header *arp = (const arp_header*)ip_header;
      if (arp->hw_size != 6) {
        fprintf(stderr, "hw size wrong\n");
        return;
      }
      if (arp->protocol_size != 4) {
        fprintf(stderr, "proto size wrong\n");
        return;
      }
      if (ntohs(arp->hardware_type) != 1) {
        fprintf(stderr, "hw type wrong\n");
        return;
      }
      if (ntohs(arp->protocol_type) != 0x800) {
        fprintf(stderr, "protocol type wrong\n");
        return;
      }
      switch (ntohs(arp->opcode)) {
      case 1: // request, what is the mac behind dst_ip
        process_arp_request(&arp->src_mac[0], arp->src_ip, arp->dst_ip);
        break;
      default:
        printf("ARP op %d\n", ntohs(arp->opcode));
      }
    } else {
      printf("UNK flags: 0x%x, proto: 0x%x\n", pi->flags, pi->proto);
    }
  } else {
    struct tun_pi *pi = (struct tun_pi*)readbuffer;
    for(unsigned int i = 0; i < sizeof(required); i++) {
      if(readbuffer[i] != required[i]) {
        puts("unsupported packet, dropping");
        dump_packet(readbuffer, size);
        return;
      }
    }
    struct in_addr* dest = (struct in_addr*) (readbuffer + 20);

    //printf("read %d bytes on master interface for %s\n", size, inet_ntoa(*dest));
    //dump_packet(readbuffer,size);

    Route route;
    if (findRoute(&route, *dest)) {
      struct {
        struct tun_pi pi;
        ethernet_header eth;
        uint8_t rest[1500];
      } newpacket;
      newpacket.pi = *pi;
      pubkey_to_mac(route.pubkey, newpacket.eth.dest);
      memcpy(newpacket.eth.src, mymac, 6);
      newpacket.eth.type = newpacket.pi.proto;
      memcpy(newpacket.rest, readbuffer+4, size-4);
      uint32_t newsize = sizeof(ethernet_header) + size;
      forwardPacket(route, (uint8_t*)&newpacket, newsize);
    } else {
      printf("no route found for %s\n", inet_ntoa(*dest));
    }
  }
}

// gets a packet with PI, eth, ip ....
// TUN based targets want just PI, ip ...
// TAP based targets want the whole packet
// the 200 prefix is tox specific
void NetworkInterface::forwardPacket(Route route, const uint8_t* readbuffer, ssize_t size) {
  uint8_t buffer[1600];
  if (route.netmode == MODE_TUN) {
    buffer[0] = 200;
    memcpy(buffer + 1, readbuffer, sizeof(tun_pi));
    int offset = sizeof(tun_pi) + sizeof(ethernet_header);
    size -= offset;
    memcpy(buffer + 1 + sizeof(tun_pi), readbuffer + offset, size);
    size += sizeof(tun_pi);
  } else {
    // TODO, sending to TAP
  }
  Tox_Err_Friend_Custom_Packet error;
  tox_friend_send_lossy_packet(my_tox, route.friend_number, buffer,
                               size + 1, &error);
  switch(error) {
  case TOX_ERR_FRIEND_CUSTOM_PACKET_OK: break;
  case TOX_ERR_FRIEND_CUSTOM_PACKET_FRIEND_NOT_CONNECTED:
      cout << size << "byte packet dropped, friend#" << route.friend_number
           << "not online" << endl;
      break;
  case TOX_ERR_FRIEND_CUSTOM_PACKET_SENDQ:
      cout << size << "byte packet dropped, sendq for friend#"
           << route.friend_number << "full" << endl;
      break;
  default: cout << "TX error code " << error << endl;
  }
}
void NetworkInterface::addPeerRoute(struct in_addr peer, int friend_number, int peer_netmode, uint8_t *pubkey) {
    Route x;
    x.network = peer;
    inet_pton(AF_INET, "255.255.255.255", &x.mask);
    x.maskbits = 32;
    x.friend_number = friend_number;
    x.netmode = peer_netmode;
    memcpy(x.pubkey, pubkey, TOX_PUBLIC_KEY_SIZE);
    routes.push_back(x);
    //systemRouteSingle(interfaceIndex, peer, "10.123.123.123");
    systemRouteDirect(interfaceIndex, peer);
}
void NetworkInterface::setPeerIp(struct in_addr peer, int friend_number, int peer_netmode, uint8_t *pubkey) {
    // TODO, flag as online, remove previous ip route
    addPeerRoute(peer, friend_number, peer_netmode, pubkey);
}
void NetworkInterface::removePeer(int friend_number) {
    // TODO, remove routes in-app and in-kernel
}
bool NetworkInterface::findRoute(Route* route, struct in_addr peer) {
  std::list<Route>::const_iterator i;
  for(i = routes.begin(); i != routes.end(); ++i) {
    Route r = *i;
    string network1(inet_ntoa(r.network));
    string mask1(inet_ntoa(r.mask));
    uint32_t network = (uint32_t) r.network.s_addr;
    uint32_t mask = (uint32_t) r.mask.s_addr;
    // printf("test %08x\n",(network & mask));
    // printf("%s %s %d\n",network1.c_str(),mask1.c_str(),r.friend_number);
    if((network & mask) == (peer.s_addr & mask)) {
      *route = r;
      return true;
    }
  }
  return false;
}

void NetworkInterface::broadcastPacket(const uint8_t* readbuffer, ssize_t size) {
  std::list<Route>::const_iterator i;
  for (i = routes.begin(); i != routes.end(); ++i) {
    Route r = *i;
    if (r.netmode == MODE_TAP) {
      forwardPacket(r, readbuffer, size);
    }
  }
}

void NetworkInterface::processPacket(const uint8_t* data, size_t size, int friend_number, int source_mode, const uint8_t *pubkey) {
  ssize_t ret = 0;

  friend_number;

  if (fd) {
    if (source_mode == MODE_TUN) {
      // received packet starts with PI + IP header, insert a ethernet header
      struct {
        struct tun_pi pi;
        ethernet_header eth;
        uint8_t rest[1500];
      } __attribute__((__packed__)) newpacket;
      newpacket.pi.flags = 0;
      newpacket.pi.proto = htons(0x800);
      memcpy(newpacket.eth.dest, mymac, 6);
      pubkey_to_mac(pubkey, newpacket.eth.src);
      newpacket.eth.type = htons(0x800);
      memcpy(newpacket.rest, data+4, size-4);
      uint32_t newsize = sizeof(struct tun_pi) + sizeof(ethernet_header) + size - 4;
      send_pi_packet_to_kernel((uint8_t*)&newpacket, newsize);
    } else {
      ret = write(fd, data, size);
      if ((size_t)ret != size)
        cerr << "partial packet write to tun\n";
    }
  }
}

// incoming packet is always in the form of PI+ETH+IP+...
void NetworkInterface::send_pi_packet_to_kernel(const uint8_t* data, uint32_t size) {
  if(fd) {
    uint8_t newpacket[1600];
    if (netmode == MODE_TUN) {
      // need to strip ethernet header
      memcpy(newpacket, data, sizeof(struct tun_pi));
      memcpy(newpacket + sizeof(struct tun_pi), data + sizeof(struct tun_pi) + sizeof(ethernet_header), size - sizeof(struct tun_pi) + sizeof(ethernet_header));
      size = size - sizeof(ethernet_header);
      data = newpacket;
    }
    ssize_t ret = write(fd, data, size);
    if (ret != size) {
      fprintf(stderr, "partial packet write to tun, %d attempted vs %ld successful\n", size, ret);
    }
  } else {
    fprintf(stderr, "tun fd not open\n");
  }
}

void NetworkInterface::pubkey_to_mac(const uint8_t *pubkey, uint8_t *mac) {
  memcpy(mac, pubkey, 6);
  mac[0] |= 2;
  mac[0] &= 254;
}
