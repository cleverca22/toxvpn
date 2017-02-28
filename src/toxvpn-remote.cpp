#include <zmq.h>
#include <unistd.h>
#include <string.h>
#include <sys/select.h>
#include <algorithm>
#include <assert.h>
#include <errno.h>
#include <string>
#include <sys/socket.h>
#include <sys/un.h>

bool keep_running;

void read_sub_socket(void* subscriber) {
    int more;
    size_t more_size = sizeof(more);
    zmq_msg_t header, msg;
    zmq_msg_init(&header);
    char buffer[512];

    int rc = zmq_msg_recv(&header, subscriber, ZMQ_DONTWAIT);
    if((rc == -1) && (errno == EAGAIN)) {
        return;
    }
    printf("%d %d\n", rc, errno);
    assert(rc == 0);

    char* msg_contents = (char*) zmq_msg_data(&header);
    size_t msg_size = zmq_msg_size(&header);
    strncpy(buffer, msg_contents, msg_size);
    buffer[msg_size] = 0;
    puts(buffer);

    zmq_getsockopt(subscriber, ZMQ_RCVMORE, &more, &more_size);
    for(int i = 0; i < more; i++) {
        zmq_msg_init(&msg);
        zmq_msg_recv(&msg, subscriber, 0);
        msg_contents = (char*) zmq_msg_data(&msg);
        msg_size = zmq_msg_size(&msg);
        strncpy(buffer, msg_contents, msg_size);
        buffer[msg_size] = 0;
        printf("more %d %d: %s\n", i, msg_size, buffer);
        zmq_msg_close(&msg);
    }
    zmq_msg_close(&header);
}

void read_stdin(int socket) {
    char buffer[512];
    ssize_t count = read(STDIN_FILENO, buffer, 512);
    if(strncmp(buffer, "quit", 4) == 0) {
        keep_running = false;
        return;
    }
    write(socket, buffer, count);
}

void read_socket(int socket) {
    char buffer[512];
    ssize_t count = read(socket, buffer, 512);
    write(STDOUT_FILENO, buffer, count);
}

int main(int argc, char** argv) {
    void* zmq = zmq_ctx_new();
    void* subscriber = zmq_socket(zmq, ZMQ_SUB);
    zmq_connect(subscriber, "ipc:///run/toxvpn/controlbroadcast");
    zmq_setsockopt(subscriber, ZMQ_SUBSCRIBE, "all", 3);

    std::string unixSocket = "/run/toxvpn/control";

    int socket = ::socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un addr;
    bzero(&addr, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, unixSocket.c_str(), sizeof(addr.sun_path) - 1);
    connect(socket, (const struct sockaddr*) &addr, sizeof(struct sockaddr_un));

    fd_set readset;
    keep_running = true;
    while(keep_running) {
        FD_ZERO(&readset);
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 1000 * 1000; // todo, lower to 100
        int maxfd = 0;

        FD_SET(STDIN_FILENO, &readset);
        maxfd = std::max(maxfd, STDIN_FILENO);

        FD_SET(socket, &readset);
        maxfd = std::max(maxfd, socket);

        int r = select(maxfd + 1, &readset, NULL, NULL, &timeout);
        read_sub_socket(subscriber);
        if(r > 0) {
            if(FD_ISSET(STDIN_FILENO, &readset))
                read_stdin(socket);
            if(FD_ISSET(socket, &readset))
                read_socket(socket);
        } else if(r == 0) {
        } else {
            printf("select error %d %d %s\n", r, errno, strerror(errno));
        }
    }

    zmq_close(subscriber);
    zmq_ctx_term(zmq);
}
