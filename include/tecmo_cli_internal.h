#ifndef TECMO_CLI_INTERNAL_H
#define TECMO_CLI_INTERNAL_H

#include <stdbool.h>
#include <stdint.h>

#include "tecmo_cli.h"

struct TecmoGameplayCourtCoordinate;
struct TecmoRuntime;
typedef struct TecmoRuntime TecmoRuntime;

typedef struct TecmoCliContext {
    const char *program;
    const char *root;
    const char *command;
    int argc;
    char **argv;
    int index;
} TecmoCliContext;

typedef struct TecmoCliRenderModeState {
    uint32_t *pixels;
    int width;
    int height;
    bool arena_render_succeeded;
    int result;
} TecmoCliRenderModeState;

enum { TECMO_CLI_NOT_HANDLED = -1 };

void tecmo_cli_print_usage(const char *program);

int tecmo_cli_run_basic_commands(const TecmoCliContext *context);
int tecmo_cli_run_audio_commands(const TecmoCliContext *context);
int tecmo_cli_run_gameplay_core_commands(const TecmoCliContext *context);
int tecmo_cli_run_gameplay_commands(const TecmoCliContext *context);
int tecmo_cli_run_gameplay_court_commands(const TecmoCliContext *context);
int tecmo_cli_run_gameplay_asset_commands(const TecmoCliContext *context);
int tecmo_cli_run_gameplay_asset_contract_command(
    const TecmoCliContext *context);
int tecmo_cli_run_gameplay_close_shots_command(
    const TecmoCliContext *context);
int tecmo_cli_run_gameplay_dunk_command(const TecmoCliContext *context);
int tecmo_cli_run_gameplay_jump_shots_command(
    const TecmoCliContext *context);
int tecmo_cli_run_gameplay_shot_resolution_command(
    const TecmoCliContext *context);
int tecmo_cli_run_render_command(const TecmoCliContext *context);
int tecmo_cli_run_asset_commands(const TecmoCliContext *context);

bool tecmo_cli_parse_render_frame_suffix(const char *mode_name,
                                          const char *prefix,
                                          unsigned *frame);
bool tecmo_cli_parse_u32_argument(const char *text,
                                   uint32_t maximum,
                                   uint32_t *value_out);
bool tecmo_cli_parse_i16_argument(const char *text,
                                   int16_t *value_out);
bool tecmo_cli_parse_court_coordinate_argument(
    const char *text,
    struct TecmoGameplayCourtCoordinate *coordinate_out);
bool tecmo_cli_parse_movement_input(const char *text, uint8_t *input_out);
const char *tecmo_cli_movement_input_name(uint8_t input);
bool tecmo_cli_render_mode_requires_roster_data(const char *mode_name);

void tecmo_cli_print_intro_render_capture_status(
    const struct TecmoRuntime *runtime,
    const char *mode_name,
    bool arena_rendered);
bool tecmo_cli_validate_render_asset_contract(
    const struct TecmoRuntime *runtime,
    const char *mode_name);
void tecmo_cli_print_render_diagnostics(
    const struct TecmoRuntime *runtime,
    const char *mode_name,
    bool arena_rendered);
bool tecmo_cli_parse_finale_render_mode(const char *mode_name,
                                        unsigned *frame_out,
                                        bool *debug_out);
bool tecmo_cli_setup_gameplay_render_checkpoint(
    struct TecmoRuntime *runtime,
    const char *mode_name);
bool tecmo_cli_configure_render_mode(
    struct TecmoRuntime *runtime,
    const char *mode_name,
    TecmoCliRenderModeState *state,
    bool *handled_out);
bool tecmo_cli_configure_render_frontend_mode(
    struct TecmoRuntime *runtime,
    const char *mode_name,
    TecmoCliRenderModeState *state,
    bool *handled_out);
bool tecmo_cli_configure_render_scene_mode(
    struct TecmoRuntime *runtime,
    const char *mode_name,
    TecmoCliRenderModeState *state,
    bool *handled_out);

#endif
