cmake_minimum_required (VERSION 2.6)
project (vpn_test)

include_directories("${SYSROOT}/include/jsoncpp/" "${SYSROOT}/include/")

if(WIN32)
set(extra_files interface_windows.cpp route_windows.cpp)
else(WIN32)
set(extra_files interface_linux.cpp route_linux.cpp)
endif(WIN32)

add_executable(toxvpn main.cpp control.cpp interface.cpp listener.cpp ${extra_files})
target_link_libraries(toxvpn jsoncpp toxcore sodium pthread)

if(WIN32)
target_link_libraries(toxvpn toxcore ws2_32 sodium pthread iphlpapi)
endif(WIN32)

install(TARGETS toxvpn DESTINATION bin)
