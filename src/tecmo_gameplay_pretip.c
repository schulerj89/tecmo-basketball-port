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

static uint32_t full_payload_fingerprint32(const uint8_t *bytes, size_t count)
{
    uint32_t hash = 2166136261U;
    size_t index;
    for (index = 0U; index < count; ++index) {
        hash ^= bytes[index];
        hash *= 16777619U;
    }
    return hash;
}

static uint64_t full_payload_fingerprint64(const uint8_t *bytes, size_t count)
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

static bool span_contains(size_t outer_offset, size_t outer_size,
                          size_t inner_offset, size_t inner_size)
{
    return inner_offset >= outer_offset &&
           inner_offset - outer_offset <= outer_size &&
           inner_size <= outer_size - (inner_offset - outer_offset);
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
    assets->mechanics_fingerprint32 = 0U;
    assets->mechanics_fingerprint64 = 0U;
    assets->tgjs_fingerprint = 0U;
    assets->tip_input_mask = 0U;
    assets->tip_no_sample_error = 0U;
    assets->tip_max_sample_error = 0U;
    assets->tip_auto_threshold_base = 0U;
    assets->tip_auto_threshold_mask = 0U;
    assets->tip_auto_threshold_shift = 0U;
    assets->tip_claim_ball_high_min = 0U;
    assets->tip_claim_ball_minus_jumper_limit = 0U;
    assets->tip_actor_jump_commit_state = 0U;
    assets->tip_slot10_claim_commit_state = 0U;
    assets->tip_selector_0380_address = 0U;
    assets->tip_selector_037f_address = 0U;
    assets->tip_jump_post_store_address = 0U;
    assets->available = false;
    (void)snprintf(assets->status, sizeof(assets->status), "%s",
                   message != NULL ? message : "TPTI-2 rejected");
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
        payload[172U] != TECMO_GAMEPLAY_PRETIP_INPUT_MASK ||
        payload[173U] != TECMO_GAMEPLAY_PRETIP_NO_SAMPLE_ERROR ||
        payload[174U] != TECMO_GAMEPLAY_PRETIP_MAX_SAMPLE_ERROR ||
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
        payload[189U] != 0U ||
        payload[190U] != 0xC6U || payload[191U] != 0xFAU ||
        read_u16(payload + 192U) !=
            TECMO_ASSET_PACK_GAMEPLAY_PRETIP_DEPENDENCY_COUNT ||
        read_u16(payload + 194U) != TECMO_GAMEPLAY_PRETIP_TPM2_VERSION ||
        read_u32(payload + TECMO_ASSET_PACK_GAMEPLAY_PRETIP_TPM2_OFFSET_FIELD) !=
            TECMO_ASSET_PACK_GAMEPLAY_PRETIP_MECHANICS_OFFSET ||
        read_u32(payload + TECMO_ASSET_PACK_GAMEPLAY_PRETIP_TPM2_SIZE_FIELD) !=
            TECMO_ASSET_PACK_GAMEPLAY_PRETIP_MECHANICS_SIZE ||
        read_u32(payload + TECMO_ASSET_PACK_GAMEPLAY_PRETIP_EXACT_SOURCE_BASE_FIELD) !=
            TECMO_ASSET_PACK_GAMEPLAY_PRETIP_EXACT_SOURCE_OFFSET ||
        !bytes_zero(payload +
                        TECMO_ASSET_PACK_GAMEPLAY_PRETIP_HEADER_RESERVED_ZERO_START,
                    TECMO_ASSET_PACK_GAMEPLAY_PRETIP_HEADER_RESERVED_ZERO_END -
                        TECMO_ASSET_PACK_GAMEPLAY_PRETIP_HEADER_RESERVED_ZERO_START) ||
        read_u32(payload + TECMO_ASSET_PACK_GAMEPLAY_PRETIP_TGJS_SIZE_OFFSET) !=
            TECMO_ASSET_PACK_GAMEPLAY_PRETIP_TGJS_SIZE ||
        read_u32(payload + TECMO_ASSET_PACK_GAMEPLAY_PRETIP_TGJS_FNV32_OFFSET) !=
            TECMO_ASSET_PACK_GAMEPLAY_PRETIP_TGJS_FNV1A32 ||
        read_u16(payload + TECMO_ASSET_PACK_GAMEPLAY_PRETIP_TGJS_VERSION_OFFSET) !=
            TECMO_ASSET_PACK_GAMEPLAY_PRETIP_TGJS_VERSION ||
        read_u16(payload + TECMO_ASSET_PACK_GAMEPLAY_PRETIP_TGJS_HEADER_OFFSET) !=
            TECMO_ASSET_PACK_GAMEPLAY_PRETIP_TGJS_HEADER_SIZE ||
        read_u32(payload + 232U) != fnv1a32(
            payload + TECMO_ASSET_PACK_GAMEPLAY_PRETIP_MECHANICS_OFFSET,
            TECMO_ASSET_PACK_GAMEPLAY_PRETIP_MECHANICS_SIZE) ||
        read_u64(payload + 236U) != fnv1a64(
            payload + TECMO_ASSET_PACK_GAMEPLAY_PRETIP_MECHANICS_OFFSET,
            TECMO_ASSET_PACK_GAMEPLAY_PRETIP_MECHANICS_SIZE) ||
        !bytes_zero(payload + 244U,
                    TECMO_ASSET_PACK_GAMEPLAY_PRETIP_HEADER_SIZE - 244U)) {
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
    if (!span_contains(
            TECMO_ASSET_PACK_GAMEPLAY_PRETIP_CLOSEUP_ENTRY_OFFSET, 510U,
            TECMO_ASSET_PACK_GAMEPLAY_PRETIP_CAPTURE_ERROR_OFFSET, 311U) ||
        !span_contains(
            TECMO_ASSET_PACK_GAMEPLAY_PRETIP_TIP_SETUP_OFFSET, 52U,
            TECMO_ASSET_PACK_GAMEPLAY_PRETIP_OPPOSING_ACTOR_OFFSET, 49U)) {
        return false;
    }
    return fnv1a32(
               payload + TECMO_ASSET_PACK_GAMEPLAY_PRETIP_DECODED_OFFSET,
               2048U) == 0xDBF66A45U &&
           fnv1a64(
               payload + TECMO_ASSET_PACK_GAMEPLAY_PRETIP_DECODED_OFFSET,
               2048U) == 0xD1B369CF288E21A5ULL;
}

static bool validate_mechanics(const uint8_t *payload)
{
    const uint8_t *block;
    if (payload == NULL) return false;
    block = payload + TECMO_ASSET_PACK_GAMEPLAY_PRETIP_MECHANICS_OFFSET;
    return memcmp(block, "TPM2", 4U) == 0 &&
           read_u16(block + 4U) == TECMO_GAMEPLAY_PRETIP_TPM2_VERSION &&
           read_u16(block + 6U) == TECMO_GAMEPLAY_PRETIP_TPM2_SIZE &&
           block[8U] == TECMO_GAMEPLAY_PRETIP_INPUT_MASK &&
           block[9U] == TECMO_GAMEPLAY_PRETIP_NO_SAMPLE_ERROR &&
           block[10U] == TECMO_GAMEPLAY_PRETIP_MAX_SAMPLE_ERROR &&
           block[11U] == TECMO_GAMEPLAY_PRETIP_AUTO_THRESHOLD_BASE &&
           block[12U] == TECMO_GAMEPLAY_PRETIP_AUTO_THRESHOLD_MASK &&
           block[13U] == TECMO_GAMEPLAY_PRETIP_AUTO_THRESHOLD_SHIFT &&
           block[14U] == TECMO_GAMEPLAY_PRETIP_CLAIM_BALL_HIGH_MIN &&
           block[15U] == TECMO_GAMEPLAY_PRETIP_CLAIM_BALL_MINUS_JUMPER_LIMIT &&
           block[16U] == TECMO_GAMEPLAY_PRETIP_ACTOR_JUMP_COMMIT_STATE &&
           block[17U] == TECMO_GAMEPLAY_PRETIP_SLOT10_CLAIM_COMMIT_STATE &&
           read_u16(block + 18U) == 0x0380U &&
           read_u16(block + 20U) == 0x037FU &&
           read_u16(block + 22U) == 0xA2D5U &&
           read_u16(block + 24U) == 0xCD96U &&
           read_u16(block + 26U) == 22U &&
           read_u16(block + 28U) == 0xE56EU &&
           read_u16(block + 30U) == 1U &&
           block[32U] == TECMO_GAMEPLAY_PRETIP_SOURCE_CAPTURE_ERROR &&
           block[33U] == TECMO_GAMEPLAY_PRETIP_SOURCE_ACTOR_DISPATCHER &&
           block[34U] == TECMO_GAMEPLAY_PRETIP_SOURCE_AUTOMATIC_ACTOR_PATH &&
           block[35U] == TECMO_GAMEPLAY_PRETIP_SOURCE_OPPOSING_DISPATCHER &&
           block[36U] == TECMO_GAMEPLAY_PRETIP_SOURCE_OPPOSING_ACTOR_PATH &&
           block[37U] == TECMO_GAMEPLAY_PRETIP_SOURCE_JUMP_COMMIT &&
           block[38U] == TECMO_GAMEPLAY_PRETIP_SOURCE_SLOT10_CLAIM &&
           block[39U] == TECMO_GAMEPLAY_PRETIP_SOURCE_E56E_HOOK_ANCHOR &&
           block[40U] == TECMO_GAMEPLAY_PRETIP_SOURCE_RNG_MIX &&
           block[41U] == 0x5AU && block[42U] == 0U && block[43U] == 0U &&
           block[44U] == TECMO_GAMEPLAY_PRETIP_RAW_SELECTOR_0380_SEED &&
           block[45U] == TECMO_GAMEPLAY_PRETIP_RAW_SELECTOR_037F_SEED &&
           bytes_zero(block + 46U, TECMO_GAMEPLAY_PRETIP_TPM2_SIZE - 46U);
}

static bool validate_padding(const uint8_t *payload)
{
    struct Gap { size_t start; size_t end; };
    static const struct Gap gaps[] = {
        {1440U,1536U},{1543U,1568U},{1592U,1600U},
        {3664U,3680U},{3729U,3744U},{4043U,4064U},
        {4120U,4128U},{4136U,4160U},{4223U,4256U},
        {4408U,4416U},{4426U,4448U},{4958U,4992U},
        {5072U,5088U},{5356U,5376U},{5470U,5472U},
        {5674U,5696U},{5748U,5760U},{6290U,6336U},
        {6457U,6496U},{6508U,6560U},{6656U,7008U},
        {7086U,7088U},{7188U,7200U},
        {7244U,7248U},{7297U,7312U},{7595U,7600U},
        {7601U,7616U},{7638U,TECMO_ASSET_PACK_GAMEPLAY_PRETIP_SIZE}
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
                  const uint8_t *tgjs, size_t tgjs_size,
                  const uint8_t *chr, size_t chr_size)
{
    uint8_t *storage;
    size_t index;
    tecmo_gameplay_pretip_destroy(assets);
    if (!validate_header(payload, payload_size))
        return reject(assets, "TPTI-2 header/size/reserved contract rejected");
    if (full_payload_fingerprint32(payload, payload_size) !=
            TECMO_ASSET_PACK_GAMEPLAY_PRETIP_FNV1A32 ||
        full_payload_fingerprint64(payload, payload_size) !=
            TECMO_ASSET_PACK_GAMEPLAY_PRETIP_FNV1A64 ||
        !validate_sources(payload, payload_size) ||
        !validate_padding(payload) ||
        !validate_mechanics(payload))
        return reject(assets, "TPTI-2 canonical/source/bounds contract rejected");
    if (!validate_dependency(gameplay, gameplay_size, "TGPL", 1U,
                             0x2047CCE0U, true) ||
        !validate_dependency(team_data, team_data_size, "TTDT", 1U,
                             0x812628F0U, false) ||
        !validate_dependency(music, music_size, "TMUS", 1U,
                             0x05C00ECBU, true) ||
        !validate_dependency(warriors, warriors_size, "TWAR", 1U,
                             TECMO_ASSET_PACK_GAMEPLAY_PRETIP_TWAR_FNV1A32,
                             false) ||
        !validate_dependency(tgjs, tgjs_size, "TGJS",
                             TECMO_ASSET_PACK_GAMEPLAY_PRETIP_TGJS_VERSION,
                             TECMO_ASSET_PACK_GAMEPLAY_PRETIP_TGJS_FNV1A32,
                             true) ||
        tgjs_size != TECMO_ASSET_PACK_GAMEPLAY_PRETIP_TGJS_SIZE ||
        read_u16(tgjs + 6U) !=
            TECMO_ASSET_PACK_GAMEPLAY_PRETIP_TGJS_HEADER_SIZE ||
        chr == NULL || chr_size != 262144U ||
        fnv1a32(chr, chr_size) != 0xF6F6E854U ||
        fnv1a64(chr, chr_size) != 0x96A64F53B240ABB4ULL)
        return reject(assets,
                      "TPTI-2 same-pack TGPL/TTDT/TMUS/TWAR/TGJS/CHR dependency rejected");
    storage = (uint8_t *)malloc(payload_size);
    if (storage == NULL) return reject(assets, "TPTI-2 allocation failed");
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
    assets->mechanics_fingerprint32 = read_u32(storage + 232U);
    assets->mechanics_fingerprint64 = read_u64(storage + 236U);
    assets->tgjs_fingerprint = read_u32(
        storage + TECMO_ASSET_PACK_GAMEPLAY_PRETIP_TGJS_FNV32_OFFSET);
    assets->tip_input_mask = storage[172U];
    assets->tip_no_sample_error = storage[173U];
    assets->tip_max_sample_error = storage[174U];
    assets->tip_auto_threshold_base = storage[
        TECMO_ASSET_PACK_GAMEPLAY_PRETIP_MECHANICS_OFFSET + 11U];
    assets->tip_auto_threshold_mask = storage[
        TECMO_ASSET_PACK_GAMEPLAY_PRETIP_MECHANICS_OFFSET + 12U];
    assets->tip_auto_threshold_shift = storage[
        TECMO_ASSET_PACK_GAMEPLAY_PRETIP_MECHANICS_OFFSET + 13U];
    assets->tip_claim_ball_high_min = storage[
        TECMO_ASSET_PACK_GAMEPLAY_PRETIP_MECHANICS_OFFSET + 14U];
    assets->tip_claim_ball_minus_jumper_limit = storage[
        TECMO_ASSET_PACK_GAMEPLAY_PRETIP_MECHANICS_OFFSET + 15U];
    assets->tip_actor_jump_commit_state = storage[
        TECMO_ASSET_PACK_GAMEPLAY_PRETIP_MECHANICS_OFFSET + 16U];
    assets->tip_slot10_claim_commit_state = storage[
        TECMO_ASSET_PACK_GAMEPLAY_PRETIP_MECHANICS_OFFSET + 17U];
    assets->tip_selector_0380_address = read_u16(
        storage + TECMO_ASSET_PACK_GAMEPLAY_PRETIP_MECHANICS_OFFSET + 18U);
    assets->tip_selector_037f_address = read_u16(
        storage + TECMO_ASSET_PACK_GAMEPLAY_PRETIP_MECHANICS_OFFSET + 20U);
    assets->tip_jump_post_store_address = read_u16(
        storage + TECMO_ASSET_PACK_GAMEPLAY_PRETIP_MECHANICS_OFFSET + 22U);
    assets->available = true;
    (void)snprintf(assets->status, sizeof(assets->status),
                   "TPTI-2 native pre-tip assetpack");
    return true;
}

bool tecmo_gameplay_pretip_load(TecmoGameplayPreTipAssets *assets,
                                const char *asset_pack_path)
{
    static const char *ids[] = {
        TECMO_ASSET_PACK_GAMEPLAY_PRETIP_ID,
        "gameplay/core", "menu/team-data", "audio/music",
        "arena/intro/warriors-transition", "gameplay/jump-shots", "chr/all"
    };
    static const uint64_t sizes[] = {
        TECMO_ASSET_PACK_GAMEPLAY_PRETIP_SIZE,
        23416U,96372U,36784U,
        TECMO_ASSET_PACK_GAMEPLAY_PRETIP_TWAR_SIZE,
        TECMO_ASSET_PACK_GAMEPLAY_PRETIP_TGJS_SIZE,262144U
    };
    uint8_t *bytes[7] = {NULL,NULL,NULL,NULL,NULL,NULL,NULL};
    uint64_t counts[7] = {0U,0U,0U,0U,0U,0U,0U};
    bool loaded = false;
    size_t index;
    if (assets == NULL || assets->lifecycle_tag != PRETIP_LIFECYCLE_TAG)
        return false;
    tecmo_gameplay_pretip_destroy(assets);
    if (asset_pack_path == NULL) {
        return reject(assets, "TPTI-2 explicit asset pack unavailable");
    }
    for (index = 0U; index < 7U; ++index) {
        if (tecmo_asset_pack_read_entry_exact(
                asset_pack_path, ids[index], sizes[index],
                &bytes[index], &counts[index]) != 0) {
            char status[192];
            size_t cleanup;
            (void)snprintf(status, sizeof(status),
                           "TPTI-2 %s missing or wrong-sized", ids[index]);
            for (cleanup = 0U; cleanup < 7U; ++cleanup)
                tecmo_asset_pack_free(bytes[cleanup]);
            return reject(assets, status);
        }
    }
    loaded = parse(
        assets,
        bytes[0], (size_t)counts[0], bytes[1], (size_t)counts[1],
        bytes[2], (size_t)counts[2], bytes[3], (size_t)counts[3],
        bytes[4], (size_t)counts[4], bytes[5], (size_t)counts[5],
        bytes[6], (size_t)counts[6]);
    for (index = 0U; index < 7U; ++index)
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
        full_payload_fingerprint32(assets->storage, assets->storage_size) !=
            TECMO_ASSET_PACK_GAMEPLAY_PRETIP_FNV1A32 ||
        full_payload_fingerprint64(assets->storage, assets->storage_size) !=
            TECMO_ASSET_PACK_GAMEPLAY_PRETIP_FNV1A64 ||
        !validate_mechanics(assets->storage) ||
        assets->mechanics_fingerprint32 != read_u32(assets->storage + 232U) ||
        assets->mechanics_fingerprint64 != read_u64(assets->storage + 236U) ||
        assets->tgjs_fingerprint != TECMO_ASSET_PACK_GAMEPLAY_PRETIP_TGJS_FNV1A32 ||
        assets->tip_input_mask != TECMO_GAMEPLAY_PRETIP_INPUT_MASK ||
        assets->tip_no_sample_error != TECMO_GAMEPLAY_PRETIP_NO_SAMPLE_ERROR ||
        assets->tip_max_sample_error != TECMO_GAMEPLAY_PRETIP_MAX_SAMPLE_ERROR ||
        assets->tip_auto_threshold_base != assets->storage[
            TECMO_ASSET_PACK_GAMEPLAY_PRETIP_MECHANICS_OFFSET + 11U] ||
        assets->tip_auto_threshold_base !=
            TECMO_GAMEPLAY_PRETIP_AUTO_THRESHOLD_BASE ||
        assets->tip_auto_threshold_mask != assets->storage[
            TECMO_ASSET_PACK_GAMEPLAY_PRETIP_MECHANICS_OFFSET + 12U] ||
        assets->tip_auto_threshold_mask !=
            TECMO_GAMEPLAY_PRETIP_AUTO_THRESHOLD_MASK ||
        assets->tip_auto_threshold_shift != assets->storage[
            TECMO_ASSET_PACK_GAMEPLAY_PRETIP_MECHANICS_OFFSET + 13U] ||
        assets->tip_auto_threshold_shift !=
            TECMO_GAMEPLAY_PRETIP_AUTO_THRESHOLD_SHIFT ||
        assets->tip_claim_ball_high_min != assets->storage[
            TECMO_ASSET_PACK_GAMEPLAY_PRETIP_MECHANICS_OFFSET + 14U] ||
        assets->tip_claim_ball_high_min !=
            TECMO_GAMEPLAY_PRETIP_CLAIM_BALL_HIGH_MIN ||
        assets->tip_claim_ball_minus_jumper_limit != assets->storage[
            TECMO_ASSET_PACK_GAMEPLAY_PRETIP_MECHANICS_OFFSET + 15U] ||
        assets->tip_claim_ball_minus_jumper_limit !=
            TECMO_GAMEPLAY_PRETIP_CLAIM_BALL_MINUS_JUMPER_LIMIT ||
        assets->tip_actor_jump_commit_state !=
            TECMO_GAMEPLAY_PRETIP_ACTOR_JUMP_COMMIT_STATE ||
        assets->tip_slot10_claim_commit_state !=
            TECMO_GAMEPLAY_PRETIP_SLOT10_CLAIM_COMMIT_STATE ||
        assets->tip_selector_0380_address != 0x0380U ||
        assets->tip_selector_037f_address != 0x037FU ||
        assets->tip_jump_post_store_address != 0xA2D5U ||
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

/* Native bridge, not ROM-exact: contest frame 0 is the target, a first held
   sample at frame N has error min(N, 11), and no sample has error 12. */
static uint8_t tip_error_for_sample(uint16_t frame)
{
    return frame < 11U ? (uint8_t)frame : 11U;
}

static uint16_t presentation_duration(
    const TecmoGameplayPreTipAssets *assets,
    TecmoGameplayPreTipPhase phase)
{
    return assets->phase_frames[phase];
}

static bool tip_sample_valid(bool sampled, uint8_t error,
                             uint16_t sample_frame,
                             TecmoGameplayPreTipPhase phase,
                             uint16_t contest_frame,
                             uint16_t contest_duration)
{
    if (!sampled) {
        return error == 12U &&
               sample_frame == TECMO_GAMEPLAY_PRETIP_NO_SAMPLE_FRAME;
    }
    if (phase < TECMO_GAMEPLAY_PRETIP_CENTER_COURT_SETUP ||
        sample_frame >= contest_duration ||
        error != tip_error_for_sample(sample_frame)) {
        return false;
    }
    return phase < TECMO_GAMEPLAY_PRETIP_BALL_DESCENT ||
           sample_frame < contest_frame || sample_frame == 0U;
}

/* CD96-CDAB is an exact bounded source seam. Its use as a native contest
   seed/mixer is deterministic, but the downstream velocity/height bridge is
   intentionally approximate because the TTDT/raw $7C48 mapping is incomplete. */
static void tip_rng_mix(TecmoGameplayPreTipState *state)
{
    uint8_t value;
    uint8_t shifted;
    if (state == NULL) return;
    value = (uint8_t)(state->tip_rng_6a ^ state->tip_rng_53);
    shifted = (uint8_t)(value << 1U);
    if ((value & 0x80U) != 0U) shifted ^= 0x1DU;
    state->tip_rng_6a = shifted;
    if (state->tip_rng_6a == 0U)
        state->tip_rng_6a ^= state->tip_rng_53;
    if (state->tip_rng_mix_count != UINT8_MAX)
        ++state->tip_rng_mix_count;
}

static uint8_t tip_ball_high_for_frame(uint16_t contest_frame)
{
    uint16_t value = (uint16_t)0x3DU + contest_frame;
    return value > UINT8_MAX ? UINT8_MAX : (uint8_t)value;
}

uint8_t tecmo_gameplay_pretip_ball_screen_y(uint8_t raw_height)
{
    return raw_height < TECMO_GAMEPLAY_PRETIP_BALL_FLOOR_SCREEN_Y
        ? (uint8_t)(TECMO_GAMEPLAY_PRETIP_BALL_FLOOR_SCREEN_Y - raw_height)
        : 0U;
}

static void tip_commit_jumper(TecmoGameplayPreTipState *state,
                              uint8_t jumper,
                              uint16_t contest_frame,
                              uint8_t error,
                              bool automatic)
{
    uint16_t velocity;
    uint16_t claim_height;
    uint16_t claim_sample_frame;
    if (state == NULL || jumper >= TECMO_GAMEPLAY_PRETIP_JUMPER_COUNT)
        return;
    /* This velocity/height mapping is native-faithful/approximate. Velocity
       is genuine Q8 visual units (0x0A00 is ten pixels); claim_height is a
       separate raw $048F analogue. It keeps the exact capture error and
       per-jumper commit seam while avoiding an asserted TTDT-to-$7C48
       interpretation. Equal human samples remain exactly equal and therefore
       defer instead of inventing a tie winner. */
    velocity = (uint16_t)(0x0A00U +
                          ((uint16_t)(TECMO_GAMEPLAY_PRETIP_MAX_SAMPLE_ERROR -
                                      error) * 0x0100U));
    if (automatic)
        velocity = (uint16_t)(velocity + 0x0040U +
                              (uint16_t)jumper * 0x0020U +
                              (uint16_t)(state->tip_rng_6a & 0x03U) * 0x0010U);
    claim_sample_frame = automatic ? contest_frame :
        (jumper == 0U ? state->away_tip_sample_frame
                      : state->home_tip_sample_frame);
    claim_height = (uint16_t)(0x30U +
                              ((uint16_t)(TECMO_GAMEPLAY_PRETIP_MAX_SAMPLE_ERROR -
                                          error) * 2U) +
                              (uint16_t)((TECMO_GAMEPLAY_PRETIP_CONTEST_INPUT_FRAMES -
                                          claim_sample_frame) / 4U));
    if (automatic) {
        /* Native-approximate claim-height policy keeps the deterministic
           CPU-vs-CPU Away result without delaying either CPU threshold. */
        claim_height = (uint16_t)(claim_height + (jumper == 0U ? 1U : 0U));
    }
    if (jumper == 0U) {
        state->away_jump_committed = true;
        state->away_jump_commit_frame = contest_frame;
        state->away_jump_velocity_q8 = velocity;
        state->away_claim_height_raw = (uint8_t)claim_height;
        state->away_tip_automatic = automatic;
        state->away_automatic_triggered = automatic;
        state->away_jump_velocity_signed_q8 =
            (int16_t)(TECMO_GAMEPLAY_PRETIP_INITIAL_VELOCITY_Q8 +
                      (automatic ? 0x20 : 0));
        state->away_actor_state = TECMO_GAMEPLAY_PRETIP_ACTOR_JUMP_COMMIT_STATE;
        state->away_animation_phase = 2U;
        ++state->away_jump_commit_count;
    } else {
        state->home_jump_committed = true;
        state->home_jump_commit_frame = contest_frame;
        state->home_jump_velocity_q8 = velocity;
        state->home_claim_height_raw = (uint8_t)claim_height;
        state->home_tip_automatic = automatic;
        state->home_automatic_triggered = automatic;
        state->home_jump_velocity_signed_q8 =
            TECMO_GAMEPLAY_PRETIP_INITIAL_VELOCITY_Q8;
        state->home_actor_state = TECMO_GAMEPLAY_PRETIP_ACTOR_JUMP_COMMIT_STATE;
        state->home_animation_phase = 2U;
        ++state->home_jump_commit_count;
    }
}

static void latch_tip_controlled(bool held, uint16_t contest_frame,
                                 uint8_t *error, uint16_t *sample_frame,
                                 bool *sampled)
{
    if (!held || sampled == NULL || *sampled || error == NULL ||
        sample_frame == NULL)
        return;
    *sampled = true;
    *error = tip_error_for_sample(contest_frame);
    *sample_frame = contest_frame;
}

static void tip_update_human_jumper(TecmoGameplayPreTipState *state,
                                    uint8_t jumper,
                                    uint16_t contest_frame)
{
    uint8_t *countdown;
    bool sampled;
    bool committed;
    uint8_t error;
    if (state == NULL || jumper >= TECMO_GAMEPLAY_PRETIP_JUMPER_COUNT ||
        state->tip_ball_high_raw < TECMO_GAMEPLAY_PRETIP_HUMAN_GATE_HEIGHT)
        return;
    countdown = jumper == 0U ? &state->away_tip_countdown
                             : &state->home_tip_countdown;
    sampled = jumper == 0U ? state->away_tip_sampled
                           : state->home_tip_sampled;
    committed = jumper == 0U ? state->away_jump_committed
                             : state->home_jump_committed;
    error = jumper == 0U ? state->away_tip_error : state->home_tip_error;
    if (*countdown > 0U) --*countdown;
    if (*countdown == 0U && sampled && !committed)
        tip_commit_jumper(state, jumper, contest_frame, error, false);
}

static bool tip_claim_ready(uint8_t ball_high, uint8_t claim_height)
{
    return ball_high >= TECMO_GAMEPLAY_PRETIP_CLAIM_BALL_HIGH_MIN &&
           ball_high >= claim_height &&
           (uint16_t)(ball_high - claim_height) <
               TECMO_GAMEPLAY_PRETIP_CLAIM_BALL_MINUS_JUMPER_LIMIT;
}

static bool tip_automatic_threshold_met(uint8_t ball_high,
                                        uint16_t threshold)
{
    /* Bank05 $839F uses BCS after CMP: equality is not a trigger. */
    return (uint16_t)ball_high > threshold;
}

static void tip_try_resolve_claim(TecmoGameplayPreTipState *state)
{
    bool away_ready;
    bool home_ready;
    uint8_t claimant = TECMO_GAMEPLAY_PRETIP_CLAIMANT_NONE;
    uint8_t away_height;
    uint8_t home_height;
    if (state == NULL || state->claim_resolved ||
        state->ball_actor_state != 0x1BU)
        return;
    away_height = (uint8_t)(state->away_jump_altitude_q8 >> 8U);
    home_height = (uint8_t)(state->home_jump_altitude_q8 >> 8U);
    away_ready = state->away_jump_committed &&
                 tip_claim_ready(state->tip_ball_high_raw,
                                  away_height);
    home_ready = state->home_jump_committed &&
                 tip_claim_ready(state->tip_ball_high_raw,
                                  home_height);
    if (!away_ready && !home_ready) return;
    if (away_ready && home_ready &&
        away_height == home_height) {
        /* Bank05 $A274 retries equal live heights on the next update.  It
           does not convert equality into a terminal contest state. */
        state->claim_deferred = true;
        return;
    }
    state->claim_deferred = false;
    if (away_ready && (!home_ready ||
                       away_height > home_height))
        claimant = 0U;
    else if (home_ready)
        claimant = 1U;
    if (claimant == TECMO_GAMEPLAY_PRETIP_CLAIMANT_NONE) return;
    state->claim_resolved = true;
    state->claimant_jumper = claimant;
    state->contact_state_17 = true;
    state->event_0588_bit20 = true;
    state->ball_actor_state = TECMO_GAMEPLAY_PRETIP_SLOT10_CLAIM_COMMIT_STATE;
    /* $A274's receiver selector is separate from the winning center and the
       later possession actor in $0308.  Preserve that distinction in the
       fixed ten-actor scene topology instead of forcing slot 0/5. */
    state->receiver_actor = claimant == 0U ? 3U : 8U;
}

static void tip_update_ballistic_jumper(TecmoGameplayPreTipState *state,
                                        uint8_t jumper)
{
    int16_t *velocity;
    uint16_t *altitude;
    uint8_t *fraction;
    uint16_t *apex_frame;
    uint8_t *actor_state;
    uint8_t *animation_phase;
    int16_t previous_velocity;
    int32_t next_height;
    uint16_t age;
    if (state == NULL || jumper >= TECMO_GAMEPLAY_PRETIP_JUMPER_COUNT)
        return;
    velocity = jumper == 0U ? &state->away_jump_velocity_signed_q8
                            : &state->home_jump_velocity_signed_q8;
    altitude = jumper == 0U ? &state->away_jump_altitude_q8
                            : &state->home_jump_altitude_q8;
    fraction = jumper == 0U ? &state->away_height_fraction
                            : &state->home_height_fraction;
    apex_frame = jumper == 0U ? &state->away_apex_frame
                              : &state->home_apex_frame;
    actor_state = jumper == 0U ? &state->away_actor_state
                               : &state->home_actor_state;
    animation_phase = jumper == 0U ? &state->away_animation_phase
                                   : &state->home_animation_phase;
    previous_velocity = *velocity;
    *velocity = (int16_t)(*velocity - TECMO_GAMEPLAY_PRETIP_GRAVITY_Q8);
    next_height = (int32_t)*altitude + *velocity;
    if (previous_velocity > 0 && *velocity <= 0 && *apex_frame == 0U)
        *apex_frame = state->simulation_tick;
    if (next_height <= 0) {
        *altitude = 0U;
        *fraction = 0U;
        *velocity = 0;
        *actor_state = 0x13U;
        *animation_phase = 0U;
        return;
    }
    *altitude = (uint16_t)next_height;
    *fraction = (uint8_t)(*altitude & 0xFFU);
    age = state->simulation_tick -
        (jumper == 0U ? state->away_jump_commit_frame
                      : state->home_jump_commit_frame);
    if (age == 0U) *animation_phase = 2U;
    else if (age == 1U) *animation_phase = 3U;
    else {
        *animation_phase = 4U;
        *actor_state = 0x0CU;
    }
}

static void tip_update_altitudes(TecmoGameplayPreTipState *state)
{
    if (state == NULL) return;
    if (state->away_jump_committed && state->away_actor_state != 0x13U)
        tip_update_ballistic_jumper(state, 0U);
    if (state->home_jump_committed && state->home_actor_state != 0x13U)
        tip_update_ballistic_jumper(state, 1U);
}

static uint16_t tip_expected_altitude(const TecmoGameplayPreTipState *state,
                                      uint16_t velocity,
                                      uint16_t commit_frame)
{
    uint16_t age;
    uint16_t elapsed;
    if (state == NULL) return 0U;
    if (state->phase >= TECMO_GAMEPLAY_PRETIP_LIVE)
        age = TECMO_GAMEPLAY_PRETIP_PRESENTATION_FRAMES;
    else
        age = state->phase_frame > TECMO_GAMEPLAY_PRETIP_PRESENTATION_FRAMES
                ? TECMO_GAMEPLAY_PRETIP_PRESENTATION_FRAMES
                : state->phase_frame;
    elapsed = age > commit_frame ? (uint16_t)(age - commit_frame) : 0U;
    if (elapsed > TECMO_GAMEPLAY_PRETIP_CONTEST_INPUT_FRAMES)
        elapsed = TECMO_GAMEPLAY_PRETIP_CONTEST_INPUT_FRAMES;
    return (uint16_t)(((uint32_t)velocity * elapsed) /
                      TECMO_GAMEPLAY_PRETIP_CONTEST_INPUT_FRAMES);
}

static uint8_t tip_rng_after_count(uint8_t count)
{
    uint8_t rng_6a = 0U;
    uint8_t index;
    for (index = 0U; index < count; ++index) {
        uint8_t value = (uint8_t)(rng_6a ^ 0x5AU);
        rng_6a = (uint8_t)(value << 1U);
        if ((value & 0x80U) != 0U) rng_6a ^= 0x1DU;
        if (rng_6a == 0U) rng_6a ^= 0x5AU;
    }
    return rng_6a;
}

static uint8_t tip_expected_ball_high(
    const TecmoGameplayPreTipState *state)
{
    uint16_t input_age;
    if (state == NULL || !state->simulation_active)
        return 0U;
    input_age = state->simulation_tick > 0U
                  ? (uint16_t)(state->simulation_tick - 1U) : 0U;
    if (input_age >= TECMO_GAMEPLAY_PRETIP_CONTEST_INPUT_FRAMES)
        input_age = (uint16_t)(TECMO_GAMEPLAY_PRETIP_CONTEST_INPUT_FRAMES - 1U);
    return tip_ball_high_for_frame(input_age);
}

static bool state_valid(const TecmoGameplayPreTipAssets *assets,
                        const TecmoGameplayPreTipState *state)
{
    uint64_t expected_total = 0U;
    size_t index;
    bool in_contest;
    if (!assets_valid(assets) || state == NULL ||
        state->contract_tag != TECMO_GAMEPLAY_PRETIP_STATE_TAG ||
        state->phase < TECMO_GAMEPLAY_PRETIP_PRESEASON ||
        state->phase > TECMO_GAMEPLAY_PRETIP_LIVE) {
        return false;
    }
    for (index = 0U;
         index < (size_t)state->phase &&
         index < TECMO_GAMEPLAY_PRETIP_TOSS_CLOSEUP; ++index) {
        uint16_t duration = presentation_duration(
            assets, (TecmoGameplayPreTipPhase)index);
        if (expected_total > UINT32_MAX - duration) return false;
        expected_total += duration;
    }
    if (state->phase < TECMO_GAMEPLAY_PRETIP_TOSS_CLOSEUP) {
        uint16_t duration = presentation_duration(assets, state->phase);
        if (state->phase_frame >= duration ||
            expected_total > UINT32_MAX - state->phase_frame) {
            return false;
        }
        expected_total += state->phase_frame;
    } else {
        if (state->first_cinematic_frame == UINT16_MAX ||
            (state->phase < TECMO_GAMEPLAY_PRETIP_LIVE &&
             state->phase_frame >= presentation_duration(
                 assets, state->phase)) ||
            (state->phase == TECMO_GAMEPLAY_PRETIP_LIVE &&
             state->phase_frame != 0U)) return false;
        expected_total = state->first_cinematic_frame;
        if (state->phase >= TECMO_GAMEPLAY_PRETIP_JUMP_CONTEST)
            expected_total += presentation_duration(
                assets, TECMO_GAMEPLAY_PRETIP_TOSS_CLOSEUP);
        if (state->phase >= TECMO_GAMEPLAY_PRETIP_LIVE)
            expected_total += presentation_duration(
                assets, TECMO_GAMEPLAY_PRETIP_JUMP_CONTEST);
        if (state->phase < TECMO_GAMEPLAY_PRETIP_LIVE)
            expected_total += state->phase_frame;
    }
    in_contest = state->simulation_active &&
                 state->phase < TECMO_GAMEPLAY_PRETIP_LIVE;
    if (state->contest_frame > TECMO_GAMEPLAY_PRETIP_CONTEST_INPUT_FRAMES ||
        (!state->simulation_active &&
         state->contest_frame != 0U) ||
        (in_contest &&
         state->contest_frame != (state->simulation_tick <
                 TECMO_GAMEPLAY_PRETIP_CONTEST_INPUT_FRAMES
              ? state->simulation_tick
              : TECMO_GAMEPLAY_PRETIP_CONTEST_INPUT_FRAMES)) ||
        (state->phase >= TECMO_GAMEPLAY_PRETIP_LIVE &&
         state->contest_frame != TECMO_GAMEPLAY_PRETIP_CONTEST_INPUT_FRAMES)) {
        return false;
    }
    if (expected_total != state->total_frame ||
        !tip_sample_valid(
            state->away_tip_sampled, state->away_tip_error,
            state->away_tip_sample_frame, state->phase, state->contest_frame,
            assets->phase_frames[TECMO_GAMEPLAY_PRETIP_JUMP_CONTEST]) ||
        !tip_sample_valid(
            state->home_tip_sampled, state->home_tip_error,
            state->home_tip_sample_frame, state->phase, state->contest_frame,
            assets->phase_frames[TECMO_GAMEPLAY_PRETIP_JUMP_CONTEST]) ||
        state->tip_rng_53 != 0x5AU ||
        state->tip_rng_mix_count > TECMO_GAMEPLAY_PRETIP_CONTEST_INPUT_FRAMES ||
        (state->simulation_active &&
         state->tip_rng_mix_count != state->contest_frame) ||
        (state->simulation_active &&
         state->tip_rng_6a != tip_rng_after_count(
             state->tip_rng_mix_count)) ||
        state->tip_ball_high_raw != tip_expected_ball_high(state) ||
        (!state->simulation_active &&
         (state->tip_rng_6a != 0U || state->tip_rng_mix_count != 0U ||
          state->tip_ball_high_raw != 0U ||
          state->away_automatic_requested ||
          state->home_automatic_requested))) {
        return false;
    }
    if ((state->away_jump_committed && !state->away_tip_sampled) ||
        (state->home_jump_committed && !state->home_tip_sampled) ||
        state->away_tip_countdown >
            TECMO_GAMEPLAY_PRETIP_HUMAN_COUNTDOWN_INITIAL ||
        state->home_tip_countdown >
            TECMO_GAMEPLAY_PRETIP_HUMAN_COUNTDOWN_INITIAL ||
        state->away_tip_automatic != state->away_automatic_triggered ||
        state->home_tip_automatic != state->home_automatic_triggered) {
        return false;
    }
    if ((state->away_tip_automatic && !state->away_automatic_requested) ||
        (state->home_tip_automatic && !state->home_automatic_requested)) {
        return false;
    }
    if (state->away_jump_committed) {
        if (state->away_jump_commit_frame >=
                UINT16_MAX ||
            state->away_jump_velocity_q8 == 0U ||
            state->away_claim_height_raw == 0U ||
            state->away_jump_commit_count != 1U ||
            state->away_animation_phase > 4U) {
            return false;
        }
    } else if (state->away_jump_commit_frame != 0U ||
               state->away_jump_velocity_q8 != 0U ||
               state->away_jump_altitude_q8 != 0U ||
               state->away_claim_height_raw != 0U ||
               state->away_jump_commit_count != 0U ||
               state->away_tip_automatic || state->away_automatic_triggered) {
        return false;
    }
    if (state->home_jump_committed) {
        if (state->home_jump_commit_frame >=
                UINT16_MAX ||
            state->home_jump_velocity_q8 == 0U ||
            state->home_claim_height_raw == 0U ||
            state->home_jump_commit_count != 1U ||
            state->home_animation_phase > 4U) {
            return false;
        }
    } else if (state->home_jump_commit_frame != 0U ||
               state->home_jump_velocity_q8 != 0U ||
               state->home_jump_altitude_q8 != 0U ||
               state->home_claim_height_raw != 0U ||
               state->home_jump_commit_count != 0U ||
               state->home_tip_automatic || state->home_automatic_triggered) {
        return false;
    }
    if (state->claim_resolved || state->claim_deferred) {
        if (!state->simulation_active) {
            return false;
        }
    }
    if (state->claim_resolved) {
        if (state->claim_deferred || state->contest_stalled ||
            state->claimant_jumper >= TECMO_GAMEPLAY_PRETIP_JUMPER_COUNT ||
            !state->contact_state_17 || !state->event_0588_bit20 ||
            state->ball_actor_state !=
                TECMO_GAMEPLAY_PRETIP_SLOT10_CLAIM_COMMIT_STATE ||
            state->raw_selector_0380 !=
                TECMO_GAMEPLAY_PRETIP_RAW_SELECTOR_0380_SEED ||
            state->raw_selector_037f !=
                TECMO_GAMEPLAY_PRETIP_RAW_SELECTOR_037F_SEED ||
            state->receiver_actor !=
                (state->claimant_jumper == 0U ? 3U : 8U)) {
            return false;
        }
    } else if (state->claimant_jumper !=
                   TECMO_GAMEPLAY_PRETIP_CLAIMANT_NONE ||
               state->raw_selector_0380 !=
                   TECMO_GAMEPLAY_PRETIP_RAW_SELECTOR_0380_SEED ||
               state->raw_selector_037f !=
                   TECMO_GAMEPLAY_PRETIP_RAW_SELECTOR_037F_SEED ||
               state->receiver_actor != 0xFFU) {
        return false;
    }
    if (!state->claim_resolved &&
        (state->contact_state_17 || state->event_0588_bit20 ||
         state->ball_actor_state ==
             TECMO_GAMEPLAY_PRETIP_SLOT10_CLAIM_COMMIT_STATE)) return false;
    if (state->aborted) {
        return state->card_cancel_enabled && !state->live_handoff &&
               state->phase <= TECMO_GAMEPLAY_PRETIP_FIRST_PERIOD &&
               !state->claim_resolved && !state->claim_deferred &&
               !state->contest_stalled;
    }
    if (state->contest_stalled) {
        return in_contest && state->phase_frame ==
                   TECMO_GAMEPLAY_PRETIP_PRESENTATION_FRAMES - 1U &&
               state->total_frame == 720U && !state->live_handoff &&
               !state->claim_resolved;
    }
    if (state->phase == TECMO_GAMEPLAY_PRETIP_LIVE)
        return state->live_handoff && state->claim_resolved &&
               !state->claim_deferred;
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
    initial.away_tip_countdown =
        TECMO_GAMEPLAY_PRETIP_HUMAN_COUNTDOWN_INITIAL;
    initial.home_tip_countdown =
        TECMO_GAMEPLAY_PRETIP_HUMAN_COUNTDOWN_INITIAL;
    initial.away_actor_state = 0x22U;
    initial.home_actor_state = 0x13U;
    initial.ball_actor_state = 0x1AU;
    initial.first_cinematic_frame = UINT16_MAX;
    initial.away_tip_sample_frame = TECMO_GAMEPLAY_PRETIP_NO_SAMPLE_FRAME;
    initial.home_tip_sample_frame = TECMO_GAMEPLAY_PRETIP_NO_SAMPLE_FRAME;
    initial.tip_rng_53 = 0x5AU;
    initial.claimant_jumper = TECMO_GAMEPLAY_PRETIP_CLAIMANT_NONE;
    initial.receiver_actor = 0xFFU;
    initial.raw_selector_0380 = TECMO_GAMEPLAY_PRETIP_RAW_SELECTOR_0380_SEED;
    initial.raw_selector_037f = TECMO_GAMEPLAY_PRETIP_RAW_SELECTOR_037F_SEED;
    initial.card_cancel_enabled = card_cancel_enabled;
    if (!state_valid(assets, &initial)) return false;
    *state = initial;
    return true;
}

bool tecmo_gameplay_pretip_update(
    const TecmoGameplayPreTipAssets *assets,
    TecmoGameplayPreTipState *state,
    bool player_one_or_away_held_b,
    bool player_two_or_home_held_b)
{
    return tecmo_gameplay_pretip_update_controlled(
        assets, state, player_one_or_away_held_b,
        player_two_or_home_held_b, false, false);
}

bool tecmo_gameplay_pretip_update_controlled(
    const TecmoGameplayPreTipAssets *assets,
    TecmoGameplayPreTipState *state,
    bool player_one_or_away_held_b,
    bool player_two_or_home_held_b,
    bool away_automatic,
    bool home_automatic)
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
    if (candidate.phase == TECMO_GAMEPLAY_PRETIP_CENTER_COURT_SETUP) {
        latch_tip_controlled(
            player_one_or_away_held_b, candidate.phase_frame,
            &candidate.away_tip_error, &candidate.away_tip_sample_frame,
            &candidate.away_tip_sampled);
        latch_tip_controlled(
            player_two_or_home_held_b, candidate.phase_frame,
            &candidate.home_tip_error, &candidate.home_tip_sample_frame,
            &candidate.home_tip_sampled);
    }
    if (candidate.phase >= TECMO_GAMEPLAY_PRETIP_BALL_DESCENT &&
        candidate.phase <= TECMO_GAMEPLAY_PRETIP_JUMP_CONTEST) {
        uint16_t threshold;
        uint16_t input_frame = candidate.contest_frame <
                TECMO_GAMEPLAY_PRETIP_CONTEST_INPUT_FRAMES
            ? candidate.contest_frame
            : TECMO_GAMEPLAY_PRETIP_CONTEST_INPUT_FRAMES - 1U;
        if (!candidate.simulation_active) {
            candidate.simulation_active = true;
            candidate.ball_actor_state = 0x1AU;
        }
        if (candidate.contest_stalled) {
            /* A stalled presentation remains responsive.  Clear the old
               terminal marker and retry the live claim on this update. */
            candidate.contest_stalled = false;
            candidate.claim_deferred = false;
            if (!candidate.away_tip_sampled &&
                player_one_or_away_held_b) {
                latch_tip_controlled(
                    true,
                    TECMO_GAMEPLAY_PRETIP_CONTEST_INPUT_FRAMES - 1U,
                    &candidate.away_tip_error,
                    &candidate.away_tip_sample_frame,
                    &candidate.away_tip_sampled);
            }
            if (!candidate.home_tip_sampled &&
                player_two_or_home_held_b) {
                latch_tip_controlled(
                    true,
                    TECMO_GAMEPLAY_PRETIP_CONTEST_INPUT_FRAMES - 1U,
                    &candidate.home_tip_error,
                    &candidate.home_tip_sample_frame,
                    &candidate.home_tip_sampled);
            }
            if (!candidate.away_automatic_requested)
                tip_update_human_jumper(
                    &candidate, 0U,
                    TECMO_GAMEPLAY_PRETIP_CONTEST_INPUT_FRAMES - 1U);
            if (!candidate.home_automatic_requested)
                tip_update_human_jumper(
                    &candidate, 1U,
                    TECMO_GAMEPLAY_PRETIP_CONTEST_INPUT_FRAMES - 1U);
        }
        if (candidate.contest_frame <
                TECMO_GAMEPLAY_PRETIP_CONTEST_INPUT_FRAMES) {
            candidate.away_automatic_requested =
                candidate.away_automatic_requested || away_automatic;
            candidate.home_automatic_requested =
                candidate.home_automatic_requested || home_automatic;
            tip_rng_mix(&candidate);
            candidate.tip_ball_high_raw = tip_ball_high_for_frame(
                candidate.contest_frame);
            threshold = (uint16_t)(
                assets->tip_auto_threshold_base +
                ((candidate.tip_rng_6a & assets->tip_auto_threshold_mask) >>
                 assets->tip_auto_threshold_shift));
        }
        threshold = (uint16_t)(assets->tip_auto_threshold_base +
            ((candidate.tip_rng_6a & assets->tip_auto_threshold_mask) >>
             assets->tip_auto_threshold_shift));
        if (!candidate.away_jump_committed) {
            if (!candidate.away_automatic_requested) {
                latch_tip_controlled(
                    player_one_or_away_held_b, input_frame,
                    &candidate.away_tip_error, &candidate.away_tip_sample_frame,
                    &candidate.away_tip_sampled);
                tip_update_human_jumper(
                    &candidate, 0U, candidate.simulation_tick);
            } else if (
                       tip_automatic_threshold_met(
                           candidate.tip_ball_high_raw, threshold)) {
                candidate.away_tip_sampled = true;
                candidate.away_tip_error =
                    tip_error_for_sample(candidate.contest_frame);
                candidate.away_tip_sample_frame = input_frame;
                tip_commit_jumper(
                    &candidate, 0U, candidate.simulation_tick,
                    candidate.away_tip_error, true);
            }
        }
        if (!candidate.home_jump_committed) {
            if (!candidate.home_automatic_requested) {
                latch_tip_controlled(
                    player_two_or_home_held_b, input_frame,
                    &candidate.home_tip_error, &candidate.home_tip_sample_frame,
                    &candidate.home_tip_sampled);
                tip_update_human_jumper(
                    &candidate, 1U, candidate.simulation_tick);
            } else if (
                       tip_automatic_threshold_met(
                           candidate.tip_ball_high_raw, threshold)) {
                candidate.home_tip_sampled = true;
                candidate.home_tip_error =
                    tip_error_for_sample(candidate.contest_frame);
                candidate.home_tip_sample_frame = input_frame;
                tip_commit_jumper(
                    &candidate, 1U, candidate.simulation_tick,
                    candidate.home_tip_error, true);
            }
        }
        if (candidate.ball_actor_state == 0x1AU)
            candidate.ball_actor_state = 0x1BU;
        tip_update_altitudes(&candidate);
        tip_try_resolve_claim(&candidate);
        ++candidate.simulation_tick;
        if (candidate.contest_frame <
                TECMO_GAMEPLAY_PRETIP_CONTEST_INPUT_FRAMES)
            ++candidate.contest_frame;
    }
    duration = presentation_duration(assets, candidate.phase);
    if (candidate.phase_frame == UINT16_MAX ||
        candidate.total_frame == UINT32_MAX) {
        return false;
    }
    ++candidate.phase_frame;
    ++candidate.total_frame;
    if (candidate.phase == TECMO_GAMEPLAY_PRETIP_BALL_DESCENT &&
        candidate.event_0588_bit20 &&
        ((candidate.claimant_jumper == 0U && candidate.away_apex_frame != 0U) ||
         (candidate.claimant_jumper == 1U && candidate.home_apex_frame != 0U))) {
        candidate.phase = TECMO_GAMEPLAY_PRETIP_TOSS_CLOSEUP;
        candidate.phase_frame = 0U;
        candidate.cinematic_visible = true;
        candidate.first_cinematic_frame = (uint16_t)candidate.total_frame;
    } else if (candidate.phase_frame >= duration) {
        if (candidate.phase == TECMO_GAMEPLAY_PRETIP_BALL_DESCENT) {
            candidate.phase_frame = (uint16_t)(duration - 1U);
            --candidate.total_frame;
        } else if (candidate.phase == TECMO_GAMEPLAY_PRETIP_JUMP_CONTEST &&
            !candidate.claim_resolved) {
            candidate.phase_frame = (uint16_t)(duration - 1U);
            --candidate.total_frame;
            candidate.contest_stalled = true;
        } else {
            candidate.phase_frame = 0U;
            candidate.phase =
                (TecmoGameplayPreTipPhase)(candidate.phase + 1);
            if (candidate.phase == TECMO_GAMEPLAY_PRETIP_JUMP_CONTEST)
                candidate.cinematic_visible = false;
            if (candidate.phase == TECMO_GAMEPLAY_PRETIP_LIVE)
                candidate.live_handoff = true;
        }
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
        !state->simulation_active ||
        !state->claim_resolved || state->claim_deferred ||
        state->contest_stalled ||
        state->claimant_jumper >= TECMO_GAMEPLAY_PRETIP_JUMPER_COUNT) {
        return false;
    }
    *winner = state->claimant_jumper == 1U
                  ? TECMO_GAMEPLAY_PRETIP_HOME_WINNER
                  : TECMO_GAMEPLAY_PRETIP_AWAY_WINNER;
    return true;
}

bool tecmo_gameplay_pretip_claimant_jumper(
    const TecmoGameplayPreTipAssets *assets,
    const TecmoGameplayPreTipState *state,
    uint8_t *jumper)
{
    if (jumper == NULL || !state_valid(assets, state) || state->aborted ||
        !state->claim_resolved || state->claim_deferred ||
        state->contest_stalled ||
        state->claimant_jumper >= TECMO_GAMEPLAY_PRETIP_JUMPER_COUNT)
        return false;
    *jumper = state->claimant_jumper;
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

static bool tip_concurrent_simulation_regression(
    const TecmoGameplayPreTipAssets *assets,
    char *message, size_t message_size)
{
    TecmoGameplayPreTipState state;
    TecmoGameplayPreTipState no_capture;
    TecmoGameplayPreTipState unresolved;
    uint16_t previous_height = 0U;
    int16_t previous_velocity = 0;
    uint16_t apex_tick = 0U;
    bool saw_fall = false;
    unsigned frame;
#define CONCURRENT_FAIL(text) do { \
    if (message != NULL && message_size > 0U) \
        (void)snprintf(message, message_size, \
            "TPTI-2 concurrent simulation regression failed: " text); \
    return false; \
} while (0)
    if (!tecmo_gameplay_pretip_state_initialize(assets, &state, false) ||
        !tecmo_gameplay_pretip_state_initialize(assets, &no_capture, false) ||
        !tecmo_gameplay_pretip_state_initialize(assets, &unresolved, false))
        CONCURRENT_FAIL("initialization");
    for (frame = 0U; frame < 451U; ++frame) {
        if (!tecmo_gameplay_pretip_update(assets, &state, false, false) ||
            !tecmo_gameplay_pretip_update(
                assets, &no_capture, false, false) ||
            !tecmo_gameplay_pretip_update(
                assets, &unresolved, false, false))
            CONCURRENT_FAIL("advance to Bank04 capture");
    }
    if (state.phase != TECMO_GAMEPLAY_PRETIP_CENTER_COURT_SETUP ||
        state.away_actor_state != 0x22U || state.home_actor_state != 0x13U ||
        state.ball_actor_state != 0x1AU || state.simulation_active)
        CONCURRENT_FAIL("live object seed states");
    if (!tecmo_gameplay_pretip_update_controlled(
            assets, &state, true, false, false, false) ||
        !state.away_tip_sampled || state.away_tip_countdown != 0x0CU ||
        state.away_jump_committed)
        CONCURRENT_FAIL("Bank04 B latch before simulation");
    if (!tecmo_gameplay_pretip_update(
            assets, &no_capture, false, false) ||
        no_capture.away_tip_sampled || no_capture.away_tip_countdown != 0x0CU)
        CONCURRENT_FAIL("Bank04 no-sample countdown");
    if (!tecmo_gameplay_pretip_update(
            assets, &unresolved, false, false))
        CONCURRENT_FAIL("unresolved setup boundary");
    for (frame = 1U; frame < 30U; ++frame) {
        if (!tecmo_gameplay_pretip_update(assets, &state, false, false))
            CONCURRENT_FAIL("center setup capture retention");
    }
    if (state.phase != TECMO_GAMEPLAY_PRETIP_BALL_DESCENT ||
        state.simulation_active || state.simulation_tick != 0U)
        CONCURRENT_FAIL("simulation setup boundary");
    for (frame = 0U; frame < 180U; ++frame) {
        if (!tecmo_gameplay_pretip_update_controlled(
                assets, &unresolved, false, false, false, false))
            CONCURRENT_FAIL("unresolved event path update");
    }
    if (unresolved.phase != TECMO_GAMEPLAY_PRETIP_BALL_DESCENT ||
        unresolved.cinematic_visible || unresolved.contact_state_17 ||
        unresolved.event_0588_bit20 || unresolved.away_jump_committed ||
        unresolved.home_jump_committed)
        CONCURRENT_FAIL("fixed duration entered cinematic without contact");
    for (frame = 0U; frame < 120U &&
         state.phase == TECMO_GAMEPLAY_PRETIP_BALL_DESCENT; ++frame) {
        if (!tecmo_gameplay_pretip_update_controlled(
                assets, &state, false, false, false, true))
            CONCURRENT_FAIL("concurrent player/ball update");
        if (state.home_jump_committed) {
            if (previous_velocity != 0 &&
                state.home_jump_velocity_signed_q8 != 0 &&
                state.home_jump_velocity_signed_q8 !=
                    (int16_t)(previous_velocity -
                        TECMO_GAMEPLAY_PRETIP_GRAVITY_Q8))
                CONCURRENT_FAIL("gravity was not exactly $0028");
            if (apex_tick == 0U && state.home_jump_velocity_signed_q8 > 0 &&
                state.home_jump_altitude_q8 < previous_height)
                CONCURRENT_FAIL("height fell before velocity sign crossing");
            if (state.home_apex_frame != 0U) apex_tick = state.home_apex_frame;
            previous_velocity = state.home_jump_velocity_signed_q8;
            previous_height = state.home_jump_altitude_q8;
        }
    }
    if (state.phase != TECMO_GAMEPLAY_PRETIP_TOSS_CLOSEUP ||
        !state.cinematic_visible || !state.contact_state_17 ||
        !state.event_0588_bit20 || state.ball_actor_state != 0x17U ||
        apex_tick == 0U || apex_tick >= state.simulation_tick)
        CONCURRENT_FAIL("contact/apex-driven cinematic ordering");
    if (state.first_cinematic_frame < 481U ||
        state.away_jump_commit_count != 1U ||
        state.home_jump_commit_count != 1U)
        CONCURRENT_FAIL("cinematic fixed-duration or duplicate commit");
    for (frame = 0U; frame < 60U; ++frame) {
        uint16_t before_height = state.home_jump_altitude_q8;
        if (!tecmo_gameplay_pretip_update_controlled(
                assets, &state, false, false, false, true))
            CONCURRENT_FAIL("simulation under cinematic");
        if (state.home_jump_velocity_signed_q8 < 0 &&
            state.home_jump_altitude_q8 < before_height)
            saw_fall = true;
    }
    if (state.phase != TECMO_GAMEPLAY_PRETIP_JUMP_CONTEST ||
        state.cinematic_visible || state.away_jump_altitude_q8 != 0U ||
        state.home_jump_altitude_q8 != 0U ||
        state.away_actor_state != 0x13U || state.home_actor_state != 0x13U ||
        !saw_fall ||
        state.away_jump_commit_count != 1U ||
        state.home_jump_commit_count != 1U)
        CONCURRENT_FAIL("cinematic exit landing/no-restart");
    for (frame = 0U; frame < 30U; ++frame) {
        if (!tecmo_gameplay_pretip_update_controlled(
                assets, &state, false, false, false, true))
            CONCURRENT_FAIL("post-cinematic handoff");
    }
    if (state.phase != TECMO_GAMEPLAY_PRETIP_LIVE || !state.live_handoff ||
        state.away_jump_commit_count != 1U ||
        state.home_jump_commit_count != 1U) {
        if (message != NULL && message_size > 0U)
            (void)snprintf(message, message_size,
                "TPTI-2 concurrent simulation regression failed: late restart phase=%u frame=%u live=%u commits=%u/%u",
                (unsigned)state.phase, (unsigned)state.phase_frame,
                state.live_handoff ? 1U : 0U,
                (unsigned)state.away_jump_commit_count,
                (unsigned)state.home_jump_commit_count);
        return false;
    }
#undef CONCURRENT_FAIL
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

    if (ok && !tip_concurrent_simulation_regression(
                  &assets, message, message_size)) {
        ok = false;
    }

    if (ok) {
        malformed = state;
        malformed.away_automatic_requested = true;
        if (!update_rejects_unchanged(&assets, &malformed)) {
            if (message != NULL && message_size > 0U)
                (void)snprintf(message, message_size,
                    "TPTI-2 pre-contest automatic request mutation was accepted");
            ok = false;
        }
    }

    if (ok) {
        uint8_t saved_threshold_base = assets.tip_auto_threshold_base;
        assets.tip_auto_threshold_base ^= 1U;
        if (!update_rejects_unchanged(&assets, &state)) {
            if (message != NULL && message_size > 0U)
                (void)snprintf(message, message_size,
                    "TPTI-2 threshold cache mutation was accepted");
            ok = false;
        }
        assets.tip_auto_threshold_base = saved_threshold_base;
    }
    if (ok) {
        uint8_t saved_claim_limit =
            assets.tip_claim_ball_minus_jumper_limit;
        assets.tip_claim_ball_minus_jumper_limit ^= 1U;
        if (!update_rejects_unchanged(&assets, &state)) {
            if (message != NULL && message_size > 0U)
                (void)snprintf(message, message_size,
                    "TPTI-2 claim-limit cache mutation was accepted");
            ok = false;
        }
        assets.tip_claim_ball_minus_jumper_limit = saved_claim_limit;
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
    if (ok) {
        malformed = state;
        malformed.contest_frame = 1U;
        ok = update_rejects_unchanged(&assets, &malformed);
    }

    if (message != NULL && message_size > 0U && ok)
        (void)snprintf(message, message_size,
                       "TPTI-2 pre-tip self-test passed");
    else if (message != NULL && message_size > 0U && message[0] == '\0')
        (void)snprintf(message, message_size,
                       "TPTI-2 pre-tip state/transaction self-test failed");
    tecmo_gameplay_pretip_destroy(&assets);
    return ok;
}
