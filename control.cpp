#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string>
#include <iostream>

#include "control.h"

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
}
