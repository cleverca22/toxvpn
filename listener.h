#pragma once

#include <list>

#include "control.h"

class SocketListener {
public:
	SocketListener(Interface *interface);
	int populate_fdset(fd_set *readset);
	void checkFds(fd_set *readset, Tox *my_tox);
	void doAccept();

	int socket;
private:
	std::list<Control*> connections;
	Interface *interface;
};