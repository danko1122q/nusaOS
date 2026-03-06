cmake_minimum_required(VERSION 3.10)
project(nsa CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(libnsa STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/nsa_lexer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/nsa_compiler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/nsa_vm.cpp
)
target_include_directories(libnsa PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
set_target_properties(libnsa PROPERTIES PREFIX "")  # output: libnsa.a not liblibnsa.a

add_executable(nsa
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
)
target_include_directories(nsa PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(nsa libnsa)

install(TARGETS nsa RUNTIME DESTINATION bin)