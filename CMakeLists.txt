cmake_minimum_required (VERSION 2.6)
project (vpn_test)

if(WIN32)
else(WIN32)
set(SYSROOT "/usr")
endif(WIN32)

if(STATIC)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DSTATIC")
endif(STATIC)

include_directories("${SYSROOT}/include/jsoncpp/" "${SYSROOT}/include/")

if(TOX_PREFIX)
include_directories("${TOX_PREFIX}/include")
LINK_DIRECTORIES("${TOX_PREFIX}/lib")
endif(TOX_PREFIX)

if(SODIUM_PREFIX)
link_directories("${SODIUM_PREFIX}/lib")
endif(SODIUM_PREFIX)

if(WIN32)
set(extra_files interface_windows.cpp route_windows.cpp)
else(WIN32)
set(extra_files interface_linux.cpp route_linux.cpp)
endif(WIN32)

add_executable(toxvpn main.cpp control.cpp interface.cpp listener.cpp ${extra_files})
target_link_libraries(toxvpn jsoncpp toxcore sodium pthread)

if(WIN32)
target_link_libraries(toxvpn toxcore ws2_32 sodium pthread iphlpapi)
ELSE(WIN32)
if(STATIC)
target_link_libraries(toxvpn cap)
else(STATIC)
target_link_libraries(toxvpn systemd cap)
endif(STATIC)
endif(WIN32)

install(TARGETS toxvpn DESTINATION bin)
