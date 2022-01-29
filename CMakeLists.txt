cmake_minimum_required(VERSION 3.10)

# Enable group projects in folders
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "cmake")

project(direct-to-video)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/")

find_package(FFmpeg REQUIRED)

add_library(direct-to-video
    # Source files
    src/frame.cpp
    src/frame_queue.cpp
    src/encoder.cpp

    # Include files
    include/frame.h
    include/ffmpeg.h
    include/frame_queue.h
    include/encoder.h
    include/dtv.h
)

add_executable(direct-to-video-demo
    # Source files
    demo/src/main.cpp

    # Include files
    demo/include/dtv.h
)

target_include_directories(direct-to-video PUBLIC
    ${AVCODEC_INCLUDE_DIR}
    ${AVFORMAT_INCLUDE_DIR}
    ${AVUTIL_INCLUDE_DIR}
    ${AVDEVICE_INCLUDE_DIR}
)

target_link_libraries(direct-to-video PUBLIC
    ${AVCODEC_LIBRARY}
    ${AVFORMAT_LIBRARY}
    ${AVUTIL_LIBRARY}
    ${AVDEVICE_LIBRARY}
    ${SWSCALE_LIBRARY}
    ${SWRESAMPLE_LIBRARY}
)

target_link_libraries(direct-to-video-demo
    direct-to-video)
