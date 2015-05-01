#include <tox/tox.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <json/json.h>
#include "tunnel.h"
#include "control.h"
#include <errno.h>
#include <iostream>
#include "main.h"

#define BOOTSTRAP_ADDRESS "23.226.230.47"
#define BOOTSTRAP_PORT 33445
#define BOOTSTRAP_KEY "A09162D68618E742FFBCA1C2C70385E6679604B2D80EA6E84AD0996A1AC8A074"

Tunnel *tunnels[100];
volatile bool keep_running = true;
std::string myip;
int epoll_handle;

void hex_string_to_bin(const char *hex_string, uint8_t *ret)
{
    // byte is represented by exactly 2 hex digits, so lenth of binary string
    // is half of that of the hex one. only hex string with even length
    // valid. the more proper implementation would be to check if strlen(hex_string)
    // is odd and return error code if it is. we assume strlen is even. if it's not
    // then the last byte just won't be written in 'ret'.
    size_t i, len = strlen(hex_string) / 2;
    const char *pos = hex_string;

    for (i = 0; i < len; ++i, pos += 2)
        sscanf(pos, "%2hhx", &ret[i]);

}
void to_hex(char *a, const uint8_t *p, int size) {
	char buffer[3];
	for (int i=0; i<size; i++) {
		int x = snprintf(buffer,3,"%02x",p[i]);
		a[i*2] = buffer[0];
		a[i*2+1] = buffer[1];
	}
}
void saveState(Tox *tox) {
	int size = tox_get_savedata_size(tox);
	uint8_t *savedata = new uint8_t[size];
	tox_get_savedata(tox,savedata);
	int fd = open("savedata",O_TRUNC|O_WRONLY|O_CREAT,0644);
	assert(fd);
	int written = write(fd,savedata,size);
	assert(written == size);
	close(fd);
}
void MyFriendRequestCallback(Tox *tox, const uint8_t *public_key, const uint8_t *message, size_t length, void *user_data) {
	char tox_printable_id[TOX_PUBLIC_KEY_SIZE * 2 + 1];

	memset(tox_printable_id, 0, sizeof(tox_printable_id));
	to_hex(tox_printable_id, public_key,TOX_PUBLIC_KEY_SIZE);
	printf("Friend request: %s\nto accept, run 'whitelist %s'\n", message, tox_printable_id);
	saveState(tox);
}
void FriendConnectionUpdate(Tox *tox, uint32_t friend_number, TOX_CONNECTION connection_status, void *user_data) {
	switch (connection_status) {
	case TOX_CONNECTION_NONE:
		printf("friend %d went offline\n",friend_number);
		break;
	case TOX_CONNECTION_TCP:
		printf("friend %d connected via tcp\n",friend_number);
		break;
	case TOX_CONNECTION_UDP:
		printf("friend %d connected via udp\n",friend_number);
		break;
	}
}
void MyFriendMessageCallback(Tox *tox, uint32_t friend_number, TOX_MESSAGE_TYPE type, const uint8_t *message, size_t length, void *user_data) {
	printf("message %d %s\n",friend_number,message);
}
void MyFriendStatusCallback(Tox *tox, uint32_t friend_number, const uint8_t *message, size_t length, void *user_data) {
	printf("status msg #%d %s\n",friend_number,message);
	Json::Reader reader;
	Json::Value root;
	if (reader.parse(std::string((const char *)message,length), root)) {
		Json::Value ip = root["ownip"];
		if (ip.isString()) {
			std::string peerip = ip.asString();
			if (!tunnels[friend_number]) {
				tunnels[friend_number] = new Tunnel(friend_number,myip,peerip);
			}
		}
	} else {
		printf("unable to parse status, ignoring\n");
	}
	saveState(tox);
}
void MyFriendLossyPacket(Tox *tox, uint32_t friend_number, const uint8_t *data, size_t length, void *user_data) {
	if (data[0] == 200) {
		if (tunnels[friend_number]) tunnels[friend_number]->processPacket(data+1,length-1);
	}
}
void handle_int(int something) {
	puts("int!");
	keep_running = false;
}
void connection_status(Tox *tox, TOX_CONNECTION connection_status, void *user_data) {
	switch (connection_status) {
	case TOX_CONNECTION_NONE:
		puts("connection lost");
		break;
	case TOX_CONNECTION_TCP:
		puts("tcp connection established");
		break;
	case TOX_CONNECTION_UDP:
		puts("udp connection established");
		break;
	}
	saveState(tox);
}
int main(int argc, char **argv) {
	uint8_t *bootstrap_pub_key = new uint8_t[TOX_PUBLIC_KEY_SIZE];
	hex_string_to_bin(BOOTSTRAP_KEY, bootstrap_pub_key);
#ifdef USE_EPOLL
	epoll_handle = epoll_create(20);
	assert(epoll_handle >= 0);
#endif
	Control control;

	struct sigaction interupt;
	memset(&interupt,0,sizeof(interupt));
	interupt.sa_handler = &handle_int;
	sigaction(SIGINT,&interupt,NULL);

	for (int i=0; i<100; i++) tunnels[i] = 0;

	assert(argc >= 2);
	myip = argv[1];
	Json::Value root;
	root["ownip"] = myip;
	Json::FastWriter fw;
	
	Tox *my_tox;
	bool want_bootstrap = false;
	int oldstate = open("savedata",O_RDONLY);
	if (oldstate >= 0) {
		struct stat info;
		fstat(oldstate,&info);
		uint8_t *temp = new uint8_t[info.st_size];
		int size = read(oldstate,temp,info.st_size);
		close(oldstate);
		assert(size == info.st_size);
		my_tox = tox_new(NULL,temp,size,NULL);
		delete temp;
		want_bootstrap = true;
	} else {
		/* Create a default Tox */
		my_tox = tox_new(NULL, NULL, 0, NULL);
		want_bootstrap = true;
	}

	uint8_t toxid[TOX_ADDRESS_SIZE];
	tox_self_get_address(my_tox,toxid);
	char tox_printable_id[TOX_ADDRESS_SIZE * 2 + 1];
	memset(tox_printable_id, 0, sizeof(tox_printable_id));
	to_hex(tox_printable_id, toxid,TOX_ADDRESS_SIZE);
	printf("my id is %s and IP is %s\n",tox_printable_id,myip.c_str());
	
	/* Register the callbacks */
	tox_callback_friend_request(my_tox, MyFriendRequestCallback, NULL);
	tox_callback_friend_message(my_tox, MyFriendMessageCallback, NULL);
	tox_callback_friend_status_message(my_tox, MyFriendStatusCallback, NULL);
	tox_callback_friend_connection_status(my_tox, FriendConnectionUpdate, NULL);
	tox_callback_friend_lossy_packet(my_tox, MyFriendLossyPacket, NULL);

	/* Define or load some user details for the sake of it */
	struct utsname hostinfo;
	uname(&hostinfo);
	tox_self_set_name(my_tox, (const uint8_t*)hostinfo.nodename, strlen(hostinfo.nodename), NULL); // Sets the username
	std::string json = fw.write(root);
	if (json[json.length()-1] == '\n') json.erase(json.length()-1, 1);
	tox_self_set_status_message(my_tox, (const uint8_t*)json.data(), json.length(), NULL); // Sets the status message

	/* Set the user status to TOX_USER_STATUS_NONE. Other possible values:
	 * TOX_USER_STATUS_AWAY and TOX_USER_STATUS_BUSY */
	tox_self_set_status(my_tox, TOX_USER_STATUS_NONE);

	tox_callback_self_connection_status(my_tox, &connection_status, 0);

	/* Bootstrap from the node defined above */
	if (want_bootstrap) tox_bootstrap(my_tox, BOOTSTRAP_ADDRESS, BOOTSTRAP_PORT, bootstrap_pub_key, NULL);


#ifdef USE_SELECT
	fd_set readset;
#endif
	while (keep_running) {
#ifdef USE_SELECT
		FD_ZERO(&readset);
		struct timeval timeout;
        int maxfd = 0;
#if 0
		maxfd = tox_populate_fdset(my_tox,&readset);
#endif
		for (int i=0; i<100; i++) {
			if (tunnels[i]) maxfd = std::max(maxfd,tunnels[i]->populate_fdset(&readset));
		}
		maxfd = std::max(maxfd,control.populate_fdset(&readset));
#endif
		int interval = tox_iteration_interval(my_tox);
#ifdef USE_SELECT
		timeout.tv_sec = 0;
		timeout.tv_usec = interval * 1000;
		int r = select(maxfd+1, &readset, NULL, NULL, &timeout);
		if (r > 0) {
			for (int i=0; i<100; i++) {
				if (tunnels[i]) {
					if (FD_ISSET(tunnels[i]->handle,&readset)) {
						tunnels[i]->handleReadData(my_tox);
					}
				}
			}
		}
		if (FD_ISSET(control.handle,&readset)) control.handleReadData(my_tox);
#endif

		tox_iterate(my_tox); // will call the callback functions defined and registered

#ifdef USE_EPOLL
		struct epoll_event events[10];
		int count = epoll_wait(epoll_handle, events, 10, interval);
		if (count == -1) std::cout << "epoll error " << strerror(errno) << std::endl;
		else {
			for (int i=0; i<count; i++) {
				EpollTarget *t = (EpollTarget *)events[i].data.ptr;
				t->handleReadData(my_tox);
			}
		}
#endif
	}
	puts("shutting down");
	saveState(my_tox);
	tox_kill(my_tox);
	return 0;
}
