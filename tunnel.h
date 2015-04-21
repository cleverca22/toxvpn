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

#include "epoll_target.h"

class Tunnel : public EpollTarget {
public:
	Tunnel(int friend_number,std::string myip, std::string peerip);
	~Tunnel();
	virtual void handleData(struct epoll_event &eventin, Tox *tox);
	void processPacket(const uint8_t *data, size_t size);

	int friend_number;
};
