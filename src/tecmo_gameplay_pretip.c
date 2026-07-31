#define _CRT_SECURE_NO_WARNINGS

#include "tecmo_gameplay_pretip.h"

#include "asset_pack/tecmo_asset_pack_gameplay_pretip.h"
#include "tecmo_asset_pack.h"

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
    memset(assets->descriptor, 0, sizeof(assets->descriptor));
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
        payload[176U] != 0x8AU || payload[177U] != 0xC1U ||
        payload[178U] != 4U || payload[179U] != 9U ||
        payload[180U] != 0U || payload[181U] != 1U ||
        payload[182U] != 10U || payload[183U] != 0x1AU ||
        payload[184U] != 0x1AU ||
        !bytes_zero(payload + 185U,
                    TECMO_ASSET_PACK_GAMEPLAY_PRETIP_HEADER_SIZE - 185U)) {
        return false;
    }
    for (index = 0U; index < TECMO_GAMEPLAY_PRETIP_PHASE_COUNT; ++index)
        if (read_u16(payload + 156U + index * 2U) == 0U) return false;
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
        {807U,832U},{856U,864U},{2928U,2944U},{2993U,3008U},
        {3307U,3328U},{3384U,3392U},{3400U,3424U},{3487U,3504U},
        {4014U,4032U},{4112U,4128U},{4396U,4416U},{4510U,4528U},
        {4580U,4608U},{4991U,4992U},{5138U,5152U},{5273U,5280U},
        {5335U,TECMO_ASSET_PACK_GAMEPLAY_PRETIP_SIZE}
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
    assets->nametables =
        storage + TECMO_ASSET_PACK_GAMEPLAY_PRETIP_DECODED_OFFSET;
    assets->palette =
        storage + TECMO_ASSET_PACK_GAMEPLAY_PRETIP_PALETTE_OFFSET;
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
    return assets != NULL && assets->lifecycle_tag == PRETIP_LIFECYCLE_TAG &&
           assets->available && assets->storage != NULL &&
           assets->storage_size == TECMO_ASSET_PACK_GAMEPLAY_PRETIP_SIZE &&
           fnv1a32(assets->storage, assets->storage_size) ==
               TECMO_ASSET_PACK_GAMEPLAY_PRETIP_FNV1A32;
}

bool tecmo_gameplay_pretip_state_initialize(
    const TecmoGameplayPreTipAssets *assets,
    TecmoGameplayPreTipState *state)
{
    TecmoGameplayPreTipState initial;
    if (!assets_valid(assets) || state == NULL) return false;
    memset(&initial, 0, sizeof(initial));
    initial.contract_tag = TECMO_GAMEPLAY_PRETIP_STATE_TAG;
    initial.phase = TECMO_GAMEPLAY_PRETIP_PRESEASON;
    initial.away_tip_error = 12U;
    initial.home_tip_error = 12U;
    *state = initial;
    return true;
}

static void sample_tip(bool held, uint16_t phase_frame, uint16_t target,
                       bool *sampled, uint8_t *error)
{
    unsigned delta;
    if (!held || *sampled) return;
    delta = phase_frame > target ? phase_frame - target
                                 : target - phase_frame;
    if (delta > 11U) delta = 11U;
    *sampled = true;
    *error = (uint8_t)delta;
}

bool tecmo_gameplay_pretip_update(
    const TecmoGameplayPreTipAssets *assets,
    TecmoGameplayPreTipState *state,
    bool player_one_held_b,
    bool player_two_held_b)
{
    uint16_t duration;
    if (!assets_valid(assets) || state == NULL ||
        state->contract_tag != TECMO_GAMEPLAY_PRETIP_STATE_TAG ||
        state->phase > TECMO_GAMEPLAY_PRETIP_LIVE ||
        state->aborted || state->live_handoff)
        return false;
    if (state->phase <= TECMO_GAMEPLAY_PRETIP_FIRST_PERIOD &&
        (player_one_held_b || player_two_held_b)) {
        state->aborted = true;
        return true;
    }
    if (state->phase == TECMO_GAMEPLAY_PRETIP_CLOSEUP) {
        uint16_t target = (uint16_t)(
            assets->phase_frames[TECMO_GAMEPLAY_PRETIP_CLOSEUP] - 14U);
        sample_tip(player_one_held_b, state->phase_frame, target,
                   &state->away_tip_sampled, &state->away_tip_error);
        sample_tip(player_two_held_b, state->phase_frame, target,
                   &state->home_tip_sampled, &state->home_tip_error);
    }
    if (state->phase == TECMO_GAMEPLAY_PRETIP_LIVE) {
        state->live_handoff = true;
        return true;
    }
    duration = assets->phase_frames[state->phase];
    ++state->phase_frame;
    ++state->total_frame;
    if (state->phase_frame >= duration) {
        state->phase_frame = 0U;
        state->phase = (TecmoGameplayPreTipPhase)(state->phase + 1);
        if (state->phase == TECMO_GAMEPLAY_PRETIP_LIVE)
            state->live_handoff = true;
    }
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

bool tecmo_gameplay_pretip_self_test(const char *asset_pack_path,
                                     char *message,
                                     size_t message_size)
{
    TecmoGameplayPreTipAssets assets;
    TecmoGameplayPreTipState state;
    TecmoGameplayPreTipState abort_state;
    TecmoGameplayPreTipState late_b_state;
    unsigned frame;
    bool ok;
    tecmo_gameplay_pretip_init(&assets);
    memset(&state, 0, sizeof(state));
    memset(&abort_state, 0, sizeof(abort_state));
    memset(&late_b_state, 0, sizeof(late_b_state));
    ok = tecmo_gameplay_pretip_load(&assets, asset_pack_path) &&
         tecmo_gameplay_pretip_state_initialize(&assets, &state) &&
         tecmo_gameplay_pretip_state_initialize(&assets, &abort_state) &&
         tecmo_gameplay_pretip_state_initialize(&assets, &late_b_state) &&
         tecmo_gameplay_pretip_update(
             &assets, &abort_state, false, true) &&
         abort_state.aborted && !abort_state.live_handoff;
    for (frame = 0U; ok && frame < 481U; ++frame)
        ok = tecmo_gameplay_pretip_update(
            &assets, &late_b_state, false, false);
    ok = ok &&
         late_b_state.phase == TECMO_GAMEPLAY_PRETIP_BALL_DESCENT &&
         late_b_state.phase_frame == 0U &&
         tecmo_gameplay_pretip_update(
             &assets, &late_b_state, true, false) &&
         !late_b_state.aborted &&
         !late_b_state.away_tip_sampled &&
         late_b_state.phase == TECMO_GAMEPLAY_PRETIP_BALL_DESCENT &&
         late_b_state.phase_frame == 1U;
    for (frame = 0U; ok && frame < 243U; ++frame)
        ok = tecmo_gameplay_pretip_update(
            &assets, &state, false, false);
    ok = ok && state.phase == TECMO_GAMEPLAY_PRETIP_CLOSEUP &&
         state.phase_frame == 0U;
    for (frame = 0U; ok && frame < 194U; ++frame)
        ok = tecmo_gameplay_pretip_update(
            &assets, &state, false, false);
    ok = ok &&
         tecmo_gameplay_pretip_update(&assets, &state, true, false) &&
         !state.aborted && state.away_tip_sampled &&
         state.away_tip_error == 0U && state.home_tip_error == 12U;
    for (frame = 438U; ok && frame < 691U; ++frame)
        ok = tecmo_gameplay_pretip_update(
            &assets, &state, false, false);
    ok = ok && state.phase == TECMO_GAMEPLAY_PRETIP_LIVE &&
         state.live_handoff && state.total_frame == 691U;
    if (message != NULL && message_size > 0U)
        (void)snprintf(message, message_size, "%s",
                       ok ? "TPTI-1 pre-tip self-test passed"
                          : assets.status);
    tecmo_gameplay_pretip_destroy(&assets);
    return ok;
}
