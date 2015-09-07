#pragma once

#include <list>

#include "control.h"

namespace ToxVPN {

class SocketListener {
public:
	SocketListener(NetworkInterface *interfarce);
	int populate_fdset(fd_set *readset);
	void checkFds(fd_set *readset, Tox *my_tox);
	void doAccept();

	int socket;
private:
	std::list<Control*> connections;
	NetworkInterface *interfarce;
};

};
