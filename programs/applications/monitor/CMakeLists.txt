SET(SOURCES
    main.cpp
    CpuGraphWidget.cpp
    MemGraphWidget.cpp
    ProcessListWidget.cpp
    ProcessInspectorWidget.cpp
    ProcessMemoryLayoutWidget.cpp
    ProcessManager.cpp
)
MAKE_APP(monitor)
TARGET_LINK_LIBRARIES(monitor libui libsys)