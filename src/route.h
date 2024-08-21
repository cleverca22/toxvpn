void route_init();
void systemRouteSingle(int ifindex, struct in_addr, const char* gateway);
void systemRouteDirect(int ifindex, struct in_addr);
