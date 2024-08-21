/*
 * This program is libre software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the COPYING file for more details.
 */
#pragma once

#include <list>
#include <tox/tox.h>
#include <arpa/inet.h>

void dump_packet(uint8_t* buffer, ssize_t size);

namespace ToxVPN {

class Route {
public:
    struct in_addr network;
    struct in_addr mask;
    int maskbits;
    int friend_number;
    int netmode;
    uint8_t pubkey[TOX_PUBLIC_KEY_SIZE];
};

class NetworkInterface {
public:
    NetworkInterface();
    ~NetworkInterface();
    void* loop();
    void setPeerIp(struct in_addr peer, int friend_number, int peer_netmode, uint8_t *pubkey);
    void removePeer(int friend_number);
    void addPeerRoute(struct in_addr peer, int friend_number, int peer_netmode, uint8_t *pubkey);
    void processPacket(const uint8_t* data, size_t bytes, int friend_number, int source_mode, const uint8_t *pubkey);
    void configure(std::string myip, Tox* my_tox);
    void send_arp_reply(const uint8_t *macsrc, struct in_addr src, struct in_addr dst, const uint8_t *dstmac);
    void process_arp_request(const uint8_t *macsrc, struct in_addr src, struct in_addr dst);
    void send_pi_packet_to_kernel(const uint8_t *data, uint32_t size);
    static void pubkey_to_mac(const uint8_t *pubkey, uint8_t *mac);

    std::list<Route> routes;

private:
    void handleReadData();
    bool findRoute(Route* route, struct in_addr peer);
    void forwardPacket(Route route, const uint8_t* buffer, ssize_t bytes);
    // accepts a packet in the form of PI + ETH + IP + ..., and sends to all TAP peers
    void broadcastPacket(const uint8_t* buffer, ssize_t bytes);

    pthread_t reader;
    int fd;
    Tox* my_tox;
    int interfaceIndex;
    uint8_t mymac[6];
};

static inline bool mac_is_multicast(const uint8_t *mac) {
  return (mac[0] & 1);
}

}
