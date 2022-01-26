cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_FLAGS "-D__STDC_CONSTANT_MACROS")

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(VC_LIB_PATH_SUFFIX win64)
else()
  set(VC_LIB_PATH_SUFFIX win32)
endif()

#find_path(AVCODEC_INCLUDE_DIR test.txt
#    HINTS
#    PATH_SUFFIXES
#        "${VC_LIB_PATH_SUFFIX}"
#        include
#        libavcodec)
#    message("hello: '$ENV{path}'")
find_library(AVCODEC_LIBRARY avcodec)

if(NOT SDL2_DIR)
  set(SDL2_DIR "" CACHE PATH "SDL2 directory")
endif()
find_path(AVCODEC_INCLUDE_DIR SDL.h
  HINTS
    ENV SDL2DIR
    ${SDL2_DIR}
  PATH_SUFFIXES SDL2
                include/SDL2 include)
message("e: ${AVCODEC_INCLUDE_DIR}")

find_path(AVFORMAT_INCLUDE_DIR libavformat/avformat.h)
find_library(AVFORMAT_LIBRARY avformat)

find_path(AVUTIL_INCLUDE_DIR libavutil/avutil.h)
find_library(AVUTIL_LIBRARY avutil)

find_path(AVDEVICE_INCLUDE_DIR libavdevice/avdevice.h)
find_library(AVDEVICE_LIBRARY avdevice)
