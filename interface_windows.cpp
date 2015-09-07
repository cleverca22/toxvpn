#include "interface.h"

using namespace std;
using namespace ToxVPN;

NetworkInterface::NetworkInterface(string myip, Tox *my_tox) {
	fd = 0;
}
