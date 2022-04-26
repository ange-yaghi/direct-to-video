cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_FLAGS "-D__STDC_CONSTANT_MACROS")

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(VC_LIB_PATH_SUFFIX win64)
else()
  set(VC_LIB_PATH_SUFFIX win32)
endif()

find_path(AVCODEC_INCLUDE_DIR libavcodec/avcodec.h
    PATH_SUFFIXES
        "${VC_LIB_PATH_SUFFIX}/include")
find_library(AVCODEC_LIBRARY avcodec
    PATH_SUFFIXES
        "${VC_LIB_PATH_SUFFIX}/bin"
        "${VC_LIB_PATH_SUFFIX}/lib")

find_path(AVFORMAT_INCLUDE_DIR libavformat/avformat.h
    PATH_SUFFIXES
        "${VC_LIB_PATH_SUFFIX}/include")
find_library(AVFORMAT_LIBRARY avformat
    PATH_SUFFIXES
        "${VC_LIB_PATH_SUFFIX}/bin"
        "${VC_LIB_PATH_SUFFIX}/lib")

find_path(AVUTIL_INCLUDE_DIR libavutil/avutil.h
    PATH_SUFFIXES
        "${VC_LIB_PATH_SUFFIX}/include")
find_library(AVUTIL_LIBRARY avutil
    PATH_SUFFIXES
        "${VC_LIB_PATH_SUFFIX}/bin"
        "${VC_LIB_PATH_SUFFIX}/lib")

find_path(AVDEVICE_INCLUDE_DIR libavdevice/avdevice.h
    PATH_SUFFIXES
        "${VC_LIB_PATH_SUFFIX}/include")
find_library(AVDEVICE_LIBRARY avdevice
    PATH_SUFFIXES
        "${VC_LIB_PATH_SUFFIX}/bin"
        "${VC_LIB_PATH_SUFFIX}/lib")

find_library(SWSCALE_LIBRARY swscale
    PATH_SUFFIXES
        "${VC_LIB_PATH_SUFFIX}/bin"
        "${VC_LIB_PATH_SUFFIX}/lib")

find_library(SWRESAMPLE_LIBRARY swresample
    PATH_SUFFIXES
        "${VC_LIB_PATH_SUFFIX}/bin"
        "${VC_LIB_PATH_SUFFIX}/lib")
