#include "asm_inventory.h"
#include "png_writer.h"
#include "tecmo_game.h"

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
    printf("  --render-test PATH      Render first playable frame to a PNG\n");
    printf("  --render-test-mode MODE PATH  Render boot-title, menu, menu-overlay, title-screen, intro-presents, chr-playground, rosters, play setup, original-title, or original-title-chr to PNG\n");
    printf("  --generate-rosters DIR  Generate static C roster source/header from Bank 02\n");
    printf("  --export-chr PATH       Export build\\baseline\\Tiles.asm to raw .chr bytes\n");
    printf("  --export-chr-png DIR    Export one PNG tile sheet per 8KB CHR bank\n");
}

int main(int argc, char **argv)
{
    const char *program = argc > 0 ? argv[0] : "tecmo_port";
    const char *root = getenv("TECMO_DECOMP_ROOT");
    const char *command = "--summary";
    int index = 1;

    if (root == NULL || root[0] == '\0') {
        root = ".";
    }

    if (index < argc && strcmp(argv[index], "--root") == 0) {
        if (index + 1 >= argc) {
            print_usage(program);
            return 2;
        }
        root = argv[index + 1];
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
                tecmo_runtime_set_mode(&runtime, TECMO_MODE_PLAY_SETUP);
            } else if (strcmp(mode_name, "title-screen") == 0) {
                tecmo_runtime_set_mode(&runtime, TECMO_MODE_TITLE_SCREEN);
            } else if (strcmp(mode_name, "boot-title") == 0) {
                /* Default runtime initialization already starts at the title screen. */
            } else if (strcmp(mode_name, "intro-presents") == 0) {
                tecmo_runtime_set_mode(&runtime, TECMO_MODE_INTRO_PROBE);
            } else if (strcmp(mode_name, "chr-playground") == 0) {
                tecmo_runtime_set_mode(&runtime, TECMO_MODE_CHR_PLAYGROUND);
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
