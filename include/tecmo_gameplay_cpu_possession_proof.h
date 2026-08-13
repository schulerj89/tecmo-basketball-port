#ifndef TECMO_GAMEPLAY_CPU_POSSESSION_PROOF_H
#define TECMO_GAMEPLAY_CPU_POSSESSION_PROOF_H

#include <stdbool.h>
#include <stddef.h>

/* Developer-only deterministic production-scene regression.  Artifacts are
   ignored local evidence; structured telemetry is authoritative. */
bool tecmo_gameplay_cpu_possession_proof(
    const char *project_root,
    const char *asset_pack_path,
    const char *trace_path,
    const char *mid_png_path,
    const char *terminal_png_path,
    char *message,
    size_t message_size);

#endif
