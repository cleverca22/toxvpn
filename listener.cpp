#include "main.h"

using namespace ToxVPN;

SocketListener::SocketListener(NetworkInterface *interfarce): interfarce(interfarce) {
	socket = dup(0);
}
#ifndef WIN32
SocketListener::SocketListener(NetworkInterface *iface, std::string unixSocket): interfarce(iface) {
	socket = ::socket(AF_UNIX, SOCK_STREAM, 0);
	struct sockaddr_un addr;
	bzero(&addr,sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, unixSocket.c_str(), sizeof(addr.sun_path)-1);
	unlink(unixSocket.c_str());
	if (bind(socket, (struct sockaddr*)&addr, sizeof(addr))) {
		printf("unable to bind control socket: %s\n",strerror(errno));
	}
	chmod(unixSocket.c_str(),0777);
	listen(socket,5);
}
#endif
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
	Control *c = new Control(interfarce,newsocket);
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
