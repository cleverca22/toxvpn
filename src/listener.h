#include "main.h"
#include "control.h"

namespace ToxVPN {

class SocketListener {
public:
    SocketListener(NetworkInterface* interfarce);
#ifndef WIN32
    SocketListener(NetworkInterface* interfarce
                   ,std::string unixSocket
#ifdef ZMQ
                   ,void* zmq
#endif
                   );
#endif
    int populate_fdset(fd_set* readset);
    void checkFds(fd_set* readset, Tox* my_tox, ToxVPNCore* toxvpn);
    void doAccept();
    void broadcast(const char* msg);

    int socket;

private:
    std::list<Control*> connections;
    NetworkInterface* interfarce;
#ifdef ZMQ
    void* zmq_broadcast;
#endif
};
}
