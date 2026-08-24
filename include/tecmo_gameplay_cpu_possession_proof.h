#ifndef TECMO_GAMEPLAY_CPU_POSSESSION_PROOF_H
#define TECMO_GAMEPLAY_CPU_POSSESSION_PROOF_H

#include <stdbool.h>
#include <stddef.h>

/* Developer-only deterministic production-scene regression.  A nonempty
   score_restart_frame_directory enables bounded contiguous PNG evidence;
   NULL preserves the original four-output CLI contract. Artifacts are ignored
   local evidence and structured telemetry remains authoritative. */
bool tecmo_gameplay_cpu_possession_proof(
    const char *project_root,
    const char *asset_pack_path,
    const char *trace_path,
    const char *mid_horizon_png_path,
    const char *terminal_png_path,
    const char *score_restart_frame_directory,
    char *message,
    size_t message_size);

#endif
