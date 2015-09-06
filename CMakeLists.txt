cmake_minimum_required (VERSION 2.6)
project (vpn_test)

if(WIN32)
set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -L/home/poker/tox/lib/ -static-libstdc++")
else(WIN32)
set(SYSROOT "/usr")
endif(WIN32)

include_directories("${SYSROOT}/include/jsoncpp/" "${SYSROOT}/include/")

add_executable(toxvpn main.cpp control.cpp interface.cpp route_linux.cpp listener.cpp)
target_link_libraries(toxvpn jsoncpp toxcore sodium pthread)

if(WIN32)
target_link_libraries(toxvpn toxcore ws2_32 sodium pthread iphlpapi)
endif(WIN32)

install(TARGETS toxvpn DESTINATION bin)
