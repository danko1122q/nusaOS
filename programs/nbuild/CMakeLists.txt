find_program(FASM_HOST_EXECUTABLE fasm)

if(NOT FASM_HOST_EXECUTABLE)
    message(WARNING "Host 'fasm' not found - skipping nbuild. Install fasm to enable.")
    return()
endif()

set(NBUILD_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/nbuild.asm)
set(NBUILD_OBJECT ${CMAKE_CURRENT_BINARY_DIR}/nbuild_asm.o)

add_custom_command(
    OUTPUT  ${NBUILD_OBJECT}
    COMMAND ${FASM_HOST_EXECUTABLE} ${NBUILD_SOURCE} ${NBUILD_OBJECT}
    DEPENDS ${NBUILD_SOURCE}
    COMMENT "Assembling nbuild for nusaOS (${FASM_HOST_EXECUTABLE})"
    VERBATIM
)

set_source_files_properties(${NBUILD_OBJECT} PROPERTIES
    EXTERNAL_OBJECT TRUE
    GENERATED       TRUE
)

add_executable(nbuild
    ${CMAKE_CURRENT_SOURCE_DIR}/stub.c
    ${NBUILD_OBJECT}
)

add_dependencies(nbuild libc)

install(TARGETS nbuild RUNTIME DESTINATION bin)
