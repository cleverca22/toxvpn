#pragma once

#include <tox/tox.h>

#include "epoll_target.h"

class Control : public EpollTarget {
public:
	Control();
	virtual void handleReadData(Tox *tox);
	int populate_fdset(fd_set *readset);
};
