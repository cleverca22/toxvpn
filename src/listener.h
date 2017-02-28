#include "main.h"
#include "control.h"

namespace ToxVPN {

class SocketListener {
public:
    SocketListener(NetworkInterface* interfarce);
#ifndef WIN32
    SocketListener(NetworkInterface* interfarce,
                   std::string unixSocket,
                   void* zmq);
#endif
    int populate_fdset(fd_set* readset);
    void checkFds(fd_set* readset, Tox* my_tox, ToxVPNCore* toxvpn);
    void doAccept();
    void broadcast(const char* msg);

    int socket;

private:
    std::list<Control*> connections;
    NetworkInterface* interfarce;
    void* zmq_broadcast;
};
};
