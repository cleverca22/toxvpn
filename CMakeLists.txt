cmake_minimum_required (VERSION 2.6)
project (vpn_test)

include (CheckIncludeFile.cmake)

include_directories("/usr/include/jsoncpp/")
CHECK_INCLUDE_FILE(json/json.h HAVE_JSONCPP)

add_executable(vpn_test main.cpp tunnel.cpp control.cpp)
target_link_libraries(vpn_test jsoncpp toxcore)
