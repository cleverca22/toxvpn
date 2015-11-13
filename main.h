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

#include <tox/tox.h>


#ifdef WIN32
  #include <json/json.h>
  #include <ws2tcpip.h>
  #include <winsock2.h>
#else
  #include <pwd.h>
  #include <sys/capability.h>
  #include <sys/prctl.h>
  #include <assert.h>
  #include <errno.h>
  #include <asm/types.h>
  #include <json/json.h>
  #include <sys/un.h>
  #include <sys/stat.h>
  #include <sys/types.h>
  #include <sys/ioctl.h>
  #include <sys/select.h>
  #include <sys/socket.h>
  #include <sys/utsname.h>
  #include <linux/if_tun.h>
  #include <linux/netlink.h>
  #include <linux/rtnetlink.h>
  #include <net/if.h>
  #include <arpa/inet.h>
  #include <netinet/in.h>

  #ifndef __APPLE__
    #include <linux/if_tun.h>
  #endif

  #ifndef STATIC
    #include <systemd/sd-daemon.h>
  #endif

#endif


#ifdef USE_EPOLL
 #include <sys/epoll.h>
#endif

#include "epoll_target.h"

#include "interface.h"
#include "control.h"
#include "route.h"
#include "listener.h"

#define USE_SELECT


void to_hex(char *a, const uint8_t *p, int size);
void hex_string_to_bin(const char *hex_string, uint8_t *ret);
void saveState(Tox *tox);
void do_bootstrap(Tox *tox);
#ifdef WIN32
void inet_pton(int type, const char *input, struct in_addr *output);
#endif

extern std::string myip;
