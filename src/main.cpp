#include "main.h"
#include "control.h"
#include "listener.h"
#include "interface.h"
#include "route.h"
#ifdef ZMQ
#include <zmq.h>
#endif
#include <chrono>

using namespace std;
using namespace ToxVPN;
using namespace std::chrono;

using json = nlohmann::json;

NetworkInterface* mynic;
volatile bool keep_running = true;
std::string myip;
int epoll_handle;

void hex_string_to_bin(const char* hex_string, uint8_t* ret) {
    // byte is represented by exactly 2 hex digits, so lenth of binary string
    // is half of that of the hex one. only hex string with even length
    // valid. the more proper implementation would be to check if
    // strlen(hex_string)
    // is odd and return error code if it is. we assume strlen is even. if it's
    // not
    // then the last byte just won't be written in 'ret'.
    size_t i, len = strlen(hex_string) / 2;
    const char* pos = hex_string;

    for(i = 0; i < len; ++i, pos += 2)
        sscanf(pos, "%2hhx", &ret[i]);
}

void to_hex(char* a, const uint8_t* p, int size) {
    char buffer[3];
    for(int i = 0; i < size; i++) {
        snprintf(buffer, 3, "%02x", p[i]);
        a[i * 2] = buffer[0];
        a[i * 2 + 1] = buffer[1];
    }
}
namespace ToxVPN {
void saveState(Tox* tox) {
    size_t size = tox_get_savedata_size(tox);
    uint8_t* savedata = new uint8_t[size];
    tox_get_savedata(tox, savedata);
    int fd = open("savedata", O_TRUNC | O_WRONLY | O_CREAT, 0644);
    assert(fd);
#ifndef NDEBUG
    ssize_t written =
#endif
        write(fd, savedata, size);
    assert(written > 0); // FIXME: check even if NDEBUG is disabled
    close(fd);
    delete[] savedata;
}

void do_bootstrap(Tox* tox, ToxVPNCore* toxvpn) {
    assert(toxvpn->nodes.size() > 0);
    size_t i = rand() % toxvpn->nodes.size();
    printf("%lu / %lu\n", i, toxvpn->nodes.size());
    uint8_t* bootstrap_pub_key = new uint8_t[TOX_PUBLIC_KEY_SIZE];
    hex_string_to_bin(toxvpn->nodes[i].pubkey.c_str(), bootstrap_pub_key);
    tox_bootstrap(tox, toxvpn->nodes[i].ipv4.c_str(), toxvpn->nodes[i].port,
                  bootstrap_pub_key, nullptr);
    delete[] bootstrap_pub_key;
    toxvpn->last_boostrap = steady_clock::now();
    fflush(stdout);
}

ToxVPNCore::ToxVPNCore() : listener(nullptr){}
}

void MyFriendRequestCallback(Tox* tox,
                             const uint8_t* public_key,
                             const uint8_t* message,
                             size_t length,
                             void* user_data) {
    ToxVPNCore* toxvpn = static_cast<ToxVPNCore*>(user_data);
    char tox_printable_id[TOX_PUBLIC_KEY_SIZE * 2 + 1];
    string msg((const char*) message, length);

    memset(tox_printable_id, 0, sizeof(tox_printable_id));
    to_hex(tox_printable_id, public_key, TOX_PUBLIC_KEY_SIZE);

    char formated[512];
    snprintf(formated, 511, "Friend request: %s\nto accept, run 'whitelist %s'",
             message, tox_printable_id);

    printf("%s\n", formated);
    fflush(stdout);

    toxvpn->listener->broadcast(formated);
    saveState(tox);
}

#ifdef SYSTEMD
static void notify(const char* message) { sd_notify(0, message); }
#endif

bool did_ready = false;

void do_ready() {
    if(did_ready)
        return;
    did_ready = true;
#ifdef SYSTEMD
    notify("READY=1");
#endif
}

void FriendConnectionUpdate(Tox* tox,
                            uint32_t friend_number,
                            Tox_Connection connection_status,
                            void* user_data) {
    ToxVPNCore* toxvpn = static_cast<ToxVPNCore*>(user_data);
    size_t namesize = tox_friend_get_name_size(tox, friend_number, nullptr);
    uint8_t* friendname = new uint8_t[namesize + 1];
    tox_friend_get_name(tox, friend_number, friendname, nullptr);
    friendname[namesize] = 0;

    char formated[512];

    switch(connection_status) {
    case TOX_CONNECTION_NONE:
        snprintf(formated, 511, "friend %d(%s) went offline", friend_number,
                 friendname);
        mynic->removePeer(friend_number);
        break;
    case TOX_CONNECTION_TCP:
        snprintf(formated, 511, "friend %d(%s) connected via tcp",
                 friend_number, friendname);
        break;
    case TOX_CONNECTION_UDP:
        snprintf(formated, 511, "friend %d(%s) connected via udp",
                 friend_number, friendname);
        break;
    }
    delete[] friendname;

    if(toxvpn->listener)
        toxvpn->listener->broadcast(formated);

    printf("%s\n", formated);
    fflush(stdout);
}

void MyFriendMessageCallback(Tox*,
                             uint32_t friend_number,
                             Tox_Message_Type type,
                             const uint8_t* message,
                             size_t length,
                             void*) {
    string msg((const char*) message, length);
    cout << "message" << friend_number << msg << type << endl;
}

#ifdef WIN32
void inet_pton(int type, const char* input, struct in_addr* output) {
    unsigned long result = inet_addr(input);
    output->S_un.S_addr = result;
}
#endif

void MyFriendStatusCallback(Tox* tox,
                            uint32_t friend_number,
                            const uint8_t* message,
                            size_t length,
                            void*) {
    printf("status msg #%d %s\n", friend_number, message);
    try {
        json root = json::parse(std::string((const char*) message, length));
        json ip = root["ownip"];
        if(ip.is_string()) {
            std::string peerip = ip;
            struct in_addr peerBinary;
            inet_pton(AF_INET, peerip.c_str(), &peerBinary);
            printf("setting friend#%d ip to %s\n", friend_number,
                   peerip.c_str());
            mynic->setPeerIp(peerBinary, friend_number);
        } else {
            // FIXME: handle error condition instead of silently failing
        }
    } catch(...) { printf("unable to parse status, ignoring\n"); }
    saveState(tox);
    fflush(stdout);
}

void MyFriendLossyPacket(Tox*,
                         uint32_t friend_number,
                         const uint8_t* data,
                         size_t length,
                         void*) {
    if(data[0] == 200) {
        mynic->processPacket(data + 1, length - 1, friend_number);
    }
}

void handle_int(int something) {
    printf("int %d!", something);
    keep_running = false;
}

void add_auto_friends(Tox* tox, ToxVPNCore* toxvpn) {
    uint8_t peerbinary[TOX_ADDRESS_SIZE];
    Tox_Err_Friend_Add error;
    const char* msg = "auto-toxvpn";
    bool need_save = false;

    for(std::vector<string>::iterator it = toxvpn->auto_friends.begin();
        it != toxvpn->auto_friends.end(); ++it) {
        string toxid = *it;
        hex_string_to_bin(toxid.c_str(), peerbinary);
        tox_friend_add(tox, (const uint8_t*) peerbinary, (const uint8_t*) msg, strlen(msg),
                       &error);
        switch(error) {
        case TOX_ERR_FRIEND_ADD_OK:
            need_save = true;
            cout << "added " << toxid << "\n";
            break;
        case TOX_ERR_FRIEND_ADD_ALREADY_SENT: break;
        case TOX_ERR_FRIEND_ADD_BAD_CHECKSUM:
            cerr << "crc error when handling auto-friend" << toxid << "\n";
            break;
        default: printf("err code %d\n", error);
        }
    }
    if(need_save)
        saveState(tox);
}

void connection_status(Tox* tox,
                       Tox_Connection connection_status,
                       void* user_data) {
    ToxVPNCore* toxvpn = static_cast<ToxVPNCore*>(user_data);
    uint8_t toxid[TOX_ADDRESS_SIZE];
    tox_self_get_address(tox, toxid);
    char tox_printable_id[TOX_ADDRESS_SIZE * 2 + 1];
    memset(tox_printable_id, 0, sizeof(tox_printable_id));
    to_hex(tox_printable_id, toxid, TOX_ADDRESS_SIZE);

    char buffer[128];
    const char* msg = nullptr;

    switch(connection_status) {
    case TOX_CONNECTION_NONE:
        msg = "offline";
        puts("connection lost");
        break;
    case TOX_CONNECTION_TCP:
        msg = "connected via tcp";
        puts("tcp connection established");
        do_ready();
        add_auto_friends(tox, toxvpn);
        break;
    case TOX_CONNECTION_UDP:
        msg = "connected via udp";
        puts("udp connection established");
        do_ready();
        add_auto_friends(tox, toxvpn);
        break;
    }
    if(msg) {
        snprintf(buffer, 120, "STATUS=%s, id=%s", msg, tox_printable_id);
#ifdef SYSTEMD
        notify(buffer);
#endif
    }
    saveState(tox);
    fflush(stdout);
}

std::string readFile(std::string path) {
    std::string output;
    FILE* handle = fopen(path.c_str(), "r");
    if(!handle)
        return "";
    char buffer[100];
    while(size_t bytes = fread(buffer, 1, 99, handle)) {
        std::string part(buffer, bytes);
        output += part;
    }
    fclose(handle);
    return output;
}

void saveConfig(json root) {
    std::string json = root.dump();
    FILE* handle = fopen("config.json", "w");
    if(!handle) {
        cerr << "unable to open config file for writting" << endl;
        exit(-1);
    }
    const char* data = json.c_str();
    fwrite(data, json.length(), 1, handle);
    fclose(handle);
}

#ifdef ZMQ
struct zmq_ctx_deleter {
    void operator()(void *zmq) const { zmq_ctx_term(zmq); }
};

using zmq_ptr = std::unique_ptr<void, zmq_ctx_deleter>;
#endif

struct tox_options_deleter {
    void operator()(Tox_Options *opts) const { tox_options_free(opts); }
};

using tox_options_ptr = std::unique_ptr<Tox_Options, tox_options_deleter>;

int main(int argc, char** argv) {
#ifdef USE_EPOLL
    epoll_handle = epoll_create(20);
    assert(epoll_handle >= 0);
#endif

#ifdef ZMQ
    zmq_ptr zmq(zmq_ctx_new());
#endif
    ToxVPNCore toxvpn;

    assert(strlen(BOOTSTRAP_FILE) > 5);

    json bootstrapRoot;

    try {
        if (strcmp(BOOTSTRAP_FILE, "") == 0) {
          cerr << "bootstrap file path is invalid\n";
          return -2;
        }
        bootstrapRoot = json::parse(readFile(BOOTSTRAP_FILE));
        json nodes = bootstrapRoot["nodes"];
        assert(nodes.is_array());
        for(size_t i = 0; i < nodes.size(); i++) {
            json e = nodes[i];
            // printf("node %d\n",i);
            std::string ipv4 = e["ipv4"];
            uint16_t port = e["port"];
            std::string pubkey = e["public_key"];
            // printf("%s %d %s\n", ipv4.c_str(), port, pubkey.c_str());
            toxvpn.nodes.push_back(bootstrap_node(ipv4, port, pubkey));
        }
    } catch(...) {
      cerr << "exception while trying to load bootstrap nodes";
      return -2;
    }

    toxvpn.nodes.shrink_to_fit();

    route_init();

#ifndef WIN32
    struct sigaction interupt;
    memset(&interupt, 0, sizeof(interupt));
    interupt.sa_handler = &handle_int;
    sigaction(SIGINT, &interupt, nullptr);
#endif

    json configRoot;

    int opt;
    Tox_Err_New new_error;
    bool stdin_is_socket = false;
    string changeIp;
    string unixSocket;
    tox_options_ptr opts(tox_options_new(nullptr));
    opts->start_port = 33445;
    opts->end_port = 33445 + 100;
    struct passwd* target_user = nullptr;
    while((opt = getopt(argc, argv, "shi:l:u:p:a:")) != -1) {
        switch(opt) {
        case 's': stdin_is_socket = true; break;
        case 'h':
        case '?':
            cout << "-s\t\ttreat stdin as a unix socket server" << endl;
            cout << "-i <IP>\t\tuse this IP on the vpn" << endl;
            cout << "-l <path>\tlisten on a unix socket at this path" << endl;
            cout << "-u <user>\tswitch to this user once root is no longer "
                    "required"
                 << endl;
            cout << "-p <port>\tbind on a given port" << endl;
            cout << "-h\t\tprint this help" << endl;
            return 0;
        case 'i': changeIp = optarg; break;
        case 'l': unixSocket = optarg; break;
        case 'u':
#if defined(WIN32) || defined(__CYGWIN__)
            puts("-u not currently supported on windows");
#else
            target_user = getpwnam(optarg);
            assert(target_user);
#endif
            break;
        case 'p':
            opts->start_port = opts->end_port =
                (uint16_t) strtol(optarg, nullptr, 10);
            break;
        case 'a': toxvpn.auto_friends.push_back(string(optarg)); break;
        }
    }
    toxvpn.auto_friends.shrink_to_fit();

    puts("creating interface");
    mynic = new NetworkInterface();
#if defined(WIN32) || defined(__CYGWIN__)
    puts("no drop root support yet");
    if(0) { // TODO, cd into %AppData%
#else
    if(target_user) {
        puts("setting uid");
#if !defined(WIN32) && !defined(__APPLE__) && !defined(__CYGWIN__)
        cap_value_t cap_values[] = {CAP_NET_ADMIN};
        cap_t caps;

        caps = cap_get_proc();
        cap_set_flag(caps, CAP_PERMITTED, 1, cap_values, CAP_SET);
        cap_set_proc(caps);
        prctl(PR_SET_KEEPCAPS, 1, 0, 0, 0);
        cap_free(caps);
#endif

        if(setgid(target_user->pw_gid)) {
            cerr << "unable to setgid()" << endl;
            return -2;
        }
        if(setuid(target_user->pw_uid)) {
            cerr << "unable to setuid()" << endl;
            return -2;
        }

#if !defined(WIN32) && !defined(__APPLE__) && !defined(__CYGWIN__)
        caps = cap_get_proc();
        cap_clear(caps);
        cap_set_flag(caps, CAP_PERMITTED, 1, cap_values, CAP_SET);
        cap_set_flag(caps, CAP_EFFECTIVE, 1, cap_values, CAP_SET);
        cap_set_proc(caps);
        cap_free(caps);
#endif
    } else
        target_user = getpwnam("root");
    if(chdir(target_user->pw_dir)) {
#endif
        printf("unable to cd into $HOME(%s): %s\n", target_user->pw_dir, strerror(errno));
        return -1;
    }
    if(chdir(".toxvpn")) {
        mkdir(".toxvpn"
#ifndef WIN32
              ,
              0755
#endif
              );
        chdir(".toxvpn");
    }

    try {
        std::string config = readFile("config.json");
        configRoot = json::parse(config);
        if(changeIp.length() > 0) {
            configRoot["myip"] = changeIp;
            saveConfig(configRoot);
        }
        json ip = configRoot["myip"];
        if(ip.is_string()) {
            myip = ip;
        }
    } catch(...) {
        if(changeIp.length() > 0) {
            configRoot["myip"] = myip = changeIp;
        } else {
            cout << "what is the VPN ip of this computer?" << endl;
            cin >> myip;
            configRoot["myip"] = myip;
        }
        saveConfig(configRoot);
    }

    json root{{"ownip", configRoot["myip"]}};

    Tox* my_tox;
    bool want_bootstrap = false;
    int oldstate = open("savedata", O_RDONLY);
    if(oldstate >= 0) {
        struct stat info;
        fstat(oldstate, &info);
        uint8_t* temp = new uint8_t[info.st_size];
        ssize_t size = read(oldstate, temp, info.st_size);
        close(oldstate);
        assert(size == info.st_size);
        opts->savedata_type = TOX_SAVEDATA_TYPE_TOX_SAVE;
        opts->savedata_data = temp;
        opts->savedata_length = size;
    }

    want_bootstrap = true;
    my_tox = tox_new(opts.get(), &new_error);
    if(!my_tox) {
        opts->ipv6_enabled = false;
        my_tox = tox_new(opts.get(), &new_error);
    }
    switch(new_error) {
    case TOX_ERR_NEW_OK: break;
    case TOX_ERR_NEW_PORT_ALLOC:
        cerr << "unable to bind to a port between " << opts->start_port
             << " and " << opts->end_port << endl;
        return 1;
    default:
        cerr << "unhandled error code on tox_new: " << new_error << endl;
        return 2;
    }
    assert(my_tox);
    if(opts->savedata_data)
        delete[] opts->savedata_data;
    opts = nullptr;

    uint8_t toxid[TOX_ADDRESS_SIZE];
    tox_self_get_address(my_tox, toxid);
    char tox_printable_id[TOX_ADDRESS_SIZE * 2 + 1];
    memset(tox_printable_id, 0, sizeof(tox_printable_id));
    to_hex(tox_printable_id, toxid, TOX_ADDRESS_SIZE);
    printf("my id is %s and IP is %s\n", tox_printable_id, myip.c_str());

    /* Register the callbacks */
    tox_callback_friend_request(my_tox, MyFriendRequestCallback);
    tox_callback_friend_message(my_tox, MyFriendMessageCallback);
    tox_callback_friend_status_message(my_tox, MyFriendStatusCallback);
    tox_callback_friend_connection_status(my_tox, FriendConnectionUpdate);
    tox_callback_friend_lossy_packet(my_tox, MyFriendLossyPacket);
    tox_callback_self_connection_status(my_tox, &connection_status);

/* Define or load some user details for the sake of it */
#ifndef WIN32
    struct utsname hostinfo;
    uname(&hostinfo);
    tox_self_set_name(my_tox, (const uint8_t*) hostinfo.nodename,
                      strlen(hostinfo.nodename), nullptr); // Sets the username
#else
    const char* hostname = "windows";
    tox_self_set_name(my_tox, (const uint8_t*) hostname, strlen(hostname),
                      nullptr);
#endif
    std::string json_str = root.dump();
    if(json_str[json_str.length() - 1] == '\n') {
        json_str.erase(json_str.length() - 1, 1);
    }
    tox_self_set_status_message(my_tox, (const uint8_t*) json_str.data(),
                                json_str.length(),
                                nullptr); // Sets the status message

    /* Set the user status to TOX_USER_STATUS_NONE. Other possible values:
     * TOX_USER_STATUS_AWAY and TOX_USER_STATUS_BUSY */
    tox_self_set_status(my_tox, TOX_USER_STATUS_NONE);

    /* Bootstrap from the node defined above */
    if(want_bootstrap)
        do_bootstrap(my_tox, &toxvpn);

#ifdef USE_SELECT
    fd_set readset;
#endif
    mynic->configure(myip, my_tox);
    Control* control = nullptr;

    if(unixSocket.length()) {
#ifdef WIN32
        puts("error, -l is linux only");
        return -1;
#elif defined(ZMQ)
        toxvpn.listener = new SocketListener(mynic, unixSocket, zmq.get());
#else
        toxvpn.listener = new SocketListener(mynic, unixSocket);
#endif
    } else if(stdin_is_socket) {
        toxvpn.listener = new SocketListener(mynic);
    } else {
        control = new Control(mynic);
    }
    fflush(stdout);
    while(keep_running) {
#ifdef USE_SELECT
        FD_ZERO(&readset);
        struct timeval timeout;
        int maxfd = 0;
#if 0
    maxfd = tox_populate_fdset(my_tox,&readset);
#endif
#ifndef WIN32
        if(control)
            maxfd = std::max(maxfd, control->populate_fdset(&readset));
        if(toxvpn.listener)
            maxfd = std::max(maxfd, toxvpn.listener->populate_fdset(&readset));
#endif
#endif
        int interval = tox_iteration_interval(my_tox);
#ifdef USE_SELECT
        timeout.tv_sec = 0;
        timeout.tv_usec = interval * 1000;
        int r;
#ifdef WIN32
        if(maxfd == 0) {
            Sleep(interval);
            r = -2;
        } else
#endif
            r = select(maxfd + 1, &readset, nullptr, nullptr, &timeout);
        if(r > 0) {
            if(control && FD_ISSET(control->handle, &readset))
                control->handleReadData(my_tox, &toxvpn);
            if(toxvpn.listener && FD_ISSET(toxvpn.listener->socket, &readset))
                toxvpn.listener->doAccept();
            if(toxvpn.listener)
                toxvpn.listener->checkFds(&readset, my_tox, &toxvpn);
        } else if(r == 0) {
        } else {
            if(r != -2) {
#ifdef WIN32
                int error = WSAGetLastError();
                printf("winsock error %d %d\n", error, r);
#endif
                printf("select error %d %d %s\n", r, errno, strerror(errno));
            }
        }
#endif

        tox_iterate(
            my_tox,
            &toxvpn); // will call the callback functions defined and registered

#ifdef USE_EPOLL
        struct epoll_event events[10];
        int count = epoll_wait(epoll_handle, events, 10, interval);
        if(count == -1)
            std::cout << "epoll error " << strerror(errno) << std::endl;
        else {
            for(int i = 0; i < count; i++) {
                EpollTarget* t = (EpollTarget*) events[i].data.ptr;
                t->handleReadData(my_tox);
            }
        }
#endif
        Tox_Connection conn_status = tox_self_get_connection_status(my_tox);
        if(conn_status == TOX_CONNECTION_NONE) {
            steady_clock::time_point now = steady_clock::now();
            duration<double> time_span =
                duration_cast<duration<double>>(now - toxvpn.last_boostrap);
            if(time_span.count() > 10) {
                do_bootstrap(my_tox, &toxvpn);
            }
        }
    } // while(keep_running)
#ifdef SYSTEMD
    notify("STOPPING=1");
#endif
    puts("shutting down");
    saveState(my_tox);
    tox_kill(my_tox);
    if(control)
        delete control;
    return 0;
}
