#include "asm_inventory.h"
#include "png_writer.h"
#include "tecmo_asset_pack.h"
#include "asset_pack/tecmo_asset_pack_gameplay_audio.h"
#include "asset_pack/tecmo_asset_pack_gameplay_camera.h"
#include "asset_pack/tecmo_asset_pack_gameplay_movement.h"
#include "asset_pack/tecmo_asset_pack_gameplay_ball_dribble.h"
#include "asset_pack/tecmo_asset_pack_gameplay_fatigue.h"
#include "asset_pack/tecmo_asset_pack_gameplay_cpu_steering.h"
#include "asset_pack/tecmo_asset_pack_gameplay_hud.h"
#include "asset_pack/tecmo_asset_pack_gameplay_court_orientation.h"
#include "asset_pack/tecmo_asset_pack_gameplay_backcourt.h"
#include "asset_pack/tecmo_asset_pack_gameplay_free_throw_lineup.h"
#include "asset_pack/tecmo_asset_pack_gameplay_violation_referee.h"
#include "asset_pack/tecmo_asset_pack_music.h"
#include "tecmo_audio_output.h"
#include "tecmo_bank07.h"
#include "tecmo_game.h"
#include "tecmo_frontend_audio.h"
#include "tecmo_gameplay_audio.h"
#include "tecmo_gameplay_assets.h"
#include "tecmo_gameplay_camera.h"
#include "tecmo_gameplay_movement.h"
#include "tecmo_gameplay_ball_dribble.h"
#include "tecmo_gameplay_fatigue.h"
#include "tecmo_gameplay_cpu_steering.h"
#include "tecmo_gameplay_hud.h"
#include "tecmo_gameplay_court.h"
#include "tecmo_gameplay_court_orientation.h"
#include "tecmo_gameplay_backcourt.h"
#include "tecmo_gameplay_close_shots.h"
#include "tecmo_gameplay_dunk_cutaway.h"
#include "tecmo_gameplay_jump_shots.h"
#include "tecmo_gameplay_shot_resolution.h"
#include "tecmo_gameplay_penalties.h"
#include "tecmo_gameplay_violation_referee.h"
#include "tecmo_gameplay_free_throw_lineup.h"
#include "tecmo_gameplay_free_throw_projection_test.h"
#include "tecmo_gameplay_scene.h"
#include "tecmo_gameplay_state.h"
#include "tecmo_intro_arena_scene.h"
#include "tecmo_nes_video.h"
#include "tecmo_win32_keys.h"

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tecmo_cli_internal.h"

int tecmo_cli_run_render_command(const TecmoCliContext *context)
{
    const char *command;
    const char *root;
    int argc;
    char **argv;
    int index;

    if (context == NULL) return TECMO_CLI_NOT_HANDLED;
    command = context->command;
    root = context->root;
    argc = context->argc;
    argv = context->argv;
    index = context->index;
    if (strcmp(command, "--render-test") == 0 || strcmp(command, "--render-test-mode") == 0) {
        const bool mode_specific = strcmp(command, "--render-test-mode") == 0;
        const char *mode_name = "menu";
        const int width = 640;
        const int height = 480;
        const size_t permanent_size = 16U * 1024U * 1024U;
        const size_t transient_size = 16U * 1024U * 1024U;
        const char *out_path;
        TecmoGameMemory memory;
        TecmoRuntime *runtime;
        TecmoFramebuffer framebuffer;
        uint32_t *pixels;
        uint8_t *rgba;
        void *permanent_block;
        void *transient_block;
        bool arena_render_succeeded = false;
        int result = 1;

        if (mode_specific) {
            mode_name = index < argc ? argv[index++] : "menu";
        }
        out_path = index < argc ? argv[index] : "build\\play_test.png";

        memset(&memory, 0, sizeof(memory));
        runtime = (TecmoRuntime *)calloc(1U, sizeof(*runtime));
        permanent_block = malloc(permanent_size);
        transient_block = malloc(transient_size);
        pixels = (uint32_t *)malloc((size_t)width * (size_t)height * sizeof(uint32_t));
        rgba = (uint8_t *)malloc((size_t)width * (size_t)height * 4U);
        if (runtime == NULL || permanent_block == NULL || transient_block == NULL ||
            pixels == NULL || rgba == NULL) {
            printf("Failed to allocate render-test memory.\n");
            free(runtime);
            free(permanent_block);
            free(transient_block);
            free(pixels);
            free(rgba);
            return 1;
        }

        tecmo_arena_init(&memory.permanent, permanent_block, permanent_size);
        tecmo_arena_init(&memory.transient, transient_block, transient_size);
        if (strcmp(mode_name, "original-title-chr") == 0) {
            TecmoOriginalTitleGlyphs glyphs;
            uint8_t *chr_bytes = NULL;
            uint64_t chr_byte_count = 0;
            if (tecmo_load_original_title_glyphs(root, &glyphs) != 0) {
                printf("Failed to load original title glyph mapping from local decomp root %s\n", root);
            } else if (tecmo_load_chr_data(root, &chr_bytes, &chr_byte_count) != 0) {
                printf("Failed to load CHR data from local decomp root %s\n", root);
            } else {
                framebuffer.pixels = pixels;
                framebuffer.width = width;
                framebuffer.height = height;
                framebuffer.pitch_pixels = width;
                tecmo_render_original_title_chr_probe(&framebuffer, &glyphs, chr_bytes, chr_byte_count, 31U);
                result = 0;
            }
            tecmo_free_buffer(chr_bytes);
        } else if (strcmp(mode_name, "original-title") == 0) {
            char title_text[TECMO_MAX_NAME_TEXT];
            if (tecmo_load_original_title_text(root, title_text, sizeof(title_text)) != 0) {
                printf("Failed to load original title text from local decomp root %s\n", root);
            } else {
                framebuffer.pixels = pixels;
                framebuffer.width = width;
                framebuffer.height = height;
                framebuffer.pitch_pixels = width;
                tecmo_render_original_title_probe(&framebuffer, title_text);
                result = 0;
            }
        } else if (strcmp(mode_name, "intro-c051-d861-model") == 0) {
            char self_test_message[96];
            framebuffer.pixels = pixels;
            framebuffer.width = width;
            framebuffer.height = height;
            framebuffer.pitch_pixels = width;
            if (!tecmo_intro_stage_self_test(self_test_message, sizeof(self_test_message))) {
                printf("Intro C051/D861 helper self-test failed: %s\n", self_test_message);
            } else {
                tecmo_render_intro_c051_d861_model(&framebuffer);
                result = 0;
            }
        } else if (!tecmo_runtime_init_with_flags(runtime,
                                                  &memory,
                                                  root,
                                                  tecmo_cli_render_mode_requires_roster_data(mode_name)
                                                      ? 0U
                                                      : TECMO_RUNTIME_INIT_ALLOW_EMPTY_ROSTER)) {
            printf("Failed to initialize runtime from %s\n", root);
        } else {
            bool render_runtime = true;
            if (strncmp(mode_name, "season-", 7) == 0) {
                runtime->season_session.season_type = TECMO_SEASON_REGULAR;
                memset(runtime->season_session.team_control, 0,
                       sizeof(runtime->season_session.team_control));
                memset(runtime->season_session.wins, 0,
                       sizeof(runtime->season_session.wins));
                memset(runtime->season_session.losses, 0,
                       sizeof(runtime->season_session.losses));
                runtime->season_session.schedule_index = 0U;
                runtime->season_session.dirty = false;
            }
            {
                bool mode_handled = false;
                TecmoCliRenderModeState mode_state;

                memset(&mode_state, 0, sizeof(mode_state));
                mode_state.pixels = pixels;
                mode_state.width = width;
                mode_state.height = height;
                mode_state.result = result;
                mode_state.arena_render_succeeded = arena_render_succeeded;
                render_runtime = tecmo_cli_configure_render_mode(
                    runtime, mode_name, &mode_state, &mode_handled);
                result = mode_state.result;
                arena_render_succeeded = mode_state.arena_render_succeeded;
                if (!mode_handled) {
                    printf("Unsupported render-test mode: %s\n", mode_name);
                    render_runtime = false;
                }
            }
            if (render_runtime) {
                framebuffer.pixels = pixels;
                framebuffer.width = width;
                framebuffer.height = height;
                framebuffer.pitch_pixels = width;
                tecmo_runtime_render(runtime, &framebuffer);
                result = tecmo_cli_validate_render_asset_contract(
                    runtime, mode_name) ? 0 : 1;
            }
            tecmo_cli_print_render_diagnostics(
                runtime, mode_name, arena_render_succeeded);
        }

        if (result == 0) {
            for (size_t i = 0; i < (size_t)width * (size_t)height; ++i) {
                uint32_t pixel = pixels[i];
                rgba[i * 4U + 0U] = (uint8_t)((pixel >> 16U) & 0xFFU);
                rgba[i * 4U + 1U] = (uint8_t)((pixel >> 8U) & 0xFFU);
                rgba[i * 4U + 2U] = (uint8_t)(pixel & 0xFFU);
                rgba[i * 4U + 3U] = (uint8_t)((pixel >> 24U) & 0xFFU);
            }
            if (png_write_rgba8(out_path, rgba, width, height) == 0) {
                printf("Rendered playable frame to %s\n", out_path);
                result = 0;
            } else {
                printf("Failed to write %s\n", out_path);
                result = 1;
            }
        }

        tecmo_runtime_shutdown(runtime);
        free(runtime);
        free(permanent_block);
        free(transient_block);
        free(pixels);
        free(rgba);
        return result;
    }
    return TECMO_CLI_NOT_HANDLED;
}
