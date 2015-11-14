#include "main.h"

using namespace std;
using namespace ToxVPN;

Control::Control(NetworkInterface *iface): interfarce(iface) {
	this->handle = STDIN_FILENO;
	input = stdin;
	output = stdout;
#ifdef USE_EPOLL
	memset(&this->event,0,sizeof(this->event));
	this->event.events = EPOLLIN | EPOLLPRI | EPOLLERR;
	this->event.data.ptr = this;
	if (epoll_ctl(epoll_handle, EPOLL_CTL_ADD, this->handle, &this->event) != 0) puts(strerror(errno));
#endif
}
Control::Control(NetworkInterface *iface, int socket): interfarce(iface) {
	this->handle = socket;
	input = fdopen(handle,"r");
	output = fdopen(handle,"w");
}
ssize_t Control::handleReadData(Tox *tox) {
	ssize_t size;
#ifdef WIN32
	std::string cmd;
	getline(cin,cmd);
	size = cmd.length();
#else
	char *line = 0;
	size_t linelen = 0;
	size = getline(&line, &linelen, input);
	if (size == -1) return -1;
	std::string cmd(line,size);
#endif
	std::string buf;
	std::stringstream ss(cmd);
	ss >> buf;
	TOX_ERR_FRIEND_QUERY fqerror;
	if (buf == "list") {
		fputs("listing friends\n",output);
		size_t friendCount = tox_self_get_friend_list_size(tox);
		uint32_t *friends = new uint32_t[friendCount];
		tox_self_get_friend_list(tox,friends);
		for (unsigned int i=0; i<friendCount; i++) {
			int friendid = friends[i];
			TOX_CONNECTION conn_status = tox_friend_get_connection_status(tox,friendid,NULL);
			string statusString;
			switch (conn_status) {
			case TOX_CONNECTION_NONE:
				statusString = "offline";
				break;
			case TOX_CONNECTION_TCP:
				statusString = "tcp";
				break;
			case TOX_CONNECTION_UDP:
				statusString = "udp";
				break;
			}
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
			fprintf(output,"friend#%2d name:%15s status:%10s %30s lastonline:%d\n",friendid,friendname,statusString.c_str(),status,hack);
			delete friendname;
			delete status;
		}
		delete friends;
	} else if (buf == "remove") {
		int friendid;
		ss >> friendid;
		fprintf(output,"going to kick %d\n",friendid);
		tox_friend_delete(tox,friendid,NULL);
		interfarce->removePeer(friendid);
	} else if (buf == "add") {
		ss >> buf;
		fprintf(output,"going to connect to %s\n",buf.c_str());
		const char *msg = "toxvpn";
		uint8_t peerbinary[TOX_ADDRESS_SIZE];
		TOX_ERR_FRIEND_ADD error;
		hex_string_to_bin(buf.c_str(),peerbinary);
		tox_friend_add(tox, (uint8_t*)peerbinary, (uint8_t*)msg,strlen(msg),&error);
		switch (error) {
		case TOX_ERR_FRIEND_ADD_OK:
			break;
		case TOX_ERR_FRIEND_ADD_ALREADY_SENT:
			fputs("already sent\n",output);
			break;
		case TOX_ERR_FRIEND_ADD_BAD_CHECKSUM:
			puts("crc error");
			break;
		default:
			fprintf(output,"err code %d\n",error);
		}
	} else if (buf == "whitelist") {
		ss >> buf;
		uint8_t peerbinary[TOX_PUBLIC_KEY_SIZE];
		TOX_ERR_FRIEND_ADD error;
		hex_string_to_bin(buf.c_str(),peerbinary);
		tox_friend_add_norequest(tox,peerbinary,&error);
		switch (error) {
		case TOX_ERR_FRIEND_ADD_OK:
			break;
		case TOX_ERR_FRIEND_ADD_ALREADY_SENT:
			fputs("already sent\n",output);
			break;
		case TOX_ERR_FRIEND_ADD_BAD_CHECKSUM:
			fputs("crc error\n",output);
			break;
		default:
			fprintf(output,"err code %d\n",error);
		}
		saveState(tox);
	} else if (buf == "status") {
		uint8_t toxid[TOX_ADDRESS_SIZE];
		tox_self_get_address(tox,toxid);
		char tox_printable_id[TOX_ADDRESS_SIZE * 2 + 1];
		memset(tox_printable_id, 0, sizeof(tox_printable_id));
		to_hex(tox_printable_id, toxid,TOX_ADDRESS_SIZE);
		fprintf(output,"my id is %s and IP is %s\n",tox_printable_id,myip.c_str());
	} else if (buf == "help") {
		fputs("list              - lists tox friends\n",output);
		fputs("remove <number>   - removes a friend, get the number from list\n",output);
		fputs("add <toxid>       - adds a friend\n",output);
		fputs("whitelist <toxid> - add/accept a friend\n",output);
		fputs("status            - shows your own id&ip\n",output);
		fputs("bootstrap         - attempt to reconnect\n",output);
	} else if (buf == "bootstrap") {
		do_bootstrap(tox);
	} else if (buf == "route") {
		ss >> buf;
		if (buf == "show") {
			std::list<Route>::const_iterator i;
			for (i=interfarce->routes.begin(); i!=interfarce->routes.end(); ++i) {
				Route r = *i;
				fprintf(output,"%s/%d via friend#%d\n",inet_ntoa(r.network),r.maskbits,r.friend_number);
			}
		}
	}
	fflush(output);
	return size;
}
int Control::populate_fdset(fd_set *readset) {
	FD_SET(this->handle,readset);
	return this->handle;
}
