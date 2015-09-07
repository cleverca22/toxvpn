#pragma once

#include <tox/tox.h>
#include <stdio.h>
#include "interface.h"

namespace ToxVPN {

class Control {
public:
	Control(NetworkInterface *interfarce);
	Control(NetworkInterface *interfarce, int socket);
	int handleReadData(Tox *tox);
	int populate_fdset(fd_set *readset);
	
	int handle;
private:
	NetworkInterface *interfarce;
	FILE *input, *output;
};

};
