#pragma once

#include "epoll_target.h"


class Tunnel : public EpollTarget {
public:
	Tunnel(int friend_number,std::string myip, std::string peerip);
	virtual void handleData(struct epoll_event &eventin, Tox *tox);
	void processPacket(const uint8_t *data, size_t size);

	int friend_number;
};
