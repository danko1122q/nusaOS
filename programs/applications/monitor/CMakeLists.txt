SET(SOURCES
    main.cpp
    CpuGraphWidget.cpp
    MemGraphWidget.cpp
)
MAKE_APP(monitor)
TARGET_LINK_LIBRARIES(monitor libui libsys)