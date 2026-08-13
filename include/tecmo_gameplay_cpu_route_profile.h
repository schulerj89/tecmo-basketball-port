#ifndef TECMO_GAMEPLAY_CPU_ROUTE_PROFILE_H
#define TECMO_GAMEPLAY_CPU_ROUTE_PROFILE_H

#include "tecmo_gameplay_movement.h"
#include "tecmo_gameplay_rebound_audit.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TECMO_GAMEPLAY_CPU_ROUTE_PROFILE_INPUT_TAG 0x49505254U
#define TECMO_GAMEPLAY_CPU_ROUTE_PROFILE_RESULT_TAG 0x52505254U
#define TECMO_GAMEPLAY_CPU_ROUTE_PROFILE_ROSTER_COUNT 12U

/* Typed inputs to fixed $C045 and Bank02 $A89E-$A907. `roster_index` is the
   actor-local $05A9 slot. `extra_adjust_admitted` deliberately does not name
   or mirror raw $030C/$030D: LIVE must label its admission policy at the call
   site, and unavailable ownership fails closed. */
typedef struct TecmoGameplayCpuRouteProfileInput {
    uint32_t contract_tag;
    uint8_t actor;
    uint8_t team;
    uint8_t roster_index;
    uint8_t profile_movement_byte;
    uint8_t speed_value;
    uint8_t difficulty;
    uint8_t condition_7c48;
    bool extra_adjust_admission_available;
    bool extra_adjust_admitted;
} TecmoGameplayCpuRouteProfileInput;

typedef struct TecmoGameplayCpuRouteProfileResult {
    uint32_t contract_tag;
    uint8_t actor;
    uint8_t player_row_7c48;
    uint8_t condition_7c48;
    uint8_t movement_value_06e7;
    int8_t mode_adjustment;
    int8_t extra_adjustment;
    bool extra_adjustment_applied;
} TecmoGameplayCpuRouteProfileResult;

/* Exact bounded projection. Arithmetic wraps after each 6502 ADC. Failure
   leaves result_out untouched. The result owns neither the live condition
   plane nor the admission lifecycle; it validates and transports both. */
bool tecmo_gameplay_cpu_route_profile_project(
    const TecmoGameplayMovementAssets *movement_assets,
    const TecmoGameplayReboundAuditAssets *c045_assets,
    const TecmoGameplayCpuRouteProfileInput *input,
    TecmoGameplayCpuRouteProfileResult *result_out);

bool tecmo_gameplay_cpu_route_profile_self_test(
    const char *asset_pack_path,
    char *message,
    size_t message_size);

#endif
