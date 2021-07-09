find_package(X11 REQUIRED)
find_library(JPEG_LIB jpeg)

MJPG_STREAMER_PLUGIN_OPTION(input_xgrab "X.org grabbing plugin")
MJPG_STREAMER_PLUGIN_COMPILE(input_xgrab  jpeg_utils.c input_xgrab.c)
target_link_libraries (input_xgrab ${X11_LIBRARIES} ${JPEG_LIB}) 
