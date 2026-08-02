#define _CRT_SECURE_NO_WARNINGS

#include "tecmo_gameplay_pretip.h"

#include "asset_pack/tecmo_asset_pack_gameplay_pretip.h"
#include "tecmo_asset_pack.h"
#include "tecmo_gameplay_assets.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PRETIP_LIFECYCLE_TAG 0x31545054U

static const uint8_t pretip_rev1_sha256[32] = {
    0x07U,0x6AU,0x6BU,0xEBU,0x27U,0x3FU,0xABU,0x39U,
    0x19U,0x8CU,0x87U,0xAEU,0x6AU,0xF6U,0x9FU,0x80U,
    0xAAU,0x54U,0x8DU,0x68U,0x17U,0x75U,0x38U,0x29U,
    0xF2U,0xC2U,0xBDU,0xE1U,0xF9U,0x74U,0x75U,0xC4U
};

static uint16_t read_u16(const uint8_t *bytes)
{
    return (uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8U);
}

static uint32_t read_u32(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] |
           ((uint32_t)bytes[1] << 8U) |
           ((uint32_t)bytes[2] << 16U) |
           ((uint32_t)bytes[3] << 24U);
}

static uint64_t read_u64(const uint8_t *bytes)
{
    uint64_t value = 0U;
    unsigned index;
    for (index = 0U; index < 8U; ++index)
        value |= (uint64_t)bytes[index] << (index * 8U);
    return value;
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

static bool bytes_zero(const uint8_t *bytes, size_t count)
{
    size_t index;
    for (index = 0U; index < count; ++index)
        if (bytes[index] != 0U) return false;
    return true;
}

static bool range_ok(size_t offset, size_t count, size_t total)
{
    return offset <= total && count <= total - offset;
}

static bool reject(TecmoGameplayPreTipAssets *assets, const char *message)
{
    free(assets->storage);
    assets->storage = NULL;
    assets->storage_size = 0U;
    assets->nametables = NULL;
    assets->palette = NULL;
    assets->character_map = NULL;
    assets->character_tiles = NULL;
    memset(assets->descriptor, 0, sizeof(assets->descriptor));
    memset(assets->card_chr_selector, 0,
           sizeof(assets->card_chr_selector));
    memset(assets->tip_actor_indices, 0,
           sizeof(assets->tip_actor_indices));
    memset(assets->phase_frames, 0, sizeof(assets->phase_frames));
    memset(assets->sources, 0, sizeof(assets->sources));
    assets->gameplay_core_fingerprint = 0U;
    assets->team_data_fingerprint = 0U;
    assets->music_fingerprint = 0U;
    assets->warriors_fingerprint = 0U;
    assets->chr_fingerprint32 = 0U;
    assets->chr_fingerprint64 = 0U;
    assets->available = false;
    (void)snprintf(assets->status, sizeof(assets->status), "%s",
                   message != NULL ? message : "TPTI-1 rejected");
    return false;
}

void tecmo_gameplay_pretip_init(TecmoGameplayPreTipAssets *assets)
{
    if (assets == NULL) return;
    memset(assets, 0, sizeof(*assets));
    assets->lifecycle_tag = PRETIP_LIFECYCLE_TAG;
}

void tecmo_gameplay_pretip_destroy(TecmoGameplayPreTipAssets *assets)
{
    if (assets == NULL || assets->lifecycle_tag != PRETIP_LIFECYCLE_TAG)
        return;
    free(assets->storage);
    tecmo_gameplay_pretip_init(assets);
}

static bool validate_header(const uint8_t *payload, size_t size)
{
    size_t index;
    if (payload == NULL ||
        size != TECMO_ASSET_PACK_GAMEPLAY_PRETIP_SIZE ||
        memcmp(payload, "TPTI", 4U) != 0 ||
        read_u16(payload + 4U) !=
            TECMO_ASSET_PACK_GAMEPLAY_PRETIP_VERSION ||
        read_u16(payload + 6U) !=
            TECMO_ASSET_PACK_GAMEPLAY_PRETIP_HEADER_SIZE ||
        read_u32(payload + 8U) !=
            TECMO_ASSET_PACK_GAMEPLAY_PRETIP_SIZE ||
        read_u16(payload + 12U) != TECMO_GAMEPLAY_PRETIP_SOURCE_COUNT ||
        read_u16(payload + 14U) !=
            TECMO_ASSET_PACK_GAMEPLAY_PRETIP_SOURCE_STRIDE ||
        read_u32(payload + 16U) !=
            TECMO_ASSET_PACK_GAMEPLAY_PRETIP_SOURCES_OFFSET ||
        payload[20U] != TECMO_GAMEPLAY_PRETIP_SCREEN_ID ||
        payload[21U] != 0x3BU || payload[22U] != 0x7DU ||
        payload[23U] != 8U ||
        read_u32(payload + 24U) != 23416U ||
        read_u32(payload + 28U) != 0x2047CCE0U ||
        read_u32(payload + 32U) != 96372U ||
        read_u32(payload + 36U) != 0x812628F0U ||
        read_u32(payload + 40U) != 36784U ||
        read_u32(payload + 44U) != 0x05C00ECBU ||
        read_u32(payload + 48U) != 262144U ||
        read_u32(payload + 52U) != 0xF6F6E854U ||
        read_u64(payload + 56U) != 0x96A64F53B240ABB4ULL ||
        read_u32(payload + 64U) != 393232U ||
        read_u32(payload + 68U) != 0x0650F5B0U ||
        memcmp(payload + 72U, pretip_rev1_sha256, 32U) != 0 ||
        read_u32(payload + 104U) != 24U ||
        read_u32(payload + 108U) != 2048U ||
        read_u32(payload + 112U) !=
            TECMO_ASSET_PACK_GAMEPLAY_PRETIP_DESCRIPTOR_OFFSET ||
        read_u32(payload + 116U) !=
            TECMO_ASSET_PACK_GAMEPLAY_PRETIP_ENCODED_OFFSET ||
        read_u32(payload + 120U) !=
            TECMO_ASSET_PACK_GAMEPLAY_PRETIP_DECODED_OFFSET ||
        read_u32(payload + 124U) !=
            TECMO_ASSET_PACK_GAMEPLAY_PRETIP_PALETTE_OFFSET ||
        read_u32(payload + 128U) != 16U ||
        read_u32(payload + 132U) != 0x4EB8B3ABU ||
        read_u32(payload + 136U) != 0x3C7D9D61U ||
        read_u32(payload + 140U) != 0xDBF66A45U ||
        read_u32(payload + 144U) != 0xB389D1A4U ||
        read_u32(payload + 148U) !=
            TECMO_ASSET_PACK_GAMEPLAY_PRETIP_TWAR_SIZE ||
        read_u32(payload + 152U) !=
            TECMO_ASSET_PACK_GAMEPLAY_PRETIP_TWAR_FNV1A32 ||
        payload[172U] != 0x40U || payload[173U] != 12U ||
        payload[174U] != 11U ||
        payload[175U] != TECMO_GAMEPLAY_PRETIP_PHASE_COUNT ||
        payload[176U] != 0x82U || payload[177U] != 0xC1U ||
        payload[TECMO_ASSET_PACK_GAMEPLAY_PRETIP_TIP_AWAY_ACTOR_OFFSET] !=
            4U ||
        payload[TECMO_ASSET_PACK_GAMEPLAY_PRETIP_TIP_HOME_ACTOR_OFFSET] !=
            9U ||
        payload[180U] != 0U || payload[181U] != 1U ||
        payload[182U] != 10U || payload[183U] != 0x1AU ||
        payload[184U] != 0x1AU ||
        payload[185U] != TECMO_GAMEPLAY_PRETIP_GLYPH_COUNT ||
        payload[186U] != TECMO_GAMEPLAY_PRETIP_GLYPH_TILE_COUNT ||
        payload[187U] != 33U || payload[188U] != 25U ||
        payload[189U] != TECMO_GAMEPLAY_PRETIP_AWAY_WINNER ||
        payload[190U] != 0xC6U || payload[191U] != 0xFAU ||
        !bytes_zero(payload + 192U,
                    TECMO_ASSET_PACK_GAMEPLAY_PRETIP_HEADER_SIZE - 192U)) {
        return false;
    }
    {
        static const uint16_t expected_frames[
            TECMO_GAMEPLAY_PRETIP_PHASE_COUNT] = {
                61U,121U,61U,208U,30U,120U,60U,30U
            };
        for (index = 0U; index < TECMO_GAMEPLAY_PRETIP_PHASE_COUNT; ++index)
            if (read_u16(payload + 156U + index * 2U) !=
                    expected_frames[index]) return false;
    }
    return true;
}

static bool validate_sources(const uint8_t *payload, size_t size)
{
    size_t index;
    for (index = 0U; index < TECMO_GAMEPLAY_PRETIP_SOURCE_COUNT; ++index) {
        const TecmoGameplayPreTipExpectedSource *expected =
            &tecmo_gameplay_pretip_expected_sources[index];
        const uint8_t *record =
            payload + TECMO_ASSET_PACK_GAMEPLAY_PRETIP_SOURCES_OFFSET +
            index * TECMO_ASSET_PACK_GAMEPLAY_PRETIP_SOURCE_STRIDE;
        uint32_t end =
            (uint32_t)expected->cpu_start + expected->byte_count - 1U;
        if (read_u16(record) != (uint16_t)expected->kind ||
            record[2U] != expected->bank ||
            record[3U] != expected->fixed_bank ||
            read_u16(record + 4U) != expected->cpu_start ||
            read_u16(record + 6U) != (uint16_t)end ||
            read_u32(record + 8U) != expected->byte_count ||
            read_u32(record + 12U) != expected->fingerprint_fnv1a32 ||
            read_u64(record + 16U) != expected->fingerprint_fnv1a64 ||
            read_u32(record + 24U) != expected->payload_offset ||
            !bytes_zero(record + 28U, 4U) ||
            !range_ok(expected->payload_offset, expected->byte_count, size) ||
            fnv1a32(payload + expected->payload_offset,
                    expected->byte_count) !=
                expected->fingerprint_fnv1a32 ||
            fnv1a64(payload + expected->payload_offset,
                    expected->byte_count) !=
                expected->fingerprint_fnv1a64) {
            return false;
        }
    }
    return fnv1a32(
               payload + TECMO_ASSET_PACK_GAMEPLAY_PRETIP_DECODED_OFFSET,
               2048U) == 0xDBF66A45U &&
           fnv1a64(
               payload + TECMO_ASSET_PACK_GAMEPLAY_PRETIP_DECODED_OFFSET,
               2048U) == 0xD1B369CF288E21A5ULL;
}

static bool validate_padding(const uint8_t *payload)
{
    struct Gap { size_t start; size_t end; };
    static const struct Gap gaps[] = {
        {903U,928U},{952U,960U},{3024U,3040U},{3089U,3104U},
        {3403U,3424U},{3480U,3488U},{3496U,3520U},{3583U,3616U},
        {3768U,3776U},{3786U,3808U},{4318U,4352U},{4432U,4448U},
        {4716U,4736U},{4830U,4864U},{5066U,5088U},{5140U,5152U},
        {5535U,5536U},{5682U,5696U},{5817U,5824U},
        {5879U,TECMO_ASSET_PACK_GAMEPLAY_PRETIP_SIZE}
    };
    size_t index;
    for (index = 0U; index < sizeof(gaps) / sizeof(gaps[0]); ++index)
        if (!bytes_zero(payload + gaps[index].start,
                        gaps[index].end - gaps[index].start)) return false;
    return true;
}

static bool validate_dependency(const uint8_t *bytes, size_t size,
                                const char magic[4], uint16_t version,
                                uint32_t expected_hash,
                                bool size_in_header)
{
    return bytes != NULL && size >= 12U &&
           memcmp(bytes, magic, 4U) == 0 &&
           read_u16(bytes + 4U) == version &&
           (!size_in_header || read_u32(bytes + 8U) == size) &&
           fnv1a32(bytes, size) == expected_hash;
}

static bool parse(TecmoGameplayPreTipAssets *assets,
                  const uint8_t *payload, size_t payload_size,
                  const uint8_t *gameplay, size_t gameplay_size,
                  const uint8_t *team_data, size_t team_data_size,
                  const uint8_t *music, size_t music_size,
                  const uint8_t *warriors, size_t warriors_size,
                  const uint8_t *chr, size_t chr_size)
{
    uint8_t *storage;
    size_t index;
    tecmo_gameplay_pretip_destroy(assets);
    if (!validate_header(payload, payload_size))
        return reject(assets, "TPTI-1 header/size/reserved contract rejected");
    if (fnv1a32(payload, payload_size) !=
            TECMO_ASSET_PACK_GAMEPLAY_PRETIP_FNV1A32 ||
        !validate_sources(payload, payload_size) ||
        !validate_padding(payload))
        return reject(assets, "TPTI-1 canonical/source/bounds contract rejected");
    if (!validate_dependency(gameplay, gameplay_size, "TGPL", 1U,
                             0x2047CCE0U, true) ||
        !validate_dependency(team_data, team_data_size, "TTDT", 1U,
                             0x812628F0U, false) ||
        !validate_dependency(music, music_size, "TMUS", 1U,
                             0x05C00ECBU, true) ||
        !validate_dependency(warriors, warriors_size, "TWAR", 1U,
                             TECMO_ASSET_PACK_GAMEPLAY_PRETIP_TWAR_FNV1A32,
                             false) ||
        chr == NULL || chr_size != 262144U ||
        fnv1a32(chr, chr_size) != 0xF6F6E854U ||
        fnv1a64(chr, chr_size) != 0x96A64F53B240ABB4ULL)
        return reject(assets,
                      "TPTI-1 same-pack TGPL/TTDT/TMUS/TWAR/CHR dependency rejected");
    storage = (uint8_t *)malloc(payload_size);
    if (storage == NULL) return reject(assets, "TPTI-1 allocation failed");
    memcpy(storage, payload, payload_size);
    assets->storage = storage;
    assets->storage_size = payload_size;
    memcpy(assets->descriptor,
           storage + TECMO_ASSET_PACK_GAMEPLAY_PRETIP_DESCRIPTOR_OFFSET, 7U);
    assets->card_chr_selector[0] = storage[190U];
    assets->card_chr_selector[1] = storage[191U];
    assets->tip_actor_indices[0U] = storage[
        TECMO_ASSET_PACK_GAMEPLAY_PRETIP_TIP_AWAY_ACTOR_OFFSET];
    assets->tip_actor_indices[1U] = storage[
        TECMO_ASSET_PACK_GAMEPLAY_PRETIP_TIP_HOME_ACTOR_OFFSET];
    assets->nametables =
        storage + TECMO_ASSET_PACK_GAMEPLAY_PRETIP_DECODED_OFFSET;
    assets->palette =
        storage + TECMO_ASSET_PACK_GAMEPLAY_PRETIP_PALETTE_OFFSET;
    assets->character_map =
        storage + TECMO_ASSET_PACK_GAMEPLAY_PRETIP_CHARMAP_OFFSET;
    assets->character_tiles =
        storage + TECMO_ASSET_PACK_GAMEPLAY_PRETIP_CHARACTER_TILES_OFFSET;
    for (index = 0U; index < TECMO_GAMEPLAY_PRETIP_PHASE_COUNT; ++index)
        assets->phase_frames[index] =
            read_u16(storage + 156U + index * 2U);
    for (index = 0U; index < TECMO_GAMEPLAY_PRETIP_SOURCE_COUNT; ++index) {
        const TecmoGameplayPreTipExpectedSource *expected =
            &tecmo_gameplay_pretip_expected_sources[index];
        TecmoGameplayPreTipSourceSpan *span = &assets->sources[index];
        span->kind = expected->kind;
        span->bank = expected->bank;
        span->fixed_bank = expected->fixed_bank != 0U;
        span->cpu_start = expected->cpu_start;
        span->cpu_end = (uint16_t)(
            expected->cpu_start + expected->byte_count - 1U);
        span->byte_count = expected->byte_count;
        span->fingerprint_fnv1a32 = expected->fingerprint_fnv1a32;
        span->fingerprint_fnv1a64 = expected->fingerprint_fnv1a64;
        span->bytes = storage + expected->payload_offset;
    }
    assets->gameplay_core_fingerprint = 0x2047CCE0U;
    assets->team_data_fingerprint = 0x812628F0U;
    assets->music_fingerprint = 0x05C00ECBU;
    assets->warriors_fingerprint =
        TECMO_ASSET_PACK_GAMEPLAY_PRETIP_TWAR_FNV1A32;
    assets->chr_fingerprint32 = 0xF6F6E854U;
    assets->chr_fingerprint64 = 0x96A64F53B240ABB4ULL;
    assets->available = true;
    (void)snprintf(assets->status, sizeof(assets->status),
                   "TPTI-1 native pre-tip assetpack");
    return true;
}

bool tecmo_gameplay_pretip_load(TecmoGameplayPreTipAssets *assets,
                                const char *asset_pack_path)
{
    static const char *ids[] = {
        TECMO_ASSET_PACK_GAMEPLAY_PRETIP_ID,
        "gameplay/core", "menu/team-data", "audio/music",
        "arena/intro/warriors-transition", "chr/all"
    };
    static const uint64_t sizes[] = {
        TECMO_ASSET_PACK_GAMEPLAY_PRETIP_SIZE,
        23416U,96372U,36784U,
        TECMO_ASSET_PACK_GAMEPLAY_PRETIP_TWAR_SIZE,262144U
    };
    uint8_t *bytes[6] = {NULL,NULL,NULL,NULL,NULL,NULL};
    uint64_t counts[6] = {0U,0U,0U,0U,0U,0U};
    bool loaded = false;
    size_t index;
    if (assets == NULL || assets->lifecycle_tag != PRETIP_LIFECYCLE_TAG)
        return false;
    tecmo_gameplay_pretip_destroy(assets);
    if (asset_pack_path == NULL) {
        return reject(assets, "TPTI-1 explicit asset pack unavailable");
    }
    for (index = 0U; index < 6U; ++index) {
        if (tecmo_asset_pack_read_entry_exact(
                asset_pack_path, ids[index], sizes[index],
                &bytes[index], &counts[index]) != 0) {
            char status[192];
            size_t cleanup;
            (void)snprintf(status, sizeof(status),
                           "TPTI-1 %s missing or wrong-sized", ids[index]);
            for (cleanup = 0U; cleanup < 6U; ++cleanup)
                tecmo_asset_pack_free(bytes[cleanup]);
            return reject(assets, status);
        }
    }
    loaded = parse(
        assets,
        bytes[0], (size_t)counts[0], bytes[1], (size_t)counts[1],
        bytes[2], (size_t)counts[2], bytes[3], (size_t)counts[3],
        bytes[4], (size_t)counts[4], bytes[5], (size_t)counts[5]);
    for (index = 0U; index < 6U; ++index)
        tecmo_asset_pack_free(bytes[index]);
    return loaded;
}

static bool assets_valid(const TecmoGameplayPreTipAssets *assets)
{
    uint64_t total = 0U;
    size_t index;
    if (assets == NULL || assets->lifecycle_tag != PRETIP_LIFECYCLE_TAG ||
        !assets->available || assets->storage == NULL ||
        assets->storage_size != TECMO_ASSET_PACK_GAMEPLAY_PRETIP_SIZE ||
        assets->nametables != assets->storage +
            TECMO_ASSET_PACK_GAMEPLAY_PRETIP_DECODED_OFFSET ||
        assets->palette != assets->storage +
            TECMO_ASSET_PACK_GAMEPLAY_PRETIP_PALETTE_OFFSET ||
        assets->character_map != assets->storage +
            TECMO_ASSET_PACK_GAMEPLAY_PRETIP_CHARMAP_OFFSET ||
        assets->character_tiles != assets->storage +
            TECMO_ASSET_PACK_GAMEPLAY_PRETIP_CHARACTER_TILES_OFFSET ||
        assets->card_chr_selector[0] != 0xC6U ||
        assets->card_chr_selector[1] != 0xFAU ||
        assets->tip_actor_indices[0U] != assets->storage[
            TECMO_ASSET_PACK_GAMEPLAY_PRETIP_TIP_AWAY_ACTOR_OFFSET] ||
        assets->tip_actor_indices[1U] != assets->storage[
            TECMO_ASSET_PACK_GAMEPLAY_PRETIP_TIP_HOME_ACTOR_OFFSET] ||
        assets->tip_actor_indices[0U] >=
            TECMO_GAMEPLAY_PRETIP_PLAYER_COUNT / 2U ||
        assets->tip_actor_indices[1U] <
            TECMO_GAMEPLAY_PRETIP_PLAYER_COUNT / 2U ||
        assets->tip_actor_indices[1U] >= TECMO_GAMEPLAY_PRETIP_PLAYER_COUNT ||
        assets->tip_actor_indices[0U] == assets->tip_actor_indices[1U] ||
        fnv1a32(assets->storage, assets->storage_size) !=
            TECMO_ASSET_PACK_GAMEPLAY_PRETIP_FNV1A32 ||
        fnv1a32(
            assets->storage +
                TECMO_ASSET_PACK_GAMEPLAY_PRETIP_TIP_INPUT_OFFSET,
            TECMO_ASSET_PACK_GAMEPLAY_PRETIP_TIP_INPUT_SIZE) !=
            TECMO_ASSET_PACK_GAMEPLAY_PRETIP_TIP_INPUT_FNV1A32 ||
        fnv1a64(
            assets->storage +
                TECMO_ASSET_PACK_GAMEPLAY_PRETIP_TIP_INPUT_OFFSET,
            TECMO_ASSET_PACK_GAMEPLAY_PRETIP_TIP_INPUT_SIZE) !=
            TECMO_ASSET_PACK_GAMEPLAY_PRETIP_TIP_INPUT_FNV1A64) {
        return false;
    }
    for (index = 0U; index < TECMO_GAMEPLAY_PRETIP_PHASE_COUNT; ++index) {
        uint16_t stored = read_u16(assets->storage + 156U + index * 2U);
        if (assets->phase_frames[index] != stored ||
            total > UINT32_MAX - stored) {
            return false;
        }
        total += stored;
    }
    return true;
}

bool tecmo_gameplay_pretip_tip_lineup(
    const TecmoGameplayPreTipAssets *assets,
    TecmoGameplayPreTipLineup *lineup)
{
    static const uint8_t setup_signature[] = {
        0xA2U,0x0AU,
        0xBDU,0xA3U,0xADU,0x95U,0x73U,
        0x9DU,0x5BU,0x05U,
        0xBDU,0xAEU,0xADU,0x95U,0xE8U,
        0x9DU,0x66U,0x05U,
        0xBDU,0xB9U,0xADU,0x95U,0xF3U,
        0x9DU,0x71U,0x05U,
        0xBDU,0x8DU,0xADU,0x9DU,0x79U,0x04U,
        0x8AU,0x9DU,0x6BU,0x07U,
        0xA9U,0x08U,0x9DU,0x2AU,0x04U,
        0xA9U,0x00U,0x9DU,0x58U,0x04U,
        0x9DU,0x7CU,0x05U,
        0xBDU,0x98U,0xADU,0x9DU,0x63U,0x04U,
        0xA8U,
        0xB9U,0xC4U,0xADU,0x9DU,0x42U,0x04U,
        0xB9U,0xCDU,0xADU,0x9DU,0x4DU,0x04U,
        0xBDU,0x82U,0xADU,0x9DU,0x6EU,0x04U,
        0xCAU,0x10U,0xB5U
    };
    const size_t setup_offset = 0xAC8CU - 0xAC76U;
    const size_t state_offset = 0U;
    const size_t sprite_slot_offset = 0xAD8DU - 0xAD82U;
    const size_t facing_offset = 0xAD98U - 0xAD82U;
    const size_t x_low_offset = 0xADA3U - 0xAD82U;
    const size_t x_high_offset = 0xADAEU - 0xAD82U;
    const size_t y_offset = 0xADB9U - 0xAD82U;
    const size_t pose_low_offset = 0xADC4U - 0xAD82U;
    const size_t pose_high_offset = 0xADCDU - 0xAD82U;
    const size_t pose_entry_count = 9U;
    const uint8_t *control;
    const uint8_t *tables;
    TecmoGameplayPreTipLineup candidate;
    size_t index;

    if (!assets_valid(assets) || lineup == NULL) return false;
    control = assets->storage +
        TECMO_ASSET_PACK_GAMEPLAY_PRETIP_CLOSEUP_CONTROL_OFFSET;
    tables = assets->storage +
        TECMO_ASSET_PACK_GAMEPLAY_PRETIP_CLOSEUP_TIMING_OFFSET;
    if (memcmp(control + setup_offset, setup_signature,
               sizeof(setup_signature)) != 0) {
        return false;
    }
    memset(&candidate, 0, sizeof(candidate));
    candidate.contract_tag = TECMO_GAMEPLAY_PRETIP_LINEUP_TAG;
    for (index = 0U; index < TECMO_GAMEPLAY_PRETIP_OBJECT_COUNT;
         ++index) {
        TecmoGameplayCourtCoordinate coordinate;
        uint8_t sprite_slot_base = tables[sprite_slot_offset + index];
        uint8_t facing = tables[facing_offset + index];
        uint16_t raw_pose;
        uint16_t pose_index;
        coordinate.x = (int16_t)(
            (uint16_t)tables[x_low_offset + index] |
            ((uint16_t)tables[x_high_offset + index] << 8U));
        coordinate.y = (int16_t)tables[y_offset + index];
        if (!tecmo_gameplay_court_coordinate_valid(&coordinate) ||
            (sprite_slot_base & 0x3FU) != 0x01U ||
            facing >= pose_entry_count) {
            return false;
        }
        raw_pose = (uint16_t)tables[pose_low_offset + facing] |
                   ((uint16_t)tables[pose_high_offset + facing] << 8U);
        if ((raw_pose & 1U) != 0U) return false;
        pose_index = (uint16_t)(raw_pose >> 1U);
        if (pose_index >= TECMO_GAMEPLAY_ASSET_POINTER_COUNT) return false;
        if (index < TECMO_GAMEPLAY_PRETIP_PLAYER_COUNT) {
            candidate.players[index] = coordinate;
            candidate.player_states[index] = tables[state_offset + index];
            candidate.player_sprite_slot_bases[index] = sprite_slot_base;
            candidate.player_facings[index] = facing;
            candidate.player_pose_indices[index] = pose_index;
        } else {
            candidate.ball = coordinate;
            candidate.ball_state = tables[state_offset + index];
            candidate.ball_sprite_slot_base = sprite_slot_base;
            candidate.ball_facing = facing;
            candidate.ball_pose_index = pose_index;
        }
    }
    *lineup = candidate;
    return true;
}

/* Native policy, not ROM-exact: contest frame 0 is the target, a first held
   sample at frame N has error min(N, 11), and no sample has error 12. */
static uint8_t tip_error_for_sample(uint16_t frame)
{
    return frame < 11U ? (uint8_t)frame : 11U;
}

static bool tip_sample_valid(bool sampled, uint8_t error,
                             uint16_t sample_frame,
                             TecmoGameplayPreTipPhase phase,
                             uint16_t phase_frame,
                             uint16_t contest_duration)
{
    if (!sampled) {
        return error == 12U &&
               sample_frame == TECMO_GAMEPLAY_PRETIP_NO_SAMPLE_FRAME;
    }
    if (phase < TECMO_GAMEPLAY_PRETIP_JUMP_CONTEST ||
        sample_frame >= contest_duration ||
        error != tip_error_for_sample(sample_frame)) {
        return false;
    }
    return phase != TECMO_GAMEPLAY_PRETIP_JUMP_CONTEST ||
           sample_frame < phase_frame;
}

static bool state_valid(const TecmoGameplayPreTipAssets *assets,
                        const TecmoGameplayPreTipState *state)
{
    uint64_t expected_total = 0U;
    size_t index;
    if (!assets_valid(assets) || state == NULL ||
        state->contract_tag != TECMO_GAMEPLAY_PRETIP_STATE_TAG ||
        state->phase < TECMO_GAMEPLAY_PRETIP_PRESEASON ||
        state->phase > TECMO_GAMEPLAY_PRETIP_LIVE) {
        return false;
    }
    for (index = 0U; index < (size_t)state->phase; ++index) {
        uint16_t duration = assets->phase_frames[index];
        if (expected_total > UINT32_MAX - duration) return false;
        expected_total += duration;
    }
    if (state->phase < TECMO_GAMEPLAY_PRETIP_LIVE) {
        uint16_t duration = assets->phase_frames[state->phase];
        if (state->phase_frame >= duration ||
            expected_total > UINT32_MAX - state->phase_frame) {
            return false;
        }
        expected_total += state->phase_frame;
    } else if (state->phase_frame != 0U) {
        return false;
    }
    if (expected_total != state->total_frame ||
        !tip_sample_valid(
            state->away_tip_sampled, state->away_tip_error,
            state->away_tip_sample_frame, state->phase, state->phase_frame,
            assets->phase_frames[TECMO_GAMEPLAY_PRETIP_JUMP_CONTEST]) ||
        !tip_sample_valid(
            state->home_tip_sampled, state->home_tip_error,
            state->home_tip_sample_frame, state->phase, state->phase_frame,
            assets->phase_frames[TECMO_GAMEPLAY_PRETIP_JUMP_CONTEST])) {
        return false;
    }
    if (state->aborted) {
        return state->card_cancel_enabled && !state->live_handoff &&
               state->phase <= TECMO_GAMEPLAY_PRETIP_FIRST_PERIOD;
    }
    if (state->phase == TECMO_GAMEPLAY_PRETIP_LIVE)
        return state->live_handoff;
    return !state->live_handoff;
}

bool tecmo_gameplay_pretip_state_validate(
    const TecmoGameplayPreTipAssets *assets,
    const TecmoGameplayPreTipState *state)
{
    return state_valid(assets, state);
}

bool tecmo_gameplay_pretip_state_initialize(
    const TecmoGameplayPreTipAssets *assets,
    TecmoGameplayPreTipState *state,
    bool card_cancel_enabled)
{
    TecmoGameplayPreTipState initial;
    if (!assets_valid(assets) || state == NULL) return false;
    memset(&initial, 0, sizeof(initial));
    initial.contract_tag = TECMO_GAMEPLAY_PRETIP_STATE_TAG;
    initial.phase = TECMO_GAMEPLAY_PRETIP_PRESEASON;
    initial.away_tip_error = 12U;
    initial.home_tip_error = 12U;
    initial.away_tip_sample_frame = TECMO_GAMEPLAY_PRETIP_NO_SAMPLE_FRAME;
    initial.home_tip_sample_frame = TECMO_GAMEPLAY_PRETIP_NO_SAMPLE_FRAME;
    initial.card_cancel_enabled = card_cancel_enabled;
    if (!state_valid(assets, &initial)) return false;
    *state = initial;
    return true;
}

static void sample_tip(bool held, uint16_t phase_frame,
                       bool *sampled, uint8_t *error,
                       uint16_t *sample_frame)
{
    if (!held || *sampled) return;
    *sampled = true;
    *error = tip_error_for_sample(phase_frame);
    *sample_frame = phase_frame;
}

bool tecmo_gameplay_pretip_update(
    const TecmoGameplayPreTipAssets *assets,
    TecmoGameplayPreTipState *state,
    bool player_one_or_away_held_b,
    bool player_two_or_home_held_b)
{
    TecmoGameplayPreTipState candidate;
    uint16_t duration;
    if (!state_valid(assets, state) ||
        state->aborted || state->live_handoff ||
        state->phase >= TECMO_GAMEPLAY_PRETIP_LIVE) {
        return false;
    }
    candidate = *state;
    if (candidate.card_cancel_enabled &&
        candidate.phase <= TECMO_GAMEPLAY_PRETIP_FIRST_PERIOD &&
        (player_one_or_away_held_b || player_two_or_home_held_b)) {
        candidate.aborted = true;
        if (!state_valid(assets, &candidate)) return false;
        *state = candidate;
        return true;
    }
    if (candidate.phase == TECMO_GAMEPLAY_PRETIP_JUMP_CONTEST) {
        sample_tip(player_one_or_away_held_b, candidate.phase_frame,
                   &candidate.away_tip_sampled, &candidate.away_tip_error,
                   &candidate.away_tip_sample_frame);
        sample_tip(player_two_or_home_held_b, candidate.phase_frame,
                   &candidate.home_tip_sampled, &candidate.home_tip_error,
                   &candidate.home_tip_sample_frame);
    }
    duration = assets->phase_frames[candidate.phase];
    if (candidate.phase_frame == UINT16_MAX ||
        candidate.total_frame == UINT32_MAX) {
        return false;
    }
    ++candidate.phase_frame;
    ++candidate.total_frame;
    if (candidate.phase_frame >= duration) {
        candidate.phase_frame = 0U;
        candidate.phase =
            (TecmoGameplayPreTipPhase)(candidate.phase + 1);
        if (candidate.phase == TECMO_GAMEPLAY_PRETIP_LIVE)
            candidate.live_handoff = true;
    }
    if (!state_valid(assets, &candidate)) return false;
    *state = candidate;
    return true;
}

bool tecmo_gameplay_pretip_tip_winner(
    const TecmoGameplayPreTipAssets *assets,
    const TecmoGameplayPreTipState *state,
    uint8_t *winner)
{
    if (winner == NULL || !state_valid(assets, state) || state->aborted ||
        state->phase < TECMO_GAMEPLAY_PRETIP_JUMP_CONTEST) {
        return false;
    }
    *winner = state->home_tip_error < state->away_tip_error
                  ? TECMO_GAMEPLAY_PRETIP_HOME_WINNER
                  : TECMO_GAMEPLAY_PRETIP_AWAY_WINNER;
    return true;
}

bool tecmo_gameplay_pretip_is_presentation(
    const TecmoGameplayPreTipState *state)
{
    return state != NULL &&
           state->contract_tag == TECMO_GAMEPLAY_PRETIP_STATE_TAG &&
           !state->aborted && !state->live_handoff &&
           state->phase < TECMO_GAMEPLAY_PRETIP_LIVE;
}

const char *tecmo_gameplay_pretip_phase_name(TecmoGameplayPreTipPhase phase)
{
    switch (phase) {
    case TECMO_GAMEPLAY_PRETIP_PRESEASON: return "preseason";
    case TECMO_GAMEPLAY_PRETIP_MATCHUP: return "matchup";
    case TECMO_GAMEPLAY_PRETIP_FIRST_PERIOD: return "first-period";
    case TECMO_GAMEPLAY_PRETIP_CLOSEUP: return "closeup";
    case TECMO_GAMEPLAY_PRETIP_CENTER_COURT_SETUP: return "center-setup";
    case TECMO_GAMEPLAY_PRETIP_BALL_DESCENT: return "ball-descent";
    case TECMO_GAMEPLAY_PRETIP_TOSS_CLOSEUP: return "toss-closeup";
    case TECMO_GAMEPLAY_PRETIP_JUMP_CONTEST: return "jump-contest";
    case TECMO_GAMEPLAY_PRETIP_LIVE: return "live";
    default: return "invalid";
    }
}

static bool update_rejects_unchanged(
    const TecmoGameplayPreTipAssets *assets,
    TecmoGameplayPreTipState *state)
{
    TecmoGameplayPreTipState before = *state;
    return !tecmo_gameplay_pretip_update(
               assets, state, false, false) &&
           memcmp(&before, state, sizeof(before)) == 0;
}

static bool tip_winner_gate_regression(
    const TecmoGameplayPreTipAssets *assets,
    char *message,
    size_t message_size)
{
    TecmoGameplayPreTipState precontest;
    TecmoGameplayPreTipState equal_errors;
    TecmoGameplayPreTipState home_sample;
    TecmoGameplayPreTipState before;
    uint8_t winner;
    unsigned frame;

    if (!tecmo_gameplay_pretip_state_initialize(
            assets, &precontest, false)) {
        if (message != NULL && message_size > 0U)
            (void)snprintf(message, message_size,
                           "TPTI-1 winner gate regression failed: "
                           "pre-contest setup initialization");
        return false;
    }
    for (frame = 0U; frame < 451U; ++frame) {
        if (!tecmo_gameplay_pretip_update(
                assets, &precontest, false, false)) {
            if (message != NULL && message_size > 0U)
                (void)snprintf(message, message_size,
                               "TPTI-1 winner gate regression failed: "
                               "pre-contest setup advance");
            return false;
        }
    }
    if (precontest.phase != TECMO_GAMEPLAY_PRETIP_CENTER_COURT_SETUP ||
        precontest.phase_frame != 0U) {
        if (message != NULL && message_size > 0U)
            (void)snprintf(message, message_size,
                           "TPTI-1 winner gate regression failed: "
                           "intermediate phase setup");
        return false;
    }
    before = precontest;
    winner = 0xA5U;
    if (tecmo_gameplay_pretip_tip_winner(
            assets, &precontest, &winner) || winner != 0xA5U ||
        memcmp(&before, &precontest, sizeof(before)) != 0) {
        if (message != NULL && message_size > 0U)
            (void)snprintf(message, message_size,
                           "TPTI-1 winner gate regression failed: "
                           "pre-contest winner accepted or output mutated");
        return false;
    }

    if (!tecmo_gameplay_pretip_state_initialize(
            assets, &equal_errors, false) ||
        !tecmo_gameplay_pretip_state_initialize(
            assets, &home_sample, false)) {
        if (message != NULL && message_size > 0U)
            (void)snprintf(message, message_size,
                           "TPTI-1 winner gate regression failed: "
                           "contest setup initialization");
        return false;
    }
    for (frame = 0U; frame < 661U; ++frame) {
        if (!tecmo_gameplay_pretip_update(
                assets, &equal_errors, false, false) ||
            !tecmo_gameplay_pretip_update(
                assets, &home_sample, false, false)) {
            if (message != NULL && message_size > 0U)
                (void)snprintf(message, message_size,
                               "TPTI-1 winner gate regression failed: "
                               "contest setup advance");
            return false;
        }
    }
    winner = 0xA5U;
    if (!tecmo_gameplay_pretip_update(
            assets, &equal_errors, true, true) ||
        !tecmo_gameplay_pretip_tip_winner(
            assets, &equal_errors, &winner) ||
        winner != TECMO_GAMEPLAY_PRETIP_AWAY_WINNER) {
        if (message != NULL && message_size > 0U)
            (void)snprintf(message, message_size,
                           "TPTI-1 winner gate regression failed: "
                           "equal-error contest did not settle Away");
        return false;
    }
    winner = TECMO_GAMEPLAY_PRETIP_AWAY_WINNER;
    if (!tecmo_gameplay_pretip_update(
            assets, &home_sample, false, true) ||
        !tecmo_gameplay_pretip_tip_winner(
            assets, &home_sample, &winner) ||
        winner != TECMO_GAMEPLAY_PRETIP_HOME_WINNER) {
        if (message != NULL && message_size > 0U)
            (void)snprintf(message, message_size,
                           "TPTI-1 winner gate regression failed: "
                           "valid Home sample did not win");
        return false;
    }
    return true;
}

bool tecmo_gameplay_pretip_self_test(const char *asset_pack_path,
                                     char *message,
                                     size_t message_size)
{
    TecmoGameplayPreTipAssets assets;
    TecmoGameplayPreTipState state;
    TecmoGameplayPreTipState preseason_b_state;
    TecmoGameplayPreTipState cancel_state;
    TecmoGameplayPreTipState malformed;
    TecmoGameplayPreTipState sampled_state;
    TecmoGameplayPreTipState last_state;
    TecmoGameplayPreTipState boundary_state;
    TecmoGameplayPreTipLineup lineup;
    TecmoGameplayPreTipLineup unchanged_lineup;
    TecmoGameplayPreTipLineup before_lineup;
    static const TecmoGameplayCourtCoordinate expected_players[
        TECMO_GAMEPLAY_PRETIP_PLAYER_COUNT] = {
            {528,144},{448,144},{362,112},{364,192},{392,144},
            {176,144},{320,144},{408,112},{400,192},{372,144}
    };
    static const uint8_t expected_states[
        TECMO_GAMEPLAY_PRETIP_PLAYER_COUNT] = {
            0x00U,0x00U,0x00U,0x00U,0x22U,
            0x00U,0x00U,0x00U,0x00U,0x13U
    };
    static const uint8_t expected_sprite_slot_bases[
        TECMO_GAMEPLAY_PRETIP_PLAYER_COUNT] = {
            0xC1U,0xC1U,0xC1U,0xC1U,0x41U,
            0xC1U,0xC1U,0xC1U,0xC1U,0x81U
    };
    static const uint8_t expected_facings[
        TECMO_GAMEPLAY_PRETIP_PLAYER_COUNT] = {
            1U,1U,2U,5U,1U,0U,0U,2U,5U,0U
    };
    static const uint16_t expected_pose_indices[
        TECMO_GAMEPLAY_PRETIP_PLAYER_COUNT] = {
            517U,517U,520U,519U,517U,
            518U,518U,520U,519U,518U
    };
    unsigned frame;
    uint8_t winner = 0xFFU;
    bool ok;
    if (message != NULL && message_size > 0U) message[0] = '\0';
    tecmo_gameplay_pretip_init(&assets);
    memset(&state, 0, sizeof(state));
    memset(&preseason_b_state, 0, sizeof(preseason_b_state));
    memset(&cancel_state, 0, sizeof(cancel_state));
    memset(&malformed, 0, sizeof(malformed));
    memset(&sampled_state, 0, sizeof(sampled_state));
    memset(&last_state, 0, sizeof(last_state));
    memset(&boundary_state, 0, sizeof(boundary_state));
    memset(&lineup, 0, sizeof(lineup));
    memset(&unchanged_lineup, 0xA5, sizeof(unchanged_lineup));
    before_lineup = unchanged_lineup;
    ok = tecmo_gameplay_pretip_load(&assets, asset_pack_path) &&
         assets.tip_actor_indices[0U] == 4U &&
         assets.tip_actor_indices[1U] == 9U &&
         tecmo_gameplay_pretip_tip_lineup(&assets, &lineup) &&
         lineup.contract_tag == TECMO_GAMEPLAY_PRETIP_LINEUP_TAG &&
         memcmp(lineup.players, expected_players,
                sizeof(expected_players)) == 0 &&
         lineup.ball.x == 384 && lineup.ball.y == 144 &&
         memcmp(lineup.player_states, expected_states,
                sizeof(expected_states)) == 0 &&
         memcmp(lineup.player_sprite_slot_bases,
                expected_sprite_slot_bases,
                sizeof(expected_sprite_slot_bases)) == 0 &&
         memcmp(lineup.player_facings, expected_facings,
                sizeof(expected_facings)) == 0 &&
         memcmp(lineup.player_pose_indices, expected_pose_indices,
                sizeof(expected_pose_indices)) == 0 &&
         lineup.ball_state == 0x1AU &&
         lineup.ball_sprite_slot_base == 0xC1U &&
         lineup.ball_facing == 8U &&
         lineup.ball_pose_index == 64U &&
         !tecmo_gameplay_pretip_tip_lineup(NULL, &unchanged_lineup) &&
         memcmp(&unchanged_lineup, &before_lineup,
                sizeof(unchanged_lineup)) == 0 &&
         !tecmo_gameplay_pretip_tip_lineup(&assets, NULL) &&
         tecmo_gameplay_pretip_state_initialize(
             &assets, &state, false) &&
         tecmo_gameplay_pretip_state_initialize(
             &assets, &preseason_b_state, false) &&
         tecmo_gameplay_pretip_state_initialize(
             &assets, &cancel_state, true) &&
         tecmo_gameplay_pretip_update(
             &assets, &preseason_b_state, false, true) &&
         !preseason_b_state.aborted &&
         preseason_b_state.phase == TECMO_GAMEPLAY_PRETIP_PRESEASON &&
         preseason_b_state.phase_frame == 1U &&
         preseason_b_state.total_frame == 1U &&
         tecmo_gameplay_pretip_update(
             &assets, &cancel_state, true, false) &&
         cancel_state.aborted && cancel_state.card_cancel_enabled &&
         cancel_state.phase_frame == 0U &&
         cancel_state.total_frame == 0U &&
         update_rejects_unchanged(&assets, &cancel_state);

    if (ok && !tip_winner_gate_regression(
                  &assets, message, message_size)) {
        ok = false;
    }

    if (ok) {
        malformed = state;
        malformed.contract_tag ^= 1U;
        ok = update_rejects_unchanged(&assets, &malformed);
    }
    if (ok) {
        malformed = state;
        malformed.phase = (TecmoGameplayPreTipPhase)0x7FFF;
        ok = update_rejects_unchanged(&assets, &malformed);
    }
    if (ok) {
        malformed = state;
        malformed.phase_frame =
            assets.phase_frames[TECMO_GAMEPLAY_PRETIP_PRESEASON];
        ok = update_rejects_unchanged(&assets, &malformed);
    }
    if (ok) {
        malformed = state;
        malformed.total_frame = 1U;
        ok = update_rejects_unchanged(&assets, &malformed);
    }
    if (ok) {
        malformed = state;
        malformed.away_tip_error = 11U;
        ok = update_rejects_unchanged(&assets, &malformed);
    }
    if (ok) {
        malformed = state;
        malformed.away_tip_sampled = true;
        malformed.away_tip_sample_frame = 0U;
        malformed.away_tip_error = 11U;
        ok = update_rejects_unchanged(&assets, &malformed);
    }
    if (ok) {
        malformed = state;
        malformed.total_frame = UINT32_MAX;
        ok = update_rejects_unchanged(&assets, &malformed);
    }
    if (ok) {
        malformed = state;
        malformed.live_handoff = true;
        ok = update_rejects_unchanged(&assets, &malformed);
    }

    for (frame = 0U; ok && frame < 437U; ++frame)
        ok = tecmo_gameplay_pretip_update(
            &assets, &state, false, false);
    ok = ok &&
         state.phase == TECMO_GAMEPLAY_PRETIP_CLOSEUP &&
         state.phase_frame == 194U &&
         tecmo_gameplay_pretip_update(
             &assets, &state, false, true) &&
         !state.home_tip_sampled && !state.away_tip_sampled &&
         state.phase_frame == 195U && state.away_tip_error == 12U;
    if (ok) {
        malformed = state;
        malformed.home_tip_error = 1U;
        ok = update_rejects_unchanged(&assets, &malformed);
    }
    if (ok) {
        malformed = state;
        malformed.home_tip_sample_frame = malformed.phase_frame;
        malformed.home_tip_error = tip_error_for_sample(
            malformed.home_tip_sample_frame);
        ok = update_rejects_unchanged(&assets, &malformed);
    }
    sampled_state = state;
    for (frame = 438U; ok && frame < 661U; ++frame)
        ok = tecmo_gameplay_pretip_update(
            &assets, &sampled_state, false, false);
    ok = ok &&
         sampled_state.phase == TECMO_GAMEPLAY_PRETIP_JUMP_CONTEST &&
         sampled_state.phase_frame == 0U &&
         tecmo_gameplay_pretip_update(
             &assets, &sampled_state, false, true) &&
         sampled_state.home_tip_sampled &&
         sampled_state.home_tip_sample_frame == 0U &&
         sampled_state.home_tip_error == 0U &&
         tecmo_gameplay_pretip_update(
             &assets, &sampled_state, false, true) &&
         sampled_state.home_tip_sample_frame == 0U &&
         sampled_state.home_tip_error == 0U &&
         tecmo_gameplay_pretip_tip_winner(
             &assets, &sampled_state, &winner) &&
         winner == TECMO_GAMEPLAY_PRETIP_HOME_WINNER;
    if (ok) {
        malformed = sampled_state;
        malformed.home_tip_sample_frame = malformed.phase_frame;
        malformed.home_tip_error = tip_error_for_sample(
            malformed.home_tip_sample_frame);
        ok = update_rejects_unchanged(&assets, &malformed);
    }
    if (ok) state = sampled_state;
    for (frame = 663U; ok && frame < 691U; ++frame)
        ok = tecmo_gameplay_pretip_update(
            &assets, &state, false, false);
    ok = ok && state.phase == TECMO_GAMEPLAY_PRETIP_LIVE &&
         state.live_handoff && state.total_frame == 691U &&
         tecmo_gameplay_pretip_tip_winner(
             &assets, &state, &winner) &&
         winner == TECMO_GAMEPLAY_PRETIP_HOME_WINNER &&
         update_rejects_unchanged(&assets, &state);
    if (ok) {
        malformed = state;
        ok = !tecmo_gameplay_pretip_update(
                 &assets, &state, true, true) &&
             memcmp(&malformed, &state, sizeof(state)) == 0;
    }
    if (ok && tecmo_gameplay_pretip_state_initialize(
                  &assets, &last_state, false)) {
        for (frame = 0U; ok && frame < 661U; ++frame)
            ok = tecmo_gameplay_pretip_update(
                &assets, &last_state, false, false);
        for (frame = 0U; ok && frame < 29U; ++frame)
            ok = tecmo_gameplay_pretip_update(
                &assets, &last_state, false, false);
        ok = ok && last_state.phase ==
                 TECMO_GAMEPLAY_PRETIP_JUMP_CONTEST &&
             last_state.phase_frame == 29U &&
             tecmo_gameplay_pretip_update(
                 &assets, &last_state, false, true) &&
             last_state.phase == TECMO_GAMEPLAY_PRETIP_LIVE &&
             last_state.home_tip_sampled &&
             last_state.home_tip_sample_frame == 29U &&
             last_state.home_tip_error == 11U &&
             tecmo_gameplay_pretip_tip_winner(
                 &assets, &last_state, &winner) &&
             winner == TECMO_GAMEPLAY_PRETIP_HOME_WINNER;
    } else {
        ok = false;
    }
    if (ok && tecmo_gameplay_pretip_state_initialize(
                  &assets, &boundary_state, false)) {
        for (frame = 0U; ok && frame < 660U; ++frame)
            ok = tecmo_gameplay_pretip_update(
                &assets, &boundary_state, false, false);
        ok = ok && boundary_state.phase ==
                 TECMO_GAMEPLAY_PRETIP_TOSS_CLOSEUP &&
             boundary_state.phase_frame == 59U &&
             tecmo_gameplay_pretip_update(
                 &assets, &boundary_state, false, true) &&
             boundary_state.phase == TECMO_GAMEPLAY_PRETIP_JUMP_CONTEST &&
             boundary_state.phase_frame == 0U &&
             !boundary_state.home_tip_sampled &&
             tecmo_gameplay_pretip_update(
                 &assets, &boundary_state, false, true) &&
             boundary_state.home_tip_sampled &&
             boundary_state.home_tip_sample_frame == 0U &&
             boundary_state.home_tip_error == 0U;
    } else {
        ok = false;
    }
    if (message != NULL && message_size > 0U && ok)
        (void)snprintf(message, message_size,
                       "TPTI-1 pre-tip self-test passed");
    else if (message != NULL && message_size > 0U && message[0] == '\0')
        (void)snprintf(message, message_size,
                       "TPTI-1 pre-tip state/transaction self-test failed");
    tecmo_gameplay_pretip_destroy(&assets);
    return ok;
}
