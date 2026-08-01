#include "asm_inventory.h"
#include "tecmo_asset_pack.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tecmo_cli_internal.h"

int tecmo_cli_run_asset_commands(const TecmoCliContext *context)
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
    if (strcmp(command, "--assetpack-test") == 0) {
        char message[256];
        if (tecmo_asset_pack_self_test(message, sizeof(message)) != 0) {
            printf("Asset pack self-test failed: %s\n", message);
            return 1;
        }
        printf("%s\n", message);
        return 0;
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
            return TECMO_CLI_USAGE_REQUESTED;
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
            return TECMO_CLI_USAGE_REQUESTED;
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
            return TECMO_CLI_USAGE_REQUESTED;
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
    return TECMO_CLI_NOT_HANDLED;
}
