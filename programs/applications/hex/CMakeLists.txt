SET(SOURCES
    main.cpp
    HexEditor.cpp
    HexWidget.cpp
    StatusBar.cpp
)
MAKE_APP(hexeditor)
TARGET_LINK_LIBRARIES(hexeditor libui libnusa libapp)