#include "tecmo_cli_internal.h"

int tecmo_cli_run_gameplay_asset_commands(const TecmoCliContext *context)
{
    int result;

    if (context == NULL) return TECMO_CLI_NOT_HANDLED;
    result = tecmo_cli_run_gameplay_asset_contract_command(context);
    if (result != TECMO_CLI_NOT_HANDLED) return result;
    result = tecmo_cli_run_gameplay_close_shots_command(context);
    if (result != TECMO_CLI_NOT_HANDLED) return result;
    result = tecmo_cli_run_gameplay_dunk_command(context);
    if (result != TECMO_CLI_NOT_HANDLED) return result;
    result = tecmo_cli_run_gameplay_jump_shots_command(context);
    if (result != TECMO_CLI_NOT_HANDLED) return result;
    result = tecmo_cli_run_gameplay_shot_resolution_command(context);
    if (result != TECMO_CLI_NOT_HANDLED) return result;
    result = tecmo_cli_run_gameplay_rebound_audit_command(context);
    if (result != TECMO_CLI_NOT_HANDLED) return result;
    return TECMO_CLI_NOT_HANDLED;
}
