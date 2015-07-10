#pragma once

#include <tox/tox.h>
#include <stdio.h>

class Interface;

class Control {
public:
	Control(Interface *interface);
	Control(Interface *iterface, int socket);
	int handleReadData(Tox *tox);
	int populate_fdset(fd_set *readset);
	
	int handle;
private:
	Interface *interface;
	FILE *input, *output;
};
