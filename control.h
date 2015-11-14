namespace ToxVPN {

class Control {
public:
	Control(NetworkInterface *interfarce);
	Control(NetworkInterface *interfarce, int socket);
	ssize_t handleReadData(Tox *tox);
	int populate_fdset(fd_set *readset);

	int handle;
private:
	NetworkInterface *interfarce;
	FILE *input, *output;
};

};
