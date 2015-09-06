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
#include <string>
#include <tox/tox.h>
#include <sys/types.h>
#ifdef WIN32
# include <winsock2.h>
#else
# include <sys/socket.h>
# include <arpa/inet.h>
#endif
#include <list>
#include <pthread.h>

class Route {
public:
	struct in_addr network;
	struct in_addr mask;
	int maskbits;
	int friend_number;
};
class Interface {
public:
	Interface(std::string myip, Tox *my_tox);
	~Interface();
	void *loop();
	void setPeerIp(struct in_addr peer, int friend_number);
	void removePeer(int friend_number);
	void addPeerRoute(struct in_addr peer, int friend_number);
	void processPacket(const uint8_t *data, size_t bytes, int friend_number);

	std::list<Route> routes;
private:
	void handleReadData();
	bool findRoute(Route *route, struct in_addr peer);
	void forwardPacket(Route route, uint8_t *buffer, int bytes);

	pthread_t reader;
	int fd;
	Tox *my_tox;
	int interfaceIndex;
};
