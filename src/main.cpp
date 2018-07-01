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
    ssize_t written = write(fd, savedata, size);
    assert(written == size); // FIXME: check even if NDEBUG is disabled
    close(fd);
    delete[] savedata;
}

void do_bootstrap(Tox* tox, ToxVPNCore* toxvpn) {
    size_t size = toxvpn->nodes.size();
    assert(size > 0);
    cout << "DHT: bootstrap list has " << size << " nodes" << endl;

    size_t i = rand() % size;
    for (size_t j = 0; j < 2; ++j) {
        i = (i + j) % size;

        bootstrap_node &node = toxvpn->nodes[i];
        cout << "DHT: bootstrapping via "
             << node.ipv4.c_str() << ":" << node.port
             << " (node "<< i << "/" << size << ")" << endl;
        cout.flush();

        uint8_t* bootstrap_pub_key = new uint8_t[TOX_PUBLIC_KEY_SIZE];
        hex_string_to_bin(node.pubkey.c_str(), bootstrap_pub_key);

        if(!tox_bootstrap(tox, node.ipv4.c_str(), node.port, bootstrap_pub_key, NULL)) {
            cerr << "DHT: error: failed bootstrapping via node " << i << endl;
        }

        delete[] bootstrap_pub_key;
    }
    toxvpn->last_boostrap = steady_clock::now();
}

ToxVPNCore::ToxVPNCore() : listener(0){};
}

void MyFriendRequestCallback(Tox* tox,
                             const uint8_t* public_key,
                             const uint8_t* message,
                             size_t length,
                             void* user_data) {
    ToxVPNCore* toxvpn = static_cast<ToxVPNCore*>(user_data);
    char tox_printable_id[TOX_PUBLIC_KEY_SIZE * 2 + 1];
    string msg((char*) message, length);

    memset(tox_printable_id, 0, sizeof(tox_printable_id));
    to_hex(tox_printable_id, public_key, TOX_PUBLIC_KEY_SIZE);

    cout << "> friend request from " << tox_printable_id << endl;
    cout << "> ------------------ BEGIN ------------------" << endl;
    cout << "> " << message << endl;
    cout << "> ------------------- END -------------------" << endl;
    cout << "> to accept run:" << endl;
    cout << ">   whitelist " << tox_printable_id << endl << endl;
    cout.flush();

    char formated[512];
    snprintf(formated, 511, "friend request from %s", tox_printable_id);
    toxvpn->listener->broadcast(formated);
    saveState(tox);
}

#ifdef SYSTEMD
static void notify(const char* message) { sd_notify(0, message); }
#else
static void notify(const char* message) { }
#endif

bool did_ready = false;

void do_ready() {
    if(did_ready)
        return;
    did_ready = true;
    notify("READY=1");
}

string get_friend_name(Tox* tox, const uint32_t friend_number)
{
    size_t namesize = tox_friend_get_name_size(tox, friend_number, 0);
    uint8_t* friend_name = new uint8_t[namesize + 1];
    if (!tox_friend_get_name(tox, friend_number, friend_name, NULL)) {
        delete[] friend_name;
        return "";
    }
    string output((char *) friend_name, namesize);
    delete[] friend_name;
    return output;
}

void FriendConnectionUpdate(Tox* tox,
                            uint32_t friend_number,
                            TOX_CONNECTION connection_status,
                            void* user_data) {
    ToxVPNCore* toxvpn = static_cast<ToxVPNCore*>(user_data);
    string friend_name = get_friend_name(tox, friend_number);

    char formated[512];
    switch(connection_status) {
    case TOX_CONNECTION_NONE:
        snprintf(formated, 511, "friend #%d (%s) went offline", friend_number,
                 friend_name.c_str());
        mynic->removePeer(friend_number);
        break;
    case TOX_CONNECTION_TCP:
        snprintf(formated, 511, "friend #%d (%s) connected via TCP relay",
                 friend_number, friend_name.c_str());
        break;
    case TOX_CONNECTION_UDP:
        snprintf(formated, 511, "friend #%d (%s) connected via direct UDP",
                 friend_number, friend_name.c_str());
        break;
    }

    cout << "> " << formated << endl << endl;
    cout.flush();

    if(toxvpn->listener)
        toxvpn->listener->broadcast(formated);
}

void MyFriendMessageCallback(Tox* tox,
                             uint32_t friend_number,
                             TOX_MESSAGE_TYPE type,
                             const uint8_t* msg,
                             size_t length,
                             void*) {
    string friend_name = get_friend_name(tox, friend_number);
    string message((char*) msg, length);

    cout << "> message from friend #" << friend_number << " (" << friend_name << ") of type " << type << endl;
    cout << "> ------------------ BEGIN ------------------" << endl;
    cout << "> " << message << endl;
    cout << "> ------------------- END -------------------" << endl << endl;
    cout.flush();
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
    string friend_name = get_friend_name(tox, friend_number);

    cout << "> status change from friend #" << friend_number << " (" << friend_name << ")" << endl;
    cout << "> ------------------ BEGIN ------------------" << endl;
    cout << "> " << message << endl;
    cout << "> ------------------- END -------------------" << endl;

    json ip;

    try {
        json root = json::parse(std::string((const char*) message, length));
        ip = root["ownip"];
    } catch(...) {
        cout << "> is of wrong format, ignoring" << endl << endl;
        cout.flush();
        return;
    }

    if(!ip.is_string()) {
        cout << "> is of wrong format, ignoring" << endl << endl;
        cout.flush();
        return;
    }

    string peerip = ip;
    struct in_addr peerBinary;
    if (!inet_pton(AF_INET, peerip.c_str(), &peerBinary)) {
        cout << "> has unparsable IP, ignoring" << endl << endl;
        cout.flush();
        return;
    }

    cout << "> parsed successfully" << endl << endl;

    mynic->setPeerIp(peerBinary, friend_number);

    cout << "NIC: routing " << peerip.c_str()
         << " to friend #" << friend_number
         << " (" << friend_name << ")"
         << endl << endl;
    cout.flush();

    saveState(tox);
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
    //cerr << "int " << something << endl;
    keep_running = false;
}

void add_auto_friends(Tox* tox, ToxVPNCore* toxvpn) {
    uint8_t peerbinary[TOX_ADDRESS_SIZE];
    TOX_ERR_FRIEND_ADD error;
    const char* msg = "auto-toxvpn";
    bool need_save = false;

    for(std::vector<string>::iterator it = toxvpn->auto_friends.begin();
        it != toxvpn->auto_friends.end(); ++it) {
        string toxid = *it;
        hex_string_to_bin(toxid.c_str(), peerbinary);
        tox_friend_add(tox, (uint8_t*) peerbinary, (uint8_t*) msg, strlen(msg),
                       &error);
        switch(error) {
        case TOX_ERR_FRIEND_ADD_OK:
            need_save = true;
            cout << "added " << toxid << endl;
            cout.flush();
            break;
        case TOX_ERR_FRIEND_ADD_ALREADY_SENT: break;
        case TOX_ERR_FRIEND_ADD_BAD_CHECKSUM:
            cerr << "crc error when handling auto-friend" << toxid << endl;
            break;
        default:
            cerr << "err code " << error << endl;
            break;
        }
    }

    if(need_save)
        saveState(tox);
}

void connection_status(Tox* tox,
                       TOX_CONNECTION connection_status,
                       void* user_data) {
    ToxVPNCore* toxvpn = static_cast<ToxVPNCore*>(user_data);
    uint8_t toxid[TOX_ADDRESS_SIZE];
    tox_self_get_address(tox, toxid);
    char tox_printable_id[TOX_ADDRESS_SIZE * 2 + 1];
    memset(tox_printable_id, 0, sizeof(tox_printable_id));
    to_hex(tox_printable_id, toxid, TOX_ADDRESS_SIZE);

    const char* msg = 0;

    switch(connection_status) {
    case TOX_CONNECTION_NONE:
        msg = "offline";
        break;
    case TOX_CONNECTION_TCP:
        msg = "connected via TCP";
        do_ready();
        add_auto_friends(tox, toxvpn);
        break;
    case TOX_CONNECTION_UDP:
        msg = "connected via UDP";
        do_ready();
        add_auto_friends(tox, toxvpn);
        break;
    }
    if(msg) {
        cout << "DHT: " << msg << endl;
        cout.flush();
        char buffer[128];
        snprintf(buffer, 120, "STATUS=%s, id=%s", msg, tox_printable_id);
        notify(buffer);
    }
    saveState(tox);
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

int main(int argc, char** argv) {
    ToxVPNCore toxvpn;

    assert(strlen(BOOTSTRAP_FILE) > 5);

    json bootstrapRoot;

    try {
        if (strcmp(BOOTSTRAP_FILE, "") == 0) {
            cerr << "bootstrap file path is invalid" << endl;
            return -2;
        }
        bootstrapRoot = json::parse(readFile(BOOTSTRAP_FILE));
        json nodes = bootstrapRoot["nodes"];
        assert(nodes.is_array());
        for(size_t i = 0; i < nodes.size(); i++) {
            json e = nodes[i];
            // cout << "node " << i << endl;
            std::string ipv4 = e["ipv4"];
            uint16_t port = e["port"];
            std::string pubkey = e["public_key"];
            // cout << ipv4.c_str() << ":" port << " key:" << pubkey.c_str() << endl;
            toxvpn.nodes.push_back(bootstrap_node(ipv4, port, pubkey));
        }
    } catch(...) {
        cerr << "exception while trying to load bootstrap nodes" << endl;
        return -2;
    }

    toxvpn.nodes.shrink_to_fit();

#ifdef USE_SELECT
    fd_set readset;
#endif
#ifdef USE_EPOLL
    epoll_handle = epoll_create(20);
    assert(epoll_handle >= 0);
#endif
#ifdef ZMQ
    void* zmq = zmq_ctx_new();
#endif

    route_init();

#ifndef WIN32
    struct sigaction interupt;
    memset(&interupt, 0, sizeof(interupt));
    interupt.sa_handler = &handle_int;
    sigaction(SIGINT, &interupt, NULL);
#endif

    json configRoot;

    int opt;
    TOX_ERR_NEW new_error;

    bool stdin_is_socket = false;
    string changeIp;
    string unixSocket;
    struct passwd* target_user = NULL;

    struct Tox_Options* opts = tox_options_new(NULL);
    opts->start_port = 33445;
    opts->end_port = 33445 + 100;
    while((opt = getopt(argc, argv, "sl:u:p:i:a:h")) != -1) {
        switch(opt) {
        case 's':
            stdin_is_socket = true;
            break;
        case 'l':
#ifdef WIN32
            cerr << "-l is not supported on windows" << endl;
            return -1;
#else
            unixSocket = optarg;
            break;
#endif
        case 'u':
#if defined(WIN32) || defined(__CYGWIN__)
            cerr << "-u is not currently supported on windows" << endl;
            return -1;
#else
            target_user = getpwnam(optarg);
            assert(target_user);
#endif
            break;
        case 'p':
            opts->start_port = opts->end_port =
                (uint16_t) strtol(optarg, 0, 10);
            break;
        case 'i':
            changeIp = optarg;
            break;
        case 'a':
            toxvpn.auto_friends.push_back(string(optarg));
            break;
        case 'h':
        case '?':
        default:
            cout << "-s\t\ttreat stdin as a unix socket server" << endl;
            cout << "-i <IP>\t\tuse this IP on the vpn" << endl;
            cout << "-p <port>\tbind on a given port" << endl;
            cout << "-a <ID>\t\tadd this ID as a friend (can be repeated)" << endl;
            cout << "-l <path>\tlisten on a unix socket at this path" << endl;
            cout << "-u <user>\tswitch to this user once root is no longer required"
                 << endl;
            cout << "-h\t\tprint this help" << endl;
            return 0;
        }
    }
    toxvpn.auto_friends.shrink_to_fit();

#if !defined(WIN32) && !defined(__CYGWIN__)
    if(!target_user)
        target_user = getpwnam("root");
#endif

    mynic = new NetworkInterface();
    if(target_user) {
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

        if(chdir(target_user->pw_dir)) {
            cerr << "unable to cd into " << target_user->pw_dir
                 << " ($HOME): " << strerror(errno) << endl;
            return -1;
        }
    }

    if(chdir(".toxvpn")) {
        mkdir(".toxvpn"
#ifndef WIN32
              , 0755
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
            cout.flush();
            cin >> myip;
            configRoot["myip"] = myip;
        }
        saveConfig(configRoot);
    }

    json root{{"ownip", configRoot["myip"]}};

    Tox* my_tox;
    bool want_bootstrap = true;

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

    my_tox = tox_new(opts, &new_error);
    if(!my_tox) {
        opts->ipv6_enabled = false;
        my_tox = tox_new(opts, &new_error);
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
    tox_options_free(opts);

    uint8_t toxid[TOX_ADDRESS_SIZE];
    tox_self_get_address(my_tox, toxid);
    char tox_printable_id[TOX_ADDRESS_SIZE * 2 + 1];
    memset(tox_printable_id, 0, sizeof(tox_printable_id));
    to_hex(tox_printable_id, toxid, TOX_ADDRESS_SIZE);

    cout << "my ToxID is " << tox_printable_id << endl;
    cout << "my    IP is " << myip.c_str() << endl;
    cout.flush();

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
    const char* hostname = hostinfo.nodename;
#else
    const char* hostname = "windows";
#endif
    tox_self_set_name(my_tox, (const uint8_t*) hostname, strlen(hostname), NULL);

    std::string json_str = root.dump();
    if(json_str[json_str.length() - 1] == '\n') {
        json_str.erase(json_str.length() - 1, 1);
    }
    tox_self_set_status_message(my_tox, (const uint8_t*) json_str.data(),
                                json_str.length(),
                                NULL); // Sets the status message

    /* Set the user status to TOX_USER_STATUS_NONE. Other possible values:
     * TOX_USER_STATUS_AWAY and TOX_USER_STATUS_BUSY */
    tox_self_set_status(my_tox, TOX_USER_STATUS_NONE);

    mynic->configure(myip, my_tox);
    Control* control = 0;

    if(unixSocket.length()) {
#if defined(ZMQ)
        toxvpn.listener = new SocketListener(mynic, unixSocket, zmq);
#else
        toxvpn.listener = new SocketListener(mynic, unixSocket);
#endif
    } else if(stdin_is_socket) {
        toxvpn.listener = new SocketListener(mynic);
    } else {
        control = new Control(mynic);
    }

    /* Bootstrap from the node defined above */
    if(want_bootstrap)
        do_bootstrap(my_tox, &toxvpn);

    while(keep_running) {
        int interval = tox_iteration_interval(my_tox);

#ifdef USE_SELECT
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = interval * 1000;

        FD_ZERO(&readset);

        int maxfd = 0;
        int r = 0;
#if 0
        maxfd = tox_populate_fdset(my_tox, &readset);
#endif
#ifndef WIN32
        if(control)
            maxfd = std::max(maxfd, control->populate_fdset(&readset));
        if(toxvpn.listener)
            maxfd = std::max(maxfd, toxvpn.listener->populate_fdset(&readset));
#else
        if(maxfd == 0) {
            Sleep(interval);
            r = -2;
        } else
#endif
            r = select(maxfd + 1, &readset, NULL, NULL, &timeout);
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
                cout << "winsock error: " << error << endl;
#endif
                cout << "select returned " << r << " error: " << strerror(errno);
            }
        }
#endif
#ifdef USE_EPOLL
        struct epoll_event events[10];
        int count = epoll_wait(epoll_handle, events, 10, interval);
        if(count == -1)
            cerr << "epoll error " << strerror(errno) << endl;
        else {
            for(int i = 0; i < count; i++) {
                EpollTarget* t = (EpollTarget*) events[i].data.ptr;
                t->handleReadData(my_tox);
            }
        }
#endif

        tox_iterate(
            my_tox,
            &toxvpn); // will call the callback functions defined and registered

        TOX_CONNECTION conn_status = tox_self_get_connection_status(my_tox);
        if(conn_status == TOX_CONNECTION_NONE) {
            steady_clock::time_point now = steady_clock::now();
            duration<double> time_span =
                duration_cast<duration<double>>(now - toxvpn.last_boostrap);
            if(time_span.count() > 10) {
                do_bootstrap(my_tox, &toxvpn);
            }
        }
    } // while(keep_running)
    //cout << "shutting down" << enl;
    notify("STOPPING=1");
    saveState(my_tox);
    tox_kill(my_tox);
#ifdef ZMQ
    zmq_ctx_term(zmq);
#endif
    if(control)
        delete control;
    return 0;
}
