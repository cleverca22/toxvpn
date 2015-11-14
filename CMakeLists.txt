cmake_minimum_required (VERSION 2.6)
project (vpn_test)

if(WIN32)
else(WIN32)
set(SYSROOT "/usr")
endif(WIN32)

if(STATIC)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DSTATIC")
endif(STATIC)

find_library(JSONCPP_LIBRARIES json REQUIRED)
find_path(JSONCPP_INCLUDE_DIRS json/value.h REQUIRED)

find_library(TOXCORE_LIBRARIES toxcore REQUIRED HINTS "${TOX_PREFIX}/lib")
find_path(TOXCORE_INCLUDE_DIRS tox/tox.h REQUIRED HINTS "${TOX_PREFIX}/include")

find_library(SODIUM_LIBRARIES sodium REQUIRED "${SODIUM_PREFIX}/lib")
find_path(SODIUM_INCLUDE_DIRS sodium.h REQUIRED "${SODIUM_PREFIX}/include")

if(WIN32)
  set(extra_files interface_windows.cpp route_windows.cpp)
elseif(APPLE)
  set(extra_files interface_mac.cpp route_mac.cpp)
else()
  set(extra_files interface_linux.cpp route_linux.cpp)
endif()

add_executable(toxvpn main.cpp control.cpp interface.cpp listener.cpp ${extra_files})
target_link_libraries(toxvpn pthread ${JSONCPP_LIBRARIES} ${TOXCORE_LIBRARIES})
include_directories(${JSONCPP_INCLUDE_DIRS} ${TOXCORE_INCLUDE_DIRS})

if(WIN32)
target_link_libraries(toxvpn toxcore ws2_32 sodium pthread iphlpapi)
elseif(APPLE)
else()
  if(STATIC)
  target_link_libraries(toxvpn cap)
  else(STATIC)
  target_link_libraries(toxvpn systemd cap)
  endif(STATIC)
endif()

install(TARGETS toxvpn DESTINATION bin)
