#define _CRT_SECURE_NO_WARNINGS

#include "tecmo_gameplay_hud.h"

#include "asset_pack/tecmo_asset_pack_gameplay.h"
#include "asset_pack/tecmo_asset_pack_gameplay_hud.h"
#include "asset_pack/tecmo_asset_pack_import_layout.h"
#include "tecmo_asset_pack.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TECMO_GAMEPLAY_HUD_LIFECYCLE_TAG 0x44554854U
#define GAMEPLAY_HUD_REV1_ROM_SIZE 393232U
#define GAMEPLAY_HUD_REV1_ROM_FNV1A32 0x0650F5B0U

static const uint8_t gameplay_hud_rev1_sha256[32] = {
    0x07U,0x6AU,0x6BU,0xEBU,0x27U,0x3FU,0xABU,0x39U,
    0x19U,0x8CU,0x87U,0xAEU,0x6AU,0xF6U,0x9FU,0x80U,
    0xAAU,0x54U,0x8DU,0x68U,0x17U,0x75U,0x38U,0x29U,
    0xF2U,0xC2U,0xBDU,0xE1U,0xF9U,0x74U,0x75U,0xC4U
};

static uint16_t read_u16(const uint8_t *bytes)
{
    return (uint16_t)((uint16_t)bytes[0] |
                      ((uint16_t)bytes[1] << 8U));
}

static uint32_t read_u32(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8U) |
           ((uint32_t)bytes[2] << 16U) |
           ((uint32_t)bytes[3] << 24U);
}

static uint64_t read_u64(const uint8_t *bytes)
{
    return (uint64_t)read_u32(bytes) |
           ((uint64_t)read_u32(bytes + 4U) << 32U);
}

static uint32_t fnv1a32(const uint8_t *bytes, size_t count)
{
    uint32_t hash = 2166136261U;
    size_t index;
    for (index = 0U; index < count; ++index) {
        hash ^= bytes[index];
        hash *= 16777619U;
    }
    return hash;
}

static uint64_t fnv1a64(const uint8_t *bytes, size_t count)
{
    uint64_t hash = 14695981039346656037ULL;
    size_t index;
    for (index = 0U; index < count; ++index) {
        hash ^= bytes[index];
        hash *= 1099511628211ULL;
    }
    return hash;
}

static bool bytes_are_zero(const uint8_t *bytes, size_t count)
{
    size_t index;
    for (index = 0U; index < count; ++index) {
        if (bytes[index] != 0U) return false;
    }
    return true;
}

static bool range_ok(size_t offset, size_t count, size_t total)
{
    return offset <= total && count <= total - offset;
}

static bool reject(TecmoGameplayHudAssets *assets, const char *message)
{
    free(assets->storage);
    assets->storage = NULL;
    assets->storage_size = 0U;
    memset(assets->sources, 0, sizeof(assets->sources));
    assets->team_label_tiles = NULL;
    assets->font_tiles = NULL;
    memset(assets->team_ppu_low, 0, sizeof(assets->team_ppu_low));
    assets->dot_tile = 0U;
    assets->blank_tile = 0U;
    assets->gameplay_core_fingerprint = 0U;
    assets->team_data_fingerprint = 0U;
    assets->chr_fingerprint32 = 0U;
    assets->chr_fingerprint64 = 0U;
    assets->available = false;
    (void)snprintf(assets->status, sizeof(assets->status), "%s",
                   message != NULL ? message : "THUD-1 rejected");
    return false;
}

void tecmo_gameplay_hud_assets_init(TecmoGameplayHudAssets *assets)
{
    if (assets == NULL) return;
    memset(assets, 0, sizeof(*assets));
    assets->lifecycle_tag = TECMO_GAMEPLAY_HUD_LIFECYCLE_TAG;
}

void tecmo_gameplay_hud_assets_destroy(TecmoGameplayHudAssets *assets)
{
    if (assets == NULL ||
        assets->lifecycle_tag != TECMO_GAMEPLAY_HUD_LIFECYCLE_TAG) {
        return;
    }
    free(assets->storage);
    tecmo_gameplay_hud_assets_init(assets);
}

static bool validate_header(const uint8_t *payload, size_t payload_size)
{
    size_t index;
    if (payload == NULL ||
        payload_size != TECMO_ASSET_PACK_GAMEPLAY_HUD_SIZE ||
        memcmp(payload, "THUD", 4U) != 0 ||
        read_u16(payload + 4U) != TECMO_ASSET_PACK_GAMEPLAY_HUD_VERSION ||
        read_u16(payload + 6U) !=
            TECMO_ASSET_PACK_GAMEPLAY_HUD_HEADER_SIZE ||
        read_u32(payload + 8U) != TECMO_ASSET_PACK_GAMEPLAY_HUD_SIZE ||
        read_u16(payload + 12U) != TECMO_GAMEPLAY_HUD_SOURCE_COUNT ||
        read_u16(payload + 14U) !=
            TECMO_ASSET_PACK_GAMEPLAY_HUD_SOURCE_STRIDE ||
        read_u32(payload + 16U) !=
            TECMO_ASSET_PACK_GAMEPLAY_HUD_SOURCES_OFFSET ||
        read_u16(payload + 20U) != TECMO_GAMEPLAY_HUD_TEAM_COUNT ||
        read_u16(payload + 22U) != TECMO_GAMEPLAY_HUD_TEAM_LABEL_WIDTH ||
        read_u16(payload + 24U) != TECMO_GAMEPLAY_HUD_FONT_FIRST ||
        read_u16(payload + 26U) != TECMO_GAMEPLAY_HUD_FONT_COUNT ||
        read_u32(payload + 28U) !=
            TECMO_ASSET_PACK_GAMEPLAY_HUD_TEAM_TILE_OFFSET ||
        read_u32(payload + 32U) !=
            TECMO_ASSET_PACK_GAMEPLAY_HUD_FONT_TILE_OFFSET ||
        read_u32(payload + 36U) != 0U ||
        read_u32(payload + 40U) != TECMO_ASSET_PACK_GAMEPLAY_SIZE ||
        read_u32(payload + 44U) != TECMO_ASSET_PACK_GAMEPLAY_FNV1A32 ||
        read_u32(payload + 48U) != TECMO_ASSET_PACK_TEAM_DATA_SIZE ||
        read_u32(payload + 52U) != TECMO_ASSET_PACK_TEAM_DATA_FNV1A32 ||
        read_u32(payload + 56U) != TECMO_ASSET_PACK_GAMEPLAY_CHR_SIZE ||
        read_u32(payload + 60U) !=
            TECMO_ASSET_PACK_GAMEPLAY_CHR_FNV1A32 ||
        read_u64(payload + 64U) !=
            TECMO_ASSET_PACK_GAMEPLAY_CHR_FNV1A64 ||
        read_u32(payload + 72U) != GAMEPLAY_HUD_REV1_ROM_SIZE ||
        read_u32(payload + 76U) != GAMEPLAY_HUD_REV1_ROM_FNV1A32 ||
        memcmp(payload + 80U, gameplay_hud_rev1_sha256,
               sizeof(gameplay_hud_rev1_sha256)) != 0 ||
        !bytes_are_zero(payload + 148U,
                        TECMO_ASSET_PACK_GAMEPLAY_HUD_HEADER_SIZE - 148U)) {
        return false;
    }
    for (index = 0U; index < TECMO_GAMEPLAY_HUD_SOURCE_COUNT; ++index) {
        const TecmoGameplayHudExpectedSource *expected =
            &tecmo_gameplay_hud_expected_sources[index];
        const uint8_t *descriptor = payload +
            TECMO_ASSET_PACK_GAMEPLAY_HUD_DESCRIPTOR_OFFSET +
            index * TECMO_ASSET_PACK_GAMEPLAY_HUD_DESCRIPTOR_STRIDE;
        if (read_u32(descriptor) != expected->payload_offset ||
            read_u32(descriptor + 4U) != expected->byte_count ||
            read_u32(descriptor + 8U) != expected->fingerprint) {
            return false;
        }
    }
    return true;
}

static bool validate_source_records(const uint8_t *payload,
                                    size_t payload_size)
{
    size_t index;
    for (index = 0U; index < TECMO_GAMEPLAY_HUD_SOURCE_COUNT; ++index) {
        const TecmoGameplayHudExpectedSource *expected =
            &tecmo_gameplay_hud_expected_sources[index];
        const uint8_t *record = payload +
            TECMO_ASSET_PACK_GAMEPLAY_HUD_SOURCES_OFFSET +
            index * TECMO_ASSET_PACK_GAMEPLAY_HUD_SOURCE_STRIDE;
        uint32_t cpu_end =
            (uint32_t)expected->cpu_start + expected->byte_count - 1U;
        if (read_u16(record) != (uint16_t)expected->kind ||
            record[2U] != expected->bank ||
            record[3U] != expected->fixed_bank ||
            read_u16(record + 4U) != expected->cpu_start ||
            read_u16(record + 6U) != (uint16_t)cpu_end ||
            read_u32(record + 8U) != expected->byte_count ||
            read_u32(record + 12U) != expected->fingerprint ||
            read_u32(record + 16U) != expected->payload_offset ||
            !bytes_are_zero(record + 20U, 12U) ||
            !range_ok(expected->payload_offset, expected->byte_count,
                      payload_size) ||
            fnv1a32(payload + expected->payload_offset,
                    expected->byte_count) != expected->fingerprint) {
            return false;
        }
    }
    return true;
}

static bool validate_padding(const uint8_t *payload)
{
    return bytes_are_zero(
               payload +
                   TECMO_ASSET_PACK_GAMEPLAY_HUD_TEAM_LAYOUT_OFFSET +
                   TECMO_ASSET_PACK_GAMEPLAY_HUD_TEAM_LAYOUT_SIZE,
               TECMO_ASSET_PACK_GAMEPLAY_HUD_TEAM_LABELS_OFFSET -
                   (TECMO_ASSET_PACK_GAMEPLAY_HUD_TEAM_LAYOUT_OFFSET +
                    TECMO_ASSET_PACK_GAMEPLAY_HUD_TEAM_LAYOUT_SIZE)) &&
           bytes_are_zero(
               payload +
                   TECMO_ASSET_PACK_GAMEPLAY_HUD_TEAM_LABELS_OFFSET +
                   TECMO_ASSET_PACK_GAMEPLAY_HUD_TEAM_LABELS_SIZE,
               TECMO_ASSET_PACK_GAMEPLAY_HUD_TEXT_FORMAT_OFFSET -
                   (TECMO_ASSET_PACK_GAMEPLAY_HUD_TEAM_LABELS_OFFSET +
                    TECMO_ASSET_PACK_GAMEPLAY_HUD_TEAM_LABELS_SIZE)) &&
           bytes_are_zero(
               payload +
                   TECMO_ASSET_PACK_GAMEPLAY_HUD_TEXT_FORMAT_OFFSET +
                   TECMO_ASSET_PACK_GAMEPLAY_HUD_TEXT_FORMAT_SIZE,
               TECMO_ASSET_PACK_GAMEPLAY_HUD_SIZE -
                   (TECMO_ASSET_PACK_GAMEPLAY_HUD_TEXT_FORMAT_OFFSET +
                    TECMO_ASSET_PACK_GAMEPLAY_HUD_TEXT_FORMAT_SIZE));
}

static bool validate_semantics(const uint8_t *payload)
{
    const uint8_t *layout = payload +
        TECMO_ASSET_PACK_GAMEPLAY_HUD_TEAM_LAYOUT_OFFSET;
    const uint8_t *labels = payload +
        TECMO_ASSET_PACK_GAMEPLAY_HUD_TEAM_LABELS_OFFSET;
    const uint8_t *text = payload +
        TECMO_ASSET_PACK_GAMEPLAY_HUD_TEXT_FORMAT_OFFSET;
    const uint8_t *font = payload +
        TECMO_ASSET_PACK_GAMEPLAY_HUD_FONT_TILE_OFFSET;
    size_t index;
    if (layout[45U] != 0x41U || layout[46U] != 0x57U ||
        text[0xAFA2U - 0xAF64U] != 0x81U || font[0U] != 0xFFU ||
        font['.' - TECMO_GAMEPLAY_HUD_FONT_FIRST] != 0x81U) {
        return false;
    }
    for (index = 0U; index < TECMO_GAMEPLAY_HUD_TEAM_COUNT; ++index) {
        if (labels[index] != index * TECMO_GAMEPLAY_HUD_TEAM_LABEL_WIDTH) {
            return false;
        }
    }
    for (index = 0U; index < 10U; ++index) {
        if (font['0' - TECMO_GAMEPLAY_HUD_FONT_FIRST + index] !=
            0x82U + index) {
            return false;
        }
    }
    for (index = 0U; index < 26U; ++index) {
        if (font['A' - TECMO_GAMEPLAY_HUD_FONT_FIRST + index] !=
            0x8CU + index) {
            return false;
        }
    }
    return true;
}

static bool validate_dependencies(
    const uint8_t *hud_payload,
    const uint8_t *gameplay_core,
    size_t gameplay_core_size,
    const uint8_t *team_data,
    size_t team_data_size,
    const uint8_t *chr_bytes,
    size_t chr_size)
{
    size_t index;
    if (hud_payload == NULL || gameplay_core == NULL ||
        gameplay_core_size != TECMO_ASSET_PACK_GAMEPLAY_SIZE ||
        memcmp(gameplay_core, "TGPL", 4U) != 0 ||
        read_u16(gameplay_core + 4U) !=
            TECMO_ASSET_PACK_GAMEPLAY_VERSION ||
        fnv1a32(gameplay_core, gameplay_core_size) !=
            TECMO_ASSET_PACK_GAMEPLAY_FNV1A32 ||
        team_data == NULL ||
        team_data_size != TECMO_ASSET_PACK_TEAM_DATA_SIZE ||
        memcmp(team_data, "TTDT", 4U) != 0 ||
        read_u16(team_data + 4U) != 1U ||
        read_u16(team_data + 6U) !=
            TECMO_ASSET_PACK_TEAM_DATA_HEADER_SIZE ||
        read_u32(team_data + 56U) != TECMO_ASSET_PACK_TEAM_DATA_SIZE ||
        fnv1a32(team_data, team_data_size) !=
            TECMO_ASSET_PACK_TEAM_DATA_FNV1A32 ||
        chr_bytes == NULL ||
        chr_size != TECMO_ASSET_PACK_GAMEPLAY_CHR_SIZE ||
        fnv1a32(chr_bytes, chr_size) !=
            TECMO_ASSET_PACK_GAMEPLAY_CHR_FNV1A32 ||
        fnv1a64(chr_bytes, chr_size) !=
            TECMO_ASSET_PACK_GAMEPLAY_CHR_FNV1A64) {
        return false;
    }
    for (index = 0U; index < TECMO_GAMEPLAY_HUD_FONT_COUNT; ++index) {
        const uint8_t *record = team_data +
            TECMO_ASSET_PACK_TEAM_DATA_FONT_OFFSET +
            index * TECMO_ASSET_PACK_TEAM_DATA_FONT_STRIDE;
        uint8_t tile = hud_payload[
            TECMO_ASSET_PACK_GAMEPLAY_HUD_FONT_TILE_OFFSET + index];
        uint32_t chr_offset =
            (uint32_t)TECMO_GAMEPLAY_HUD_FONT_CHR_SELECTOR * 1024U +
            (uint32_t)(tile & 0x7FU) * 16U;
        if (record[0U] !=
                (uint8_t)(TECMO_GAMEPLAY_HUD_FONT_FIRST + index) ||
            record[1U] != tile ||
            record[2U] != TECMO_GAMEPLAY_HUD_FONT_CHR_SELECTOR ||
            record[3U] != TECMO_GAMEPLAY_HUD_FONT_CHR_SELECTOR ||
            read_u32(record + 4U) != chr_offset ||
            chr_offset > chr_size || chr_size - chr_offset < 16U) {
            return false;
        }
    }
    /* Keep each same-pack dependency explicit above so a valid fingerprint
       cannot hide a mismatched THUD-to-TTDT font binding. */
    return true;
}

bool tecmo_gameplay_hud_assets_parse(
    TecmoGameplayHudAssets *assets,
    const uint8_t *payload,
    size_t payload_size,
    const uint8_t *gameplay_core,
    size_t gameplay_core_size,
    const uint8_t *team_data,
    size_t team_data_size,
    const uint8_t *chr_bytes,
    size_t chr_size)
{
    uint8_t *storage;
    size_t index;
    if (assets == NULL ||
        assets->lifecycle_tag != TECMO_GAMEPLAY_HUD_LIFECYCLE_TAG) {
        return false;
    }
    tecmo_gameplay_hud_assets_destroy(assets);
    if (!validate_header(payload, payload_size)) {
        return reject(assets, "THUD-1 header/size/reserved contract rejected");
    }
    if (fnv1a32(payload, payload_size) !=
            TECMO_ASSET_PACK_GAMEPLAY_HUD_FNV1A32) {
        return reject(assets, "THUD-1 canonical payload fingerprint rejected");
    }
    if (!validate_source_records(payload, payload_size) ||
        !validate_padding(payload) || !validate_semantics(payload)) {
        return reject(assets, "THUD-1 source/padding contract rejected");
    }
    if (!validate_dependencies(
            payload, gameplay_core, gameplay_core_size,
            team_data, team_data_size,
            chr_bytes, chr_size)) {
        return reject(
            assets,
            "THUD-1 TGPL-1/TTDT-1/chr dependency contract rejected");
    }

    storage = (uint8_t *)malloc(payload_size);
    if (storage == NULL) return reject(assets, "THUD-1 allocation failed");
    memcpy(storage, payload, payload_size);
    assets->storage = storage;
    assets->storage_size = payload_size;
    for (index = 0U; index < TECMO_GAMEPLAY_HUD_SOURCE_COUNT; ++index) {
        const TecmoGameplayHudExpectedSource *expected =
            &tecmo_gameplay_hud_expected_sources[index];
        TecmoGameplayHudSourceSpan *source = &assets->sources[index];
        source->kind = expected->kind;
        source->bank = expected->bank;
        source->fixed_bank = expected->fixed_bank != 0U;
        source->cpu_start = expected->cpu_start;
        source->cpu_end = (uint16_t)(
            (uint32_t)expected->cpu_start + expected->byte_count - 1U);
        source->byte_count = expected->byte_count;
        source->fingerprint = expected->fingerprint;
        source->bytes = storage + expected->payload_offset;
    }
    assets->team_label_tiles =
        (const uint8_t (*)[TECMO_GAMEPLAY_HUD_TEAM_LABEL_WIDTH])(
            storage + TECMO_ASSET_PACK_GAMEPLAY_HUD_TEAM_TILE_OFFSET);
    assets->font_tiles =
        storage + TECMO_ASSET_PACK_GAMEPLAY_HUD_FONT_TILE_OFFSET;
    assets->team_ppu_low[0] = storage[
        TECMO_ASSET_PACK_GAMEPLAY_HUD_TEAM_LAYOUT_OFFSET + 45U];
    assets->team_ppu_low[1] = storage[
        TECMO_ASSET_PACK_GAMEPLAY_HUD_TEAM_LAYOUT_OFFSET + 46U];
    assets->dot_tile = assets->font_tiles[
        '.' - TECMO_GAMEPLAY_HUD_FONT_FIRST];
    assets->blank_tile = assets->font_tiles[0U];
    assets->gameplay_core_fingerprint = TECMO_ASSET_PACK_GAMEPLAY_FNV1A32;
    assets->team_data_fingerprint = TECMO_ASSET_PACK_TEAM_DATA_FNV1A32;
    assets->chr_fingerprint32 = TECMO_ASSET_PACK_GAMEPLAY_CHR_FNV1A32;
    assets->chr_fingerprint64 = TECMO_ASSET_PACK_GAMEPLAY_CHR_FNV1A64;
    assets->available = true;
    (void)snprintf(
        assets->status, sizeof(assets->status),
        "THUD-1 ROM-backed live gameplay HUD assetpack");
    return true;
}

bool tecmo_gameplay_hud_assets_load(
    TecmoGameplayHudAssets *assets,
    const char *asset_pack_path)
{
    uint8_t *payload = NULL;
    uint8_t *gameplay_core = NULL;
    uint8_t *team_data = NULL;
    uint8_t *chr_bytes = NULL;
    uint64_t payload_size = 0U;
    uint64_t gameplay_core_size = 0U;
    uint64_t team_data_size = 0U;
    uint64_t chr_size = 0U;
    bool loaded;
    if (assets == NULL ||
        assets->lifecycle_tag != TECMO_GAMEPLAY_HUD_LIFECYCLE_TAG) {
        return false;
    }
    tecmo_gameplay_hud_assets_destroy(assets);
    if (asset_pack_path == NULL ||
        tecmo_asset_pack_read_entry_exact(
            asset_pack_path, TECMO_ASSET_PACK_GAMEPLAY_HUD_ID,
            TECMO_ASSET_PACK_GAMEPLAY_HUD_SIZE,
            &payload, &payload_size) != 0) {
        return reject(
            assets, "THUD-1 gameplay/hud entry missing or wrong-sized");
    }
    if (tecmo_asset_pack_read_entry_exact(
            asset_pack_path, TECMO_ASSET_PACK_GAMEPLAY_ID,
            TECMO_ASSET_PACK_GAMEPLAY_SIZE,
            &gameplay_core, &gameplay_core_size) != 0 ||
        tecmo_asset_pack_read_entry_exact(
            asset_pack_path, TECMO_ASSET_PACK_TEAM_DATA_ID,
            TECMO_ASSET_PACK_TEAM_DATA_SIZE,
            &team_data, &team_data_size) != 0 ||
        tecmo_asset_pack_read_entry_exact(
            asset_pack_path, "chr/all",
            TECMO_ASSET_PACK_GAMEPLAY_CHR_SIZE,
            &chr_bytes, &chr_size) != 0) {
        tecmo_asset_pack_free(payload);
        tecmo_asset_pack_free(gameplay_core);
        tecmo_asset_pack_free(team_data);
        tecmo_asset_pack_free(chr_bytes);
        return reject(
            assets, "THUD-1 same-pack dependency missing or wrong-sized");
    }
    loaded = tecmo_gameplay_hud_assets_parse(
        assets, payload, (size_t)payload_size,
        gameplay_core, (size_t)gameplay_core_size,
        team_data, (size_t)team_data_size,
        chr_bytes, (size_t)chr_size);
    tecmo_asset_pack_free(payload);
    tecmo_asset_pack_free(gameplay_core);
    tecmo_asset_pack_free(team_data);
    tecmo_asset_pack_free(chr_bytes);
    return loaded;
}

const TecmoGameplayHudSourceSpan *tecmo_gameplay_hud_find_source(
    const TecmoGameplayHudAssets *assets,
    TecmoGameplayHudSourceKind kind)
{
    size_t index;
    if (assets == NULL || !assets->available) return NULL;
    for (index = 0U; index < TECMO_GAMEPLAY_HUD_SOURCE_COUNT; ++index) {
        if (assets->sources[index].kind == kind) {
            return &assets->sources[index];
        }
    }
    return NULL;
}

bool tecmo_gameplay_hud_tile_for_ascii(
    const TecmoGameplayHudAssets *assets,
    unsigned char character,
    uint8_t *tile_out)
{
    uint8_t tile;
    if (assets == NULL || !assets->available || tile_out == NULL ||
        character < TECMO_GAMEPLAY_HUD_FONT_FIRST ||
        character >= TECMO_GAMEPLAY_HUD_FONT_FIRST +
                         TECMO_GAMEPLAY_HUD_FONT_COUNT) {
        return false;
    }
    tile = assets->font_tiles[
        character - TECMO_GAMEPLAY_HUD_FONT_FIRST];
    if (tile == 0U && character != ' ') return false;
    *tile_out = tile;
    return true;
}

bool tecmo_gameplay_hud_self_test(
    const char *asset_pack_path,
    char *message,
    size_t message_size)
{
    TecmoGameplayHudAssets assets;
    uint8_t tile = 0U;
    bool passed;
    tecmo_gameplay_hud_assets_init(&assets);
    passed = asset_pack_path != NULL &&
        tecmo_gameplay_hud_assets_load(&assets, asset_pack_path) &&
        assets.team_ppu_low[0] == 0x41U &&
        assets.team_ppu_low[1] == 0x57U &&
        assets.dot_tile == 0x81U && assets.blank_tile == 0xFFU &&
        tecmo_gameplay_hud_find_source(
            &assets, TECMO_GAMEPLAY_HUD_SOURCE_TEAM_LAYOUT) != NULL &&
        tecmo_gameplay_hud_find_source(
            &assets, TECMO_GAMEPLAY_HUD_SOURCE_TEAM_LABELS) != NULL &&
        tecmo_gameplay_hud_find_source(
            &assets, TECMO_GAMEPLAY_HUD_SOURCE_TEXT_FORMAT) != NULL &&
        tecmo_gameplay_hud_tile_for_ascii(&assets, 'A', &tile) &&
        tile == 0x8CU;
    tile = 0x5AU;
    if (passed &&
        (tecmo_gameplay_hud_tile_for_ascii(&assets, ':', &tile) ||
         tile != 0x5AU)) {
        passed = false;
    }
    if (message != NULL && message_size > 0U) {
        (void)snprintf(
            message, message_size, "%s",
            passed ? "THUD-1 strict gameplay HUD self-test passed."
                   : (assets.status[0] != '\0'
                          ? assets.status
                          : "THUD-1 strict gameplay HUD self-test failed."));
    }
    tecmo_gameplay_hud_assets_destroy(&assets);
    return passed;
}
