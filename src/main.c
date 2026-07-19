#include "asm_inventory.h"
#include "png_writer.h"
#include "tecmo_asset_pack.h"
#include "tecmo_audio_output.h"
#include "tecmo_bank07.h"
#include "tecmo_game.h"
#include "tecmo_gameplay_audio.h"
#include "tecmo_gameplay_assets.h"
#include "tecmo_gameplay_court.h"
#include "tecmo_gameplay_close_shots.h"
#include "tecmo_gameplay_scene.h"
#include "tecmo_gameplay_state.h"
#include "tecmo_intro_arena_scene.h"
#include "tecmo_nes_video.h"

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_usage(const char *program)
{
    printf("Usage: %s [--root PATH] COMMAND\n", program);
    printf("       %s COMMAND   # uses TECMO_DECOMP_ROOT or current directory\n", program);
    printf("\n");
    printf("Commands:\n");
    printf("  --summary               Inventory banks, lifted chunks, contracts, roster, and CHR\n");
    printf("  --banks                 Scan baseline PRG banks\n");
    printf("  --chunks                Count lifted chunks by bank and show sample chunk summaries\n");
    printf("  --assets                Analyze raw CHR bytes in build\\baseline\\Tiles.asm\n");
    printf("  --roster [TEAM|--all]   Parse labeled Bank 02 roster records\n");
    printf("  --play                  Launch native playable prototype window\n");
    printf("  --flow-test             Run headless native menu/play/quit flow checks\n");
    printf("  --controls-test         Run portable held/pressed/released control-state checks\n");
    printf("  --bank07-test           Run fixed-bank helper C counterpart checks\n");
    printf("  --video-test            Run embedded FCEUX 2.6.6 NES palette mapping checks\n");
    printf("  --music-test            Run strict TMUS parser/sequencer/synth checks\n");
    printf("  --gameplay-audio-test   Run strict TSFX/TDMC parser/mixer checks\n");
    printf("  --team-management-test  Run strict TTMG parser and native STARTERS/PLAYBOOK checks\n");
    printf("  --season-test           Run strict TSNS/TSAV season-management checks\n");
    printf("  --gameplay-state-test   Run deterministic gameplay clock/rules/shot-state checks\n");
    printf("  --gameplay-scene-test PACK  Run native gameplay launch/input/shot checks\n");
    printf("  --arena-scene-test      Run native arena intro scene anchor checks\n");
    printf("  --render-test PATH      Render first playable frame to a PNG\n");
    printf("  --render-test-mode MODE PATH  Render menus, intro scenes, or strict gameplay-start/jump-frameN/dunk-frameN checkpoints to PNG\n");
    printf("  --generate-rosters DIR  Generate static C roster source/header from Bank 02\n");
    printf("  --build-assetpack ROM PATH  Build a private .assetpack from an iNES ROM only; no decomp/capture imports\n");
    printf("  --assetpack-test       Run asset-pack builder/list/read self-tests\n");
    printf("  --gameplay-assets-test PACK  Validate strict TGPL-1 gameplay assets\n");
    printf("  --gameplay-court-test PACK  Validate strict TGCT-1 static court assets\n");
    printf("  --gameplay-close-shots-test PACK  Validate strict TGCS-1 close-shot assets\n");
    printf("  --assetpack-list PACK  Print an asset-pack directory listing\n");
    printf("  --export-chr PATH       Export build\\baseline\\Tiles.asm to raw .chr bytes\n");
    printf("  --export-chr-png DIR    Export one PNG tile sheet per 8KB CHR bank\n");
}

static void print_intro_render_capture_status(const TecmoRuntime *runtime,
                                              const char *mode_name,
                                              bool arena_rendered)
{
    const char *assetpack_marker = "assetpack entry ";
    const char *entry_start;
    char entry_id[64];

    if (runtime == NULL || mode_name == NULL) {
        return;
    }

    if (strcmp(mode_name, "intro-license") == 0 ||
        strcmp(mode_name, "play") == 0 ||
        strncmp(mode_name, "play-fade", 9) == 0 ||
        strcmp(mode_name, "play-step6") == 0 ||
        strcmp(mode_name, "play-step7") == 0 ||
        strcmp(mode_name, "title-screen") == 0 ||
        strcmp(mode_name, "boot-title") == 0) {
        const bool presents_renderable =
            runtime->intro_presents_asset.available &&
            tecmo_intro_screen_chr_available(&runtime->intro_presents_asset,
                                             runtime->title_chr_bytes,
                                             runtime->title_chr_byte_count);
        const bool license_renderable =
            runtime->intro_license_asset.available &&
            tecmo_intro_screen_chr_available(&runtime->intro_license_asset,
                                             runtime->title_chr_bytes,
                                             runtime->title_chr_byte_count);
        const bool license_mode = strcmp(mode_name, "intro-license") == 0 ||
                                  strcmp(mode_name, "play-step7") == 0;
        const bool loose_trace_enabled =
            strcmp(runtime->intro_trace_status, "LOOSE INTRO TRACE DISABLED") != 0;
        const TecmoIntroScreenAsset *asset = license_mode
                                                 ? &runtime->intro_license_asset
                                                 : &runtime->intro_presents_asset;
        printf("intro-opening-render-source presents=%u license=%u chr=%u schema=TISC-1 loose_trace=%u\n",
               presents_renderable ? 1U : 0U,
               license_renderable ? 1U : 0U,
               runtime->title_chr_bytes != NULL ? 1U : 0U,
               loose_trace_enabled ? 1U : 0U);
        printf("intro-opening-state kind=%s frame=%u palette=%u duration=%u sprites=%u\n",
               license_mode ? "nba-license" : "tecmo-presents",
               runtime->mode_frame_counter,
               (unsigned)tecmo_intro_screen_palette_stage(asset,
                                                          runtime->mode_frame_counter),
               (unsigned)asset->duration_frames,
               asset->sprite_count > 0U &&
                       runtime->mode_frame_counter >= asset->sprite_first_frame &&
                       runtime->mode_frame_counter < asset->sprite_hide_frame
                   ? (unsigned)asset->sprite_count
                   : 0U);
    } else if (strncmp(mode_name, "intro-arena", 11) == 0) {
        bool native_layer_available;
        bool native_sprite_groups_available;
        size_t native_sprite_group_count;
        size_t jumbotron_piece_count;
        size_t goal_piece_count;
        TecmoIntroArenaTransitionState arena_state;
        TecmoArenaNativeSpriteVisibleCounts visible_counts = {0U, 0U};

        entry_id[0] = '\0';
        entry_start = strstr(runtime->intro_arena_capture.status, assetpack_marker);
        if (entry_start != NULL) {
            const char *entry_end;
            size_t entry_length;

            entry_start += strlen(assetpack_marker);
            entry_end = strstr(entry_start, " pack ");
            entry_length = entry_end != NULL ? (size_t)(entry_end - entry_start) : 0U;
            if (entry_length > 0U && entry_length < sizeof(entry_id)) {
                memcpy(entry_id, entry_start, entry_length);
                entry_id[entry_length] = '\0';
            }
        }
        printf("intro-capture-status kind=arena available=%u nt=%u attr=%u pal=%u oam=%u\n",
               runtime->intro_arena_capture.available ? 1U : 0U,
               (unsigned)(runtime->intro_arena_capture.tile_count[0] +
                          runtime->intro_arena_capture.tile_count[1]),
               runtime->intro_arena_capture.available
                   ? (unsigned)(TECMO_INTRO_ARENA_PAGE_COUNT * 64U)
                   : 0U,
               (unsigned)runtime->intro_arena_capture.palette_stage_count,
               (unsigned)runtime->intro_arena_capture.sprite_count);
        printf("intro-capture-source kind=arena assetpack=%u entry=%s\n",
               entry_id[0] != '\0' ? 1U : 0U,
               entry_id[0] != '\0' ? entry_id : "none");
        native_layer_available = tecmo_intro_arena_tile_layer_chr_available(
            &runtime->intro_arena_tile_layer,
            runtime->title_chr_bytes,
            runtime->title_chr_byte_count);
        native_sprite_groups_available = tecmo_intro_arena_native_sprite_chr_available(
            &runtime->intro_arena_sprite_groups,
            runtime->title_chr_bytes,
            runtime->title_chr_byte_count);
        native_sprite_group_count = tecmo_intro_arena_native_sprite_group_count(
            &runtime->intro_arena_sprite_groups);
        jumbotron_piece_count = tecmo_intro_arena_native_sprite_piece_count(
            &runtime->intro_arena_sprite_groups,
            TECMO_ARENA_NATIVE_SPRITE_GROUP_JUMBOTRON);
        goal_piece_count = tecmo_intro_arena_native_sprite_piece_count(
            &runtime->intro_arena_sprite_groups,
            TECMO_ARENA_NATIVE_SPRITE_GROUP_GOAL);
        if (native_sprite_groups_available) {
            tecmo_intro_arena_transition_state(runtime->mode_frame_counter, &arena_state);
            visible_counts = tecmo_intro_arena_native_sprite_visible_counts(
                &runtime->intro_arena_sprite_groups,
                &arena_state);
        }
        printf("intro-arena-render-source kind=arena exact_layer=%u rendered=%u cells=%u palette=16 sprite_groups=%u jumbotron_pieces=%u goal_pieces=%u visible_jumbotron=%u visible_goal=%u\n",
               native_layer_available ? 1U : 0U,
               arena_rendered ? 1U : 0U,
               native_layer_available ? (unsigned)runtime->intro_arena_tile_layer.cell_count : 0U,
               native_sprite_groups_available ? (unsigned)native_sprite_group_count : 0U,
               native_sprite_groups_available ? (unsigned)jumbotron_piece_count : 0U,
               native_sprite_groups_available ? (unsigned)goal_piece_count : 0U,
               (unsigned)visible_counts.jumbotron,
               (unsigned)visible_counts.goal);
    } else if (strncmp(mode_name, "intro-finale", 12) == 0 ||
               strcmp(mode_name, "play-step14") == 0) {
        TecmoIntroFinaleState state;
        tecmo_intro_finale_state(&runtime->intro_finale_asset,
                                 runtime->mode_frame_counter,
                                 &state);
        printf("intro-finale-render-source finale=%u chr=%u schema=TFIN-1\n",
               runtime->intro_finale_asset.available ? 1U : 0U,
               runtime->title_chr_bytes != NULL ? 1U : 0U);
        printf("intro-finale-state frame=%u scene=%s phase=%s local=%u palette=%u variant=%u loop=%u anchor=%u,%u title=%u primary=%u:%u secondary=%u:%u sprites=%u black=%u hold=%u\n",
               runtime->mode_frame_counter,
               tecmo_intro_finale_scene_name(state.scene),
               tecmo_intro_finale_phase_name(state.phase),
               state.scene_frame,
               (unsigned)state.palette_stage,
               (unsigned)state.sprite_variant_index,
               (unsigned)state.short_loop_step,
               (unsigned)state.player_x,
               (unsigned)state.player_y,
               (unsigned)state.title_slots_written,
               (unsigned)state.scroll_page,
               (unsigned)state.scroll_x,
               (unsigned)state.secondary_scroll_page,
               (unsigned)state.secondary_scroll_x,
               state.sprites_visible ? 1U : 0U,
               state.black ? 1U : 0U,
               state.persistent_hold ? 1U : 0U);
    } else if (strncmp(mode_name, "intro-ready", 11) == 0 ||
               strncmp(mode_name, "intro-warriors", 14) == 0 ||
               strncmp(mode_name, "intro-clippers", 14) == 0 ||
               strncmp(mode_name, "intro-bucks", 11) == 0 ||
               strncmp(mode_name, "intro-pass", 10) == 0 ||
               strcmp(mode_name, "play-step9") == 0 ||
               strcmp(mode_name, "play-step10") == 0 ||
               strcmp(mode_name, "play-step11") == 0) {
        TecmoIntroReadyState ready_state;
        TecmoIntroWarriorsState warriors_state;
        TecmoIntroClippersState clippers_state;
        TecmoIntroBucksState bucks_state;
        TecmoIntroPassState pass_state;
        tecmo_intro_ready_state(runtime->mode_frame_counter, &ready_state);
        tecmo_intro_warriors_state(runtime->mode_frame_counter, &warriors_state);
        tecmo_intro_clippers_state(runtime->mode_frame_counter, &clippers_state);
        tecmo_intro_bucks_state(runtime->mode_frame_counter, &bucks_state);
        tecmo_intro_pass_state(runtime->mode_frame_counter, &pass_state);
        printf("intro-post-render-source ready=%u warriors=%u clippers=%u bucks=%u pass=%u chr=%u ready_schema=TRDY-1 warriors_schema=TWAR-1 clippers_schema=TCLP-1 bucks_schema=TBUC-1 pass_schema=TPAS-1\n",
               runtime->intro_ready_asset.available ? 1U : 0U,
               runtime->intro_warriors_asset.available ? 1U : 0U,
               runtime->intro_clippers_asset.available ? 1U : 0U,
               runtime->intro_bucks_asset.available ? 1U : 0U,
               runtime->intro_pass_asset.available ? 1U : 0U,
               runtime->title_chr_bytes != NULL ? 1U : 0U);
        if (strncmp(mode_name, "intro-ready", 11) == 0 || strcmp(mode_name, "play-step9") == 0) {
            printf("intro-ready-state frame=%u palette=%u mask=%u black=%u handoff=%u\n",
                   runtime->mode_frame_counter,
                   (unsigned)ready_state.palette_stage,
                   (unsigned)ready_state.mask_index,
                   ready_state.black ? 1U : 0U,
                   ready_state.handoff ? 1U : 0U);
        } else if (strncmp(mode_name, "intro-warriors", 14) == 0 ||
                   strcmp(mode_name, "play-step10") == 0) {
            printf("intro-warriors-state frame=%u phase=%s palette=%u pan=%u wordmark=%u patches=%u black=%u handoff=%u next_screen=%02X\n",
                   runtime->mode_frame_counter,
                   tecmo_intro_warriors_phase_name(warriors_state.phase),
                   (unsigned)warriors_state.palette_stage,
                   (unsigned)warriors_state.pan,
                   (unsigned)warriors_state.wordmark_glyph_count,
                   (unsigned)warriors_state.patch_count,
                   warriors_state.black ? 1U : 0U,
                   warriors_state.handoff ? 1U : 0U,
                   (unsigned)warriors_state.next_screen);
        } else if (strncmp(mode_name, "intro-clippers", 14) == 0 ||
                   strcmp(mode_name, "play-step11") == 0) {
            printf("intro-clippers-state frame=%u palette=%u motion=%u scroll=%u page=%u wordmark=%u handoff=%u next_route=%04X\n",
                   runtime->mode_frame_counter,
                   (unsigned)clippers_state.palette_stage,
                   (unsigned)clippers_state.motion,
                   (unsigned)clippers_state.scroll_x,
                   (unsigned)clippers_state.pose_page,
                   clippers_state.wordmark_visible ? 1U : 0U,
                   clippers_state.handoff ? 1U : 0U,
                   (unsigned)clippers_state.next_route);
        } else if (strncmp(mode_name, "intro-bucks", 11) == 0) {
            printf("intro-bucks-state frame=%u palette=%u flash=%u scroll=%u wordmark=%u prior=%u black=%u handoff=%u next_route=%04X\n",
                   runtime->mode_frame_counter, (unsigned)bucks_state.palette_stage,
                   (unsigned)bucks_state.flash_pass, (unsigned)bucks_state.scroll_x,
                   (unsigned)bucks_state.wordmark_glyph_count, bucks_state.prior ? 1U : 0U,
                   bucks_state.black ? 1U : 0U, bucks_state.handoff ? 1U : 0U,
                   (unsigned)bucks_state.next_route);
        } else {
            printf("intro-pass-state frame=%u phase=%s palette=%u x=%u scroll=%u first=%u second=%u sprites=%u black=%u handoff=%u next_route=%04X\n",
                   runtime->mode_frame_counter, tecmo_intro_pass_phase_name(pass_state.phase),
                   (unsigned)pass_state.palette_stage, (unsigned)pass_state.player_x,
                   (unsigned)pass_state.scroll_x, (unsigned)pass_state.first_move_count,
                   (unsigned)pass_state.second_move_count, pass_state.sprites_visible ? 1U : 0U,
                   pass_state.black ? 1U : 0U, pass_state.handoff ? 1U : 0U,
                   (unsigned)pass_state.next_route);
        }
    }
}

static bool render_mode_requires_roster_data(const char *mode_name)
{
    return mode_name != NULL &&
           (strcmp(mode_name, "rosters") == 0 ||
            strcmp(mode_name, "play-setup") == 0);
}

static bool parse_render_frame_suffix(const char *mode_name,
                                      const char *prefix,
                                      unsigned *frame)
{
    const char *suffix;
    char *end;
    unsigned long value;
    size_t prefix_length;

    if (mode_name == NULL || prefix == NULL || frame == NULL) {
        return false;
    }
    prefix_length = strlen(prefix);
    if (strncmp(mode_name, prefix, prefix_length) != 0) {
        return false;
    }
    suffix = mode_name + prefix_length;
    if (*suffix < '0' || *suffix > '9') {
        return false;
    }

    errno = 0;
    value = strtoul(suffix, &end, 10);
    if (errno == ERANGE || value > UINT_MAX || *end != '\0') {
        return false;
    }
    *frame = (unsigned)value;
    return true;
}

static bool parse_finale_render_mode(const char *mode_name,
                                     unsigned *frame_out,
                                     bool *debug_out)
{
    static const struct FinaleModePrefix {
        const char *clean_prefix;
        const char *debug_prefix;
        TecmoIntroFinaleScene scene;
    } scene_prefixes[] = {
        {"intro-finale-opening-clean-frame", "intro-finale-opening-frame",
         TECMO_INTRO_FINALE_OPENING_SCREEN},
        {"intro-finale-short-clean-frame", "intro-finale-short-frame",
         TECMO_INTRO_FINALE_SHORT_SPRITE_LOOP},
        {"intro-finale-reverse-clean-frame", "intro-finale-reverse-frame",
         TECMO_INTRO_FINALE_SELECTOR_TRANSITION},
        {"intro-finale-staged-clean-frame", "intro-finale-staged-frame",
         TECMO_INTRO_FINALE_STAGED_GROUP},
        {"intro-finale-title-clean-frame", "intro-finale-title-frame",
         TECMO_INTRO_FINALE_TITLE},
        {"intro-finale-hold-clean-frame", "intro-finale-hold-frame",
         TECMO_INTRO_FINALE_TERMINATOR_HOLD}
    };
    unsigned local_frame;

    if (parse_render_frame_suffix(mode_name, "intro-finale-clean-frame", &local_frame)) {
        *frame_out = local_frame;
        *debug_out = false;
        return true;
    }
    if (parse_render_frame_suffix(mode_name, "intro-finale-frame", &local_frame)) {
        *frame_out = local_frame;
        *debug_out = true;
        return true;
    }
    for (size_t i = 0U; i < sizeof(scene_prefixes) / sizeof(scene_prefixes[0]); ++i) {
        unsigned start = tecmo_intro_finale_scene_start_frame(scene_prefixes[i].scene);
        unsigned duration = tecmo_intro_finale_scene_duration(scene_prefixes[i].scene);
        if (parse_render_frame_suffix(mode_name, scene_prefixes[i].clean_prefix,
                                      &local_frame)) {
            if ((duration != 0U && local_frame >= duration) ||
                local_frame > UINT_MAX - start) return false;
            *frame_out = start + local_frame;
            *debug_out = false;
            return true;
        }
        if (parse_render_frame_suffix(mode_name, scene_prefixes[i].debug_prefix,
                                      &local_frame)) {
            if ((duration != 0U && local_frame >= duration) ||
                local_frame > UINT_MAX - start) return false;
            *frame_out = start + local_frame;
            *debug_out = true;
            return true;
        }
    }
    return false;
}

static bool setup_gameplay_render_checkpoint(TecmoRuntime *runtime,
                                             const char *mode_name)
{
    TecmoGameplaySceneLaunch launch;
    TecmoInput input;
    unsigned checkpoint = 0U;
    unsigned update;
    bool jump = false;
    bool dunk = false;

    if (runtime == NULL || mode_name == NULL) return false;
    if (strcmp(mode_name, "gameplay-start") == 0) {
        checkpoint = 0U;
    } else if (parse_render_frame_suffix(
                   mode_name, "gameplay-jump-frame", &checkpoint)) {
        jump = true;
    } else if (parse_render_frame_suffix(
                   mode_name, "gameplay-dunk-frame", &checkpoint)) {
        dunk = true;
    } else {
        return false;
    }
    if ((jump && (checkpoint == 0U || checkpoint > 40U)) ||
        (dunk && (checkpoint == 0U || checkpoint > 32U))) {
        return false;
    }

    memset(&launch, 0, sizeof(launch));
    launch.source = TECMO_GAMEPLAY_SCENE_PRESEASON;
    launch.away_team = 0U;
    launch.home_team = 1U;
    launch.regulation_minutes = 3U;
    launch.difficulty = 1U;
    launch.control_mode = 1U;
    launch.speed_value = 1U;
    launch.controller_team[0] = TECMO_GAMEPLAY_TEAM_AWAY;
    launch.controller_team[1] = TECMO_GAMEPLAY_SCENE_NO_TEAM;
    launch.game_music_enabled = false;
    if (!tecmo_gameplay_scene_launch(&runtime->gameplay_scene, &launch)) {
        return false;
    }
    tecmo_runtime_set_mode(runtime, TECMO_MODE_COURT);
    if (!jump && !dunk) return true;

    if (dunk) {
        TecmoGameplaySceneActor *actor = &runtime->gameplay_scene.actors[0];
        actor->x = 205;
        actor->y = 160;
        actor->anchor_x = actor->x;
        actor->anchor_y = actor->y;
        actor->facing_right = true;
        runtime->gameplay_scene.ball_holder = 0U;
        runtime->gameplay_scene.ball_x_q8 = (int32_t)(actor->x + 7) * 256;
        runtime->gameplay_scene.ball_y_q8 = (int32_t)(actor->y - 18) * 256;
    }
    memset(&input, 0, sizeof(input));
    input.cancel = true;
    tecmo_runtime_update(runtime, &input);
    memset(&input, 0, sizeof(input));
    for (update = 1U; update < checkpoint; ++update) {
        tecmo_runtime_update(runtime, &input);
    }
    return runtime->mode == TECMO_MODE_COURT &&
           runtime->gameplay_scene.active &&
           runtime->gameplay_scene.shot_kind ==
               (dunk ? TECMO_GAMEPLAY_SCENE_SHOT_DUNK
                     : TECMO_GAMEPLAY_SCENE_SHOT_JUMP);
}

int main(int argc, char **argv)
{
    const char *program = argc > 0 ? argv[0] : "tecmo_port";
    const char *env_root = getenv("TECMO_DECOMP_ROOT");
    const char *root = env_root;
    const char *command = "--summary";
    bool root_from_env = env_root != NULL && env_root[0] != '\0';
    bool root_explicit = false;
    int index = 1;

    if (!root_from_env) {
        root = ".";
    }

    if (index < argc && strcmp(argv[index], "--root") == 0) {
        if (index + 1 >= argc) {
            print_usage(program);
            return 2;
        }
        root = argv[index + 1];
        root_explicit = true;
        index += 2;
    }

    if (index < argc) {
        command = argv[index++];
    }

    if (strcmp(command, "--summary") == 0) {
        tecmo_print_summary(root);
        return 0;
    }

    if (strcmp(command, "--banks") == 0) {
        tecmo_print_banks(root);
        return 0;
    }

    if (strcmp(command, "--chunks") == 0) {
        tecmo_print_chunks(root);
        return 0;
    }

    if (strcmp(command, "--assets") == 0) {
        tecmo_print_assets(root);
        return 0;
    }

    if (strcmp(command, "--roster") == 0) {
        const char *team = index < argc ? argv[index] : "CHICAGO";
        tecmo_print_roster(root, team);
        return 0;
    }

    if (strcmp(command, "--play") == 0) {
#ifdef _WIN32
        return tecmo_run_win32_game(root);
#else
        printf("--play currently has a Win32 backend only. The game core is platform-neutral.\n");
        return 1;
#endif
    }

    if (strcmp(command, "--flow-test") == 0) {
        const size_t permanent_size = 16U * 1024U * 1024U;
        const size_t transient_size = 16U * 1024U * 1024U;
        TecmoGameMemory memory;
        TecmoRuntime *runtime;
        void *permanent_block;
        void *transient_block;
        char message[160];
        int result = 1;

        memset(&memory, 0, sizeof(memory));
        runtime = (TecmoRuntime *)calloc(1U, sizeof(*runtime));
        permanent_block = malloc(permanent_size);
        transient_block = malloc(transient_size);
        if (runtime == NULL || permanent_block == NULL || transient_block == NULL) {
            printf("Failed to allocate flow-test memory.\n");
            free(runtime);
            free(permanent_block);
            free(transient_block);
            return 1;
        }

        tecmo_arena_init(&memory.permanent, permanent_block, permanent_size);
        tecmo_arena_init(&memory.transient, transient_block, transient_size);
        if (!tecmo_runtime_init(runtime, &memory, root)) {
            printf("Failed to initialize runtime from %s\n", root);
        } else if (!tecmo_runtime_flow_self_test(runtime, message, sizeof(message))) {
            printf("Native flow test failed: %s\n", message);
        } else {
            printf("%s\n", message);
            result = 0;
        }

        tecmo_runtime_shutdown(runtime);
        free(runtime);
        free(permanent_block);
        free(transient_block);
        return result;
    }

    if (strcmp(command, "--controls-test") == 0) {
        char message[128];
        if (!tecmo_controls_self_test(message, sizeof(message))) {
            printf("Controls test failed: %s\n", message);
            return 1;
        }
        printf("%s\n", message);
        return 0;
    }

    if (strcmp(command, "--bank07-test") == 0) {
        char message[128];
        if (!tecmo_bank07_self_test(message, sizeof(message))) {
            printf("Bank07 C helper test failed: %s\n", message);
            return 1;
        }
        printf("%s\n", message);
        return 0;
    }

    if (strcmp(command, "--video-test") == 0) {
        char message[160];
        if (!tecmo_nes_video_self_test(message, sizeof(message))) {
            printf("NES video test failed: %s\n", message);
            return 1;
        }
        printf("%s\n", message);
        return 0;
    }

    if (strcmp(command, "--music-test") == 0) {
        char message[384];
        char output_message[64];
        if (!tecmo_music_self_test(root, message, sizeof(message))) {
            printf("Music test failed: %s\n", message);
            return 1;
        }
        if (!tecmo_audio_output_self_test(output_message,
                                          sizeof(output_message))) {
            printf("Music output test failed: %s\n", output_message);
            return 1;
        }
        printf("%s %s\n", message, output_message);
        return 0;
    }

    if (strcmp(command, "--gameplay-audio-test") == 0) {
        char message[512] = {0};
        char output_message[64];
        if (!tecmo_gameplay_audio_self_test(root, message, sizeof(message))) {
            printf("Gameplay audio test failed: %s\n", message);
            return 1;
        }
        if (!tecmo_audio_output_self_test(output_message,
                                          sizeof(output_message))) {
            printf("Gameplay audio output test failed: %s\n",
                   output_message);
            return 1;
        }
        printf("%s\n", message);
        return 0;
    }

    if (strcmp(command, "--team-management-test") == 0) {
        char message[256];
        if (!tecmo_team_management_self_test(root, message, sizeof(message))) {
            printf("TEAM management test failed: %s\n", message);
            return 1;
        }
        printf("%s\n", message);
        return 0;
    }

    if (strcmp(command, "--season-test") == 0) {
        char message[192];
        if (!tecmo_season_self_test(message, sizeof(message))) {
            printf("Season management test failed: %s\n", message);
            return 1;
        }
        printf("%s\n", message);
        return 0;
    }

    if (strcmp(command, "--gameplay-state-test") == 0) {
        char message[192];
        if (!tecmo_gameplay_state_self_test(message, sizeof(message))) {
            printf("Gameplay state test failed: %s\n", message);
            return 1;
        }
        printf("%s\n", message);
        return 0;
    }

    if (strcmp(command, "--gameplay-scene-test") == 0) {
        const char *pack_path = index < argc ? argv[index] : NULL;
        TecmoMusicAsset music_asset;
        TecmoMusicPlayer music_player;
        char message[256];
        bool passed;
        memset(&music_asset, 0, sizeof(music_asset));
        if (pack_path == NULL ||
            !tecmo_music_asset_load_from_pack(&music_asset, pack_path)) {
            printf("Gameplay scene test failed: %s\n",
                   pack_path == NULL ? "PACK path required"
                                     : music_asset.status);
            tecmo_music_asset_shutdown(&music_asset);
            return 1;
        }
        tecmo_music_player_init(&music_player, &music_asset);
        passed = tecmo_gameplay_scene_self_test(
            root, pack_path, &music_player, message, sizeof(message));
        tecmo_music_asset_shutdown(&music_asset);
        if (!passed) {
            printf("Gameplay scene test failed: %s\n", message);
            return 1;
        }
        printf("%s\n", message);
        return 0;
    }

    if (strcmp(command, "--arena-scene-test") == 0) {
        char message[160];
        if (!tecmo_arena_intro_scene_self_test(message, sizeof(message))) {
            printf("Arena intro scene self-test failed: %s\n", message);
            return 1;
        }
        printf("%s\n", message);
        return 0;
    }

    if (strcmp(command, "--assetpack-test") == 0) {
        char message[256];
        if (tecmo_asset_pack_self_test(message, sizeof(message)) != 0) {
            printf("Asset pack self-test failed: %s\n", message);
            return 1;
        }
        printf("%s\n", message);
        return 0;
    }

    if (strcmp(command, "--gameplay-court-test") == 0) {
        const char *pack_path = index < argc ? argv[index] : NULL;
        TecmoGameplayCourt court;
        const uint8_t *nametable;
        const uint8_t *palette;
        size_t nametable_size;
        size_t palette_size;
        tecmo_gameplay_court_init(&court);
        if (tecmo_gameplay_court_nametable(&court, &nametable_size) != NULL ||
            nametable_size != 0U ||
            tecmo_gameplay_court_palette(&court, &palette_size) != NULL ||
            palette_size != 0U || pack_path == NULL ||
            !tecmo_gameplay_court_load(&court, pack_path)) {
            printf("Gameplay court test failed: %s\n",
                   pack_path != NULL ? court.status : "PACK path required");
            tecmo_gameplay_court_destroy(&court);
            return 1;
        }
        if (!tecmo_gameplay_court_load(&court, pack_path)) {
            printf("Gameplay court test failed: reload contract: %s\n",
                   court.status);
            tecmo_gameplay_court_destroy(&court);
            return 1;
        }
        nametable = tecmo_gameplay_court_nametable(
            &court, &nametable_size);
        palette = tecmo_gameplay_court_palette(&court, &palette_size);
        if (nametable == NULL || palette == NULL ||
            nametable_size != TECMO_GAMEPLAY_COURT_NAMETABLE_SIZE ||
            palette_size != TECMO_GAMEPLAY_COURT_PALETTE_SIZE ||
            court.minimum_macro_index != 0U ||
            court.maximum_macro_index != 360U ||
            court.unique_macro_count != 130U ||
            court.nametable_fingerprint != 0x0CF54A0EU ||
            court.palette_fingerprint != 0xB20C1E11U ||
            court.chr_fingerprint32 != 0xF6F6E854U ||
            court.chr_fingerprint64 != 0x96A64F53B240ABB4ULL) {
            printf("Gameplay court test failed: TGCT-1 golden mismatch\n");
            tecmo_gameplay_court_destroy(&court);
            return 1;
        }
        printf("TGCT-1 gameplay court passed: size=%u palette=%u min=%u max=%u unique=%u nametable=%08X palette-fnv=%08X\n",
               (unsigned)nametable_size, (unsigned)palette_size,
               (unsigned)court.minimum_macro_index,
               (unsigned)court.maximum_macro_index,
               (unsigned)court.unique_macro_count,
               court.nametable_fingerprint, court.palette_fingerprint);
        tecmo_gameplay_court_destroy(&court);
        return 0;
    }

    if (strcmp(command, "--gameplay-assets-test") == 0) {
        const char *pack_path = index < argc ? argv[index] : NULL;
        TecmoGameplayAssets assets;
        TecmoGameplayResolvedPose pose;
        TecmoGameplayPoseContext pose_context;
        TecmoGameplayLiveBackgroundContext live_context;
        const TecmoGameplaySourceSpan *foul;
        const TecmoGameplaySourceSpan *halftime;
        unsigned resolved = 0U;
        uint32_t orientation_visual_hash = 2166136261U;
        tecmo_gameplay_assets_init(&assets);
        if (pack_path == NULL ||
            !tecmo_gameplay_assets_load(&assets, pack_path)) {
            printf("Gameplay asset test failed: %s\n",
                   pack_path != NULL ? assets.status : "PACK path required");
            tecmo_gameplay_assets_destroy(&assets);
            return 1;
        }
        if (!tecmo_gameplay_assets_load(&assets, pack_path)) {
            printf("Gameplay asset test failed: reload contract: %s\n",
                   assets.status);
            tecmo_gameplay_assets_destroy(&assets);
            return 1;
        }
        foul = tecmo_gameplay_assets_find_source(
            &assets, TECMO_GAMEPLAY_SOURCE_FOUL_OVERLAY);
        halftime = tecmo_gameplay_assets_find_source(
            &assets, TECMO_GAMEPLAY_SOURCE_HALFTIME_BANNER);
        for (unsigned pointer = 0U;
             pointer < TECMO_GAMEPLAY_ASSET_POINTER_COUNT; ++pointer) {
            pose_context.actor_slot_base =
                (uint8_t)(1U + (pointer % 4U) * 0x40U);
            pose_context.actor_attributes = (uint8_t)(pointer % 4U);
            pose_context.palette_group = (uint8_t)(pointer % 2U);
            pose_context.mmc3_r2_r5[0] = 0x40U;
            pose_context.mmc3_r2_r5[1] = 0x41U;
            pose_context.mmc3_r2_r5[2] = 0x42U;
            pose_context.mmc3_r2_r5[3] = 0x43U;
            if (!tecmo_gameplay_assets_resolve_pose(
                    &assets, (uint16_t)pointer, &pose_context, &pose)) {
                printf("Gameplay asset test failed: pose pointer %u rejected\n",
                       pointer);
                tecmo_gameplay_assets_destroy(&assets);
                return 1;
            }
            ++resolved;
        }
        memset(&pose_context, 0, sizeof(pose_context));
        pose_context.actor_slot_base = 0x01U;
        pose_context.actor_attributes = 0x02U;
        pose_context.palette_group = 0U;
        pose_context.mmc3_r2_r5[0] = 0x40U;
        pose_context.mmc3_r2_r5[1] = 0x41U;
        pose_context.mmc3_r2_r5[2] = 0x42U;
        pose_context.mmc3_r2_r5[3] = 0x43U;
        if (!tecmo_gameplay_assets_resolve_pose(
                &assets, 16U, &pose_context, &pose) ||
            pose.columns != 2U || pose.rows != 3U || pose.piece_count != 5U ||
            pose.pieces[0].dx != -11 || pose.pieces[0].dy != -46 ||
            pose.pieces[0].cell_byte != 0x42U ||
            pose.pieces[0].tile_id != 0x03U ||
            pose.pieces[0].oam_attributes != 0x42U ||
            !pose.pieces[0].flip_horizontal ||
            pose.pieces[0].palette_index != 2U ||
            pose.pieces[0].top_chr_offset != 0x10020U ||
            pose.pieces[0].bottom_chr_offset != 0x10030U ||
            memcmp(pose.pieces[0].top_chr,
                   "\x00\x00\x00\x00\x00\x00\x80\x40"
                   "\x00\x00\x00\x00\x00\x00\x00\x80", 16U) != 0 ||
            memcmp(pose.pieces[0].palette,
                   "\x1B\x01\x26\x36", 4U) != 0) {
            printf("Gameplay asset test failed: $D413 pose golden mismatch\n");
            tecmo_gameplay_assets_destroy(&assets);
            return 1;
        }
        pose_context.actor_slot_base = 0x02U;
        if (tecmo_gameplay_assets_resolve_pose(
                &assets, 16U, &pose_context, &pose)) {
            printf("Gameplay asset test failed: even actor slot accepted\n");
            tecmo_gameplay_assets_destroy(&assets);
            return 1;
        }
        pose_context.actor_slot_base = 0x03U;
        if (tecmo_gameplay_assets_resolve_pose(
                &assets, 16U, &pose_context, &pose)) {
            printf("Gameplay asset test failed: non-ROM odd actor slot accepted\n");
            tecmo_gameplay_assets_destroy(&assets);
            return 1;
        }
        pose_context.actor_slot_base = 0x01U;
        pose_context.actor_attributes = 0x04U;
        if (tecmo_gameplay_assets_resolve_pose(
                &assets, 16U, &pose_context, &pose)) {
            printf("Gameplay asset test failed: invalid actor attributes accepted\n");
            tecmo_gameplay_assets_destroy(&assets);
            return 1;
        }
        if (tecmo_gameplay_assets_build_live_background_context(
                &assets, 0x3FU, &live_context) ||
            tecmo_gameplay_assets_build_live_background_context(
                &assets, 0x5BU, &live_context)) {
            printf("Gameplay asset test failed: invalid live selector accepted\n");
            tecmo_gameplay_assets_destroy(&assets);
            return 1;
        }
        if (tecmo_gameplay_assets_live_band_for_scanline(0U) != 0U ||
            tecmo_gameplay_assets_live_band_for_scanline(31U) != 0U ||
            tecmo_gameplay_assets_live_band_for_scanline(32U) != 1U ||
            tecmo_gameplay_assets_live_band_for_scanline(47U) != 1U ||
            tecmo_gameplay_assets_live_band_for_scanline(48U) != 2U ||
            tecmo_gameplay_assets_live_band_for_scanline(79U) != 2U ||
            tecmo_gameplay_assets_live_band_for_scanline(80U) != 3U ||
            tecmo_gameplay_assets_live_band_for_scanline(127U) != 3U ||
            tecmo_gameplay_assets_live_band_for_scanline(128U) != 4U ||
            tecmo_gameplay_assets_live_band_for_scanline(175U) != 4U ||
            tecmo_gameplay_assets_live_band_for_scanline(176U) != 5U ||
            tecmo_gameplay_assets_live_band_for_scanline(239U) != 5U) {
            printf("Gameplay asset test failed: live band boundaries changed\n");
            tecmo_gameplay_assets_destroy(&assets);
            return 1;
        }
        if (!tecmo_gameplay_assets_build_live_background_context(
                &assets, 0x43U, &live_context)) {
            printf("Gameplay asset test failed: live band context rejected\n");
            tecmo_gameplay_assets_destroy(&assets);
            return 1;
        }
        for (unsigned screen_index = 0U;
             screen_index < TECMO_GAMEPLAY_ASSET_SCREEN_COUNT; ++screen_index) {
          for (unsigned y = 32U; y < 240U; ++y) {
            for (unsigned x = 0U; x < 256U; ++x) {
                TecmoGameplayResolvedOrientationTile tile;
                uint8_t pattern_index;
                unsigned bit = 7U - (x & 7U);
                if (!tecmo_gameplay_assets_resolve_live_orientation_tile(
                        &assets, (uint8_t)screen_index, 0U,
                        (uint8_t)(y / 8U),
                        (uint8_t)(x / 8U), (uint8_t)y,
                        &live_context, &tile)) {
                    printf("Gameplay asset test failed: orientation-base tile rejected\n");
                    tecmo_gameplay_assets_destroy(&assets);
                    return 1;
                }
                pattern_index = (uint8_t)(
                    ((tile.chr[y & 7U] >> bit) & 1U) |
                    (((tile.chr[8U + (y & 7U)] >> bit) & 1U) << 1U));
                orientation_visual_hash ^= tile.palette[pattern_index];
                orientation_visual_hash *= 16777619U;
            }
          }
        }
        if (orientation_visual_hash != 0x37381BBDU) {
            printf("Gameplay asset test failed: orientation-base visual golden mismatch (got %08X)\n",
                   orientation_visual_hash);
            tecmo_gameplay_assets_destroy(&assets);
            return 1;
        }
        if (foul == NULL || foul->cpu_start != 0xB0F8U ||
            foul->cpu_end != 0xB2B0U || halftime == NULL ||
            halftime->cpu_start != 0xBC3CU || halftime->cpu_end != 0xBD10U) {
            printf("Gameplay asset test failed: event provenance missing\n");
            tecmo_gameplay_assets_destroy(&assets);
            return 1;
        }
        printf("TGPL-1 gameplay assets passed: orientation-bases=%u sources=%u pointers=%u chr=%08X base-visual=%08X\n",
               TECMO_GAMEPLAY_ASSET_SCREEN_COUNT,
               TECMO_GAMEPLAY_ASSET_SOURCE_COUNT, resolved,
               assets.chr_fingerprint32, orientation_visual_hash);
        tecmo_gameplay_assets_destroy(&assets);
        return 0;
    }

    if (strcmp(command, "--gameplay-close-shots-test") == 0) {
        const char *pack_path = index < argc ? argv[index] : NULL;
        TecmoGameplayCloseShotAssets assets;
        TecmoGameplayCloseShotVariantInfo variant_info;
        const TecmoGameplayCloseShotSourceSpan *pose_table;
        static const TecmoGameplayCloseShotVariant variants[2] = {
            TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_0,
            TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_2
        };
        uint32_t phase_hash = 2166136261U;
        uint32_t pose_hash = 2166136261U;
        unsigned step_count = 0U;
        unsigned pose_count = 0U;
        uint8_t phase = 0U;
        uint16_t pointer = 0U;
        tecmo_gameplay_close_shots_init(&assets);
        if (tecmo_gameplay_close_shots_get_variant_info(
                &assets, TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_0,
                &variant_info) ||
            tecmo_gameplay_close_shots_phase_for_step(
                &assets, TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_0, 0U, &phase) ||
            tecmo_gameplay_close_shots_resolve_pose_pointer_index(
                &assets, TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_0,
                TECMO_GAMEPLAY_CLOSE_SHOT_PROFILE_0,
                TECMO_GAMEPLAY_CLOSE_SHOT_DIRECTION_0, 0U, &pointer) ||
            tecmo_gameplay_close_shots_find_source(
                &assets,
                TECMO_GAMEPLAY_CLOSE_SHOT_SOURCE_POSE_LOW_HIGH_TABLE) != NULL) {
            printf("Close-shot asset test failed: unavailable helper accepted\n");
            tecmo_gameplay_close_shots_destroy(&assets);
            return 1;
        }
        if (pack_path == NULL ||
            !tecmo_gameplay_close_shots_load(&assets, pack_path)) {
            printf("Close-shot asset test failed: %s\n",
                   pack_path != NULL ? assets.status : "PACK path required");
            tecmo_gameplay_close_shots_destroy(&assets);
            return 1;
        }
        if (!tecmo_gameplay_close_shots_load(&assets, pack_path)) {
            printf("Close-shot asset test failed: reload contract: %s\n",
                   assets.status);
            tecmo_gameplay_close_shots_destroy(&assets);
            return 1;
        }
        pose_table = tecmo_gameplay_close_shots_find_source(
            &assets, TECMO_GAMEPLAY_CLOSE_SHOT_SOURCE_POSE_LOW_HIGH_TABLE);
        if (pose_table == NULL || pose_table->bank != 5U ||
            pose_table->cpu_start != 0x8CEDU ||
            pose_table->cpu_end != 0x8D3CU ||
            pose_table->byte_count != 80U ||
            pose_table->fingerprint != 0x9BFCCE7CU ||
            tecmo_gameplay_close_shots_find_source(
                &assets, (TecmoGameplayCloseShotSourceKind)0) != NULL ||
            tecmo_gameplay_close_shots_find_source(
                &assets, (TecmoGameplayCloseShotSourceKind)14) != NULL) {
            printf("Close-shot asset test failed: source provenance contract\n");
            tecmo_gameplay_close_shots_destroy(&assets);
            return 1;
        }
        if (!tecmo_gameplay_close_shots_get_variant_info(
                &assets, TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_0,
                &variant_info) ||
            variant_info.numeric_variant != 0U ||
            variant_info.semantic_kind !=
                TECMO_GAMEPLAY_CLOSE_SHOT_SEMANTIC_DUNK ||
            variant_info.family_flags !=
                (TECMO_GAMEPLAY_CLOSE_SHOT_FAMILY_DIRECT |
                 TECMO_GAMEPLAY_CLOSE_SHOT_FAMILY_HELD_RELEASE) ||
            variant_info.step_count != 32U ||
            variant_info.pose_phase_count != 7U ||
            !tecmo_gameplay_close_shots_get_variant_info(
                &assets, TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_2,
                &variant_info) ||
            variant_info.numeric_variant != 2U ||
            variant_info.semantic_kind !=
                TECMO_GAMEPLAY_CLOSE_SHOT_SEMANTIC_LAYUP ||
            variant_info.family_flags !=
                (TECMO_GAMEPLAY_CLOSE_SHOT_FAMILY_ARC |
                 TECMO_GAMEPLAY_CLOSE_SHOT_FAMILY_LONGER_TRAJECTORY |
                 TECMO_GAMEPLAY_CLOSE_SHOT_FAMILY_CONTACTABLE) ||
            variant_info.step_count != 16U ||
            variant_info.pose_phase_count != 6U ||
            tecmo_gameplay_close_shots_get_variant_info(
                &assets, (TecmoGameplayCloseShotVariant)1, &variant_info) ||
            tecmo_gameplay_close_shots_get_variant_info(
                &assets, (TecmoGameplayCloseShotVariant)3, &variant_info) ||
            tecmo_gameplay_close_shots_get_variant_info(
                &assets, (TecmoGameplayCloseShotVariant)-1, &variant_info) ||
            tecmo_gameplay_close_shots_get_variant_info(
                &assets, TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_0, NULL)) {
            printf("Close-shot asset test failed: numeric/semantic variant contract\n");
            tecmo_gameplay_close_shots_destroy(&assets);
            return 1;
        }
        for (unsigned variant_index = 0U; variant_index < 2U;
             ++variant_index) {
            unsigned steps = variant_index == 0U ? 32U : 16U;
            unsigned phases = variant_index == 0U ? 7U : 6U;
            for (unsigned step = 0U; step < steps; ++step) {
                if (!tecmo_gameplay_close_shots_phase_for_step(
                        &assets, variants[variant_index], (uint8_t)step,
                        &phase)) {
                    printf("Close-shot asset test failed: phase step %u/%u\n",
                           variant_index, step);
                    tecmo_gameplay_close_shots_destroy(&assets);
                    return 1;
                }
                phase_hash ^= phase;
                phase_hash *= 16777619U;
                ++step_count;
            }
            for (unsigned profile = 0U; profile < 2U; ++profile) {
                for (unsigned direction = 0U; direction < 8U; ++direction) {
                    for (unsigned pose_phase = 0U; pose_phase < phases;
                         ++pose_phase) {
                        if (!tecmo_gameplay_close_shots_resolve_pose_pointer_index(
                                &assets, variants[variant_index],
                                (TecmoGameplayCloseShotProfile)profile,
                                (TecmoGameplayCloseShotDirection)direction,
                                (uint8_t)pose_phase, &pointer)) {
                            printf("Close-shot asset test failed: pose %u/%u/%u/%u\n",
                                   variant_index, profile, direction,
                                   pose_phase);
                            tecmo_gameplay_close_shots_destroy(&assets);
                            return 1;
                        }
                        pose_hash ^= (uint8_t)(pointer & 0xFFU);
                        pose_hash *= 16777619U;
                        pose_hash ^= (uint8_t)(pointer >> 8U);
                        pose_hash *= 16777619U;
                        ++pose_count;
                    }
                }
            }
        }
        if (phase_hash != 0x0445C745U || step_count != 48U ||
            pose_hash != 0xBFDB4095U || pose_count != 208U ||
            !tecmo_gameplay_close_shots_resolve_pose_pointer_index(
                &assets, TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_0,
                TECMO_GAMEPLAY_CLOSE_SHOT_PROFILE_0,
                TECMO_GAMEPLAY_CLOSE_SHOT_DIRECTION_0, 0U, &pointer) ||
            pointer != 637U ||
            !tecmo_gameplay_close_shots_resolve_pose_pointer_index(
                &assets, TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_0,
                TECMO_GAMEPLAY_CLOSE_SHOT_PROFILE_1,
                TECMO_GAMEPLAY_CLOSE_SHOT_DIRECTION_7, 6U, &pointer) ||
            pointer != 664U ||
            !tecmo_gameplay_close_shots_resolve_pose_pointer_index(
                &assets, TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_2,
                TECMO_GAMEPLAY_CLOSE_SHOT_PROFILE_0,
                TECMO_GAMEPLAY_CLOSE_SHOT_DIRECTION_0, 0U, &pointer) ||
            pointer != 807U ||
            !tecmo_gameplay_close_shots_resolve_pose_pointer_index(
                &assets, TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_2,
                TECMO_GAMEPLAY_CLOSE_SHOT_PROFILE_1,
                TECMO_GAMEPLAY_CLOSE_SHOT_DIRECTION_7, 5U, &pointer) ||
            pointer != 830U) {
            printf("Close-shot asset test failed: phase/pose golden contract\n");
            tecmo_gameplay_close_shots_destroy(&assets);
            return 1;
        }
        if (tecmo_gameplay_close_shots_phase_for_step(
                &assets, TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_0, 32U, &phase) ||
            tecmo_gameplay_close_shots_phase_for_step(
                &assets, TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_2, 16U, &phase) ||
            tecmo_gameplay_close_shots_phase_for_step(
                &assets, (TecmoGameplayCloseShotVariant)1, 0U, &phase) ||
            tecmo_gameplay_close_shots_phase_for_step(
                &assets, (TecmoGameplayCloseShotVariant)-1, 0U, &phase) ||
            tecmo_gameplay_close_shots_phase_for_step(
                &assets, TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_0, 0U, NULL) ||
            tecmo_gameplay_close_shots_resolve_pose_pointer_index(
                &assets, TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_0,
                (TecmoGameplayCloseShotProfile)2,
                TECMO_GAMEPLAY_CLOSE_SHOT_DIRECTION_0, 0U, &pointer) ||
            tecmo_gameplay_close_shots_resolve_pose_pointer_index(
                &assets, TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_0,
                (TecmoGameplayCloseShotProfile)-1,
                TECMO_GAMEPLAY_CLOSE_SHOT_DIRECTION_0, 0U, &pointer) ||
            tecmo_gameplay_close_shots_resolve_pose_pointer_index(
                &assets, TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_0,
                TECMO_GAMEPLAY_CLOSE_SHOT_PROFILE_0,
                (TecmoGameplayCloseShotDirection)8, 0U, &pointer) ||
            tecmo_gameplay_close_shots_resolve_pose_pointer_index(
                &assets, TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_0,
                TECMO_GAMEPLAY_CLOSE_SHOT_PROFILE_0,
                (TecmoGameplayCloseShotDirection)-1, 0U, &pointer) ||
            tecmo_gameplay_close_shots_resolve_pose_pointer_index(
                &assets, TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_0,
                TECMO_GAMEPLAY_CLOSE_SHOT_PROFILE_0,
                TECMO_GAMEPLAY_CLOSE_SHOT_DIRECTION_0, 7U, &pointer) ||
            tecmo_gameplay_close_shots_resolve_pose_pointer_index(
                &assets, TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_2,
                TECMO_GAMEPLAY_CLOSE_SHOT_PROFILE_0,
                TECMO_GAMEPLAY_CLOSE_SHOT_DIRECTION_0, 6U, &pointer) ||
            tecmo_gameplay_close_shots_resolve_pose_pointer_index(
                &assets, TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_2,
                TECMO_GAMEPLAY_CLOSE_SHOT_PROFILE_0,
                TECMO_GAMEPLAY_CLOSE_SHOT_DIRECTION_0, 0U, NULL)) {
            printf("Close-shot asset test failed: invalid input accepted\n");
            tecmo_gameplay_close_shots_destroy(&assets);
            return 1;
        }
        printf("TGCS-1 close-shot assets passed: sources=13 variants=2 semantics=0:dunk,2:layup steps=48 poses=208 phases=0445C745 pose-sequence=BFDB4095\n");
        tecmo_gameplay_close_shots_destroy(&assets);
        tecmo_gameplay_close_shots_destroy(&assets);
        return 0;
    }

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
                                                  render_mode_requires_roster_data(mode_name)
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
            if (strcmp(mode_name, "menu") == 0) {
                tecmo_runtime_set_mode(runtime, TECMO_MODE_MAIN_MENU);
            } else if (strcmp(mode_name, "start-game-menu") == 0) {
                tecmo_runtime_set_mode(runtime, TECMO_MODE_START_GAME_MENU);
                runtime->start_game_menu_state.frame = 32U;
                runtime->start_game_menu_state.phase = TECMO_START_GAME_MENU_ROOT;
            } else if (strncmp(mode_name, "start-game-menu-frame", 21) == 0) {
                unsigned frame;
                if (!parse_render_frame_suffix(mode_name, "start-game-menu-frame", &frame) ||
                    frame > 32U) {
                    printf("Unsupported render-test mode: %s\n", mode_name);
                    render_runtime = false;
                } else {
                    tecmo_runtime_set_mode(runtime, TECMO_MODE_START_GAME_MENU);
                    runtime->start_game_menu_state.frame = frame;
                    runtime->start_game_menu_state.phase = frame < 32U
                        ? TECMO_START_GAME_MENU_REVEAL : TECMO_START_GAME_MENU_ROOT;
                }
            } else if (strncmp(mode_name, "start-game-menu-cursor", 22) == 0) {
                unsigned selection;
                if (!parse_render_frame_suffix(mode_name, "start-game-menu-cursor", &selection) ||
                    selection >= TECMO_START_GAME_MENU_ROOT_COUNT) {
                    printf("Unsupported render-test mode: %s\n", mode_name);
                    render_runtime = false;
                } else {
                    tecmo_runtime_set_mode(runtime, TECMO_MODE_START_GAME_MENU);
                    runtime->start_game_menu_state.frame = 32U;
                    runtime->start_game_menu_state.phase = TECMO_START_GAME_MENU_ROOT;
                    runtime->start_game_menu_state.root_selection = (uint8_t)selection;
                }
            } else if (strncmp(mode_name, "start-game-menu-season-frame", 28) == 0) {
                unsigned frame;
                if (!parse_render_frame_suffix(mode_name, "start-game-menu-season-frame", &frame) ||
                    frame > 32U) {
                    printf("Unsupported render-test mode: %s\n", mode_name);
                    render_runtime = false;
                } else {
                    tecmo_runtime_set_mode(runtime, TECMO_MODE_START_GAME_MENU);
                    runtime->start_game_menu_state.frame = 32U + frame;
                    runtime->start_game_menu_state.slide_frame = (uint16_t)frame;
                    runtime->start_game_menu_state.root_selection = 1U;
                    runtime->start_game_menu_state.phase = frame < 32U
                        ? TECMO_START_GAME_MENU_SEASON_SLIDE_IN : TECMO_START_GAME_MENU_SEASON;
                    runtime->start_game_menu_state.cursor_delay = frame < 32U ? 0U :
                        runtime->start_game_menu_asset.cursor_commit_delay_frames;
                    runtime->start_game_menu_state.direction_cooldown = frame < 32U
                        ? runtime->start_game_menu_asset.accepted_input_seed
                        : (uint16_t)(runtime->start_game_menu_asset.accepted_input_seed - 1U);
                }
            } else if (strcmp(mode_name, "start-game-menu-season") == 0) {
                tecmo_runtime_set_mode(runtime, TECMO_MODE_START_GAME_MENU);
                runtime->start_game_menu_state.frame = 64U;
                runtime->start_game_menu_state.slide_frame = 32U;
                runtime->start_game_menu_state.phase = TECMO_START_GAME_MENU_SEASON;
            } else if (strncmp(mode_name, "start-game-menu-music-setup-frame", 33) == 0 ||
                       strncmp(mode_name, "start-game-menu-speed-setup-frame", 33) == 0 ||
                       strncmp(mode_name, "start-game-menu-period-setup-frame", 34) == 0) {
                const char *prefix;
                TecmoStartGameMenuPhase popup_phase;
                size_t overlay_index;
                unsigned frame;
                unsigned setup_frames;
                if (strncmp(mode_name, "start-game-menu-music-setup-frame", 33) == 0) {
                    prefix = "start-game-menu-music-setup-frame";
                    popup_phase = TECMO_START_GAME_MENU_MUSIC;
                    overlay_index = 0U;
                } else if (strncmp(mode_name, "start-game-menu-speed-setup-frame", 33) == 0) {
                    prefix = "start-game-menu-speed-setup-frame";
                    popup_phase = TECMO_START_GAME_MENU_SPEED;
                    overlay_index = 1U;
                } else {
                    prefix = "start-game-menu-period-setup-frame";
                    popup_phase = TECMO_START_GAME_MENU_PERIOD;
                    overlay_index = 2U;
                }
                setup_frames = runtime->start_game_menu_asset.overlays[overlay_index].height *
                               runtime->start_game_menu_asset.popup_row_cadence;
                if (popup_phase == TECMO_START_GAME_MENU_PERIOD)
                    setup_frames += runtime->start_game_menu_asset.period_setup_extra_frames;
                if (!parse_render_frame_suffix(mode_name, prefix, &frame) ||
                    frame > setup_frames) {
                    printf("Unsupported render-test mode: %s\n", mode_name);
                    render_runtime = false;
                } else {
                    tecmo_runtime_set_mode(runtime, TECMO_MODE_START_GAME_MENU);
                    runtime->start_game_menu_state.frame = 32U + frame;
                    runtime->start_game_menu_state.popup_phase = popup_phase;
                    runtime->start_game_menu_state.phase = frame < setup_frames
                        ? TECMO_START_GAME_MENU_POPUP_SETUP : popup_phase;
                    runtime->start_game_menu_state.transition_frame = (uint16_t)frame;
                    runtime->start_game_menu_state.direction_cooldown = frame < setup_frames
                        ? runtime->start_game_menu_asset.accepted_input_seed
                        : (uint16_t)(runtime->start_game_menu_asset.accepted_input_seed - 1U);
                    runtime->start_game_menu_state.cursor_delay = frame < setup_frames ? 0U :
                        runtime->start_game_menu_asset.cursor_commit_delay_frames;
                    runtime->start_game_menu_state.root_selection = popup_phase == TECMO_START_GAME_MENU_MUSIC
                        ? 6U : popup_phase == TECMO_START_GAME_MENU_SPEED ? 4U : 5U;
                    runtime->start_game_menu_state.setting_selection = popup_phase == TECMO_START_GAME_MENU_MUSIC
                        ? runtime->start_game_menu_state.music_value :
                        popup_phase == TECMO_START_GAME_MENU_SPEED
                            ? runtime->start_game_menu_state.speed_value
                            : runtime->start_game_menu_state.period_index;
                }
            } else if (strncmp(mode_name, "start-game-menu-music-teardown-frame", 36) == 0 ||
                       strncmp(mode_name, "start-game-menu-speed-teardown-frame", 36) == 0 ||
                       strncmp(mode_name, "start-game-menu-period-teardown-frame", 37) == 0) {
                const char *prefix;
                TecmoStartGameMenuPhase popup_phase;
                size_t overlay_index;
                unsigned frame;
                unsigned teardown_frames;
                if (strncmp(mode_name, "start-game-menu-music-teardown-frame", 36) == 0) {
                    prefix = "start-game-menu-music-teardown-frame";
                    popup_phase = TECMO_START_GAME_MENU_MUSIC;
                    overlay_index = 0U;
                } else if (strncmp(mode_name, "start-game-menu-speed-teardown-frame", 36) == 0) {
                    prefix = "start-game-menu-speed-teardown-frame";
                    popup_phase = TECMO_START_GAME_MENU_SPEED;
                    overlay_index = 1U;
                } else {
                    prefix = "start-game-menu-period-teardown-frame";
                    popup_phase = TECMO_START_GAME_MENU_PERIOD;
                    overlay_index = 2U;
                }
                teardown_frames = runtime->start_game_menu_asset.overlays[overlay_index].height *
                                  runtime->start_game_menu_asset.popup_row_cadence;
                if (!parse_render_frame_suffix(mode_name, prefix, &frame) ||
                    frame > teardown_frames) {
                    printf("Unsupported render-test mode: %s\n", mode_name);
                    render_runtime = false;
                } else {
                    tecmo_runtime_set_mode(runtime, TECMO_MODE_START_GAME_MENU);
                    runtime->start_game_menu_state.frame = 40U + frame;
                    runtime->start_game_menu_state.popup_phase = popup_phase;
                    runtime->start_game_menu_state.phase = frame < teardown_frames
                        ? TECMO_START_GAME_MENU_POPUP_TEARDOWN : TECMO_START_GAME_MENU_ROOT;
                    runtime->start_game_menu_state.transition_frame = (uint16_t)frame;
                    runtime->start_game_menu_state.direction_cooldown = frame < teardown_frames
                        ? runtime->start_game_menu_asset.accepted_input_seed
                        : (uint16_t)(runtime->start_game_menu_asset.accepted_input_seed - 1U);
                    runtime->start_game_menu_state.cursor_delay = frame < teardown_frames ? 0U :
                        runtime->start_game_menu_asset.cursor_commit_delay_frames;
                    runtime->start_game_menu_state.root_selection = popup_phase == TECMO_START_GAME_MENU_MUSIC
                        ? 6U : popup_phase == TECMO_START_GAME_MENU_SPEED ? 4U : 5U;
                    runtime->start_game_menu_state.setting_selection = popup_phase == TECMO_START_GAME_MENU_MUSIC
                        ? runtime->start_game_menu_state.music_value :
                        popup_phase == TECMO_START_GAME_MENU_SPEED
                            ? runtime->start_game_menu_state.speed_value
                            : runtime->start_game_menu_state.period_index;
                }
            } else if (strncmp(mode_name, "start-game-menu-exit-root-frame", 31) == 0 ||
                       strncmp(mode_name, "start-game-menu-exit-season-frame", 33) == 0) {
                bool from_season = strncmp(mode_name,
                                           "start-game-menu-exit-season-frame", 33) == 0;
                const char *prefix = from_season ? "start-game-menu-exit-season-frame"
                                                 : "start-game-menu-exit-root-frame";
                unsigned frame;
                if (!parse_render_frame_suffix(mode_name, prefix, &frame) ||
                    frame >= runtime->start_game_menu_asset.exit_handoff_frame) {
                    printf("Unsupported render-test mode: %s\n", mode_name);
                    render_runtime = false;
                } else {
                    tecmo_runtime_set_mode(runtime, TECMO_MODE_START_GAME_MENU);
                    runtime->start_game_menu_state.frame = from_season ? 64U + frame : 32U + frame;
                    runtime->start_game_menu_state.phase = TECMO_START_GAME_MENU_EXIT;
                    runtime->start_game_menu_state.transition_frame = (uint16_t)frame;
                    runtime->start_game_menu_state.pending_action = from_season
                        ? TECMO_START_GAME_MENU_ACTION_PLAY_SETUP
                        : TECMO_START_GAME_MENU_ACTION_ROSTERS;
                    runtime->start_game_menu_state.exit_from_season = from_season;
                    runtime->start_game_menu_state.root_selection = from_season ? 1U : 3U;
                    runtime->start_game_menu_state.season_selection = from_season ? 2U : 0U;
                    runtime->start_game_menu_state.direction_cooldown =
                        runtime->start_game_menu_asset.accepted_input_seed;
                    runtime->start_game_menu_state.slide_frame = from_season
                        ? runtime->start_game_menu_asset.slide_frames : 0U;
                }
            } else if (strcmp(mode_name, "start-game-menu-music") == 0 ||
                       strcmp(mode_name, "start-game-menu-speed") == 0 ||
                       strcmp(mode_name, "start-game-menu-period") == 0) {
                tecmo_runtime_set_mode(runtime, TECMO_MODE_START_GAME_MENU);
                runtime->start_game_menu_state.frame = 32U;
                if (strcmp(mode_name, "start-game-menu-music") == 0) {
                    runtime->start_game_menu_state.phase = TECMO_START_GAME_MENU_MUSIC;
                    runtime->start_game_menu_state.setting_selection =
                        runtime->start_game_menu_state.music_value;
                } else if (strcmp(mode_name, "start-game-menu-speed") == 0) {
                    runtime->start_game_menu_state.phase = TECMO_START_GAME_MENU_SPEED;
                    runtime->start_game_menu_state.setting_selection =
                        runtime->start_game_menu_state.speed_value;
                } else {
                    runtime->start_game_menu_state.phase = TECMO_START_GAME_MENU_PERIOD;
                    runtime->start_game_menu_state.setting_selection =
                        runtime->start_game_menu_state.period_index;
                }
            } else if (strcmp(mode_name, "preseason-control") == 0) {
                tecmo_runtime_set_mode(runtime, TECMO_MODE_PRESEASON_MENU);
                runtime->preseason_state.phase = TECMO_PRESEASON_CONTROL;
                runtime->preseason_state.transition_frame =
                    runtime->preseason_asset.overlays[0].height;
            } else if (strncmp(mode_name, "preseason-control-setup-frame", 29) == 0) {
                unsigned frame;
                unsigned overlay_height = runtime->preseason_asset.overlays[0].height;
                if (!parse_render_frame_suffix(mode_name,
                                               "preseason-control-setup-frame",
                                               &frame) || frame > overlay_height) {
                    printf("Unsupported render-test mode: %s\n", mode_name);
                    render_runtime = false;
                } else {
                    tecmo_runtime_set_mode(runtime, TECMO_MODE_PRESEASON_MENU);
                    runtime->preseason_state.phase = frame < overlay_height
                        ? TECMO_PRESEASON_CONTROL_SETUP : TECMO_PRESEASON_CONTROL;
                    runtime->preseason_state.transition_frame = (uint16_t)frame;
                    runtime->preseason_state.cursor_delay = frame < overlay_height ? 0U :
                        runtime->preseason_asset.cursor_commit_delay_frames;
                }
            } else if (strcmp(mode_name, "preseason-difficulty") == 0) {
                tecmo_runtime_set_mode(runtime, TECMO_MODE_PRESEASON_MENU);
                runtime->preseason_state.phase = TECMO_PRESEASON_DIFFICULTY;
                runtime->preseason_state.transition_frame =
                    runtime->preseason_asset.overlays[2].height;
                runtime->preseason_state.difficulty_selection = 1U;
                runtime->preseason_state.committed_difficulty = 1U;
            } else if (strcmp(mode_name, "preseason-division") == 0) {
                tecmo_runtime_set_mode(runtime, TECMO_MODE_PRESEASON_MENU);
                runtime->preseason_state.phase = TECMO_PRESEASON_DIVISION;
                runtime->preseason_state.transition_frame =
                    runtime->preseason_asset.overlays[1].height;
                runtime->preseason_state.control_selection = 2U;
            } else if (strncmp(mode_name, "preseason-team-entry-frame", 26) == 0) {
                unsigned frame;
                unsigned ready = runtime->preseason_asset.team_input_ready_frames;
                if (!parse_render_frame_suffix(mode_name,
                                               "preseason-team-entry-frame",
                                               &frame) || frame > ready) {
                    printf("Unsupported render-test mode: %s\n", mode_name);
                    render_runtime = false;
                } else {
                    tecmo_runtime_set_mode(runtime, TECMO_MODE_PRESEASON_MENU);
                    runtime->preseason_state.phase = frame < ready
                        ? TECMO_PRESEASON_TEAM_ENTRY : TECMO_PRESEASON_TEAM;
                    runtime->preseason_state.transition_frame = (uint16_t)frame;
                    runtime->preseason_state.control_selection = 2U;
                    runtime->preseason_state.team_palette_frame = frame < ready
                        ? 0U : (uint8_t)ready;
                    runtime->preseason_state.cursor_delay = frame < ready ? 0U :
                        runtime->preseason_asset.cursor_commit_delay_frames;
                }
            } else if (strncmp(mode_name, "preseason-p1-team-frame", 23) == 0) {
                unsigned frame;
                if (!parse_render_frame_suffix(mode_name, "preseason-p1-team-frame",
                                               &frame) ||
                    frame < runtime->preseason_asset.team_input_ready_frames ||
                    frame > runtime->preseason_asset.team_palette_full_frames) {
                    printf("Unsupported render-test mode: %s\n", mode_name);
                    render_runtime = false;
                } else {
                    tecmo_runtime_set_mode(runtime, TECMO_MODE_PRESEASON_MENU);
                    runtime->preseason_state.phase = TECMO_PRESEASON_TEAM;
                    runtime->preseason_state.transition_frame =
                        runtime->preseason_asset.team_input_ready_frames;
                    runtime->preseason_state.control_selection = 2U;
                    runtime->preseason_state.team_palette_frame = (uint8_t)frame;
                }
            } else if (strncmp(mode_name, "preseason-team-exit-frame", 25) == 0) {
                unsigned frame;
                if (!parse_render_frame_suffix(mode_name, "preseason-team-exit-frame",
                                               &frame) ||
                    frame > runtime->preseason_asset.team_exit_frames) {
                    printf("Unsupported render-test mode: %s\n", mode_name);
                    render_runtime = false;
                } else {
                    tecmo_runtime_set_mode(runtime, TECMO_MODE_PRESEASON_MENU);
                    runtime->preseason_state.phase = TECMO_PRESEASON_TEAM_EXIT;
                    runtime->preseason_state.transition_frame = (uint16_t)frame;
                    runtime->preseason_state.control_selection = 2U;
                    runtime->preseason_state.team_palette_frame =
                        runtime->preseason_asset.team_palette_full_frames;
                }
            } else if (strncmp(mode_name, "preseason-p2-division-return-frame", 34) == 0) {
                unsigned frame;
                if (!parse_render_frame_suffix(mode_name,
                                               "preseason-p2-division-return-frame",
                                               &frame) ||
                    frame > runtime->preseason_asset.division_return_full_frame) {
                    printf("Unsupported render-test mode: %s\n", mode_name);
                    render_runtime = false;
                } else {
                    tecmo_runtime_set_mode(runtime, TECMO_MODE_PRESEASON_MENU);
                    runtime->preseason_state.phase = TECMO_PRESEASON_DIVISION;
                    runtime->preseason_state.transition_frame =
                        runtime->preseason_asset.overlays[1].height;
                    runtime->preseason_state.control_selection = 2U;
                    runtime->preseason_state.active_player = 1U;
                    runtime->preseason_state.team_selection[1] = 1U;
                    runtime->preseason_state.division_return_fade_frame = (uint8_t)frame;
                    runtime->preseason_state.division_return_fade_active =
                        frame < runtime->preseason_asset.division_return_full_frame;
                }
            } else if (strcmp(mode_name, "preseason-p2-team") == 0 ||
                       strncmp(mode_name, "preseason-p2-team-frame", 23) == 0) {
                unsigned frame = runtime->preseason_asset.team_palette_full_frames;
                if (strcmp(mode_name, "preseason-p2-team") != 0 &&
                    (!parse_render_frame_suffix(mode_name, "preseason-p2-team-frame",
                                                &frame) ||
                     frame < runtime->preseason_asset.team_input_ready_frames ||
                     frame > runtime->preseason_asset.team_palette_full_frames)) {
                    printf("Unsupported render-test mode: %s\n", mode_name);
                    render_runtime = false;
                } else {
                    tecmo_runtime_set_mode(runtime, TECMO_MODE_PRESEASON_MENU);
                    runtime->preseason_state.phase = TECMO_PRESEASON_TEAM;
                    runtime->preseason_state.transition_frame =
                        runtime->preseason_asset.team_input_ready_frames;
                    runtime->preseason_state.control_selection = 2U;
                    runtime->preseason_state.active_player = 1U;
                    runtime->preseason_state.team_selection[0] = 0U;
                    runtime->preseason_state.team_selection[1] = 1U;
                    runtime->preseason_state.team_palette_frame = (uint8_t)frame;
                }
            } else if (strcmp(mode_name, "preseason-invalid-state") == 0) {
                tecmo_runtime_set_mode(runtime, TECMO_MODE_PRESEASON_MENU);
                runtime->preseason_state.active_player = 2U;
                framebuffer.pixels = pixels;
                framebuffer.width = width;
                framebuffer.height = height;
                framebuffer.pitch_pixels = width;
                arena_render_succeeded = tecmo_preseason_draw(
                    &framebuffer, &runtime->preseason_asset, &runtime->preseason_state,
                    &runtime->start_game_menu_asset, runtime->title_chr_bytes,
                    runtime->title_chr_byte_count, 64, 0, 2);
                render_runtime = false;
                result = arena_render_succeeded ? 0 : 1;
            } else if (strcmp(mode_name, "all-star-control") == 0) {
                tecmo_runtime_set_mode(runtime, TECMO_MODE_ALL_STAR_MENU);
                runtime->all_star_state.phase = TECMO_ALL_STAR_CONTROL;
                runtime->all_star_state.transition_frame = 14U;
            } else if (strncmp(mode_name, "all-star-control-setup-frame", 28) == 0) {
                unsigned frame;
                if (!parse_render_frame_suffix(mode_name,
                                               "all-star-control-setup-frame",
                                               &frame) || frame > 14U) {
                    printf("Unsupported render-test mode: %s\n", mode_name);
                    render_runtime = false;
                } else {
                    tecmo_runtime_set_mode(runtime, TECMO_MODE_ALL_STAR_MENU);
                    runtime->all_star_state.phase = frame < 14U
                        ? TECMO_ALL_STAR_CONTROL_SETUP : TECMO_ALL_STAR_CONTROL;
                    runtime->all_star_state.transition_frame = (uint16_t)frame;
                    runtime->all_star_state.cursor_delay = frame < 14U ? 0U :
                        runtime->all_star_asset.cursor_commit_delay_frames;
                }
            } else if (strncmp(mode_name, "all-star-control-teardown-frame", 31) == 0) {
                unsigned frame;
                if (!parse_render_frame_suffix(
                        mode_name, "all-star-control-teardown-frame", &frame) ||
                    frame > 14U) {
                    printf("Unsupported render-test mode: %s\n", mode_name);
                    render_runtime = false;
                } else {
                    tecmo_runtime_set_mode(runtime, TECMO_MODE_ALL_STAR_MENU);
                    runtime->all_star_state.phase = frame < 14U
                        ? TECMO_ALL_STAR_CONTROL_TEARDOWN_ROOT
                        : TECMO_ALL_STAR_CONTROL;
                    runtime->all_star_state.transition_frame = (uint16_t)frame;
                    runtime->all_star_state.cursor_delay = frame < 14U ? 0U :
                        runtime->all_star_asset.cursor_commit_delay_frames;
                }
            } else if (strcmp(mode_name, "all-star-difficulty") == 0) {
                tecmo_runtime_set_mode(runtime, TECMO_MODE_ALL_STAR_MENU);
                runtime->all_star_state.phase = TECMO_ALL_STAR_DIFFICULTY;
                runtime->all_star_state.transition_frame = 8U;
                runtime->all_star_state.difficulty_selection = 1U;
                runtime->all_star_state.committed_difficulty = 1U;
            } else if (strcmp(mode_name, "all-star-team-east") == 0 ||
                       strcmp(mode_name, "all-star-team-west") == 0) {
                tecmo_runtime_set_mode(runtime, TECMO_MODE_ALL_STAR_MENU);
                runtime->all_star_state.phase = TECMO_ALL_STAR_TEAM;
                runtime->all_star_state.transition_frame = 6U;
                runtime->all_star_state.control_selection =
                    strcmp(mode_name, "all-star-team-west") == 0 ? 4U : 1U;
                runtime->all_star_state.team_selection =
                    strcmp(mode_name, "all-star-team-west") == 0 ? 1U : 0U;
            } else if (strcmp(mode_name, "all-star-invalid-state") == 0) {
                tecmo_runtime_set_mode(runtime, TECMO_MODE_ALL_STAR_MENU);
                runtime->all_star_state.control_selection =
                    TECMO_ALL_STAR_CONTROL_COUNT;
                framebuffer.pixels = pixels;
                framebuffer.width = width;
                framebuffer.height = height;
                framebuffer.pitch_pixels = width;
                arena_render_succeeded = tecmo_all_star_draw(
                    &framebuffer, &runtime->all_star_asset,
                    &runtime->all_star_state, &runtime->preseason_asset,
                    &runtime->start_game_menu_asset,
                    runtime->title_chr_bytes, runtime->title_chr_byte_count,
                    64, 0, 2);
                render_runtime = false;
                result = arena_render_succeeded ? 0 : 1;
            } else if (strcmp(mode_name, "team-data-select") == 0) {
                tecmo_runtime_set_mode(runtime, TECMO_MODE_TEAM_DATA);
                runtime->team_data_state.cursor_delay = 1U;
            } else if (strcmp(mode_name, "team-data-profile") == 0) {
                tecmo_runtime_set_mode(runtime, TECMO_MODE_TEAM_DATA);
                runtime->team_data_state.phase = TECMO_TEAM_DATA_PROFILE;
                runtime->team_data_state.team_id = 0U;
                runtime->team_data_state.cursor_delay = 1U;
            } else if (strcmp(mode_name, "team-data-starters") == 0 ||
                       strcmp(mode_name, "team-data-starters-reset") == 0 ||
                       strcmp(mode_name, "team-data-starters-bench") == 0) {
                tecmo_runtime_set_mode(runtime, TECMO_MODE_TEAM_DATA);
                runtime->team_data_state.phase = TECMO_TEAM_DATA_STARTERS;
                runtime->team_data_state.team_id = 0U;
                tecmo_team_management_view_init_starters(
                    &runtime->team_data_state.management_view);
                if (strcmp(mode_name, "team-data-starters-reset") == 0) {
                    runtime->team_data_state.management_view.view =
                        TECMO_TEAM_MANAGEMENT_VIEW_STARTER_RESET;
                } else if (strcmp(mode_name, "team-data-starters-bench") == 0) {
                    runtime->team_data_state.management_view.view =
                        TECMO_TEAM_MANAGEMENT_VIEW_STARTER_BENCH;
                    runtime->team_data_state.management_view.selection = 1U;
                }
            } else if (strcmp(mode_name, "team-data-playbook") == 0 ||
                       strcmp(mode_name, "team-data-playbook-reset") == 0 ||
                       strncmp(mode_name, "team-data-playbook-replace-frame", 32) == 0) {
                unsigned frame = 0U;
                tecmo_runtime_set_mode(runtime, TECMO_MODE_TEAM_DATA);
                runtime->team_data_state.phase = TECMO_TEAM_DATA_PLAYBOOK;
                runtime->team_data_state.team_id = 0U;
                tecmo_team_management_view_init_playbook(
                    &runtime->team_data_state.management_view);
                if (strcmp(mode_name, "team-data-playbook-reset") == 0) {
                    runtime->team_data_state.management_view.view =
                        TECMO_TEAM_MANAGEMENT_VIEW_PLAYBOOK_RESET;
                } else if (strncmp(mode_name,
                                   "team-data-playbook-replace-frame", 32) == 0) {
                    if (!parse_render_frame_suffix(
                            mode_name, "team-data-playbook-replace-frame", &frame) ||
                        frame > runtime->team_management_asset.carousel_frames) {
                        printf("Unsupported render-test mode: %s\n", mode_name);
                        render_runtime = false;
                    } else {
                        runtime->team_data_state.management_view.view =
                            TECMO_TEAM_MANAGEMENT_VIEW_PLAYBOOK_REPLACE;
                        runtime->team_data_state.management_view.carousel_direction =
                            frame < runtime->team_management_asset.carousel_frames
                                ? 1 : 0;
                        runtime->team_data_state.management_view.carousel_frame =
                            (uint8_t)frame;
                        runtime->team_data_state.management_view.carousel_origin =
                            frame == runtime->team_management_asset.carousel_frames
                                ? 1U : 0U;
                    }
                }
            } else if (strcmp(mode_name, "team-data-roster-page1") == 0 ||
                       strcmp(mode_name, "team-data-roster-page2") == 0) {
                tecmo_runtime_set_mode(runtime, TECMO_MODE_TEAM_DATA);
                runtime->team_data_state.phase = TECMO_TEAM_DATA_ROSTER;
                runtime->team_data_state.team_id = 0U;
                runtime->team_data_state.roster_page =
                    strcmp(mode_name, "team-data-roster-page2") == 0 ? 1U : 0U;
                runtime->team_data_state.cursor_delay = 1U;
            } else if (strncmp(mode_name, "team-data-roster-slide-frame", 28) == 0) {
                unsigned frame;
                if (!parse_render_frame_suffix(mode_name,
                                               "team-data-roster-slide-frame",
                                               &frame) ||
                    frame > runtime->team_data_asset.slide_frames) {
                    printf("Unsupported render-test mode: %s\n", mode_name);
                    render_runtime = false;
                } else {
                    tecmo_runtime_set_mode(runtime, TECMO_MODE_TEAM_DATA);
                    runtime->team_data_state.phase = TECMO_TEAM_DATA_ROSTER;
                    runtime->team_data_state.team_id = 0U;
                    runtime->team_data_state.slide_from_page = 0U;
                    runtime->team_data_state.slide_to_page = 1U;
                    runtime->team_data_state.slide_direction = 1;
                    runtime->team_data_state.slide_frame = (uint8_t)frame;
                }
            } else if (strcmp(mode_name, "team-data-player-detail") == 0) {
                tecmo_runtime_set_mode(runtime, TECMO_MODE_TEAM_DATA);
                runtime->team_data_state.phase = TECMO_TEAM_DATA_PLAYER_DETAIL;
                runtime->team_data_state.team_id = 0U;
                runtime->team_data_state.player_index = 0U;
                runtime->team_data_state.cursor_delay = 1U;
            } else if (strncmp(mode_name,
                               "team-data-entry-transition-frame", 32) == 0) {
                unsigned frame;
                if (!parse_render_frame_suffix(
                        mode_name, "team-data-entry-transition-frame", &frame) ||
                    frame >
                        runtime->team_data_asset.entry_transition_stable_frame) {
                    printf("Unsupported render-test mode: %s\n", mode_name);
                    render_runtime = false;
                } else {
                    tecmo_runtime_set_mode(runtime, TECMO_MODE_TEAM_DATA);
                    runtime->team_data_state.transition =
                        TECMO_TEAM_DATA_TRANSITION_ENTRY_TO_SELECTOR;
                    runtime->team_data_state.transition_frame = (uint8_t)frame;
                }
            } else if (strncmp(mode_name,
                               "team-data-selector-profile-transition-frame",
                               43) == 0) {
                unsigned frame;
                if (!parse_render_frame_suffix(
                        mode_name,
                        "team-data-selector-profile-transition-frame",
                        &frame) ||
                    frame >
                        runtime->team_data_asset.selector_transition_stable_frame) {
                    printf("Unsupported render-test mode: %s\n", mode_name);
                    render_runtime = false;
                } else {
                    tecmo_runtime_set_mode(runtime, TECMO_MODE_TEAM_DATA);
                    runtime->team_data_state.phase = TECMO_TEAM_DATA_TEAM_SELECT;
                    runtime->team_data_state.selector_index = 2U;
                    runtime->team_data_state.team_id = 0U;
                    runtime->team_data_state.transition =
                        TECMO_TEAM_DATA_TRANSITION_SELECTOR_TO_PROFILE;
                    runtime->team_data_state.transition_frame = (uint8_t)frame;
                }
            } else if (strncmp(mode_name,
                               "team-data-roster-detail-transition-frame",
                               40) == 0) {
                unsigned frame;
                if (!parse_render_frame_suffix(
                        mode_name,
                        "team-data-roster-detail-transition-frame", &frame) ||
                    frame >
                        runtime->team_data_asset.detail_transition_stable_frame) {
                    printf("Unsupported render-test mode: %s\n", mode_name);
                    render_runtime = false;
                } else {
                    tecmo_runtime_set_mode(runtime, TECMO_MODE_TEAM_DATA);
                    runtime->team_data_state.phase = TECMO_TEAM_DATA_ROSTER;
                    runtime->team_data_state.team_id = 0U;
                    runtime->team_data_state.player_index = 0U;
                    runtime->team_data_state.transition =
                        TECMO_TEAM_DATA_TRANSITION_ROSTER_TO_DETAIL;
                    runtime->team_data_state.transition_frame = (uint8_t)frame;
                }
            } else if (strncmp(mode_name,
                               "team-data-detail-roster-transition-frame",
                               40) == 0) {
                unsigned frame;
                if (!parse_render_frame_suffix(
                        mode_name,
                        "team-data-detail-roster-transition-frame", &frame) ||
                    frame >
                        runtime->team_data_asset.selector_transition_stable_frame) {
                    printf("Unsupported render-test mode: %s\n", mode_name);
                    render_runtime = false;
                } else {
                    tecmo_runtime_set_mode(runtime, TECMO_MODE_TEAM_DATA);
                    runtime->team_data_state.phase =
                        TECMO_TEAM_DATA_PLAYER_DETAIL;
                    runtime->team_data_state.team_id = 0U;
                    runtime->team_data_state.player_index = 0U;
                    runtime->team_data_state.transition =
                        TECMO_TEAM_DATA_TRANSITION_DETAIL_TO_ROSTER;
                    runtime->team_data_state.transition_frame = (uint8_t)frame;
                }
            } else if (strcmp(mode_name, "team-data-invalid-state") == 0) {
                tecmo_runtime_set_mode(runtime, TECMO_MODE_TEAM_DATA);
                runtime->team_data_state.team_id = TECMO_TEAM_DATA_TEAM_COUNT;
                framebuffer.pixels = pixels;
                framebuffer.width = width;
                framebuffer.height = height;
                framebuffer.pitch_pixels = width;
                arena_render_succeeded = tecmo_team_data_draw(
                    &framebuffer, &runtime->team_data_asset,
                    &runtime->team_data_state,
                    &runtime->team_management_asset,
                    &runtime->team_management_session,
                    runtime->title_chr_bytes,
                    runtime->title_chr_byte_count, 64, 0, 2);
                render_runtime = false;
                result = arena_render_succeeded ? 0 : 1;
            } else if (strcmp(mode_name, "season-team-control") == 0) {
                tecmo_runtime_set_mode(runtime, TECMO_MODE_SEASON_MENU);
                tecmo_season_state_init(&runtime->season_state,
                                        TECMO_SEASON_ROUTE_TEAM_CONTROL,
                                        &runtime->season_session);
            } else if (strcmp(mode_name, "season-schedule") == 0 ||
                       strcmp(mode_name, "season-schedule-popup") == 0) {
                tecmo_runtime_set_mode(runtime, TECMO_MODE_SEASON_MENU);
                tecmo_season_state_init(&runtime->season_state,
                                        TECMO_SEASON_ROUTE_SCHEDULE,
                                        &runtime->season_session);
                if (strcmp(mode_name, "season-schedule-popup") == 0)
                    runtime->season_state.phase = TECMO_SEASON_SCHEDULE_POPUP;
                if (strcmp(mode_name, "season-schedule-popup") == 0)
                    runtime->season_state.popup_rows_visible =
                        runtime->season_asset.menu_boxes[0][1];
            } else if (strcmp(mode_name, "season-playoff") == 0 ||
                       strcmp(mode_name, "season-playoff-mid") == 0 ||
                       strcmp(mode_name, "season-playoff-east") == 0) {
                tecmo_runtime_set_mode(runtime, TECMO_MODE_SEASON_MENU);
                tecmo_season_state_init(&runtime->season_state,
                                        TECMO_SEASON_ROUTE_SCHEDULE,
                                        &runtime->season_session);
                runtime->season_state.phase = TECMO_SEASON_PLAYOFF;
                if (strcmp(mode_name, "season-playoff-mid") == 0)
                    runtime->season_state.playoff_scroll = 128U;
                else if (strcmp(mode_name, "season-playoff-east") == 0)
                    runtime->season_state.playoff_scroll = 252U;
            } else if (strcmp(mode_name, "season-standings-east") == 0 ||
                       strcmp(mode_name, "season-standings-west") == 0) {
                tecmo_runtime_set_mode(runtime, TECMO_MODE_SEASON_MENU);
                tecmo_season_state_init(&runtime->season_state,
                                        TECMO_SEASON_ROUTE_STANDINGS,
                                        &runtime->season_session);
                runtime->season_state.standings_page =
                    strcmp(mode_name, "season-standings-west") == 0 ? 1U : 0U;
            } else if (strcmp(mode_name, "season-standings-programmed") == 0) {
                runtime->season_session.season_type = TECMO_SEASON_PROGRAMMED;
                tecmo_runtime_set_mode(runtime, TECMO_MODE_SEASON_MENU);
                tecmo_season_state_init(&runtime->season_state,
                                        TECMO_SEASON_ROUTE_STANDINGS,
                                        &runtime->season_session);
            } else if (strcmp(mode_name, "season-leaders-results") == 0) {
                tecmo_runtime_set_mode(runtime, TECMO_MODE_SEASON_MENU);
                tecmo_season_state_init(&runtime->season_state,
                                        TECMO_SEASON_ROUTE_LEADERS,
                                        &runtime->season_session);
                runtime->season_state.leaders_results = true;
            } else if (strncmp(mode_name, "season-leaders", 14) == 0) {
                unsigned category = 0U;
                if (mode_name[14] != '\0' &&
                    (!parse_render_frame_suffix(mode_name, "season-leaders",
                                                &category) || category >= 7U)) {
                    printf("Unsupported render-test mode: %s\n", mode_name);
                    render_runtime = false;
                } else {
                    tecmo_runtime_set_mode(runtime, TECMO_MODE_SEASON_MENU);
                    tecmo_season_state_init(&runtime->season_state,
                                            TECMO_SEASON_ROUTE_LEADERS,
                                            &runtime->season_session);
                    runtime->season_state.leader_category = (uint8_t)category;
                }
            } else if (strcmp(mode_name, "season-game-start") == 0) {
                TecmoControlFrame no_input;
                tecmo_runtime_set_mode(runtime, TECMO_MODE_SEASON_MENU);
                tecmo_season_state_init(&runtime->season_state,
                                        TECMO_SEASON_ROUTE_GAME_START,
                                        &runtime->season_session);
                memset(&no_input, 0, sizeof(no_input));
                (void)tecmo_season_update(&runtime->season_state,
                                          &runtime->season_asset,
                                          &runtime->season_session,
                                          &no_input);
            } else if (strcmp(mode_name, "season-invalid-state") == 0) {
                tecmo_runtime_set_mode(runtime, TECMO_MODE_SEASON_MENU);
                tecmo_season_state_init(&runtime->season_state,
                                        TECMO_SEASON_ROUTE_TEAM_CONTROL,
                                        &runtime->season_session);
                runtime->season_state.team_selection = TECMO_SEASON_TEAM_COUNT;
                memset(pixels, 0,
                       (size_t)width * (size_t)height * sizeof(*pixels));
                framebuffer.pixels = pixels;
                framebuffer.width = width;
                framebuffer.height = height;
                framebuffer.pitch_pixels = width;
                arena_render_succeeded = tecmo_season_draw(
                    &framebuffer, &runtime->season_asset,
                    &runtime->season_session, &runtime->season_state,
                    &runtime->team_data_asset, runtime->title_chr_bytes,
                    runtime->title_chr_byte_count, 64, 0, 2);
                render_runtime = false;
                result = arena_render_succeeded ? 0 : 1;
            } else if (strncmp(mode_name, "gameplay-", 9) == 0) {
                framebuffer.pixels = pixels;
                framebuffer.width = width;
                framebuffer.height = height;
                framebuffer.pitch_pixels = width;
                if (!setup_gameplay_render_checkpoint(runtime, mode_name)) {
                    printf("Unsupported or unavailable gameplay render-test mode: %s\n",
                           mode_name);
                    result = 1;
                } else {
                    arena_render_succeeded = tecmo_render_gameplay_scene(
                        runtime, &framebuffer);
                    result = arena_render_succeeded ? 0 : 1;
                    printf("gameplay-state frame=%u shot=%s phase=%s score=%u/%u clock=%u:%02u period=%u overtime=%u shot-clock=%u\n",
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
                           (unsigned)runtime->gameplay_scene.state.shot_clock);
                }
                render_runtime = false;
            } else if (strcmp(mode_name, "menu-overlay") == 0) {
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
                if (stage < 0) {
                    stage = 0;
                }
                if (stage > 4) {
                    stage = 4;
                }
                tecmo_runtime_set_mode(runtime, TECMO_MODE_FIRST_SPRITE);
                runtime->mode_frame_counter = (unsigned)stage * 4U;
            } else if (strncmp(mode_name, "play-step", 9) == 0) {
                long step = strtol(mode_name + 9, NULL, 10);
                if (step < 0) {
                    step = 0;
                }
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
            } else if (strcmp(mode_name, "first-sprite") == 0 || strcmp(mode_name, "first-sprite-debug") == 0) {
                framebuffer.pixels = pixels;
                framebuffer.width = width;
                framebuffer.height = height;
                framebuffer.pitch_pixels = width;
                tecmo_render_first_sprite_probe(runtime, &framebuffer);
                render_runtime = false;
                result = 0;
            } else if (strcmp(mode_name, "intro-l88e7-proof") == 0) {
                framebuffer.pixels = pixels;
                framebuffer.width = width;
                framebuffer.height = height;
                framebuffer.pitch_pixels = width;
                tecmo_render_intro_l88e7_proof(runtime, &framebuffer);
                render_runtime = false;
                result = 0;
            } else if (strcmp(mode_name, "intro-license") == 0) {
                framebuffer.pixels = pixels;
                framebuffer.width = width;
                framebuffer.height = height;
                framebuffer.pitch_pixels = width;
                runtime->mode_frame_counter = 48U;
                arena_render_succeeded =
                    tecmo_render_intro_license_screen(runtime, &framebuffer);
                render_runtime = false;
                result = arena_render_succeeded ? 0 : 1;
            } else if (strcmp(mode_name, "intro-arena-transition") == 0) {
                framebuffer.pixels = pixels;
                framebuffer.width = width;
                framebuffer.height = height;
                framebuffer.pitch_pixels = width;
                runtime->debug_overlay = true;
                runtime->mode_frame_counter = 240U;
                arena_render_succeeded = tecmo_render_intro_arena_transition(runtime, &framebuffer);
                render_runtime = false;
                result = arena_render_succeeded ? 0 : 1;
            } else if (strncmp(mode_name, "intro-arena-clean-frame", 23) == 0) {
                unsigned frame;
                if (!parse_render_frame_suffix(mode_name,
                                               "intro-arena-clean-frame",
                                               &frame)) {
                    printf("Unsupported render-test mode: %s\n", mode_name);
                    render_runtime = false;
                } else {
                    framebuffer.pixels = pixels;
                    framebuffer.width = width;
                    framebuffer.height = height;
                    framebuffer.pitch_pixels = width;
                    runtime->debug_overlay = false;
                    runtime->mode_frame_counter = frame;
                    arena_render_succeeded =
                        tecmo_render_intro_arena_transition(runtime, &framebuffer);
                    render_runtime = false;
                    result = arena_render_succeeded ? 0 : 1;
                }
            } else if (strncmp(mode_name, "intro-arena-frame", 17) == 0) {
                long frame = strtol(mode_name + 17, NULL, 10);
                if (frame < 0) {
                    frame = 0;
                }
                framebuffer.pixels = pixels;
                framebuffer.width = width;
                framebuffer.height = height;
                framebuffer.pitch_pixels = width;
                runtime->debug_overlay = true;
                runtime->mode_frame_counter = (unsigned)frame;
                arena_render_succeeded = tecmo_render_intro_arena_transition(runtime, &framebuffer);
                render_runtime = false;
                result = arena_render_succeeded ? 0 : 1;
            } else if (strncmp(mode_name, "intro-ready-clean-frame", 23) == 0 ||
                       strncmp(mode_name, "intro-ready-frame", 17) == 0) {
                const char *prefix = strncmp(mode_name, "intro-ready-clean-frame", 23) == 0
                                         ? "intro-ready-clean-frame"
                                         : "intro-ready-frame";
                unsigned frame;
                if (!parse_render_frame_suffix(mode_name, prefix, &frame)) {
                    printf("Unsupported render-test mode: %s\n", mode_name);
                    render_runtime = false;
                } else {
                framebuffer.pixels = pixels;
                framebuffer.width = width;
                framebuffer.height = height;
                framebuffer.pitch_pixels = width;
                runtime->debug_overlay = strcmp(prefix, "intro-ready-frame") == 0;
                runtime->mode_frame_counter = frame;
                arena_render_succeeded = tecmo_render_intro_ready_screen(runtime, &framebuffer);
                render_runtime = false;
                result = arena_render_succeeded ? 0 : 1;
                }
            } else if (strncmp(mode_name, "intro-warriors-clean-frame", 26) == 0 ||
                       strncmp(mode_name, "intro-warriors-frame", 20) == 0) {
                const char *prefix = strncmp(mode_name, "intro-warriors-clean-frame", 26) == 0
                                         ? "intro-warriors-clean-frame"
                                         : "intro-warriors-frame";
                unsigned frame;
                if (!parse_render_frame_suffix(mode_name, prefix, &frame)) {
                    printf("Unsupported render-test mode: %s\n", mode_name);
                    render_runtime = false;
                } else {
                framebuffer.pixels = pixels;
                framebuffer.width = width;
                framebuffer.height = height;
                framebuffer.pitch_pixels = width;
                runtime->debug_overlay = strcmp(prefix, "intro-warriors-frame") == 0;
                runtime->mode_frame_counter = frame;
                arena_render_succeeded = tecmo_render_intro_warriors_transition(runtime, &framebuffer);
                render_runtime = false;
                result = arena_render_succeeded ? 0 : 1;
                }
            } else if (strncmp(mode_name, "intro-clippers-clean-frame", 26) == 0 ||
                       strncmp(mode_name, "intro-clippers-frame", 20) == 0) {
                const char *prefix = strncmp(mode_name, "intro-clippers-clean-frame", 26) == 0
                                         ? "intro-clippers-clean-frame"
                                         : "intro-clippers-frame";
                unsigned frame;
                if (!parse_render_frame_suffix(mode_name, prefix, &frame)) {
                    printf("Unsupported render-test mode: %s\n", mode_name);
                    render_runtime = false;
                } else {
                framebuffer.pixels = pixels;
                framebuffer.width = width;
                framebuffer.height = height;
                framebuffer.pitch_pixels = width;
                runtime->debug_overlay = strcmp(prefix, "intro-clippers-frame") == 0;
                runtime->mode_frame_counter = frame;
                arena_render_succeeded = tecmo_render_intro_clippers_transition(runtime, &framebuffer);
                render_runtime = false;
                result = arena_render_succeeded ? 0 : 1;
                }
            } else if (strncmp(mode_name, "intro-bucks-clean-frame", 23) == 0 ||
                       strncmp(mode_name, "intro-bucks-frame", 17) == 0) {
                const char *prefix = strncmp(mode_name, "intro-bucks-clean-frame", 23) == 0
                                         ? "intro-bucks-clean-frame"
                                         : "intro-bucks-frame";
                unsigned frame;
                if (!parse_render_frame_suffix(mode_name, prefix, &frame)) {
                    printf("Unsupported render-test mode: %s\n", mode_name);
                    render_runtime = false;
                } else {
                    framebuffer.pixels = pixels;
                    framebuffer.width = width;
                    framebuffer.height = height;
                    framebuffer.pitch_pixels = width;
                    runtime->debug_overlay = strcmp(prefix, "intro-bucks-frame") == 0;
                    runtime->mode_frame_counter = frame;
                    arena_render_succeeded = tecmo_render_intro_bucks_transition(runtime, &framebuffer);
                    render_runtime = false;
                    result = arena_render_succeeded ? 0 : 1;
                }
            } else if (strncmp(mode_name, "intro-pass-clean-frame", 22) == 0 ||
                       strncmp(mode_name, "intro-pass-frame", 16) == 0) {
                const char *prefix = strncmp(mode_name, "intro-pass-clean-frame", 22) == 0
                                         ? "intro-pass-clean-frame"
                                         : "intro-pass-frame";
                unsigned frame;
                if (!parse_render_frame_suffix(mode_name, prefix, &frame)) {
                    printf("Unsupported render-test mode: %s\n", mode_name);
                    render_runtime = false;
                } else {
                    framebuffer.pixels = pixels;
                    framebuffer.width = width;
                    framebuffer.height = height;
                    framebuffer.pitch_pixels = width;
                    runtime->debug_overlay = strcmp(prefix, "intro-pass-frame") == 0;
                    runtime->mode_frame_counter = frame;
                    arena_render_succeeded = tecmo_render_intro_pass_transition(runtime, &framebuffer);
                    render_runtime = false;
                    result = arena_render_succeeded ? 0 : 1;
                }
            } else if (strncmp(mode_name, "intro-finale", 12) == 0) {
                unsigned frame;
                bool debug;
                if (!parse_finale_render_mode(mode_name, &frame, &debug)) {
                    printf("Unsupported render-test mode: %s\n", mode_name);
                    render_runtime = false;
                } else {
                    framebuffer.pixels = pixels;
                    framebuffer.width = width;
                    framebuffer.height = height;
                    framebuffer.pitch_pixels = width;
                    runtime->debug_overlay = debug;
                    runtime->mode_frame_counter = frame;
                    arena_render_succeeded = tecmo_render_intro_finale(runtime, &framebuffer);
                    render_runtime = false;
                    result = arena_render_succeeded ? 0 : 1;
                }
            } else if (strncmp(mode_name, "title-confirm-frame", 19) == 0) {
                unsigned frame;
                if (!parse_render_frame_suffix(mode_name, "title-confirm-frame", &frame) || frame > 126U) {
                    printf("Unsupported render-test mode: %s\n", mode_name);
                    render_runtime = false;
                } else {
                    tecmo_runtime_set_mode(runtime, TECMO_MODE_TITLE_SCREEN);
                    runtime->title_confirming = true;
                    runtime->title_confirmation_frame = frame;
                    runtime->mode_frame_counter = TECMO_TITLE_START_LOAD_FRAMES + frame;
                }
            } else if (strncmp(mode_name, "title-attract-frame", 19) == 0) {
                unsigned frame;
                if (!parse_render_frame_suffix(mode_name, "title-attract-frame", &frame) || frame > 642U) {
                    printf("Unsupported render-test mode: %s\n", mode_name);
                    render_runtime = false;
                } else {
                    tecmo_runtime_set_mode(runtime, TECMO_MODE_FIRST_SPRITE);
                    runtime->intro_output_step = 15U;
                    runtime->mode_frame_counter = frame;
                }
            } else if (strcmp(mode_name, "play-setup") == 0) {
                tecmo_runtime_set_mode(runtime, TECMO_MODE_PLAY_SETUP);
            } else if (strcmp(mode_name, "title-screen") == 0) {
                tecmo_runtime_set_mode(runtime, TECMO_MODE_TITLE_SCREEN);
                runtime->mode_frame_counter = 16U;
            } else if (strcmp(mode_name, "boot-title") == 0) {
                tecmo_runtime_set_mode(runtime, TECMO_MODE_TITLE_SCREEN);
                runtime->mode_frame_counter = 16U;
            } else if (strcmp(mode_name, "intro-presents") == 0) {
                tecmo_runtime_set_mode(runtime, TECMO_MODE_INTRO_PROBE);
            } else if (strcmp(mode_name, "intro-builder-sample") == 0) {
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
                               "SAMPLE RECORD  SPACE ADDS  S SAVES");
            } else if (strcmp(mode_name, "intro-rabbit-preset") == 0) {
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
                tecmo_runtime_set_mode(runtime, TECMO_MODE_INTRO_PROBE);
                runtime->selected_chr_table = 1U;
            } else if (strcmp(mode_name, "chr-playground") == 0) {
                tecmo_runtime_set_mode(runtime, TECMO_MODE_CHR_PLAYGROUND);
            } else if (strcmp(mode_name, "chr-playground-table1") == 0) {
                tecmo_runtime_set_mode(runtime, TECMO_MODE_CHR_PLAYGROUND);
                runtime->selected_chr_table = 1U;
            } else {
                printf("Unsupported render-test mode: %s\n", mode_name);
                render_runtime = false;
            }
            if (render_runtime) {
                framebuffer.pixels = pixels;
                framebuffer.width = width;
                framebuffer.height = height;
                framebuffer.pitch_pixels = width;
                tecmo_runtime_render(runtime, &framebuffer);
                result = 0;
                if ((strncmp(mode_name, "title-confirm-frame", 19) == 0 ||
                     strncmp(mode_name, "title-attract-frame", 19) == 0 ||
                     strcmp(mode_name, "title-screen") == 0 ||
                     strcmp(mode_name, "boot-title") == 0) &&
                    (!runtime->title_asset.attract_available ||
                     !runtime->title_asset.start_available ||
                     !tecmo_title_asset_chr_available(&runtime->title_asset,
                                                       runtime->title_chr_bytes,
                                                       runtime->title_chr_byte_count))) {
                    result = 1;
                } else if (strncmp(mode_name, "start-game-menu", 15) == 0 &&
                           (!runtime->start_game_menu_asset.available ||
                            !tecmo_start_game_menu_asset_chr_available(
                                &runtime->start_game_menu_asset,
                                runtime->title_chr_bytes,
                                runtime->title_chr_byte_count) ||
                            (runtime->start_game_menu_state.frame < 8U &&
                             (!runtime->title_asset.start_available ||
                              !tecmo_title_asset_chr_available(
                                  &runtime->title_asset,
                                  runtime->title_chr_bytes,
                                  runtime->title_chr_byte_count))))) {
                    result = 1;
                } else if (strncmp(mode_name, "preseason", 9) == 0 &&
                           (!runtime->preseason_asset.available ||
                            !runtime->start_game_menu_asset.available ||
                            !tecmo_preseason_asset_chr_available(
                                &runtime->preseason_asset,
                                runtime->title_chr_bytes,
                                runtime->title_chr_byte_count))) {
                    result = 1;
                } else if (strncmp(mode_name, "all-star", 8) == 0 &&
                           (!runtime->all_star_asset.available ||
                            !runtime->preseason_asset.available ||
                            !runtime->start_game_menu_asset.available ||
                            !tecmo_all_star_asset_chr_available(
                                &runtime->all_star_asset,
                                runtime->title_chr_bytes,
                                runtime->title_chr_byte_count))) {
                    result = 1;
                } else if (strncmp(mode_name, "team-data", 9) == 0 &&
                           (!runtime->team_data_asset.available ||
                            !tecmo_team_data_asset_chr_available(
                                &runtime->team_data_asset,
                                runtime->title_chr_bytes,
                                runtime->title_chr_byte_count) ||
                            ((runtime->team_data_state.phase ==
                                  TECMO_TEAM_DATA_STARTERS ||
                              runtime->team_data_state.phase ==
                                  TECMO_TEAM_DATA_PLAYBOOK) &&
                             (!runtime->team_management_asset.available ||
                              !tecmo_team_management_session_valid(
                                  &runtime->team_management_session) ||
                              !tecmo_team_management_asset_chr_available(
                                  &runtime->team_management_asset,
                                  runtime->title_chr_bytes,
                                  runtime->title_chr_byte_count))))) {
                    result = 1;
                } else if (strncmp(mode_name, "season-", 7) == 0 &&
                           (!runtime->season_asset.available ||
                            !runtime->team_data_asset.available ||
                            !tecmo_season_asset_chr_available(
                                &runtime->season_asset,
                                runtime->title_chr_bytes,
                                runtime->title_chr_byte_count))) {
                    result = 1;
                } else if ((strcmp(mode_name, "play") == 0 ||
                     strncmp(mode_name, "play-fade", 9) == 0 ||
                     strcmp(mode_name, "play-step6") == 0) &&
                    (!runtime->intro_presents_asset.available ||
                     !tecmo_intro_screen_chr_available(&runtime->intro_presents_asset,
                                                       runtime->title_chr_bytes,
                                                       runtime->title_chr_byte_count))) {
                    result = 1;
                } else if (strcmp(mode_name, "play-step7") == 0 &&
                           (!runtime->intro_license_asset.available ||
                            !tecmo_intro_screen_chr_available(&runtime->intro_license_asset,
                                                              runtime->title_chr_bytes,
                                                              runtime->title_chr_byte_count))) {
                    result = 1;
                }
            }
            if (strncmp(mode_name, "start-game-menu", 15) == 0) {
                printf("start-game-menu-state frame=%u phase=%s root=%u season=%u slide=%u setting=%u transition=%u rows=%u palette=%u cursor=%u cursor-delay=%u cooldown=%u pending=%u\n",
                       runtime->start_game_menu_state.frame,
                       tecmo_start_game_menu_phase_name(runtime->start_game_menu_state.phase),
                       (unsigned)runtime->start_game_menu_state.root_selection,
                       (unsigned)runtime->start_game_menu_state.season_selection,
                       (unsigned)runtime->start_game_menu_state.slide_frame,
                       (unsigned)runtime->start_game_menu_state.setting_selection,
                       (unsigned)runtime->start_game_menu_state.transition_frame,
                       tecmo_start_game_menu_overlay_visible_rows(
                           &runtime->start_game_menu_asset, &runtime->start_game_menu_state),
                       tecmo_start_game_menu_palette_stage(
                           &runtime->start_game_menu_asset, &runtime->start_game_menu_state),
                       tecmo_start_game_menu_cursor_visible(
                           &runtime->start_game_menu_asset, &runtime->start_game_menu_state) ? 1U : 0U,
                       (unsigned)runtime->start_game_menu_state.cursor_delay,
                       (unsigned)runtime->start_game_menu_state.direction_cooldown,
                       (unsigned)runtime->start_game_menu_state.pending_action);
            }
            if (strncmp(mode_name, "preseason", 9) == 0) {
                printf("preseason-state phase=%s transition=%u control=%u difficulty=%u committed=%u active-player=%u divisions=%u/%u teams=%u/%u palette=%u return-fade=%u/%u cooldown=%u\n",
                       tecmo_preseason_phase_name(runtime->preseason_state.phase),
                       (unsigned)runtime->preseason_state.transition_frame,
                       (unsigned)runtime->preseason_state.control_selection,
                       (unsigned)runtime->preseason_state.difficulty_selection,
                       (unsigned)runtime->preseason_state.committed_difficulty,
                       (unsigned)runtime->preseason_state.active_player,
                       (unsigned)runtime->preseason_state.division_selection[0],
                       (unsigned)runtime->preseason_state.division_selection[1],
                       (unsigned)runtime->preseason_state.team_selection[0],
                       (unsigned)runtime->preseason_state.team_selection[1],
                       (unsigned)runtime->preseason_state.team_palette_frame,
                       runtime->preseason_state.division_return_fade_active ? 1U : 0U,
                       (unsigned)runtime->preseason_state.division_return_fade_frame,
                       (unsigned)runtime->preseason_state.direction_cooldown);
            }
            if (strncmp(mode_name, "all-star", 8) == 0) {
                printf("all-star-state phase=%s transition=%u control=%u difficulty=%u committed=%u team=%u owners=%u/%u teams=%u/%u terminal=%u cooldown=%u rows=%u/%u/%u\n",
                       tecmo_all_star_phase_name(runtime->all_star_state.phase),
                       (unsigned)runtime->all_star_state.transition_frame,
                       (unsigned)runtime->all_star_state.control_selection,
                       (unsigned)runtime->all_star_state.difficulty_selection,
                       (unsigned)runtime->all_star_state.committed_difficulty,
                       (unsigned)runtime->all_star_state.team_selection,
                       (unsigned)runtime->all_star_state.west_owner,
                       (unsigned)runtime->all_star_state.east_owner,
                       (unsigned)runtime->all_star_state.west_team,
                       (unsigned)runtime->all_star_state.east_team,
                       runtime->all_star_state.terminal_commit ? 1U : 0U,
                       (unsigned)runtime->all_star_state.direction_cooldown,
                       tecmo_all_star_overlay_visible_rows(
                           &runtime->all_star_asset, &runtime->all_star_state, 0U),
                       tecmo_all_star_overlay_visible_rows(
                           &runtime->all_star_asset, &runtime->all_star_state, 1U),
                       tecmo_all_star_overlay_visible_rows(
                           &runtime->all_star_asset, &runtime->all_star_state, 2U));
            }
            if (strncmp(mode_name, "team-data", 9) == 0) {
                printf("team-data-state phase=%s selector=%u team=%u profile=%u page=%u row=%u player=%u slide=%u/%u direction=%d cooldown=%u transition=%u transition-frame=%u palette=%u render=%u\n",
                       tecmo_team_data_phase_name(runtime->team_data_state.phase),
                       (unsigned)runtime->team_data_state.selector_index,
                       (unsigned)runtime->team_data_state.team_id,
                       (unsigned)runtime->team_data_state.profile_selection,
                       (unsigned)runtime->team_data_state.roster_page,
                       (unsigned)runtime->team_data_state.roster_row,
                       (unsigned)runtime->team_data_state.player_index,
                       (unsigned)runtime->team_data_state.slide_frame,
                       (unsigned)runtime->team_data_asset.slide_frames,
                       (int)runtime->team_data_state.slide_direction,
                       (unsigned)runtime->team_data_state.direction_cooldown,
                       (unsigned)runtime->team_data_state.transition,
                       (unsigned)runtime->team_data_state.transition_frame,
                       tecmo_team_data_transition_palette_stage(
                           &runtime->team_data_asset,
                           &runtime->team_data_state),
                       tecmo_team_data_transition_render_enabled(
                           &runtime->team_data_asset,
                           &runtime->team_data_state) ? 1U : 0U);
            }
            if (strncmp(mode_name, "season-", 7) == 0) {
                printf("season-state phase=%s type=%s schedule=%u team=%u popup=%u popup-rows=%u playoff-scroll=%u page=%u panel=%u editor-team=%u leader=%u leader-result=%u game-results=%u/%u game-pending=%u launch-blocked=%u save=%u\n",
                       tecmo_season_phase_name(runtime->season_state.phase),
                       tecmo_season_type_name(runtime->season_session.season_type),
                       (unsigned)runtime->season_state.schedule_selection,
                       (unsigned)runtime->season_state.team_selection,
                       (unsigned)runtime->season_state.popup_selection,
                       (unsigned)runtime->season_state.popup_rows_visible,
                       (unsigned)runtime->season_state.playoff_scroll,
                       (unsigned)runtime->season_state.standings_page,
                       (unsigned)runtime->season_state.editor_panel,
                       (unsigned)runtime->season_state.editor_team,
                       (unsigned)runtime->season_state.leader_category,
                       runtime->season_state.leaders_results ? 1U : 0U,
                       (unsigned)runtime->season_state.game_result_visible_rows,
                       (unsigned)(runtime->season_state.game_result_count * 2U),
                       runtime->season_state.game_result_pending ? 1U : 0U,
                       runtime->season_state.game_launch_blocked ? 1U : 0U,
                       (unsigned)runtime->season_session.save_status);
            }
            print_intro_render_capture_status(runtime, mode_name, arena_render_succeeded);
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

    if (strcmp(command, "--generate-rosters") == 0) {
        const char *out_dir = index < argc ? argv[index] : "generated";
        if (tecmo_generate_roster_c(root, out_dir) != 0) {
            printf("Failed to generate roster C files in %s\n", out_dir);
            return 1;
        }
        printf("Generated roster C files in %s\n", out_dir);
        return 0;
    }

    if (strcmp(command, "--build-assetpack") == 0) {
        const char *rom_path;
        const char *out_path;
        char message[256];

        if (index + 1 >= argc) {
            print_usage(program);
            return 2;
        }

        rom_path = argv[index++];
        out_path = argv[index++];
        if (tecmo_asset_pack_build_from_ines(rom_path,
                                             out_path,
                                             message,
                                             sizeof(message)) != 0) {
            printf("Failed to build asset pack: %s\n", message);
            return 1;
        }
        printf("%s\n", message);
        return 0;
    }

    if (strcmp(command, "--assetpack-list") == 0) {
        const char *pack_path;
        char *dump = NULL;
        size_t required_size = 0U;
        int result;

        if (index >= argc) {
            print_usage(program);
            return 2;
        }

        pack_path = argv[index++];
        if (tecmo_asset_pack_dump_directory(pack_path, NULL, 0U, &required_size) != 0 ||
            required_size == 0U) {
            printf("Failed to read asset pack directory from %s\n", pack_path);
            return 1;
        }

        dump = (char *)malloc(required_size);
        if (dump == NULL) {
            printf("Failed to allocate asset pack directory listing.\n");
            return 1;
        }

        result = tecmo_asset_pack_dump_directory(pack_path, dump, required_size, &required_size);
        if (result != 0) {
            printf("Failed to read asset pack directory from %s\n", pack_path);
            free(dump);
            return 1;
        }

        printf("%s", dump);
        free(dump);
        return 0;
    }

    if (strcmp(command, "--export-chr") == 0) {
        uint64_t written = 0;
        if (index >= argc) {
            print_usage(program);
            return 2;
        }
        if (tecmo_export_chr(root, argv[index], &written) != 0) {
            printf("Failed to export CHR to %s\n", argv[index]);
            return 1;
        }
        printf("Exported %llu bytes to %s\n", (unsigned long long)written, argv[index]);
        return 0;
    }

    if (strcmp(command, "--export-chr-png") == 0) {
        uint64_t written = 0;
        const char *out_dir = index < argc ? argv[index] : "build\\chr_png";
        if (tecmo_export_chr_png_sheets(root, out_dir, &written) != 0) {
            printf("Failed to export CHR PNG sheets to %s\n", out_dir);
            return 1;
        }
        printf("Exported %llu CHR PNG sheets to %s\n", (unsigned long long)written, out_dir);
        return 0;
    }

    print_usage(program);
    return 2;
}
