#include "asset_pack/tecmo_asset_pack_gameplay_backcourt.h"
#include "asset_pack/tecmo_asset_pack_gameplay_ball_dribble.h"
#include "asset_pack/tecmo_asset_pack_gameplay_court_orientation.h"
#include "asset_pack/tecmo_asset_pack_gameplay_fatigue.h"
#include "asset_pack/tecmo_asset_pack_gameplay_violation_referee.h"
#include "tecmo_gameplay_backcourt.h"
#include "tecmo_gameplay_ball_dribble.h"
#include "tecmo_gameplay_camera.h"
#include "tecmo_gameplay_court_orientation.h"
#include "tecmo_gameplay_fatigue.h"
#include "tecmo_gameplay_free_throw_lineup.h"
#include "tecmo_gameplay_movement.h"
#include "tecmo_gameplay_live_proof.h"
#include "tecmo_gameplay_penalties.h"
#include "tecmo_gameplay_state.h"
#include "tecmo_gameplay_violation_referee.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tecmo_cli_internal.h"


int tecmo_cli_run_gameplay_core_commands(const TecmoCliContext *context)
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
    if (strcmp(command, "--gameplay-penalties-test") == 0) {
        const char *pack_path = index < argc ? argv[index] : NULL;
        char message[256];
        if (!tecmo_gameplay_penalties_self_test(
                pack_path, message, sizeof(message))) {
            printf("Penalty asset test failed: %s\n", message);
            return 1;
        }
        printf("%s\n", message);
        return 0;
    }

    if (strcmp(command, "--gameplay-violation-referee-test") == 0) {
        const char *pack_path = index < argc ? argv[index] : NULL;
        const char *rom_path = index + 1 < argc ? argv[index + 1] : NULL;
        char message[256];
        if (rom_path != NULL &&
            tecmo_asset_pack_gameplay_violation_referee_source_test(
                rom_path, message, sizeof(message)) != 0) {
            printf("Violation referee source test failed: %s\n", message);
            return 1;
        }
        if (!tecmo_gameplay_violation_referee_self_test(
                pack_path, message, sizeof(message))) {
            printf("Violation referee asset test failed: %s\n", message);
            return 1;
        }
        printf("%s\n", message);
        return 0;
    }

    if (strcmp(command, "--gameplay-free-throw-lineup-test") == 0) {
        const char *pack_path = index < argc ? argv[index] : NULL;
        char message[256];
        if (!tecmo_gameplay_free_throw_lineup_self_test(
                pack_path, message, sizeof(message))) {
            printf("Free-throw lineup asset test failed: %s\n", message);
            return 1;
        }
        printf("%s\n", message);
        return 0;
    }

    if (strcmp(command, "--gameplay-court-orientation-test") == 0) {
        const char *pack_path = index < argc ? argv[index] : NULL;
        const char *rom_path = index + 1 < argc ? argv[index + 1] : NULL;
        char message[256];
        if (rom_path != NULL &&
            tecmo_asset_pack_gameplay_court_orientation_source_test(
                rom_path, message, sizeof(message)) != 0) {
            printf("Court-orientation source test failed: %s\n", message);
            return 1;
        }
        if (!tecmo_gameplay_court_orientation_self_test(
                pack_path, message, sizeof(message))) {
            printf("Court-orientation asset test failed: %s\n", message);
            return 1;
        }
        printf("%s\n", message);
        return 0;
    }

    if (strcmp(command, "--gameplay-backcourt-test") == 0) {
        const char *pack_path = index < argc ? argv[index] : NULL;
        const char *rom_path = index + 1 < argc ? argv[index + 1] : NULL;
        char message[256];
        if (rom_path != NULL &&
            tecmo_asset_pack_gameplay_backcourt_source_test(
                rom_path, message, sizeof(message)) != 0) {
            printf("Backcourt source test failed: %s\n", message);
            return 1;
        }
        if (!tecmo_gameplay_backcourt_self_test(
                pack_path, message, sizeof(message))) {
            printf("Backcourt asset test failed: %s\n", message);
            return 1;
        }
        printf("%s\n", message);
        return 0;
    }

    if (strcmp(command, "--gameplay-camera-projection-test") == 0) {
        const char *pack_path = index < argc ? argv[index] : NULL;
        char message[256];
        if (!tecmo_gameplay_camera_self_test(
                pack_path, message, sizeof(message))) {
            printf("Gameplay camera asset test failed: %s\n", message);
            return 1;
        }
        printf("%s\n", message);
        return 0;
    }

    if (strcmp(command, "--gameplay-movement-test") == 0) {
        const char *pack_path = index < argc ? argv[index] : NULL;
        char message[256];
        if (!tecmo_gameplay_movement_self_test(
                pack_path, message, sizeof(message))) {
            printf("Gameplay movement test failed: %s\n", message);
            return 1;
        }
        printf("%s\n", message);
        return 0;
    }

    if (strcmp(command, "--gameplay-fatigue-test") == 0) {
        const char *pack_path = index < argc ? argv[index] : NULL;
        const char *rom_path = index + 1 < argc ? argv[index + 1] : NULL;
        char message[256];
        if (rom_path != NULL &&
            tecmo_asset_pack_gameplay_fatigue_source_test(
                rom_path, message, sizeof(message)) != 0) {
            printf("Gameplay fatigue source test failed: %s\n", message);
            return 1;
        }
        if (!tecmo_gameplay_fatigue_self_test(
                pack_path, message, sizeof(message))) {
            printf("Gameplay fatigue test failed: %s\n", message);
            return 1;
        }
        printf("%s\n", message);
        return 0;
    }

    if (strcmp(command, "--gameplay-ball-dribble-test") == 0) {
        const char *pack_path = index < argc ? argv[index] : NULL;
        const char *rom_path = index + 1 < argc ? argv[index + 1] : NULL;
        char message[256];
        if (rom_path != NULL &&
            tecmo_asset_pack_gameplay_ball_dribble_source_test(
                rom_path, message, sizeof(message)) != 0) {
            printf("Gameplay ball-dribble source test failed: %s\n",
                   message);
            return 1;
        }
        if (!tecmo_gameplay_ball_dribble_self_test(
                pack_path, message, sizeof(message))) {
            printf("Gameplay ball-dribble test failed: %s\n", message);
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

    if (strcmp(command, "--gameplay-live-foundation-proof") == 0) {
        const char *pack_path = index < argc ? argv[index++] : NULL;
        const char *event = index < argc ? argv[index++] : NULL;
        const char *output_path = index < argc ? argv[index] : NULL;
        char message[8192];
        if (!tecmo_gameplay_live_foundation_proof(
                root, pack_path, event, output_path,
                message, sizeof(message))) {
            printf("LIVE proof failed: %s\n", message);
            return 1;
        }
        printf("%s\n", message);
        return 0;
    }

    return TECMO_CLI_NOT_HANDLED;
}
