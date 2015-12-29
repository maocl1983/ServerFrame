cmake_minimum_required(VERSION 2.6)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")
set(CMAKE_INSTALL_PREFIX "/usr/")
find_package(glib REQUIRED)
set(CMAKE_USE_RELATIVE_PATHS  true )	

set (SRC_LIST 
	./statistic/msglog.c
	./conf_parser/config.c
	./inet/mcast.c
	./inet/tcp.c
	./inet/inet_utils.c
	./log.c
	./time/time.c
	./time/timer.c
	./tm_dirty/tm_dirty.c
    ./tm_dirty/utf8_punc.c
    ./tm_dirty/acsm.c
	)
set (HEADER_LIST 
	./random/random.h
	./project/utilities.h
	./project/constants.h
	./project/types.h
	./statistic/msglog.h
	./utils.h
	./conf_parser/config.h
	./inet/tcp.h
	./inet/mcast.h
	./inet/ipaddr_cat.h
	./inet/inet_utils.h
	./list.h
    ./log_old.h
	./log.h
	./time/time.h
	./time/timer.h
	./atomic.h
	./tm_dirty/tm_dirty.h
    ./tm_dirty/utf8_punc.h
    ./tm_dirty/acsm.h
   	)

INCLUDE_DIRECTORIES( "./" "../" ${GLIB_INCLUDE_DIR} )
ADD_LIBRARY(common ${SRC_LIST}	)
#target_link_libraries (common "lua" )
ADD_DEFINITIONS( "-g -DDEBUG -DUSE_TLOG -Wall  -MMD  -funroll-loops -D__USE_STRING_INLINES -fhosted  -fPIC  -D_GNU_SOURCE" )
INSTALL (TARGETS common DESTINATION lib)
FOREACH (HEADER_FILE ${HEADER_LIST})
 	STRING( REGEX REPLACE "\\.(.*)/.*\\.h$" "\\1" DIR_NAME ${HEADER_FILE} ) 
	INSTALL (FILES ${HEADER_FILE} DESTINATION  include/libcommon/${DIR_NAME}	)	
ENDFOREACH(HEADER_FILE)
