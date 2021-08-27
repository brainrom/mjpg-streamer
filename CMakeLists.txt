cmake_minimum_required(VERSION 2.8.3)
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

project("mjpg-streamer" C)

#Input plugins
option(PLUGIN_INPUT_FILE "File input plugin" OFF)
option(PLUGIN_INPUT_HTTP "HTTP input plugin" OFF)
option(PLUGIN_INPUT_OPENCV "OpenCV input plugin" OFF)
option(PLUGIN_INPUT_PTP2 "PTP2 input plugin" OFF)
option(PLUGIN_INPUT_RASPICAM "RaspiCam input plugin" OFF)
option(PLUGIN_INPUT_UVC "UVC input plugin" OFF)
option(PLUGIN_INPUT_XGRAB "Xgrab input plugin" OFF)

#Output plugins
option(PLUGIN_OUTPUT_FILE "File ouput plugin" OFF)
option(PLUGIN_OUTPUT_HTTP "HTTP ouput plugin" OFF)
option(PLUGIN_OUTPUT_RTSP "RTSP ouput plugin" OFF)
option(PLUGIN_OUTPUT_UDP "UDP ouput plugin" OFF)
option(PLUGIN_OUTPUT_VIEWER "Viewer ouput plugin" OFF)
option(PLUGIN_OUTPUT_ZMQSERVER "ZMQ server ouput plugin" OFF)

# If the user doesn't manually specify a build type, use 'Release'
message("CMAKE_BUILD_TYPE = ${CMAKE_BUILD_TYPE}")
if("${CMAKE_BUILD_TYPE}" STREQUAL "")
  SET(CMAKE_BUILD_TYPE "Release")
endif()

SET(COMPILE_DEFINITIONS -Werror -Wall)

include(CheckLibraryExists) 
include(CheckIncludeFiles)
include(FeatureSummary)

include(mjpg_streamer_utils)

#
# Get the current git hash
#
execute_process(
  COMMAND git rev-parse HEAD
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  RESULT_VARIABLE GIT_RESULT
  OUTPUT_VARIABLE GIT_HASH
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

if(GIT_RESULT EQUAL 0)
  add_definitions("-DGIT_HASH=\"${GIT_HASH}\"")
endif()

#
# Options
#
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DDEBUG")

add_feature_option(WXP_COMPAT "Enable compatibility with WebcamXP" OFF)

if (WXP_COMPAT)
    add_definitions(-DWXP_COMPAT)
endif (WXP_COMPAT)

set (MJPG_STREAMER_PLUGIN_INSTALL_PATH "lib/mjpg-streamer")

#
# Global dependencies
#

find_library(JPEG_LIB jpeg)


#
# Input plugins
#

if (PLUGIN_INPUT_FILE)
    add_subdirectory(plugins/input_file)
endif()
if (PLUGIN_INPUT_HTTP)
    add_subdirectory(plugins/input_http)
endif()
if (PLUGIN_INPUT_OPENCV)
    add_subdirectory(plugins/input_opencv)
endif()
if (PLUGIN_INPUT_RASPICAM)
    add_subdirectory(plugins/input_raspicam)
endif()
if (PLUGIN_INPUT_PTP2)
    add_subdirectory(plugins/input_ptp2)
endif()
if (PLUGIN_INPUT_UVC)
    add_subdirectory(plugins/input_uvc)
endif()
if (PLUGIN_INPUT_XGRAB)
    add_subdirectory(plugins/input_xgrab)
endif()

#
# Output plugins
#

if (PLUGIN_OUTPUT_FILE)
    add_subdirectory(plugins/output_file)
endif()
if (PLUGIN_OUTPUT_HTTP)
    add_subdirectory(plugins/output_http)
endif()
if (PLUGIN_OUTPUT_RTSP)
    add_subdirectory(plugins/output_rtsp)
endif()
if (PLUGIN_OUTPUT_UDP)
    add_subdirectory(plugins/output_udp)
endif()
if (PLUGIN_OUTPUT_VIEWER)
    add_subdirectory(plugins/output_viewer)
endif()
if (PLUGIN_OUTPUT_ZMQSERVER)
    add_subdirectory(plugins/output_zmqserver)
endif()
#
# mjpg_streamer executable
#

# This adds the plugin installation directory to the default DT_RUNPATH, so
# that the user shouldn't need to set LD_LIBRARY_PATH if using 'make install'
# ... however, DT_RUNPATH allows overriding via LD_LIBRARY_PATH if you really
#     need to do it

set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--enable-new-dtags")
set (CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
set (CMAKE_INSTALL_RPATH ${CMAKE_INSTALL_PREFIX}/${MJPG_STREAMER_PLUGIN_INSTALL_PATH})
set (CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)


add_executable(mjpg_streamer mjpg_streamer.c
                             utils.c)

target_link_libraries(mjpg_streamer pthread dl)
install(TARGETS mjpg_streamer DESTINATION bin)

#
# www directory
#

install(DIRECTORY www DESTINATION share/mjpg-streamer)


#
# Show enabled/disabled features
#

feature_summary(WHAT ALL)

#
# Final warning
#

if("${CMAKE_CURRENT_SOURCE_DIR}" STREQUAL "${CMAKE_CURRENT_BINARY_DIR}")
  message(WARNING "The source directory is the same as binary directory. \"make clean\" may damage the source tree")
endif()
