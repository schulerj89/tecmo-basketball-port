cmake_minimum_required(VERSION 3.16)

project(tecmo_basketball_c_port LANGUAGES C)

add_executable(tecmo_port
    src/main.c
    src/asm_inventory.c
    src/png_writer.c
)

target_include_directories(tecmo_port PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set_target_properties(tecmo_port PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED YES
    C_EXTENSIONS NO
)

if(MSVC)
    target_compile_definitions(tecmo_port PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(tecmo_port PRIVATE /W4)
else()
    target_compile_options(tecmo_port PRIVATE -Wall -Wextra -Wpedantic)
endif()
