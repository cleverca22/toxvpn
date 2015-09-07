#include <sys/socket.h>
#include <unistd.h>
#include <sys/select.h>

#include "listener.h"

using namespace ToxVPN;

SocketListener::SocketListener(Interface *interface): interface(interface) {
	socket = dup(0);
}
int SocketListener::populate_fdset(fd_set *readset) {
	std::list<Control*>::const_iterator i;
	int max = socket;
	FD_SET(socket,readset);
	for (i=connections.begin(); i!=connections.end(); ++i) {
		Control *c = *i;
		max = std::max(max,c->populate_fdset(readset));
	}
	return max;
}
void SocketListener::doAccept() {
	int newsocket = accept(socket,0,0);
	Control *c = new Control(interface,newsocket);
	connections.push_back(c);
}
void SocketListener::checkFds(fd_set *readset, Tox *my_tox) {
	std::list<Control*>::iterator i;
	for (i=connections.begin(); i!=connections.end(); ++i) {
		Control *c = *i;
		if (FD_ISSET(c->handle,readset)) {
			int x = c->handleReadData(my_tox);
			if (x == -1) {
				connections.erase(i);
				return; // FIXME
			}
		}
	}
}
