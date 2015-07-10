#include <asm/types.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <sys/socket.h>
#include <string.h>
#include <arpa/inet.h>

int netlink_socket;
void route_init() {
	netlink_socket = socket(AF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE);
}
static struct {
	struct nlmsghdr nl;
	struct rtmsg rt;
	char buf[8192];
} req;
void send_request();

void systemRouteSingle(int ifindex, struct in_addr peer, const char *gateway) {
	// http://www.linuxjournal.com/article/8498?page=0,2
	struct rtattr *rtap;

	//char *dest = "192.168.123.2";
	int pn = 32;

	// initialize RTNETLINK request buffer
	bzero(&req,sizeof(req));

	// compute the initial length of the service request
	int rtl = sizeof(struct rtmsg);
	
	// add first attrib
	// set destination ip addr and increment the netlink buf size
	rtap = (struct rtattr*) req.buf;
	rtap->rta_type = RTA_DST;
	rtap->rta_len = sizeof(struct rtattr) + 4;
	memcpy( ((char *)rtap) + sizeof(struct rtattr), &peer, 4);
	//inet_pton(AF_INET,dest,((char *)rtap) + sizeof(struct rtattr));
	rtl += rtap->rta_len;

	// add second attrib
	// set gateway
	rtap = (struct rtattr*) ( ((char*)rtap) + rtap->rta_len);
	rtap->rta_type = RTA_GATEWAY;
	rtap->rta_len = sizeof(struct rtattr) + 4;
	inet_pton(AF_INET,gateway,((char *)rtap) + sizeof(struct rtattr));
	rtl += rtap->rta_len;

	// add third attrib
	// set ifc index andincrement the netlink size
	rtap = (struct rtattr*) ( ((char*)rtap) + rtap->rta_len);
	rtap->rta_type = RTA_OIF;
	rtap->rta_len = sizeof(struct rtattr) + 4;
	memcpy( ((char *)rtap) + sizeof(struct rtattr), &ifindex,4);
	rtl += rtap->rta_len;

	// setup netlink header
	req.nl.nlmsg_len = NLMSG_LENGTH(rtl);
	req.nl.nlmsg_flags = NLM_F_REQUEST | NLM_F_CREATE;
	req.nl.nlmsg_type = RTM_NEWROUTE;

	// setup service header
	req.rt.rtm_family = AF_INET;
	req.rt.rtm_table = RT_TABLE_MAIN;
	req.rt.rtm_protocol = RTPROT_STATIC;
	req.rt.rtm_scope = RT_SCOPE_UNIVERSE;
	req.rt.rtm_type = RTN_UNICAST;
	req.rt.rtm_dst_len = pn;

	send_request();
}
void send_request() {
	struct sockaddr_nl pa;
	bzero(&pa,sizeof(pa));
	pa.nl_family = AF_NETLINK;

	// initialize and create the msghdr
	struct msghdr msg;
	bzero(&msg,sizeof(msg));
	msg.msg_name = &pa;
	msg.msg_namelen = sizeof(pa);

	// place the pointer and size in it
	struct iovec iov;
	iov.iov_base = (void*)&req.nl;
	iov.iov_len = req.nl.nlmsg_len;
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

	sendmsg(netlink_socket, &msg, 0);
}
