cmake_minimum_required(VERSION 2.6)
set(CMAKE_CXX_COMPILER "g++")

set (CMAKE_INCLUDE_DIRECTORIES_PROJECT_BEFORE  true )
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")
set(CMAKE_INSTALL_PREFIX "/usr/local")
SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/../ )
set(CMAKE_USE_RELATIVE_PATHS  true )
find_package(glib REQUIRED)
find_package(common REQUIRED)
set (SRC_LIST 
	binding.cpp
	daemon.cpp
	dll.cpp
	main.cpp
	mcast.cpp
	socket.cpp
	net_if.cpp
	worker.cpp
	shmq.cpp
	)
#add_definitions("-Wall -MMD -g  -DDEBUG -O2 -funroll-loops -D__USE_STRING_INLINES -D_REENTRANT -D_GNU_SOURCE")
add_definitions("-Wall -MMD -g  -DDEBUG -DENABLE_TRACE_LOG -funroll-loops -D__USE_STRING_INLINES -D_REENTRANT -D_GNU_SOURCE")
INCLUDE_DIRECTORIES( ${GLIB_INCLUDE_DIR}	${COMMON_INCLUDE_DIR} 	)
set (EXTRA_LIBS ${EXTRA_LIBS} glib-2.0 m common dl ssl)
add_executable(AsynServ_Dev ${SRC_LIST}   )
target_link_libraries (AsynServ_Dev ${EXTRA_LIBS})
INSTALL (FILES net_if.hpp DESTINATION  include/asyn_serv)

