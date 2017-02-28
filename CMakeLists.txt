cmake_minimum_required(VERSION 2.6)
project(toxvpn)

set(BOOTSTRAP_PATH "${CMAKE_INSTALL_PREFIX}/share/toxvpn/bootstrap.json")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DBOOTSTRAP_FILE=\\\"${BOOTSTRAP_PATH}\\\"")

if(WIN32)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DWIN32")
else(WIN32)
endif(WIN32)

if(STATIC)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DSTATIC")
set(LIBMODE STATIC)
endif(STATIC)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wconversion -Wshadow -Wextra -std=c++14")

if(SYSTEMD)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DSYSTEMD")
set(SYSTEMD_LIBRARIES "systemd")
endif(SYSTEMD)

find_package(nlohmann_json)

find_library(TOXCORE_LIBRARIES toxcore REQUIRED HINTS "${TOX_PREFIX}/lib")
find_path(TOXCORE_INCLUDE_DIRS tox/tox.h REQUIRED HINTS "${TOX_PREFIX}/include")

find_library(SODIUM_LIBRARIES sodium REQUIRED "${SODIUM_PREFIX}/lib")
find_path(SODIUM_INCLUDE_DIRS sodium.h REQUIRED "${SODIUM_PREFIX}/include")

if(WIN32)
  set(extra_files interface_windows.cpp route_windows.cpp)
elseif(CYGWIN)
  set(extra_files interface_windows.cpp route_windows.cpp)
elseif(APPLE)
  set(extra_files interface_mac.cpp route_mac.cpp)
else()
  set(extra_files interface_linux.cpp route_linux.cpp)
endif()

add_executable(toxvpn main.cpp control.cpp interface.cpp listener.cpp ${extra_files})
target_link_libraries(toxvpn pthread ${JSONCPP_LIBRARIES} ${TOXCORE_LIBRARIES})
include_directories(${JSONCPP_INCLUDE_DIRS} ${TOXCORE_INCLUDE_DIRS})

add_executable(toxvpn-remote toxvpn-remote.cpp)
target_link_libraries(toxvpn-remote zmq)

if(WIN32)
  target_link_libraries(toxvpn ${TOXCORE_LIBRARIES} ws2_32 ${SODIUM_LIBRARIES} pthread iphlpapi ${JSONCPP_LIBRARIES})
elseif(CYGWIN)
  target_link_libraries(toxvpn ${TOXCORE_LIBRARIES} ws2_32 ${SODIUM_LIBRARIES} pthread iphlpapi)
elseif(APPLE)
else()
  if(STATIC)
  target_link_libraries(toxvpn cap zmq pthread ${SODIUM_LIBRARIES})
  else(STATIC)
  target_link_libraries(toxvpn cap zmq ${SYSTEMD_LIBRARIES})
  endif(STATIC)
endif()

install(TARGETS toxvpn toxvpn-remote DESTINATION bin)
install(FILES bootstrap.json DESTINATION share/toxvpn)
