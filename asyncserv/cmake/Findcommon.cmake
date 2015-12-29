FIND_PATH( COMMON_INCLUDE_DIR libcommon/log.h
	DOC "The directory where common.h resides"  )
 
FIND_LIBRARY( COMMON_LIBRARY
	NAMES common 
	PATHS /usr/lib/
	DOC "The COMMON library")

IF (COMMON_INCLUDE_DIR )
	SET( COMMON_FOUND 1 CACHE STRING "Set to 1 if common is found, 0 otherwise")
ELSE (COMMON_INCLUDE_DIR)
	SET( COMMON_FOUND 0 CACHE STRING "Set to 1 if common is found, 0 otherwise")
ENDIF (COMMON_INCLUDE_DIR)

MARK_AS_ADVANCED( COMMON_FOUND )

IF(COMMON_FOUND)
	MESSAGE(STATUS "找到了 common 库")
ELSE(COMMON_FOUND)
	MESSAGE(FATAL_ERROR "没有找到 common库 :请安装它, 然后删除build目录:rm -rf ./build/* ,重新 安装 ")
ENDIF(COMMON_FOUND)


