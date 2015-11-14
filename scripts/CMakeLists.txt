cmake_minimum_required (VERSION 2.6)
project (toxcore)

find_library(SODIUM_LIBRARIES sodium REQUIRED "${SODIUM_PREFIX}/lib")
find_path(SODIUM_INCLUDE_DIRS sodium.h REQUIRED "${SODIUM_PREFIX}/include")

include_directories(${SODIUM_INCLUDE_DIRS})
add_library(toxcore SHARED toxcore/tox.c toxcore/Messenger.c toxcore/group.c
	toxcore/crypto_core.c toxcore/friend_requests.c toxcore/logger.c
	toxcore/DHT.c toxcore/network.c toxcore/net_crypto.c toxcore/TCP_server.c
	toxcore/onion.c toxcore/onion_client.c toxcore/util.c toxcore/friend_connection.c
	toxcore/onion_announce.c toxcore/LAN_discovery.c toxcore/ping.c toxcore/ping_array.c
	toxcore/list.c toxcore/TCP_connection.c toxcore/TCP_client.c)
target_link_libraries(toxcore ${SODIUM_LIBRARIES} ws2_32 iphlpapi)
