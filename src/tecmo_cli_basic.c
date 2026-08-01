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


int tecmo_cli_run_basic_commands(const TecmoCliContext *context)
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
        if (!tecmo_win32_keys_self_test(message, sizeof(message))) {
            printf("Win32 key translation test failed: %s\n", message);
            return 1;
        }
        printf("CONTROLS SELF TEST PASS: held/pressed/released and Win32 keyboard mapping\n");
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

    return TECMO_CLI_NOT_HANDLED;
}
