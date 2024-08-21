#pragma once
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
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>

#include <assert.h>
#include <iostream>
#include <list>
#include <pthread.h>
#include <signal.h>
#include <sstream>

#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <tox/tox.h>

#include <nlohmann/json.hpp>

#if defined(__CYGWIN__)
#include <arpa/inet.h>
#include <errno.h>
#include <sys/utsname.h>
#include <sys/socket.h>
#include <sys/un.h>
#elif defined(WIN32)
#include <ws2tcpip.h>
#include <winsock2.h>
#else
// linux+mac includes
#include <pwd.h>
#include <assert.h>
#include <errno.h>
#include <sys/un.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/utsname.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <netinet/in.h>
// linux-only includes
#ifndef __APPLE__
#include <linux/if_tun.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <sys/capability.h>
#include <sys/prctl.h>
#include <asm/types.h>
#ifdef SYSTEMD
#include <systemd/sd-daemon.h>
#endif
#endif
#endif

#include <chrono>

#define USE_SELECT

#ifdef USE_EPOLL
#include <sys/epoll.h>
#endif

#include "epoll_target.h"

namespace ToxVPN {
class SocketListener;

enum {
  MODE_TUN, MODE_TAP
};

extern int netmode;

class bootstrap_node {
public:
    bootstrap_node(std::string ipv4_in, uint16_t port_in, std::string pubkey_in)
        : ipv4(ipv4_in), pubkey(pubkey_in), port(port_in) {}
    std::string ipv4, pubkey;
    uint16_t port;
};

class ToxVPNCore {
public:
    ToxVPNCore();
    SocketListener* listener;
    std::vector<std::string> auto_friends;
    std::vector<bootstrap_node> nodes;
    std::chrono::steady_clock::time_point last_boostrap;
};

void saveState(Tox* tox);
void do_bootstrap(Tox* tox, ToxVPNCore* toxvpn);
}

void to_hex(char* a, const uint8_t* p, int size);
void hex_string_to_bin(const char* hex_string, uint8_t* ret);
#ifdef WIN32
void inet_pton(int type, const char* input, struct in_addr* output);
#endif

extern std::string myip;
