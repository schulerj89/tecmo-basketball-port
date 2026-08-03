#ifndef TECMO_GAMEPLAY_LIVE_PROOF_H
#define TECMO_GAMEPLAY_LIVE_PROOF_H

#include <stdbool.h>
#include <stddef.h>

/*
 * Replays one bounded, deterministic LIVE foundation event from a bound
 * non-identity scene launch, advances the real TPTI/pre-tip path unless the
 * event explicitly requests the launch frame, renders through the normal
 * TecmoRuntime court path, and writes one 640x480 RGBA PNG.  The JSON state
 * line returned in message is the machine-readable proof seam; it is not a
 * replacement for the production game.c launch bridge.
 */
bool tecmo_gameplay_live_foundation_proof(
    const char *project_root,
    const char *asset_pack_path,
    const char *event,
    const char *output_png_path,
    char *message,
    size_t message_size);

#endif
