#pragma once

#include <sys/epoll.h>

extern int epoll_handle;

class EpollTarget {
public:
	virtual void handleReadData(Tox *tox) = 0;
	struct epoll_event event;
	int handle;
};
