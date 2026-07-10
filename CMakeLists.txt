cmake_minimum_required(VERSION 3.16)

project(tecmo_basketball_c_port LANGUAGES C)

add_executable(tecmo_port
    src/main.c
    src/asm_inventory.c
    src/png_writer.c
    src/tecmo_asset_pack.c
    src/asset_pack/tecmo_asset_pack_d9f6.c
    src/asset_pack/tecmo_asset_pack_reader.c
    src/asset_pack/tecmo_asset_pack_util.c
    src/asset_pack/tecmo_asset_pack_writer.c
    src/tecmo_bank07.c
    src/tecmo_controls.c
    src/tecmo_flow_test.c
    src/tecmo_game.c
    src/tecmo_intro_arena.c
    src/tecmo_intro_arena_scene.c
    src/tecmo_intro_layout.c
    src/tecmo_intro_finale.c
    src/tecmo_intro_post_arena.c
    src/tecmo_intro_screen.c
    src/tecmo_intro_stage.c
    src/tecmo_intro_trace.c
    src/tecmo_memory.c
    src/tecmo_nametable_screen.c
    src/tecmo_nes_video.c
)

if(WIN32)
    target_sources(tecmo_port PRIVATE src/win32_platform.c)
    target_link_libraries(tecmo_port PRIVATE user32 gdi32)
endif()

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
