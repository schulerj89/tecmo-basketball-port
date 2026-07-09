#include "asm_inventory.h"
#include "png_writer.h"
#include "tecmo_asset_pack.h"
#include "tecmo_bank07.h"
#include "tecmo_game.h"

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
    printf("  --render-test PATH      Render first playable frame to a PNG\n");
    printf("  --render-test-mode MODE PATH  Render boot-title, menu, menu-overlay, title-screen, first-sprite, first-sprite-debug, intro-license, intro-arena-transition, intro-arena-frameN, intro-ready-frameN, intro-warriors-frameN, intro-l88e7-proof, intro-presents, intro-builder-sample, intro-rabbit-preset, intro-tecmo-preset, intro-composite-preset, intro-c051-d861-model, intro-presents-table1, chr-playground, chr-playground-table1, rosters, play, play-fade0..play-fade4, play-step0..play-step10, play-setup, original-title, or original-title-chr to PNG\n");
    printf("  --generate-rosters DIR  Generate static C roster source/header from Bank 02\n");
    printf("  --build-assetpack ROM PATH  Extract local iNES PRG/CHR data and metadata to a private .assetpack\n");
    printf("  --assetpack-test       Run asset-pack builder/list/read self-tests\n");
    printf("  --assetpack-list PACK  Print an asset-pack directory listing\n");
    printf("  --export-chr PATH       Export build\\baseline\\Tiles.asm to raw .chr bytes\n");
    printf("  --export-chr-png DIR    Export one PNG tile sheet per 8KB CHR bank\n");
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
        } else if (!tecmo_runtime_init(&runtime, &memory, root)) {
            printf("Failed to initialize runtime from %s\n", root);
        } else {
            bool render_runtime = true;
            if (strcmp(mode_name, "menu") == 0) {
                tecmo_runtime_set_mode(&runtime, TECMO_MODE_MAIN_MENU);
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
                tecmo_render_intro_license_screen(&runtime, &framebuffer);
                render_runtime = false;
                result = 0;
            } else if (strcmp(mode_name, "intro-arena-transition") == 0) {
                framebuffer.pixels = pixels;
                framebuffer.width = width;
                framebuffer.height = height;
                framebuffer.pitch_pixels = width;
                runtime.debug_overlay = true;
                runtime.mode_frame_counter = 240U;
                tecmo_render_intro_arena_transition(&runtime, &framebuffer);
                render_runtime = false;
                result = 0;
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
                tecmo_render_intro_arena_transition(&runtime, &framebuffer);
                render_runtime = false;
                result = 0;
            } else if (strncmp(mode_name, "intro-ready-frame", 17) == 0) {
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
                tecmo_render_intro_ready_screen(&runtime, &framebuffer);
                render_runtime = false;
                result = 0;
            } else if (strncmp(mode_name, "intro-warriors-frame", 20) == 0) {
                long frame = strtol(mode_name + 20, NULL, 10);
                if (frame < 0) {
                    frame = 0;
                }
                framebuffer.pixels = pixels;
                framebuffer.width = width;
                framebuffer.height = height;
                framebuffer.pitch_pixels = width;
                runtime.debug_overlay = true;
                runtime.mode_frame_counter = (unsigned)frame;
                tecmo_render_intro_warriors_transition(&runtime, &framebuffer);
                render_runtime = false;
                result = 0;
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
            }
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
        const char *asset_project_root = (root_explicit || root_from_env) ? root : NULL;
        const char *asset_capture_root = ".";
        char message[256];

        if (index + 1 >= argc) {
            print_usage(program);
            return 2;
        }

        rom_path = argv[index++];
        out_path = argv[index++];
        if (tecmo_asset_pack_build_from_ines(rom_path,
                                             out_path,
                                             asset_project_root,
                                             asset_capture_root,
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
