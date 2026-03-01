SET(SOURCES
    main.cpp
    TetrisWidget.cpp)

MAKE_APP(tetris)
TARGET_LINK_LIBRARIES(tetris libui)