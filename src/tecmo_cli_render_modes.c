#include "tecmo_cli_internal.h"

bool tecmo_cli_configure_render_mode(
    TecmoRuntime *runtime,
    const char *mode_name,
    TecmoCliRenderModeState *state,
    bool *handled_out)
{
    bool handled;
    bool result;

    result = tecmo_cli_configure_render_frontend_mode(
        runtime, mode_name, state, &handled);
    if (handled) {
        *handled_out = true;
        return result;
    }
    result = tecmo_cli_configure_render_scene_mode(
        runtime, mode_name, state, &handled);
    if (handled) {
        *handled_out = true;
        return result;
    }
    *handled_out = false;
    return true;
}
