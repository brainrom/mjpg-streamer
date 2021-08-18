find_package(X11 REQUIRED)
find_library(JPEG_LIB jpeg)

option(PLUGIN_INPUT_XGRAB_XSHM "XSHM support in Xgrab" ON)

if (PLUGIN_INPUT_XGRAB_XSHM)
    add_definitions(-DXSHM)
endif()

MJPG_STREAMER_PLUGIN_OPTION(input_xgrab "X.org grabbing plugin")
MJPG_STREAMER_PLUGIN_COMPILE(input_xgrab  jpeg_utils.c input_xgrab.c)
target_link_libraries (input_xgrab ${X11_LIBRARIES} ${X11_Xfixes_LIB}
 ${JPEG_LIB})
