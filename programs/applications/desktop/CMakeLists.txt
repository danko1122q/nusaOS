cmake_minimum_required(VERSION 3.16)
project(desktop)

set(CMAKE_CXX_STANDARD 17)

add_executable(desktop
	main.cpp
	Desktop.cpp
	Desktop.h
	DesktopWidget.cpp
	DesktopWidget.h
)

target_link_libraries(desktop
	libui
	libapp
	libpond
	libgraphics
	libnusa
)

install(TARGETS desktop RUNTIME DESTINATION apps/desktop.app)
install(DIRECTORY resources/ DESTINATION apps/desktop.app)