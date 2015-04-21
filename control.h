#pragma once

#include <tox/tox.h>

#include "epoll_target.h"

class Control : public EpollTarget {
public:
	Control();
	virtual void handleData(epoll_event &eventin, Tox *tox);
};
