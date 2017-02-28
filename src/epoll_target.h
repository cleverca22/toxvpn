extern int epoll_handle;

class EpollTarget {
public:
    virtual void handleReadData(Tox* tox) = 0;
#ifdef USE_EPOLL
    struct epoll_event event;
#endif
    int handle;
};
