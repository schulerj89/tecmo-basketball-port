#ifndef TECMO_GAMEPLAY_CLOSE_SHOTS_H
#define TECMO_GAMEPLAY_CLOSE_SHOTS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TECMO_GAMEPLAY_CLOSE_SHOT_SOURCE_COUNT 13U
#define TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_COUNT 2U
#define TECMO_GAMEPLAY_CLOSE_SHOT_PROFILE_COUNT 2U
#define TECMO_GAMEPLAY_CLOSE_SHOT_DIRECTION_COUNT 8U
#define TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT0_STEP_COUNT 32U
#define TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT2_STEP_COUNT 16U
#define TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT0_POSE_PHASE_COUNT 7U
#define TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT2_POSE_PHASE_COUNT 6U
#define TECMO_GAMEPLAY_CLOSE_SHOT_POSE_BASE_COUNT 32U

#define TECMO_GAMEPLAY_CLOSE_SHOT_FAMILY_DIRECT 0x01U
#define TECMO_GAMEPLAY_CLOSE_SHOT_FAMILY_HELD_RELEASE 0x02U
#define TECMO_GAMEPLAY_CLOSE_SHOT_FAMILY_ARC 0x04U
#define TECMO_GAMEPLAY_CLOSE_SHOT_FAMILY_LONGER_TRAJECTORY 0x08U
#define TECMO_GAMEPLAY_CLOSE_SHOT_FAMILY_CONTACTABLE 0x10U

typedef enum TecmoGameplayCloseShotSourceKind {
    TECMO_GAMEPLAY_CLOSE_SHOT_SOURCE_BANK05_8542_8694 = 1,
    TECMO_GAMEPLAY_CLOSE_SHOT_SOURCE_BANK05_919C_91BB = 2,
    TECMO_GAMEPLAY_CLOSE_SHOT_SOURCE_BANK05_98E1_9A5F = 3,
    TECMO_GAMEPLAY_CLOSE_SHOT_SOURCE_BANK05_A214_A25E = 4,
    TECMO_GAMEPLAY_CLOSE_SHOT_SOURCE_BANK05_A503_A6ED = 5,
    TECMO_GAMEPLAY_CLOSE_SHOT_SOURCE_BANK05_AB36_AC09 = 6,
    TECMO_GAMEPLAY_CLOSE_SHOT_SOURCE_BANK05_B100_B13E = 7,
    TECMO_GAMEPLAY_CLOSE_SHOT_SOURCE_BANK05_B32C_B521 = 8,
    TECMO_GAMEPLAY_CLOSE_SHOT_SOURCE_BANK05_B678_B6E4 = 9,
    TECMO_GAMEPLAY_CLOSE_SHOT_SOURCE_BANK05_B775_B7AC = 10,
    TECMO_GAMEPLAY_CLOSE_SHOT_SOURCE_BANK05_BDEF_BDF6 = 11,
    TECMO_GAMEPLAY_CLOSE_SHOT_SOURCE_BANK05_BFC2_BFC8 = 12,
    TECMO_GAMEPLAY_CLOSE_SHOT_SOURCE_POSE_LOW_HIGH_TABLE = 13
} TecmoGameplayCloseShotSourceKind;

typedef enum TecmoGameplayCloseShotVariant {
    TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_0 = 0,
    TECMO_GAMEPLAY_CLOSE_SHOT_VARIANT_2 = 2
} TecmoGameplayCloseShotVariant;

typedef enum TecmoGameplayCloseShotProfile {
    TECMO_GAMEPLAY_CLOSE_SHOT_PROFILE_0 = 0,
    TECMO_GAMEPLAY_CLOSE_SHOT_PROFILE_1 = 1
} TecmoGameplayCloseShotProfile;

typedef enum TecmoGameplayCloseShotDirection {
    TECMO_GAMEPLAY_CLOSE_SHOT_DIRECTION_0 = 0,
    TECMO_GAMEPLAY_CLOSE_SHOT_DIRECTION_1 = 1,
    TECMO_GAMEPLAY_CLOSE_SHOT_DIRECTION_2 = 2,
    TECMO_GAMEPLAY_CLOSE_SHOT_DIRECTION_3 = 3,
    TECMO_GAMEPLAY_CLOSE_SHOT_DIRECTION_4 = 4,
    TECMO_GAMEPLAY_CLOSE_SHOT_DIRECTION_5 = 5,
    TECMO_GAMEPLAY_CLOSE_SHOT_DIRECTION_6 = 6,
    TECMO_GAMEPLAY_CLOSE_SHOT_DIRECTION_7 = 7
} TecmoGameplayCloseShotDirection;

typedef struct TecmoGameplayCloseShotVariantInfo {
    uint8_t numeric_variant;
    uint8_t family_flags;
    uint8_t step_count;
    uint8_t pose_phase_count;
} TecmoGameplayCloseShotVariantInfo;

typedef struct TecmoGameplayCloseShotSourceSpan {
    TecmoGameplayCloseShotSourceKind kind;
    uint8_t bank;
    uint16_t cpu_start;
    uint16_t cpu_end;
    uint32_t byte_count;
    uint32_t fingerprint;
    const uint8_t *bytes;
} TecmoGameplayCloseShotSourceSpan;

typedef struct TecmoGameplayCloseShotAssets {
    uint32_t lifecycle_tag;
    bool available;
    char status[160];
    uint8_t *storage;
    size_t storage_size;
    TecmoGameplayCloseShotSourceSpan
        sources[TECMO_GAMEPLAY_CLOSE_SHOT_SOURCE_COUNT];
    const uint8_t *variant0_phases;
    const uint8_t *variant2_phases;
    const uint8_t *pose_bases;
    uint32_t gameplay_core_fingerprint;
} TecmoGameplayCloseShotAssets;

void tecmo_gameplay_close_shots_init(TecmoGameplayCloseShotAssets *assets);
void tecmo_gameplay_close_shots_destroy(TecmoGameplayCloseShotAssets *assets);

bool tecmo_gameplay_close_shots_parse(
    TecmoGameplayCloseShotAssets *assets,
    const uint8_t *payload,
    size_t payload_size,
    const uint8_t *gameplay_core,
    size_t gameplay_core_size);

bool tecmo_gameplay_close_shots_load(TecmoGameplayCloseShotAssets *assets,
                                     const char *asset_pack_path);

const TecmoGameplayCloseShotSourceSpan *tecmo_gameplay_close_shots_find_source(
    const TecmoGameplayCloseShotAssets *assets,
    TecmoGameplayCloseShotSourceKind kind);

bool tecmo_gameplay_close_shots_get_variant_info(
    const TecmoGameplayCloseShotAssets *assets,
    TecmoGameplayCloseShotVariant variant,
    TecmoGameplayCloseShotVariantInfo *info);

bool tecmo_gameplay_close_shots_phase_for_step(
    const TecmoGameplayCloseShotAssets *assets,
    TecmoGameplayCloseShotVariant variant,
    uint8_t step,
    uint8_t *phase);

/* Resolves a semantic close-shot phase to the TGPL actor-pointer index. */
bool tecmo_gameplay_close_shots_resolve_pose_pointer_index(
    const TecmoGameplayCloseShotAssets *assets,
    TecmoGameplayCloseShotVariant variant,
    TecmoGameplayCloseShotProfile profile,
    TecmoGameplayCloseShotDirection direction,
    uint8_t phase,
    uint16_t *pointer_index);

#endif
