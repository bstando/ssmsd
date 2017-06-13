cmake_minimum_required(VERSION 2.8.12)
project(SSMSD)

set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_LIBS   ON)
#ADD_DEFINITIONS(-DBOOST_LOG_DYN_LINK)
FIND_PACKAGE( Boost 1.58 COMPONENTS REQUIRED log system log_setup program_options thread filesystem date_time )
INCLUDE_DIRECTORIES( ${Boost_INCLUDE_DIR} )
INCLUDE_DIRECTORIES( sqlite_modern_cpp/hdr )
INCLUDE_DIRECTORIES( ArduinoJson/src/ )

set(CMAKE_CXX_FLAGS " -std=c++11")


add_executable(ssmsd
    server.cpp
        DataExchangeServer.cpp DataExchangeServer.hpp
        db_helper.cpp db_helper.hpp
        sensor.cpp sensor.hpp Zeroconf.cpp Zeroconf.hpp)

target_link_libraries(ssmsd sqlite3 avahi-client avahi-common pthread ${Boost_LIBRARIES})
