#include "tecmo_controls.h"
#include "tecmo_framebuffer.h"
#include "tecmo_game.h"
#include "tecmo_gameplay_scene.h"
#include "tecmo_gameplay_state.h"
#include "tecmo_intro_layout.h"
#include "tecmo_title_screen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tecmo_cli_internal.h"

#define TECMO_INTRO_PRODUCTION_CLEAN_FRAME_PREFIX \
    "intro-production-clean-frame"
#define TECMO_INTRO_PRODUCTION_CLEAN_FRAME_MAX 4096U


static bool configure_intro_production_clean_frame_mode(
    TecmoRuntime *runtime,
    const char *mode_name,
    TecmoCliRenderModeState *state,
    bool *handled_out)
{
    TecmoInput neutral_input;
    unsigned frame;

    *handled_out = false;
    if (strncmp(mode_name,
                TECMO_INTRO_PRODUCTION_CLEAN_FRAME_PREFIX,
                sizeof(TECMO_INTRO_PRODUCTION_CLEAN_FRAME_PREFIX) - 1U) != 0) {
        return true;
    }

    *handled_out = true;
    if (!tecmo_cli_parse_render_frame_suffix(
            mode_name, TECMO_INTRO_PRODUCTION_CLEAN_FRAME_PREFIX, &frame) ||
        frame > TECMO_INTRO_PRODUCTION_CLEAN_FRAME_MAX) {
        printf("Unsupported render-test mode: %s\n", mode_name);
        return false;
    }

    memset(&neutral_input, 0, sizeof(neutral_input));
    tecmo_runtime_set_mode(runtime, TECMO_MODE_FIRST_SPRITE);
    runtime->debug_overlay = false;
    for (unsigned logical_frame = 0U; logical_frame < frame; ++logical_frame) {
        tecmo_runtime_update(runtime, &neutral_input);
    }
    printf("intro-production-state global=%u step=%u local=%u mode=%u attract=%u title_armed=%u title_confirming=%u title_frame=%u\n",
           runtime->frame_counter,
           (unsigned)runtime->intro_output_step,
           runtime->mode_frame_counter,
           (unsigned)runtime->mode,
           runtime->intro_output_step == 15U ? 1U : 0U,
           runtime->title_start_armed ? 1U : 0U,
           runtime->title_confirming ? 1U : 0U,
           runtime->title_confirmation_frame);
    state->result = 0;
    return true;
}


static bool configure_gameplay_mode(TecmoRuntime *runtime, const char *mode_name, TecmoCliRenderModeState *state, bool *handled_out)
{
    uint32_t *pixels = state->pixels;
    const int width = state->width;
    const int height = state->height;
    TecmoFramebuffer framebuffer = {0};
    bool arena_render_succeeded = state->arena_render_succeeded;
    bool render_runtime = true;
    int result = state->result;

    *handled_out = false;
            if (strncmp(mode_name, "gameplay-", 9) == 0) {
                *handled_out = true;
                framebuffer.pixels = pixels;
                framebuffer.width = width;
                framebuffer.height = height;
                framebuffer.pitch_pixels = width;
                if (!tecmo_cli_setup_gameplay_render_checkpoint(runtime, mode_name)) {
                    printf("Unsupported or unavailable gameplay render-test mode: %s\n",
                           mode_name);
                    result = 1;
                } else {
                    arena_render_succeeded = tecmo_render_gameplay_scene(
                        runtime, &framebuffer);
                    result = arena_render_succeeded ? 0 : 1;
                    printf("gameplay-state frame=%u shot=%s phase=%s score=%u/%u clock=%u:%02u period=%u overtime=%u shot-clock=%u pretip=%s phase-frame=%u violation=%s\n",
                           runtime->gameplay_scene.frame,
                           tecmo_gameplay_scene_shot_name(
                               runtime->gameplay_scene.shot_kind),
                           tecmo_gameplay_phase_name(
                               runtime->gameplay_scene.state.phase),
                           (unsigned)runtime->gameplay_scene.state.score[
                               TECMO_GAMEPLAY_TEAM_AWAY],
                           (unsigned)runtime->gameplay_scene.state.score[
                               TECMO_GAMEPLAY_TEAM_HOME],
                           (unsigned)runtime->gameplay_scene.state.clock_minutes,
                           (unsigned)runtime->gameplay_scene.state.clock_seconds,
                           (unsigned)runtime->gameplay_scene.state.period,
                           (unsigned)runtime->gameplay_scene.state.overtime_count,
                           (unsigned)runtime->gameplay_scene.state.shot_clock,
                           tecmo_gameplay_pretip_phase_name(
                               runtime->gameplay_scene.pretip_state.phase),
                           (unsigned)runtime->gameplay_scene.state.phase_frame,
                           tecmo_gameplay_violation_name(
                               runtime->gameplay_scene.state.violation));
                }
                render_runtime = false;
}

    state->arena_render_succeeded = arena_render_succeeded;
    state->result = result;
    return render_runtime;
}

static TecmoFramebuffer make_render_framebuffer(
    const TecmoCliRenderModeState *state)
{
    TecmoFramebuffer framebuffer = {0};
    framebuffer.pixels = state->pixels;
    framebuffer.width = state->width;
    framebuffer.height = state->height;
    framebuffer.pitch_pixels = state->width;
    return framebuffer;
}

static bool configure_intro_opening_mode(
    TecmoRuntime *runtime,
    const char *mode_name,
    TecmoCliRenderModeState *state,
    bool *handled_out)
{
    TecmoFramebuffer framebuffer;
    bool arena_render_succeeded = state->arena_render_succeeded;
    bool render_runtime = true;
    int result = state->result;

    if (!configure_intro_production_clean_frame_mode(
            runtime, mode_name, state, handled_out)) {
        return false;
    }
    if (*handled_out) {
        state->arena_render_succeeded = arena_render_succeeded;
        return true;
    }

    *handled_out = true;
    if (strcmp(mode_name, "menu-overlay") == 0) {
        TecmoInput input;
        memset(&input, 0, sizeof(input));
        tecmo_runtime_set_mode(runtime, TECMO_MODE_MAIN_MENU);
        runtime->debug_overlay = true;
        runtime->frame_seconds = 1.0f / 60.0f;
        tecmo_runtime_update(runtime, &input);
    } else if (strcmp(mode_name, "rosters") == 0) {
        tecmo_runtime_set_mode(runtime, TECMO_MODE_ROSTERS);
    } else if (strcmp(mode_name, "play") == 0) {
        tecmo_runtime_set_mode(runtime, TECMO_MODE_FIRST_SPRITE);
        runtime->mode_frame_counter = 16U;
    } else if (strncmp(mode_name, "play-fade", 9) == 0) {
        long stage = strtol(mode_name + 9, NULL, 10);
        if (stage < 0) stage = 0;
        if (stage > 4) stage = 4;
        tecmo_runtime_set_mode(runtime, TECMO_MODE_FIRST_SPRITE);
        runtime->mode_frame_counter = (unsigned)stage * 4U;
    } else if (strncmp(mode_name, "play-step", 9) == 0) {
        long step = strtol(mode_name + 9, NULL, 10);
        if (step < 0) step = 0;
        tecmo_runtime_set_mode(runtime, TECMO_MODE_FIRST_SPRITE);
        runtime->intro_output_step = (uint8_t)step;
        if (step == 8) {
            runtime->mode_frame_counter = 320U;
        } else if (step == 7) {
            runtime->mode_frame_counter = 48U;
        } else if (step == 9) {
            runtime->mode_frame_counter = 35U;
        } else if (step >= 10) {
            runtime->mode_frame_counter = 28U;
        } else {
            runtime->mode_frame_counter = 16U;
        }
    } else if (strcmp(mode_name, "first-sprite") == 0 ||
               strcmp(mode_name, "first-sprite-debug") == 0) {
        framebuffer = make_render_framebuffer(state);
        tecmo_render_first_sprite_probe(runtime, &framebuffer);
        render_runtime = false;
        result = 0;
    } else if (strcmp(mode_name, "intro-l88e7-proof") == 0) {
        framebuffer = make_render_framebuffer(state);
        tecmo_render_intro_l88e7_proof(runtime, &framebuffer);
        render_runtime = false;
        result = 0;
    } else if (strcmp(mode_name, "intro-license") == 0) {
        framebuffer = make_render_framebuffer(state);
        runtime->mode_frame_counter = 48U;
        arena_render_succeeded =
            tecmo_render_intro_license_screen(runtime, &framebuffer);
        render_runtime = false;
        result = arena_render_succeeded ? 0 : 1;
    } else {
        *handled_out = false;
        return true;
    }
    state->arena_render_succeeded = arena_render_succeeded;
    state->result = result;
    return render_runtime;
}

static bool configure_intro_arena_mode(
    TecmoRuntime *runtime,
    const char *mode_name,
    TecmoCliRenderModeState *state,
    bool *handled_out)
{
    TecmoFramebuffer framebuffer;
    bool arena_render_succeeded = state->arena_render_succeeded;
    bool render_runtime = true;
    int result = state->result;

    *handled_out = true;
    if (strcmp(mode_name, "intro-arena-transition") == 0) {
        framebuffer = make_render_framebuffer(state);
        runtime->debug_overlay = true;
        runtime->mode_frame_counter = 240U;
        arena_render_succeeded =
            tecmo_render_intro_arena_transition(runtime, &framebuffer);
        render_runtime = false;
        result = arena_render_succeeded ? 0 : 1;
    } else if (strncmp(mode_name, "intro-arena-clean-frame", 23) == 0) {
        unsigned frame;
        if (!tecmo_cli_parse_render_frame_suffix(
                mode_name, "intro-arena-clean-frame", &frame)) {
            printf("Unsupported render-test mode: %s\n", mode_name);
            render_runtime = false;
        } else {
            framebuffer = make_render_framebuffer(state);
            runtime->debug_overlay = false;
            runtime->mode_frame_counter = frame;
            arena_render_succeeded =
                tecmo_render_intro_arena_transition(runtime, &framebuffer);
            render_runtime = false;
            result = arena_render_succeeded ? 0 : 1;
        }
    } else if (strncmp(mode_name, "intro-arena-frame", 17) == 0) {
        long frame = strtol(mode_name + 17, NULL, 10);
        if (frame < 0) frame = 0;
        framebuffer = make_render_framebuffer(state);
        runtime->debug_overlay = true;
        runtime->mode_frame_counter = (unsigned)frame;
        arena_render_succeeded =
            tecmo_render_intro_arena_transition(runtime, &framebuffer);
        render_runtime = false;
        result = arena_render_succeeded ? 0 : 1;
    } else {
        *handled_out = false;
        return true;
    }
    state->arena_render_succeeded = arena_render_succeeded;
    state->result = result;
    return render_runtime;
}

static bool configure_intro_post_mode(
    TecmoRuntime *runtime,
    const char *mode_name,
    TecmoCliRenderModeState *state,
    bool *handled_out)
{
    TecmoFramebuffer framebuffer;
    bool arena_render_succeeded = state->arena_render_succeeded;
    bool render_runtime = true;
    int result = state->result;

    *handled_out = true;
    if (strncmp(mode_name, "intro-ready-clean-frame", 23) == 0 ||
        strncmp(mode_name, "intro-ready-frame", 17) == 0) {
        const char *prefix = strncmp(mode_name, "intro-ready-clean-frame", 23) == 0
                                 ? "intro-ready-clean-frame"
                                 : "intro-ready-frame";
        unsigned frame;
        if (!tecmo_cli_parse_render_frame_suffix(mode_name, prefix, &frame)) {
            printf("Unsupported render-test mode: %s\n", mode_name);
            render_runtime = false;
        } else {
            framebuffer = make_render_framebuffer(state);
            runtime->debug_overlay = strcmp(prefix, "intro-ready-frame") == 0;
            runtime->mode_frame_counter = frame;
            arena_render_succeeded =
                tecmo_render_intro_ready_screen(runtime, &framebuffer);
            render_runtime = false;
            result = arena_render_succeeded ? 0 : 1;
        }
    } else if (strncmp(mode_name, "intro-warriors-clean-frame", 26) == 0 ||
               strncmp(mode_name, "intro-warriors-frame", 20) == 0) {
        const char *prefix = strncmp(mode_name, "intro-warriors-clean-frame", 26) == 0
                                 ? "intro-warriors-clean-frame"
                                 : "intro-warriors-frame";
        unsigned frame;
        if (!tecmo_cli_parse_render_frame_suffix(mode_name, prefix, &frame)) {
            printf("Unsupported render-test mode: %s\n", mode_name);
            render_runtime = false;
        } else {
            framebuffer = make_render_framebuffer(state);
            runtime->debug_overlay = strcmp(prefix, "intro-warriors-frame") == 0;
            runtime->mode_frame_counter = frame;
            arena_render_succeeded =
                tecmo_render_intro_warriors_transition(runtime, &framebuffer);
            render_runtime = false;
            result = arena_render_succeeded ? 0 : 1;
        }
    } else if (strncmp(mode_name, "intro-clippers-clean-frame", 26) == 0 ||
               strncmp(mode_name, "intro-clippers-frame", 20) == 0) {
        const char *prefix = strncmp(mode_name, "intro-clippers-clean-frame", 26) == 0
                                 ? "intro-clippers-clean-frame"
                                 : "intro-clippers-frame";
        unsigned frame;
        if (!tecmo_cli_parse_render_frame_suffix(mode_name, prefix, &frame)) {
            printf("Unsupported render-test mode: %s\n", mode_name);
            render_runtime = false;
        } else {
            framebuffer = make_render_framebuffer(state);
            runtime->debug_overlay = strcmp(prefix, "intro-clippers-frame") == 0;
            runtime->mode_frame_counter = frame;
            arena_render_succeeded =
                tecmo_render_intro_clippers_transition(runtime, &framebuffer);
            render_runtime = false;
            result = arena_render_succeeded ? 0 : 1;
        }
    } else if (strncmp(mode_name, "intro-bucks-clean-frame", 23) == 0 ||
               strncmp(mode_name, "intro-bucks-frame", 17) == 0) {
        const char *prefix = strncmp(mode_name, "intro-bucks-clean-frame", 23) == 0
                                 ? "intro-bucks-clean-frame"
                                 : "intro-bucks-frame";
        unsigned frame;
        if (!tecmo_cli_parse_render_frame_suffix(mode_name, prefix, &frame)) {
            printf("Unsupported render-test mode: %s\n", mode_name);
            render_runtime = false;
        } else {
            framebuffer = make_render_framebuffer(state);
            runtime->debug_overlay = strcmp(prefix, "intro-bucks-frame") == 0;
            runtime->mode_frame_counter = frame;
            arena_render_succeeded =
                tecmo_render_intro_bucks_transition(runtime, &framebuffer);
            render_runtime = false;
            result = arena_render_succeeded ? 0 : 1;
        }
    } else if (strncmp(mode_name, "intro-pass-clean-frame", 22) == 0 ||
               strncmp(mode_name, "intro-pass-frame", 16) == 0) {
        const char *prefix = strncmp(mode_name, "intro-pass-clean-frame", 22) == 0
                                 ? "intro-pass-clean-frame"
                                 : "intro-pass-frame";
        unsigned frame;
        if (!tecmo_cli_parse_render_frame_suffix(mode_name, prefix, &frame)) {
            printf("Unsupported render-test mode: %s\n", mode_name);
            render_runtime = false;
        } else {
            framebuffer = make_render_framebuffer(state);
            runtime->debug_overlay = strcmp(prefix, "intro-pass-frame") == 0;
            runtime->mode_frame_counter = frame;
            arena_render_succeeded =
                tecmo_render_intro_pass_transition(runtime, &framebuffer);
            render_runtime = false;
            result = arena_render_succeeded ? 0 : 1;
        }
    } else if (strncmp(mode_name, "intro-finale", 12) == 0) {
        unsigned frame;
        bool debug;
        if (!tecmo_cli_parse_finale_render_mode(mode_name, &frame, &debug)) {
            printf("Unsupported render-test mode: %s\n", mode_name);
            render_runtime = false;
        } else {
            framebuffer = make_render_framebuffer(state);
            runtime->debug_overlay = debug;
            runtime->mode_frame_counter = frame;
            arena_render_succeeded =
                tecmo_render_intro_finale(runtime, &framebuffer);
            render_runtime = false;
            result = arena_render_succeeded ? 0 : 1;
        }
    } else {
        *handled_out = false;
        return true;
    }
    state->arena_render_succeeded = arena_render_succeeded;
    state->result = result;
    return render_runtime;
}

static bool configure_intro_scene_mode(
    TecmoRuntime *runtime,
    const char *mode_name,
    TecmoCliRenderModeState *state,
    bool *handled_out)
{
    bool handled;
    bool result;

    result = configure_intro_opening_mode(runtime, mode_name, state, &handled);
    if (handled) {
        *handled_out = true;
        return result;
    }
    result = configure_intro_arena_mode(runtime, mode_name, state, &handled);
    if (handled) {
        *handled_out = true;
        return result;
    }
    result = configure_intro_post_mode(runtime, mode_name, state, &handled);
    *handled_out = handled;
    return result;
}

static bool configure_render_probe_mode(TecmoRuntime *runtime, const char *mode_name, TecmoCliRenderModeState *state, bool *handled_out)
{
    bool arena_render_succeeded = state->arena_render_succeeded;
    bool render_runtime = true;
    int result = state->result;

    *handled_out = false;
            if (strncmp(mode_name, "title-confirm-frame", 19) == 0) {
                *handled_out = true;
                unsigned frame;
                if (!tecmo_cli_parse_render_frame_suffix(mode_name, "title-confirm-frame", &frame) || frame > 126U) {
                    printf("Unsupported render-test mode: %s\n", mode_name);
                    render_runtime = false;
                } else {
                    tecmo_runtime_set_mode(runtime, TECMO_MODE_TITLE_SCREEN);
                    runtime->title_confirming = true;
                    runtime->title_confirmation_frame = frame;
                    runtime->mode_frame_counter = TECMO_TITLE_START_LOAD_FRAMES + frame;
                }
            } else if (strncmp(mode_name, "title-attract-frame", 19) == 0) {
                *handled_out = true;
                unsigned frame;
                if (!tecmo_cli_parse_render_frame_suffix(mode_name, "title-attract-frame", &frame) || frame > 642U) {
                    printf("Unsupported render-test mode: %s\n", mode_name);
                    render_runtime = false;
                } else {
                    tecmo_runtime_set_mode(runtime, TECMO_MODE_FIRST_SPRITE);
                    runtime->intro_output_step = 15U;
                    runtime->mode_frame_counter = frame;
                }
            } else if (strcmp(mode_name, "play-setup") == 0) {
                *handled_out = true;
                tecmo_runtime_set_mode(runtime, TECMO_MODE_PLAY_SETUP);
            } else if (strcmp(mode_name, "title-screen") == 0) {
                *handled_out = true;
                tecmo_runtime_set_mode(runtime, TECMO_MODE_TITLE_SCREEN);
                runtime->mode_frame_counter = 16U;
            } else if (strcmp(mode_name, "boot-title") == 0) {
                *handled_out = true;
                tecmo_runtime_set_mode(runtime, TECMO_MODE_TITLE_SCREEN);
                runtime->mode_frame_counter = 16U;
            } else if (strcmp(mode_name, "intro-presents") == 0) {
                *handled_out = true;
                tecmo_runtime_set_mode(runtime, TECMO_MODE_INTRO_PROBE);
            } else if (strcmp(mode_name, "intro-builder-sample") == 0) {
                *handled_out = true;
                TecmoIntroPlacement *placement;
                tecmo_runtime_set_mode(runtime, TECMO_MODE_INTRO_PROBE);
                runtime->selected_chr_table = 1U;
                runtime->intro_source_tile = 0xB6U;
                runtime->intro_canvas_focus = true;
                runtime->intro_canvas_cell_x = 5;
                runtime->intro_canvas_cell_y = 5;
                placement = &runtime->intro_placements[0];
                memset(placement, 0, sizeof(*placement));
                placement->active = true;
                placement->chr_bank = runtime->selected_chr_bank;
                placement->chr_table = runtime->selected_chr_table;
                placement->tile_ids[0] = 0x1B6U;
                placement->tile_count = 1;
                placement->canvas_cell_x = runtime->intro_canvas_cell_x;
                placement->canvas_cell_y = runtime->intro_canvas_cell_y;
                placement->pixel_x = placement->canvas_cell_x * 16;
                placement->pixel_y = placement->canvas_cell_y * 16;
                placement->scale = 2;
                (void)snprintf(placement->label, sizeof(placement->label), "B31 T1 1B6");
                runtime->intro_placement_count = 1;
                (void)snprintf(runtime->intro_layout_status,
                               sizeof(runtime->intro_layout_status),
                               "SAMPLE RECORD  Z ADDS  S SAVES");
            } else if (strcmp(mode_name, "intro-rabbit-preset") == 0) {
                *handled_out = true;
                TecmoInput input;
                tecmo_runtime_set_mode(runtime, TECMO_MODE_INTRO_PROBE);
                runtime->selected_chr_table = 1U;
                runtime->intro_source_tile = 0x25U;
                runtime->intro_canvas_focus = true;
                runtime->intro_canvas_cell_x = 5;
                runtime->intro_canvas_cell_y = 5;
                memset(&input, 0, sizeof(input));
                input.preset_rabbit = true;
                tecmo_runtime_update(runtime, &input);
                {
                    TecmoInput released_input;
                    memset(&released_input, 0, sizeof(released_input));
                    tecmo_runtime_update(runtime, &released_input);
                }
            } else if (strcmp(mode_name, "intro-tecmo-preset") == 0) {
                *handled_out = true;
                TecmoInput input;
                tecmo_runtime_set_mode(runtime, TECMO_MODE_INTRO_PROBE);
                runtime->selected_chr_table = 1U;
                runtime->intro_source_tile = 0x80U;
                runtime->intro_canvas_focus = true;
                runtime->intro_canvas_cell_x = 4;
                runtime->intro_canvas_cell_y = 5;
                memset(&input, 0, sizeof(input));
                input.preset_tecmo = true;
                tecmo_runtime_update(runtime, &input);
                {
                    TecmoInput released_input;
                    memset(&released_input, 0, sizeof(released_input));
                    tecmo_runtime_update(runtime, &released_input);
                }
            } else if (strcmp(mode_name, "intro-composite-preset") == 0) {
                *handled_out = true;
                TecmoInput input;
                tecmo_runtime_set_mode(runtime, TECMO_MODE_INTRO_PROBE);
                runtime->selected_chr_table = 1U;
                runtime->intro_source_tile = 0x80U;
                runtime->intro_canvas_focus = true;
                memset(&input, 0, sizeof(input));
                input.preset_composite = true;
                tecmo_runtime_update(runtime, &input);
                {
                    TecmoInput released_input;
                    memset(&released_input, 0, sizeof(released_input));
                    tecmo_runtime_update(runtime, &released_input);
                }
            } else if (strcmp(mode_name, "intro-presents-table1") == 0) {
                *handled_out = true;
                tecmo_runtime_set_mode(runtime, TECMO_MODE_INTRO_PROBE);
                runtime->selected_chr_table = 1U;
            } else if (strcmp(mode_name, "chr-playground") == 0) {
                *handled_out = true;
                tecmo_runtime_set_mode(runtime, TECMO_MODE_CHR_PLAYGROUND);
            } else if (strcmp(mode_name, "chr-playground-table1") == 0) {
                *handled_out = true;
                tecmo_runtime_set_mode(runtime, TECMO_MODE_CHR_PLAYGROUND);
                runtime->selected_chr_table = 1U;
            } else {
                *handled_out = false;
                return true;
            }

    state->arena_render_succeeded = arena_render_succeeded;
    state->result = result;
    return render_runtime;
}

bool tecmo_cli_configure_render_scene_mode(TecmoRuntime *runtime, const char *mode_name, TecmoCliRenderModeState *state, bool *handled_out)
{
    bool handled;
    bool result;

    result = configure_gameplay_mode(runtime, mode_name, state, &handled);
    if (handled) { *handled_out = true; return result; }
    result = configure_intro_scene_mode(runtime, mode_name, state, &handled);
    if (handled) { *handled_out = true; return result; }
    result = configure_render_probe_mode(runtime, mode_name, state, &handled);
    if (handled) { *handled_out = true; return result; }
    *handled_out = false;
    return true;
}
