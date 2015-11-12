#include "interface.h"

using namespace std;
using namespace ToxVPN;

NetworkInterface::NetworkInterface() {
	fd = 0;
}
void NetworkInterface::configure(string myip, Tox *my_tox) {
  this->my_tox = my_tox;
}
