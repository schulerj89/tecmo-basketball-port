cmake_minimum_required(VERSION 3.16)

project(tecmo_basketball_c_port LANGUAGES C)

set(TECMO_PORT_SOURCES
    src/main.c
    src/asm_inventory.c
    src/png_writer.c
    src/tecmo_asset_pack.c
    src/asset_pack/tecmo_asset_pack_arena.c
    src/asset_pack/tecmo_asset_pack_d9f6.c
    src/asset_pack/tecmo_asset_pack_finale.c
    src/asset_pack/tecmo_asset_pack_gameplay.c
    src/asset_pack/tecmo_asset_pack_gameplay_court.c
    src/asset_pack/tecmo_asset_pack_title.c
    src/asset_pack/tecmo_asset_pack_opening.c
    src/asset_pack/tecmo_asset_pack_music.c
    src/asset_pack/tecmo_asset_pack_post_arena.c
    src/asset_pack/tecmo_asset_pack_preseason.c
    src/asset_pack/tecmo_asset_pack_reader.c
    src/asset_pack/tecmo_asset_pack_source_map.c
    src/asset_pack/tecmo_asset_pack_start_menu.c
    src/asset_pack/tecmo_asset_pack_team_data.c
    src/asset_pack/tecmo_asset_pack_util.c
    src/asset_pack/tecmo_asset_pack_writer.c
    src/tecmo_bank07.c
    src/tecmo_controls.c
    src/tecmo_music.c
    src/tecmo_audio_output.c
    src/tecmo_flow_test.c
    src/tecmo_game.c
    src/tecmo_gameplay_assets.c
    src/tecmo_gameplay_court.c
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
    src/tecmo_preseason_menu.c
    src/tecmo_start_game_menu.c
    src/tecmo_team_data.c
    src/tecmo_title_screen.c
)

function(configure_tecmo_target target_name)
    target_include_directories(${target_name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    set_target_properties(${target_name} PROPERTIES
        C_STANDARD 11
        C_STANDARD_REQUIRED YES
        C_EXTENSIONS NO
    )
    if(MSVC)
        target_compile_options(${target_name} PRIVATE /W4)
    else()
        target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
        target_link_libraries(${target_name} PRIVATE m)
    endif()
    if(WIN32)
        target_sources(${target_name} PRIVATE src/win32_platform.c)
        target_link_libraries(${target_name} PRIVATE user32 gdi32 winmm)
    endif()
endfunction()

add_executable(tecmo_port ${TECMO_PORT_SOURCES})
configure_tecmo_target(tecmo_port)

if(WIN32)
    add_executable(tecmo_port_game WIN32 ${TECMO_PORT_SOURCES})
    configure_tecmo_target(tecmo_port_game)
    if(MSVC)
        target_link_options(tecmo_port_game PRIVATE /ENTRY:mainCRTStartup)
    else()
        target_link_options(tecmo_port_game PRIVATE -Wl,-e,mainCRTStartup)
    endif()
endif()
