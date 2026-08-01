#include "asset_pack/tecmo_asset_pack_gameplay_audio.h"
#include "asset_pack/tecmo_asset_pack_music.h"
#include "tecmo_audio_output.h"
#include "tecmo_frontend_audio.h"
#include "tecmo_gameplay_audio.h"
#include "tecmo_music.h"

#include <stdio.h>
#include <string.h>

#include "tecmo_cli_internal.h"


int tecmo_cli_run_audio_commands(const TecmoCliContext *context)
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

    if (strcmp(command, "--music-source-test") == 0) {
        const char *rom_path = index < argc ? argv[index] : NULL;
        char message[256] = {0};
        if (tecmo_asset_pack_music_source_test(
                rom_path, message, sizeof(message)) != 0) {
            printf("Music source test failed: %s\n", message);
            return 1;
        }
        printf("%s\n", message);
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

    if (strcmp(command, "--frontend-audio-test") == 0) {
        char message[256] = {0};
        if (!tecmo_frontend_audio_self_test(
                root, message, sizeof(message))) {
            printf("Frontend audio test failed: %s\n", message);
            return 1;
        }
        printf("%s\n", message);
        return 0;
    }

    if (strcmp(command, "--frontend-audio-source-test") == 0) {
        const char *rom_path = index < argc ? argv[index] : NULL;
        char message[256] = {0};
        if (tecmo_asset_pack_frontend_audio_source_test(
                rom_path, message, sizeof(message)) != 0) {
            printf("Frontend audio source test failed: %s\n", message);
            return 1;
        }
        printf("%s\n", message);
        return 0;
    }

    if (strcmp(command, "--frontend-audio-cross-pack-test") == 0) {
        const char *frontend_pack = index < argc ? argv[index++] : NULL;
        const char *music_pack = index < argc ? argv[index] : NULL;
        char message[256] = {0};
        if (!tecmo_frontend_audio_cross_pack_self_test(
                frontend_pack, music_pack, message, sizeof(message))) {
            printf("Frontend audio cross-pack test failed: %s\n", message);
            return 1;
        }
        printf("%s\n", message);
        return 0;
    }

    return TECMO_CLI_NOT_HANDLED;
}
