#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string>
#include <iostream>
#include <sstream>

#include "control.h"

using namespace std;
Control::Control() {
	this->handle = STDIN_FILENO;
	memset(&this->event,0,sizeof(this->event));
	this->event.events = EPOLLIN | EPOLLPRI | EPOLLERR;
	this->event.data.ptr = this;
	if (epoll_ctl(epoll_handle, EPOLL_CTL_ADD, this->handle, &this->event) != 0) puts(strerror(errno));
}
void Control::handleData(epoll_event &eventin, Tox *tox) {
	char *line = 0;
	size_t linelen = 0;
	int size = getline(&line, &linelen, stdin);
	std::string cmd(line,size);
	std::cout << "msg is " << cmd;
	std::string buf;
	std::stringstream ss(cmd);
	ss >> buf;
	TOX_ERR_FRIEND_QUERY fqerror;
	if (buf == "list") {
		cout << "listing friends" << endl;
		int friendCount = tox_self_get_friend_list_size(tox);
		uint32_t *friends = new uint32_t[friendCount];
		tox_self_get_friend_list(tox,friends);
		for (int i=0; i<friendCount; i++) {
			int friendid = friends[i];
			TOX_CONNECTION conn_status = tox_friend_get_connection_status(tox,friendid,NULL);
			uint64_t lastonline = tox_friend_get_last_online(tox,friendid,NULL);
			size_t namesize = tox_friend_get_name_size(tox,friendid,&fqerror);
			uint8_t *friendname = new uint8_t[namesize+1];
			tox_friend_get_name(tox,friendid,friendname,NULL);
			friendname[namesize] = 0;
			size_t statusSize = tox_friend_get_status_message_size(tox,friendid,NULL);
			uint8_t *status = new uint8_t[statusSize+1];
			tox_friend_get_status_message(tox,friendid,status,NULL);
			status[statusSize] = 0;
			uint32_t hack = lastonline;
			printf("friend#%2d name:%15s status:%30s lastonline:%d\n",friendid,friendname,status,hack);
			delete friendname;
			delete status;
		}
		delete friends;
	}
	//while (ss >> buf) {
		//std::cout << "word is " << buf << std::endl;
	//}
}
