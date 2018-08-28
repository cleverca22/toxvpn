#include "main.h"

void route_init() {}
void systemRouteSingle(int ifindex, struct in_addr dest, const char* gateway) {
    char buffer[512];
    char network[16];
    const char* netmask = "255.255.255.255";
    strncpy(network, inet_ntoa(dest), 16);
    printf("adding route for %s\n", network);
    snprintf(buffer, 500, "route add -net %s 10.123.123.123 %s", network,
             netmask);
    system(buffer);
}
