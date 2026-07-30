#ifndef TECMO_ASSET_PACK_GAMEPLAY_CAMERA_H
#define TECMO_ASSET_PACK_GAMEPLAY_CAMERA_H

#include "tecmo_gameplay_camera.h"

#include <stddef.h>
#include <stdint.h>

#define TECMO_ASSET_PACK_GAMEPLAY_CAMERA_ID "gameplay/camera-projection"
#define TECMO_ASSET_PACK_GAMEPLAY_CAMERA_VERSION 1U
#define TECMO_ASSET_PACK_GAMEPLAY_CAMERA_HEADER_SIZE 256U
#define TECMO_ASSET_PACK_GAMEPLAY_CAMERA_SOURCE_STRIDE 32U
#define TECMO_ASSET_PACK_GAMEPLAY_CAMERA_SOURCES_OFFSET 256U
#define TECMO_ASSET_PACK_GAMEPLAY_CAMERA_INITIALIZE_OFFSET 448U
#define TECMO_ASSET_PACK_GAMEPLAY_CAMERA_INITIALIZE_SIZE 26U
#define TECMO_ASSET_PACK_GAMEPLAY_CAMERA_STREAM_OFFSET 480U
#define TECMO_ASSET_PACK_GAMEPLAY_CAMERA_STREAM_SIZE 249U
#define TECMO_ASSET_PACK_GAMEPLAY_CAMERA_ATTRIBUTE_OFFSET 736U
#define TECMO_ASSET_PACK_GAMEPLAY_CAMERA_ATTRIBUTE_SIZE 85U
#define TECMO_ASSET_PACK_GAMEPLAY_CAMERA_FOLLOW_OFFSET 832U
#define TECMO_ASSET_PACK_GAMEPLAY_CAMERA_FOLLOW_SIZE 383U
#define TECMO_ASSET_PACK_GAMEPLAY_CAMERA_SETTLE_OFFSET 1216U
#define TECMO_ASSET_PACK_GAMEPLAY_CAMERA_SETTLE_SIZE 62U
#define TECMO_ASSET_PACK_GAMEPLAY_CAMERA_PROJECTION_OFFSET 1280U
#define TECMO_ASSET_PACK_GAMEPLAY_CAMERA_PROJECTION_SIZE 38U
#define TECMO_ASSET_PACK_GAMEPLAY_CAMERA_SIZE 1344U

#define TECMO_ASSET_PACK_GAMEPLAY_CAMERA_INITIALIZE_FNV1A32 0xA5CF7665U
#define TECMO_ASSET_PACK_GAMEPLAY_CAMERA_STREAM_FNV1A32 0x0F3761F5U
#define TECMO_ASSET_PACK_GAMEPLAY_CAMERA_ATTRIBUTE_FNV1A32 0x7FE800D4U
#define TECMO_ASSET_PACK_GAMEPLAY_CAMERA_FOLLOW_FNV1A32 0x19038AEAU
#define TECMO_ASSET_PACK_GAMEPLAY_CAMERA_SETTLE_FNV1A32 0xAF5725C0U
#define TECMO_ASSET_PACK_GAMEPLAY_CAMERA_PROJECTION_FNV1A32 0x24A58210U

#define TECMO_ASSET_PACK_GAMEPLAY_CAMERA_FNV1A32 0x6423B023U

typedef struct TecmoGameplayCameraExpectedSource {
    TecmoGameplayCameraSourceKind kind;
    uint8_t bank;
    uint8_t fixed_bank;
    uint16_t cpu_start;
    uint32_t byte_count;
    uint32_t fingerprint;
    uint32_t payload_offset;
} TecmoGameplayCameraExpectedSource;

typedef struct TecmoGameplayCameraProvenance {
    uint64_t source_offsets[TECMO_GAMEPLAY_CAMERA_SOURCE_COUNT];
} TecmoGameplayCameraProvenance;

extern const TecmoGameplayCameraExpectedSource
    tecmo_gameplay_camera_expected_sources[
        TECMO_GAMEPLAY_CAMERA_SOURCE_COUNT];

int tecmo_asset_pack_build_gameplay_camera(
    const uint8_t *rom,
    uint64_t rom_size,
    uint64_t prg_offset,
    uint32_t prg_banks,
    int enforce_revision_fingerprints,
    uint8_t *payload,
    size_t payload_size,
    TecmoGameplayCameraProvenance *provenance,
    char *message,
    size_t message_size);

int tecmo_asset_pack_gameplay_camera_source_test(
    const char *rom_path,
    char *message,
    size_t message_size);

int tecmo_asset_pack_gameplay_camera_self_test(
    char *message,
    size_t message_size);

#endif
