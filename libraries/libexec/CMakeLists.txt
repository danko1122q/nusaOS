SET(SOURCES Object.cpp Loader.cpp dlfunc.cpp)
MAKE_LIBRARY(libexec)
TARGET_LINK_LIBRARIES(libexec libnusa)
TARGET_LINK_LIBRARIES(libexec_static libnusa_static)