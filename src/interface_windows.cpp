#include "main.h"

using namespace std;
using namespace ToxVPN;

NetworkInterface::NetworkInterface() { fd = 0; }
void NetworkInterface::configure(string ip_in, Tox* tox_in) { my_tox = tox_in; }
