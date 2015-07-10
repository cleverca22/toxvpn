#pragma once

#include <tox/tox.h>

#include "epoll_target.h"

class Interface;

class Control : public EpollTarget {
public:
	Control(Interface *interface);
	virtual void handleReadData(Tox *tox);
	int populate_fdset(fd_set *readset);
private:
	Interface *interface;
};
