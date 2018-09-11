#include "main.h"
#include "listener.h"
#ifdef ZMQ
#include <zmq.h>
#endif

using namespace ToxVPN;

SocketListener::SocketListener(NetworkInterface* iface) : interfarce(iface) {
    socket = dup(0);
}

#ifndef WIN32
SocketListener::SocketListener(NetworkInterface* iface
                               ,std::string unixSocket
#ifdef ZMQ
                               ,void* zmq
#endif
                               )
    : interfarce(iface) {
    socket = ::socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un addr;
    bzero(&addr, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, unixSocket.c_str(), sizeof(addr.sun_path) - 1);
    unlink(unixSocket.c_str());
    if(bind(socket, (struct sockaddr*) &addr, sizeof(addr))) {
        printf("unable to bind control socket: %s\n", strerror(errno));
    }
    chmod(unixSocket.c_str(), 0777);
    listen(socket, 5);

#ifdef ZMQ
    zmq_broadcast = zmq_socket(zmq, ZMQ_PUB);
#ifndef NDEBUG
    int rc =
#endif
        zmq_bind(zmq_broadcast,
                 (std::string("ipc://") + unixSocket + "broadcast").c_str());
    assert(rc == 0);
#endif
}
#endif

int SocketListener::populate_fdset(fd_set* readset) {
    std::list<Control*>::const_iterator i;
    int max = socket;
    FD_SET(socket, readset);
    for(i = connections.begin(); i != connections.end(); ++i) {
        Control* c = *i;
        max = std::max(max, c->populate_fdset(readset));
    }
    return max;
}

void SocketListener::doAccept() {
    int newsocket = accept(socket, nullptr, nullptr);
    Control* c = new Control(interfarce, newsocket);
    connections.push_back(c);
}

void SocketListener::checkFds(fd_set* readset,
                              Tox* my_tox,
                              ToxVPNCore* toxvpn) {
    std::list<Control*>::iterator i;
    for(i = connections.begin(); i != connections.end(); ++i) {
        Control* c = *i;
        if(FD_ISSET(c->handle, readset)) {
            ssize_t x = c->handleReadData(my_tox, toxvpn);
            if(x == -1) {
                connections.erase(i);
                return; // FIXME
            }
        }
    }
}

void SocketListener::broadcast(const char* msg) {
  printf("in broadcast with '%s'\n", msg);
#ifdef ZMQ
    zmq_msg_t header;
    char* hack = new char[4];
    strcpy(hack, "all");
    hack[3] = 0;
#ifndef NDEBUG
    int rc =
#endif
        zmq_msg_init_data(&header, hack, 3, nullptr, nullptr);
    assert(rc == 0);
    zmq_msg_send(&header, zmq_broadcast, ZMQ_SNDMORE);

    char* copy = new char[strlen(msg)];
    strncpy(copy, msg, strlen(msg));

    zmq_msg_t msg_out;
#ifndef NDEBUG
    rc =
#endif
        zmq_msg_init_data(&msg_out, (void*) copy, strlen(msg), nullptr, nullptr);
    assert(rc == 0);
    zmq_msg_send(&msg_out, zmq_broadcast, 0);
#endif
}
