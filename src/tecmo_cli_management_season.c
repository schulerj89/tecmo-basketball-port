#include "tecmo_season_menu.h"
#include "tecmo_team_management.h"

#include <stdio.h>
#include <string.h>

#include "tecmo_cli_internal.h"

int tecmo_cli_run_management_season_commands(
    const TecmoCliContext *context)
{
    const char *command;
    const char *root;

    if (context == NULL) return TECMO_CLI_NOT_HANDLED;
    command = context->command;
    root = context->root;

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

    return TECMO_CLI_NOT_HANDLED;
}
