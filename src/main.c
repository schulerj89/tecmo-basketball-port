#include "asm_inventory.h"
#include "png_writer.h"
#include "tecmo_asset_pack.h"
#include "tecmo_bank07.h"
#include "tecmo_game.h"
#include "tecmo_intro_arena_scene.h"

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
    printf("  --arena-scene-test      Run native arena intro scene anchor checks\n");
    printf("  --render-test PATH      Render first playable frame to a PNG\n");
    printf("  --render-test-mode MODE PATH  Render boot-title, native start-game menu, intro scenes (arena through finale/title frameN), play, or probe modes to PNG\n");
    printf("  --generate-rosters DIR  Generate static C roster source/header from Bank 02\n");
    printf("  --build-assetpack ROM PATH  Build a private .assetpack from an iNES ROM only; no decomp/capture imports\n");
    printf("  --assetpack-test       Run asset-pack builder/list/read self-tests\n");
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
        TecmoRuntime runtime;
        void *permanent_block;
        void *transient_block;
        char message[160];
        int result = 1;

        memset(&memory, 0, sizeof(memory));
        permanent_block = malloc(permanent_size);
        transient_block = malloc(transient_size);
        if (permanent_block == NULL || transient_block == NULL) {
            printf("Failed to allocate flow-test memory.\n");
            free(permanent_block);
            free(transient_block);
            return 1;
        }

        tecmo_arena_init(&memory.permanent, permanent_block, permanent_size);
        tecmo_arena_init(&memory.transient, transient_block, transient_size);
        if (!tecmo_runtime_init(&runtime, &memory, root)) {
            printf("Failed to initialize runtime from %s\n", root);
        } else if (!tecmo_runtime_flow_self_test(&runtime, message, sizeof(message))) {
            printf("Native flow test failed: %s\n", message);
            tecmo_runtime_shutdown(&runtime);
        } else {
            printf("%s\n", message);
            tecmo_runtime_shutdown(&runtime);
            result = 0;
        }

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

    if (strcmp(command, "--render-test") == 0 || strcmp(command, "--render-test-mode") == 0) {
        const bool mode_specific = strcmp(command, "--render-test-mode") == 0;
        const char *mode_name = "menu";
        const int width = 640;
        const int height = 480;
        const size_t permanent_size = 16U * 1024U * 1024U;
        const size_t transient_size = 16U * 1024U * 1024U;
        const char *out_path;
        TecmoGameMemory memory;
        TecmoRuntime runtime;
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
        permanent_block = malloc(permanent_size);
        transient_block = malloc(transient_size);
        pixels = (uint32_t *)malloc((size_t)width * (size_t)height * sizeof(uint32_t));
        rgba = (uint8_t *)malloc((size_t)width * (size_t)height * 4U);
        if (permanent_block == NULL || transient_block == NULL || pixels == NULL || rgba == NULL) {
            printf("Failed to allocate render-test memory.\n");
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
        } else if (!tecmo_runtime_init_with_flags(&runtime,
                                                  &memory,
                                                  root,
                                                  render_mode_requires_roster_data(mode_name)
                                                      ? 0U
                                                      : TECMO_RUNTIME_INIT_ALLOW_EMPTY_ROSTER)) {
            printf("Failed to initialize runtime from %s\n", root);
        } else {
            bool render_runtime = true;
            if (strcmp(mode_name, "menu") == 0) {
                tecmo_runtime_set_mode(&runtime, TECMO_MODE_MAIN_MENU);
            } else if (strcmp(mode_name, "start-game-menu") == 0) {
                tecmo_runtime_set_mode(&runtime, TECMO_MODE_START_GAME_MENU);
                runtime.start_game_menu_state.frame = 32U;
                runtime.start_game_menu_state.phase = TECMO_START_GAME_MENU_ROOT;
            } else if (strncmp(mode_name, "start-game-menu-frame", 21) == 0) {
                unsigned frame;
                if (!parse_render_frame_suffix(mode_name, "start-game-menu-frame", &frame) ||
                    frame > 32U) {
                    printf("Unsupported render-test mode: %s\n", mode_name);
                    render_runtime = false;
                } else {
                    tecmo_runtime_set_mode(&runtime, TECMO_MODE_START_GAME_MENU);
                    runtime.start_game_menu_state.frame = frame;
                    runtime.start_game_menu_state.phase = frame < 32U
                        ? TECMO_START_GAME_MENU_REVEAL : TECMO_START_GAME_MENU_ROOT;
                }
            } else if (strncmp(mode_name, "start-game-menu-cursor", 22) == 0) {
                unsigned selection;
                if (!parse_render_frame_suffix(mode_name, "start-game-menu-cursor", &selection) ||
                    selection >= TECMO_START_GAME_MENU_ROOT_COUNT) {
                    printf("Unsupported render-test mode: %s\n", mode_name);
                    render_runtime = false;
                } else {
                    tecmo_runtime_set_mode(&runtime, TECMO_MODE_START_GAME_MENU);
                    runtime.start_game_menu_state.frame = 32U;
                    runtime.start_game_menu_state.phase = TECMO_START_GAME_MENU_ROOT;
                    runtime.start_game_menu_state.root_selection = (uint8_t)selection;
                }
            } else if (strncmp(mode_name, "start-game-menu-season-frame", 28) == 0) {
                unsigned frame;
                if (!parse_render_frame_suffix(mode_name, "start-game-menu-season-frame", &frame) ||
                    frame > 32U) {
                    printf("Unsupported render-test mode: %s\n", mode_name);
                    render_runtime = false;
                } else {
                    tecmo_runtime_set_mode(&runtime, TECMO_MODE_START_GAME_MENU);
                    runtime.start_game_menu_state.frame = 32U + frame;
                    runtime.start_game_menu_state.slide_frame = (uint16_t)frame;
                    runtime.start_game_menu_state.root_selection = 1U;
                    runtime.start_game_menu_state.phase = frame < 32U
                        ? TECMO_START_GAME_MENU_SEASON_SLIDE_IN : TECMO_START_GAME_MENU_SEASON;
                    runtime.start_game_menu_state.cursor_delay = frame < 32U ? 0U :
                        runtime.start_game_menu_asset.cursor_commit_delay_frames;
                    runtime.start_game_menu_state.direction_cooldown = frame < 32U
                        ? runtime.start_game_menu_asset.accepted_input_seed
                        : (uint16_t)(runtime.start_game_menu_asset.accepted_input_seed - 1U);
                }
            } else if (strcmp(mode_name, "start-game-menu-season") == 0) {
                tecmo_runtime_set_mode(&runtime, TECMO_MODE_START_GAME_MENU);
                runtime.start_game_menu_state.frame = 64U;
                runtime.start_game_menu_state.slide_frame = 32U;
                runtime.start_game_menu_state.phase = TECMO_START_GAME_MENU_SEASON;
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
                setup_frames = runtime.start_game_menu_asset.overlays[overlay_index].height *
                               runtime.start_game_menu_asset.popup_row_cadence;
                if (popup_phase == TECMO_START_GAME_MENU_PERIOD)
                    setup_frames += runtime.start_game_menu_asset.period_setup_extra_frames;
                if (!parse_render_frame_suffix(mode_name, prefix, &frame) ||
                    frame > setup_frames) {
                    printf("Unsupported render-test mode: %s\n", mode_name);
                    render_runtime = false;
                } else {
                    tecmo_runtime_set_mode(&runtime, TECMO_MODE_START_GAME_MENU);
                    runtime.start_game_menu_state.frame = 32U + frame;
                    runtime.start_game_menu_state.popup_phase = popup_phase;
                    runtime.start_game_menu_state.phase = frame < setup_frames
                        ? TECMO_START_GAME_MENU_POPUP_SETUP : popup_phase;
                    runtime.start_game_menu_state.transition_frame = (uint16_t)frame;
                    runtime.start_game_menu_state.direction_cooldown = frame < setup_frames
                        ? runtime.start_game_menu_asset.accepted_input_seed
                        : (uint16_t)(runtime.start_game_menu_asset.accepted_input_seed - 1U);
                    runtime.start_game_menu_state.cursor_delay = frame < setup_frames ? 0U :
                        runtime.start_game_menu_asset.cursor_commit_delay_frames;
                    runtime.start_game_menu_state.root_selection = popup_phase == TECMO_START_GAME_MENU_MUSIC
                        ? 6U : popup_phase == TECMO_START_GAME_MENU_SPEED ? 4U : 5U;
                    runtime.start_game_menu_state.setting_selection = popup_phase == TECMO_START_GAME_MENU_MUSIC
                        ? runtime.start_game_menu_state.music_value :
                        popup_phase == TECMO_START_GAME_MENU_SPEED
                            ? runtime.start_game_menu_state.speed_value
                            : runtime.start_game_menu_state.period_index;
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
                teardown_frames = runtime.start_game_menu_asset.overlays[overlay_index].height *
                                  runtime.start_game_menu_asset.popup_row_cadence;
                if (!parse_render_frame_suffix(mode_name, prefix, &frame) ||
                    frame > teardown_frames) {
                    printf("Unsupported render-test mode: %s\n", mode_name);
                    render_runtime = false;
                } else {
                    tecmo_runtime_set_mode(&runtime, TECMO_MODE_START_GAME_MENU);
                    runtime.start_game_menu_state.frame = 40U + frame;
                    runtime.start_game_menu_state.popup_phase = popup_phase;
                    runtime.start_game_menu_state.phase = frame < teardown_frames
                        ? TECMO_START_GAME_MENU_POPUP_TEARDOWN : TECMO_START_GAME_MENU_ROOT;
                    runtime.start_game_menu_state.transition_frame = (uint16_t)frame;
                    runtime.start_game_menu_state.direction_cooldown = frame < teardown_frames
                        ? runtime.start_game_menu_asset.accepted_input_seed
                        : (uint16_t)(runtime.start_game_menu_asset.accepted_input_seed - 1U);
                    runtime.start_game_menu_state.cursor_delay = frame < teardown_frames ? 0U :
                        runtime.start_game_menu_asset.cursor_commit_delay_frames;
                    runtime.start_game_menu_state.root_selection = popup_phase == TECMO_START_GAME_MENU_MUSIC
                        ? 6U : popup_phase == TECMO_START_GAME_MENU_SPEED ? 4U : 5U;
                    runtime.start_game_menu_state.setting_selection = popup_phase == TECMO_START_GAME_MENU_MUSIC
                        ? runtime.start_game_menu_state.music_value :
                        popup_phase == TECMO_START_GAME_MENU_SPEED
                            ? runtime.start_game_menu_state.speed_value
                            : runtime.start_game_menu_state.period_index;
                }
            } else if (strncmp(mode_name, "start-game-menu-exit-root-frame", 31) == 0 ||
                       strncmp(mode_name, "start-game-menu-exit-season-frame", 33) == 0) {
                bool from_season = strncmp(mode_name,
                                           "start-game-menu-exit-season-frame", 33) == 0;
                const char *prefix = from_season ? "start-game-menu-exit-season-frame"
                                                 : "start-game-menu-exit-root-frame";
                unsigned frame;
                if (!parse_render_frame_suffix(mode_name, prefix, &frame) ||
                    frame >= runtime.start_game_menu_asset.exit_handoff_frame) {
                    printf("Unsupported render-test mode: %s\n", mode_name);
                    render_runtime = false;
                } else {
                    tecmo_runtime_set_mode(&runtime, TECMO_MODE_START_GAME_MENU);
                    runtime.start_game_menu_state.frame = from_season ? 64U + frame : 32U + frame;
                    runtime.start_game_menu_state.phase = TECMO_START_GAME_MENU_EXIT;
                    runtime.start_game_menu_state.transition_frame = (uint16_t)frame;
                    runtime.start_game_menu_state.pending_action = from_season
                        ? TECMO_START_GAME_MENU_ACTION_PLAY_SETUP
                        : TECMO_START_GAME_MENU_ACTION_ROSTERS;
                    runtime.start_game_menu_state.exit_from_season = from_season;
                    runtime.start_game_menu_state.root_selection = from_season ? 1U : 3U;
                    runtime.start_game_menu_state.season_selection = from_season ? 2U : 0U;
                    runtime.start_game_menu_state.direction_cooldown =
                        runtime.start_game_menu_asset.accepted_input_seed;
                    runtime.start_game_menu_state.slide_frame = from_season
                        ? runtime.start_game_menu_asset.slide_frames : 0U;
                }
            } else if (strcmp(mode_name, "start-game-menu-music") == 0 ||
                       strcmp(mode_name, "start-game-menu-speed") == 0 ||
                       strcmp(mode_name, "start-game-menu-period") == 0) {
                tecmo_runtime_set_mode(&runtime, TECMO_MODE_START_GAME_MENU);
                runtime.start_game_menu_state.frame = 32U;
                if (strcmp(mode_name, "start-game-menu-music") == 0) {
                    runtime.start_game_menu_state.phase = TECMO_START_GAME_MENU_MUSIC;
                    runtime.start_game_menu_state.setting_selection =
                        runtime.start_game_menu_state.music_value;
                } else if (strcmp(mode_name, "start-game-menu-speed") == 0) {
                    runtime.start_game_menu_state.phase = TECMO_START_GAME_MENU_SPEED;
                    runtime.start_game_menu_state.setting_selection =
                        runtime.start_game_menu_state.speed_value;
                } else {
                    runtime.start_game_menu_state.phase = TECMO_START_GAME_MENU_PERIOD;
                    runtime.start_game_menu_state.setting_selection =
                        runtime.start_game_menu_state.period_index;
                }
            } else if (strcmp(mode_name, "menu-overlay") == 0) {
                TecmoInput input;
                memset(&input, 0, sizeof(input));
                tecmo_runtime_set_mode(&runtime, TECMO_MODE_MAIN_MENU);
                runtime.debug_overlay = true;
                runtime.frame_seconds = 1.0f / 60.0f;
                tecmo_runtime_update(&runtime, &input);
            } else if (strcmp(mode_name, "rosters") == 0) {
                tecmo_runtime_set_mode(&runtime, TECMO_MODE_ROSTERS);
            } else if (strcmp(mode_name, "play") == 0) {
                tecmo_runtime_set_mode(&runtime, TECMO_MODE_FIRST_SPRITE);
                runtime.mode_frame_counter = 16U;
            } else if (strncmp(mode_name, "play-fade", 9) == 0) {
                long stage = strtol(mode_name + 9, NULL, 10);
                if (stage < 0) {
                    stage = 0;
                }
                if (stage > 4) {
                    stage = 4;
                }
                tecmo_runtime_set_mode(&runtime, TECMO_MODE_FIRST_SPRITE);
                runtime.mode_frame_counter = (unsigned)stage * 4U;
            } else if (strncmp(mode_name, "play-step", 9) == 0) {
                long step = strtol(mode_name + 9, NULL, 10);
                if (step < 0) {
                    step = 0;
                }
                tecmo_runtime_set_mode(&runtime, TECMO_MODE_FIRST_SPRITE);
                runtime.intro_output_step = (uint8_t)step;
                if (step == 8) {
                    runtime.mode_frame_counter = 320U;
                } else if (step == 7) {
                    runtime.mode_frame_counter = 48U;
                } else if (step == 9) {
                    runtime.mode_frame_counter = 35U;
                } else if (step >= 10) {
                    runtime.mode_frame_counter = 28U;
                } else {
                    runtime.mode_frame_counter = 16U;
                }
            } else if (strcmp(mode_name, "first-sprite") == 0 || strcmp(mode_name, "first-sprite-debug") == 0) {
                framebuffer.pixels = pixels;
                framebuffer.width = width;
                framebuffer.height = height;
                framebuffer.pitch_pixels = width;
                tecmo_render_first_sprite_probe(&runtime, &framebuffer);
                render_runtime = false;
                result = 0;
            } else if (strcmp(mode_name, "intro-l88e7-proof") == 0) {
                framebuffer.pixels = pixels;
                framebuffer.width = width;
                framebuffer.height = height;
                framebuffer.pitch_pixels = width;
                tecmo_render_intro_l88e7_proof(&runtime, &framebuffer);
                render_runtime = false;
                result = 0;
            } else if (strcmp(mode_name, "intro-license") == 0) {
                framebuffer.pixels = pixels;
                framebuffer.width = width;
                framebuffer.height = height;
                framebuffer.pitch_pixels = width;
                runtime.mode_frame_counter = 48U;
                arena_render_succeeded =
                    tecmo_render_intro_license_screen(&runtime, &framebuffer);
                render_runtime = false;
                result = arena_render_succeeded ? 0 : 1;
            } else if (strcmp(mode_name, "intro-arena-transition") == 0) {
                framebuffer.pixels = pixels;
                framebuffer.width = width;
                framebuffer.height = height;
                framebuffer.pitch_pixels = width;
                runtime.debug_overlay = true;
                runtime.mode_frame_counter = 240U;
                arena_render_succeeded = tecmo_render_intro_arena_transition(&runtime, &framebuffer);
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
                    runtime.debug_overlay = false;
                    runtime.mode_frame_counter = frame;
                    arena_render_succeeded =
                        tecmo_render_intro_arena_transition(&runtime, &framebuffer);
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
                runtime.debug_overlay = true;
                runtime.mode_frame_counter = (unsigned)frame;
                arena_render_succeeded = tecmo_render_intro_arena_transition(&runtime, &framebuffer);
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
                runtime.debug_overlay = strcmp(prefix, "intro-ready-frame") == 0;
                runtime.mode_frame_counter = frame;
                arena_render_succeeded = tecmo_render_intro_ready_screen(&runtime, &framebuffer);
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
                runtime.debug_overlay = strcmp(prefix, "intro-warriors-frame") == 0;
                runtime.mode_frame_counter = frame;
                arena_render_succeeded = tecmo_render_intro_warriors_transition(&runtime, &framebuffer);
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
                runtime.debug_overlay = strcmp(prefix, "intro-clippers-frame") == 0;
                runtime.mode_frame_counter = frame;
                arena_render_succeeded = tecmo_render_intro_clippers_transition(&runtime, &framebuffer);
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
                    runtime.debug_overlay = strcmp(prefix, "intro-bucks-frame") == 0;
                    runtime.mode_frame_counter = frame;
                    arena_render_succeeded = tecmo_render_intro_bucks_transition(&runtime, &framebuffer);
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
                    runtime.debug_overlay = strcmp(prefix, "intro-pass-frame") == 0;
                    runtime.mode_frame_counter = frame;
                    arena_render_succeeded = tecmo_render_intro_pass_transition(&runtime, &framebuffer);
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
                    runtime.debug_overlay = debug;
                    runtime.mode_frame_counter = frame;
                    arena_render_succeeded = tecmo_render_intro_finale(&runtime, &framebuffer);
                    render_runtime = false;
                    result = arena_render_succeeded ? 0 : 1;
                }
            } else if (strncmp(mode_name, "title-confirm-frame", 19) == 0) {
                unsigned frame;
                if (!parse_render_frame_suffix(mode_name, "title-confirm-frame", &frame) || frame > 126U) {
                    printf("Unsupported render-test mode: %s\n", mode_name);
                    render_runtime = false;
                } else {
                    tecmo_runtime_set_mode(&runtime, TECMO_MODE_TITLE_SCREEN);
                    runtime.title_confirming = true;
                    runtime.title_confirmation_frame = frame;
                    runtime.mode_frame_counter = TECMO_TITLE_START_LOAD_FRAMES + frame;
                }
            } else if (strncmp(mode_name, "title-attract-frame", 19) == 0) {
                unsigned frame;
                if (!parse_render_frame_suffix(mode_name, "title-attract-frame", &frame) || frame > 642U) {
                    printf("Unsupported render-test mode: %s\n", mode_name);
                    render_runtime = false;
                } else {
                    tecmo_runtime_set_mode(&runtime, TECMO_MODE_FIRST_SPRITE);
                    runtime.intro_output_step = 15U;
                    runtime.mode_frame_counter = frame;
                }
            } else if (strcmp(mode_name, "play-setup") == 0) {
                tecmo_runtime_set_mode(&runtime, TECMO_MODE_PLAY_SETUP);
            } else if (strcmp(mode_name, "title-screen") == 0) {
                tecmo_runtime_set_mode(&runtime, TECMO_MODE_TITLE_SCREEN);
                runtime.mode_frame_counter = 16U;
            } else if (strcmp(mode_name, "boot-title") == 0) {
                tecmo_runtime_set_mode(&runtime, TECMO_MODE_TITLE_SCREEN);
                runtime.mode_frame_counter = 16U;
            } else if (strcmp(mode_name, "intro-presents") == 0) {
                tecmo_runtime_set_mode(&runtime, TECMO_MODE_INTRO_PROBE);
            } else if (strcmp(mode_name, "intro-builder-sample") == 0) {
                TecmoIntroPlacement *placement;
                tecmo_runtime_set_mode(&runtime, TECMO_MODE_INTRO_PROBE);
                runtime.selected_chr_table = 1U;
                runtime.intro_source_tile = 0xB6U;
                runtime.intro_canvas_focus = true;
                runtime.intro_canvas_cell_x = 5;
                runtime.intro_canvas_cell_y = 5;
                placement = &runtime.intro_placements[0];
                memset(placement, 0, sizeof(*placement));
                placement->active = true;
                placement->chr_bank = runtime.selected_chr_bank;
                placement->chr_table = runtime.selected_chr_table;
                placement->tile_ids[0] = 0x1B6U;
                placement->tile_count = 1;
                placement->canvas_cell_x = runtime.intro_canvas_cell_x;
                placement->canvas_cell_y = runtime.intro_canvas_cell_y;
                placement->pixel_x = placement->canvas_cell_x * 16;
                placement->pixel_y = placement->canvas_cell_y * 16;
                placement->scale = 2;
                (void)snprintf(placement->label, sizeof(placement->label), "B31 T1 1B6");
                runtime.intro_placement_count = 1;
                (void)snprintf(runtime.intro_layout_status,
                               sizeof(runtime.intro_layout_status),
                               "SAMPLE RECORD  SPACE ADDS  S SAVES");
            } else if (strcmp(mode_name, "intro-rabbit-preset") == 0) {
                TecmoInput input;
                tecmo_runtime_set_mode(&runtime, TECMO_MODE_INTRO_PROBE);
                runtime.selected_chr_table = 1U;
                runtime.intro_source_tile = 0x25U;
                runtime.intro_canvas_focus = true;
                runtime.intro_canvas_cell_x = 5;
                runtime.intro_canvas_cell_y = 5;
                memset(&input, 0, sizeof(input));
                input.preset_rabbit = true;
                tecmo_runtime_update(&runtime, &input);
                {
                    TecmoInput released_input;
                    memset(&released_input, 0, sizeof(released_input));
                    tecmo_runtime_update(&runtime, &released_input);
                }
            } else if (strcmp(mode_name, "intro-tecmo-preset") == 0) {
                TecmoInput input;
                tecmo_runtime_set_mode(&runtime, TECMO_MODE_INTRO_PROBE);
                runtime.selected_chr_table = 1U;
                runtime.intro_source_tile = 0x80U;
                runtime.intro_canvas_focus = true;
                runtime.intro_canvas_cell_x = 4;
                runtime.intro_canvas_cell_y = 5;
                memset(&input, 0, sizeof(input));
                input.preset_tecmo = true;
                tecmo_runtime_update(&runtime, &input);
                {
                    TecmoInput released_input;
                    memset(&released_input, 0, sizeof(released_input));
                    tecmo_runtime_update(&runtime, &released_input);
                }
            } else if (strcmp(mode_name, "intro-composite-preset") == 0) {
                TecmoInput input;
                tecmo_runtime_set_mode(&runtime, TECMO_MODE_INTRO_PROBE);
                runtime.selected_chr_table = 1U;
                runtime.intro_source_tile = 0x80U;
                runtime.intro_canvas_focus = true;
                memset(&input, 0, sizeof(input));
                input.preset_composite = true;
                tecmo_runtime_update(&runtime, &input);
                {
                    TecmoInput released_input;
                    memset(&released_input, 0, sizeof(released_input));
                    tecmo_runtime_update(&runtime, &released_input);
                }
            } else if (strcmp(mode_name, "intro-presents-table1") == 0) {
                tecmo_runtime_set_mode(&runtime, TECMO_MODE_INTRO_PROBE);
                runtime.selected_chr_table = 1U;
            } else if (strcmp(mode_name, "chr-playground") == 0) {
                tecmo_runtime_set_mode(&runtime, TECMO_MODE_CHR_PLAYGROUND);
            } else if (strcmp(mode_name, "chr-playground-table1") == 0) {
                tecmo_runtime_set_mode(&runtime, TECMO_MODE_CHR_PLAYGROUND);
                runtime.selected_chr_table = 1U;
            } else {
                printf("Unsupported render-test mode: %s\n", mode_name);
                render_runtime = false;
            }
            if (render_runtime) {
                framebuffer.pixels = pixels;
                framebuffer.width = width;
                framebuffer.height = height;
                framebuffer.pitch_pixels = width;
                tecmo_runtime_render(&runtime, &framebuffer);
                result = 0;
                if ((strncmp(mode_name, "title-confirm-frame", 19) == 0 ||
                     strncmp(mode_name, "title-attract-frame", 19) == 0 ||
                     strcmp(mode_name, "title-screen") == 0 ||
                     strcmp(mode_name, "boot-title") == 0) &&
                    (!runtime.title_asset.attract_available ||
                     !runtime.title_asset.start_available ||
                     !tecmo_title_asset_chr_available(&runtime.title_asset,
                                                       runtime.title_chr_bytes,
                                                       runtime.title_chr_byte_count))) {
                    result = 1;
                } else if (strncmp(mode_name, "start-game-menu", 15) == 0 &&
                           (!runtime.start_game_menu_asset.available ||
                            !tecmo_start_game_menu_asset_chr_available(
                                &runtime.start_game_menu_asset,
                                runtime.title_chr_bytes,
                                runtime.title_chr_byte_count) ||
                            (runtime.start_game_menu_state.frame < 8U &&
                             (!runtime.title_asset.start_available ||
                              !tecmo_title_asset_chr_available(
                                  &runtime.title_asset,
                                  runtime.title_chr_bytes,
                                  runtime.title_chr_byte_count))))) {
                    result = 1;
                } else if ((strcmp(mode_name, "play") == 0 ||
                     strncmp(mode_name, "play-fade", 9) == 0 ||
                     strcmp(mode_name, "play-step6") == 0) &&
                    (!runtime.intro_presents_asset.available ||
                     !tecmo_intro_screen_chr_available(&runtime.intro_presents_asset,
                                                       runtime.title_chr_bytes,
                                                       runtime.title_chr_byte_count))) {
                    result = 1;
                } else if (strcmp(mode_name, "play-step7") == 0 &&
                           (!runtime.intro_license_asset.available ||
                            !tecmo_intro_screen_chr_available(&runtime.intro_license_asset,
                                                              runtime.title_chr_bytes,
                                                              runtime.title_chr_byte_count))) {
                    result = 1;
                }
            }
            if (strncmp(mode_name, "start-game-menu", 15) == 0) {
                printf("start-game-menu-state frame=%u phase=%s root=%u season=%u slide=%u setting=%u transition=%u rows=%u palette=%u cursor=%u cursor-delay=%u cooldown=%u pending=%u\n",
                       runtime.start_game_menu_state.frame,
                       tecmo_start_game_menu_phase_name(runtime.start_game_menu_state.phase),
                       (unsigned)runtime.start_game_menu_state.root_selection,
                       (unsigned)runtime.start_game_menu_state.season_selection,
                       (unsigned)runtime.start_game_menu_state.slide_frame,
                       (unsigned)runtime.start_game_menu_state.setting_selection,
                       (unsigned)runtime.start_game_menu_state.transition_frame,
                       tecmo_start_game_menu_overlay_visible_rows(
                           &runtime.start_game_menu_asset, &runtime.start_game_menu_state),
                       tecmo_start_game_menu_palette_stage(
                           &runtime.start_game_menu_asset, &runtime.start_game_menu_state),
                       tecmo_start_game_menu_cursor_visible(
                           &runtime.start_game_menu_asset, &runtime.start_game_menu_state) ? 1U : 0U,
                       (unsigned)runtime.start_game_menu_state.cursor_delay,
                       (unsigned)runtime.start_game_menu_state.direction_cooldown,
                       (unsigned)runtime.start_game_menu_state.pending_action);
            }
            print_intro_render_capture_status(&runtime, mode_name, arena_render_succeeded);
            tecmo_runtime_shutdown(&runtime);
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
