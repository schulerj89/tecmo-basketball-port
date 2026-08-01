#define _CRT_SECURE_NO_WARNINGS

#include "tecmo_gameplay_fatigue.h"

#include "asset_pack/tecmo_asset_pack_gameplay_fatigue.h"
#include "asset_pack/tecmo_asset_pack_import_layout.h"
#include "tecmo_asset_pack.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TECMO_GAMEPLAY_FATIGUE_LIFECYCLE_TAG 0x46544731U
#define FATIGUE_REV1_ROM_SIZE 393232U
#define FATIGUE_REV1_ROM_FNV1A32 0x0650F5B0U

static const uint8_t fatigue_rev1_sha256[32] = {
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

static uint32_t fnv1a32(const uint8_t *bytes, size_t count)
{
    uint32_t hash = 2166136261U;
    for (size_t index = 0U; index < count; ++index) {
        hash ^= bytes[index];
        hash *= 16777619U;
    }
    return hash;
}

static bool bytes_are_zero(const uint8_t *bytes, size_t count)
{
    for (size_t index = 0U; index < count; ++index) {
        if (bytes[index] != 0U) return false;
    }
    return true;
}

static bool range_ok(size_t offset, size_t count, size_t total)
{
    return offset <= total && count <= total - offset;
}

static bool reject(TecmoGameplayFatigueAssets *assets,
                   const char *message)
{
    free(assets->storage);
    assets->storage = NULL;
    assets->storage_size = 0U;
    memset(assets->sources, 0, sizeof(assets->sources));
    memset(assets->cadence_reload, 0, sizeof(assets->cadence_reload));
    assets->capacity_profile_index = 0U;
    assets->condition_maximum = 0U;
    assets->recovery_increment = 0U;
    assets->recovery_reload = 0U;
    assets->team_data_fingerprint = 0U;
    assets->available = false;
    (void)snprintf(assets->status, sizeof(assets->status), "%s",
                   message != NULL ? message : "TGFT-1 rejected");
    return false;
}

void tecmo_gameplay_fatigue_assets_init(TecmoGameplayFatigueAssets *assets)
{
    if (assets == NULL) return;
    memset(assets, 0, sizeof(*assets));
    assets->lifecycle_tag = TECMO_GAMEPLAY_FATIGUE_LIFECYCLE_TAG;
}

void tecmo_gameplay_fatigue_assets_destroy(
    TecmoGameplayFatigueAssets *assets)
{
    if (assets == NULL ||
        assets->lifecycle_tag != TECMO_GAMEPLAY_FATIGUE_LIFECYCLE_TAG) {
        return;
    }
    free(assets->storage);
    tecmo_gameplay_fatigue_assets_init(assets);
}

static bool validate_header(const uint8_t *payload, size_t payload_size)
{
    static const uint8_t cadence[3] = {6U,4U,1U};
    if (payload_size != TECMO_ASSET_PACK_GAMEPLAY_FATIGUE_SIZE ||
        memcmp(payload, "TGFT", 4U) != 0 ||
        read_u16(payload + 4U) !=
            TECMO_ASSET_PACK_GAMEPLAY_FATIGUE_VERSION ||
        read_u16(payload + 6U) !=
            TECMO_ASSET_PACK_GAMEPLAY_FATIGUE_HEADER_SIZE ||
        read_u32(payload + 8U) !=
            TECMO_ASSET_PACK_GAMEPLAY_FATIGUE_SIZE ||
        read_u16(payload + 12U) != TECMO_GAMEPLAY_FATIGUE_SOURCE_COUNT ||
        read_u16(payload + 14U) !=
            TECMO_ASSET_PACK_GAMEPLAY_FATIGUE_SOURCE_STRIDE ||
        read_u32(payload + 16U) !=
            TECMO_ASSET_PACK_GAMEPLAY_FATIGUE_SOURCES_OFFSET ||
        read_u32(payload + 20U) != TECMO_ASSET_PACK_TEAM_DATA_SIZE ||
        read_u32(payload + 24U) != TECMO_ASSET_PACK_TEAM_DATA_FNV1A32 ||
        read_u32(payload + 28U) != FATIGUE_REV1_ROM_SIZE ||
        read_u32(payload + 32U) != FATIGUE_REV1_ROM_FNV1A32 ||
        memcmp(payload + 36U, fatigue_rev1_sha256,
               sizeof(fatigue_rev1_sha256)) != 0 ||
        payload[68U] != TECMO_GAMEPLAY_FATIGUE_DIFFICULTY_COUNT ||
        payload[69U] != TECMO_GAMEPLAY_FATIGUE_TEAM_COUNT ||
        payload[70U] != TECMO_GAMEPLAY_FATIGUE_ROSTER_COUNT ||
        payload[71U] != TECMO_GAMEPLAY_FATIGUE_ACTIVE_COUNT ||
        payload[72U] != 3U || payload[73U] != 100U ||
        payload[74U] != 4U || payload[75U] != 30U ||
        memcmp(payload + 76U, cadence, sizeof(cadence)) != 0 ||
        payload[79U] != 0U ||
        !bytes_are_zero(
            payload + 104U,
            TECMO_ASSET_PACK_GAMEPLAY_FATIGUE_HEADER_SIZE - 104U)) {
        return false;
    }
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_FATIGUE_SOURCE_COUNT; ++index) {
        const TecmoGameplayFatigueExpectedSource *expected =
            &tecmo_gameplay_fatigue_expected_sources[index];
        const uint8_t *descriptor = payload +
            TECMO_ASSET_PACK_GAMEPLAY_FATIGUE_DESCRIPTOR_OFFSET +
            index * TECMO_ASSET_PACK_GAMEPLAY_FATIGUE_DESCRIPTOR_STRIDE;
        if (read_u32(descriptor) != expected->payload_offset ||
            read_u32(descriptor + 4U) != expected->byte_count ||
            read_u32(descriptor + 8U) != expected->fingerprint) {
            return false;
        }
    }
    return true;
}

static bool validate_sources(const uint8_t *payload, size_t payload_size)
{
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_FATIGUE_SOURCE_COUNT; ++index) {
        const TecmoGameplayFatigueExpectedSource *expected =
            &tecmo_gameplay_fatigue_expected_sources[index];
        const uint8_t *record = payload +
            TECMO_ASSET_PACK_GAMEPLAY_FATIGUE_SOURCES_OFFSET +
            index * TECMO_ASSET_PACK_GAMEPLAY_FATIGUE_SOURCE_STRIDE;
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

static bool validate_dependency(const uint8_t *team_data,
                                size_t team_data_size)
{
    return team_data != NULL &&
           team_data_size == TECMO_ASSET_PACK_TEAM_DATA_SIZE &&
           memcmp(team_data, "TTDT", 4U) == 0 &&
           read_u16(team_data + 4U) == 1U &&
           read_u16(team_data + 6U) ==
               TECMO_ASSET_PACK_TEAM_DATA_HEADER_SIZE &&
           read_u32(team_data + 56U) == TECMO_ASSET_PACK_TEAM_DATA_SIZE &&
           fnv1a32(team_data, team_data_size) ==
               TECMO_ASSET_PACK_TEAM_DATA_FNV1A32;
}

bool tecmo_gameplay_fatigue_assets_parse(
    TecmoGameplayFatigueAssets *assets,
    const uint8_t *payload,
    size_t payload_size,
    const uint8_t *team_data,
    size_t team_data_size)
{
    uint8_t *storage;
    if (assets == NULL ||
        assets->lifecycle_tag != TECMO_GAMEPLAY_FATIGUE_LIFECYCLE_TAG) {
        return false;
    }
    tecmo_gameplay_fatigue_assets_destroy(assets);
    if (payload == NULL || !validate_header(payload, payload_size)) {
        return reject(
            assets, "TGFT-1 header/size/reserved contract rejected");
    }
    if (fnv1a32(payload, payload_size) !=
            TECMO_ASSET_PACK_GAMEPLAY_FATIGUE_FNV1A32 ||
        !validate_sources(payload, payload_size) ||
        !bytes_are_zero(
            payload +
                TECMO_ASSET_PACK_GAMEPLAY_FATIGUE_EVOLUTION_OFFSET +
                TECMO_ASSET_PACK_GAMEPLAY_FATIGUE_EVOLUTION_SIZE,
            TECMO_ASSET_PACK_GAMEPLAY_FATIGUE_CALLER_OFFSET -
                (TECMO_ASSET_PACK_GAMEPLAY_FATIGUE_EVOLUTION_OFFSET +
                 TECMO_ASSET_PACK_GAMEPLAY_FATIGUE_EVOLUTION_SIZE))) {
        return reject(
            assets, "TGFT-1 canonical source/padding contract rejected");
    }
    if (!validate_dependency(team_data, team_data_size)) {
        return reject(
            assets, "TGFT-1 same-pack TTDT-1 dependency rejected");
    }
    storage = (uint8_t *)malloc(payload_size);
    if (storage == NULL) return reject(assets, "TGFT-1 allocation failed");
    memcpy(storage, payload, payload_size);
    assets->storage = storage;
    assets->storage_size = payload_size;
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_FATIGUE_SOURCE_COUNT; ++index) {
        const TecmoGameplayFatigueExpectedSource *expected =
            &tecmo_gameplay_fatigue_expected_sources[index];
        TecmoGameplayFatigueSourceSpan *source = &assets->sources[index];
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
    memcpy(assets->cadence_reload, storage + 76U,
           sizeof(assets->cadence_reload));
    assets->capacity_profile_index = storage[72U];
    assets->condition_maximum = storage[73U];
    assets->recovery_increment = storage[74U];
    assets->recovery_reload = storage[75U];
    assets->team_data_fingerprint = TECMO_ASSET_PACK_TEAM_DATA_FNV1A32;
    assets->available = true;
    (void)snprintf(
        assets->status, sizeof(assets->status),
        "TGFT-1 gameplay fatigue assetpack");
    return true;
}

bool tecmo_gameplay_fatigue_assets_load(
    TecmoGameplayFatigueAssets *assets,
    const char *asset_pack_path)
{
    uint8_t *payload = NULL;
    uint8_t *team_data = NULL;
    uint64_t payload_size = 0U;
    uint64_t team_data_size = 0U;
    bool loaded;
    if (assets == NULL ||
        assets->lifecycle_tag != TECMO_GAMEPLAY_FATIGUE_LIFECYCLE_TAG) {
        return false;
    }
    tecmo_gameplay_fatigue_assets_destroy(assets);
    if (asset_pack_path == NULL ||
        tecmo_asset_pack_read_entry_exact(
            asset_pack_path, TECMO_ASSET_PACK_GAMEPLAY_FATIGUE_ID,
            TECMO_ASSET_PACK_GAMEPLAY_FATIGUE_SIZE,
            &payload, &payload_size) != 0) {
        return reject(
            assets,
            "TGFT-1 gameplay/fatigue entry missing or wrong-sized");
    }
    if (tecmo_asset_pack_read_entry_exact(
            asset_pack_path, TECMO_ASSET_PACK_TEAM_DATA_ID,
            TECMO_ASSET_PACK_TEAM_DATA_SIZE,
            &team_data, &team_data_size) != 0) {
        tecmo_asset_pack_free(payload);
        tecmo_asset_pack_free(team_data);
        return reject(
            assets,
            "TGFT-1 menu/team-data dependency missing or wrong-sized");
    }
    loaded = tecmo_gameplay_fatigue_assets_parse(
        assets, payload, (size_t)payload_size,
        team_data, (size_t)team_data_size);
    tecmo_asset_pack_free(payload);
    tecmo_asset_pack_free(team_data);
    return loaded;
}

static bool assets_valid(const TecmoGameplayFatigueAssets *assets)
{
    static const uint8_t expected_cadence[3] = {6U,4U,1U};
    return assets != NULL &&
           assets->lifecycle_tag == TECMO_GAMEPLAY_FATIGUE_LIFECYCLE_TAG &&
           assets->available && assets->storage != NULL &&
           assets->storage_size == TECMO_ASSET_PACK_GAMEPLAY_FATIGUE_SIZE &&
           fnv1a32(assets->storage, assets->storage_size) ==
               TECMO_ASSET_PACK_GAMEPLAY_FATIGUE_FNV1A32 &&
           memcmp(assets->cadence_reload, expected_cadence,
                  sizeof(expected_cadence)) == 0 &&
           assets->capacity_profile_index == 3U &&
           assets->condition_maximum == 100U &&
           assets->recovery_increment == 4U &&
           assets->recovery_reload == 30U &&
           assets->team_data_fingerprint ==
               TECMO_ASSET_PACK_TEAM_DATA_FNV1A32;
}

const TecmoGameplayFatigueSourceSpan *tecmo_gameplay_fatigue_find_source(
    const TecmoGameplayFatigueAssets *assets,
    TecmoGameplayFatigueSourceKind kind)
{
    if (!assets_valid(assets)) return NULL;
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_FATIGUE_SOURCE_COUNT; ++index) {
        if (assets->sources[index].kind == kind) {
            return &assets->sources[index];
        }
    }
    return NULL;
}

bool tecmo_gameplay_fatigue_state_valid(
    const TecmoGameplayFatigueAssets *assets,
    const TecmoGameplayFatigueState *state)
{
    if (!assets_valid(assets) || state == NULL ||
        state->contract_tag != TECMO_GAMEPLAY_FATIGUE_STATE_TAG ||
        state->cadence_counter > assets->cadence_reload[0U]) {
        return false;
    }
    for (size_t team = 0U; team < TECMO_GAMEPLAY_FATIGUE_TEAM_COUNT;
         ++team) {
        for (size_t roster = 0U;
             roster < TECMO_GAMEPLAY_FATIGUE_ROSTER_COUNT; ++roster) {
            uint8_t maximum = state->maximum_capacity[team][roster];
            if (maximum < 10U || maximum > assets->condition_maximum ||
                state->capacity[team][roster] > maximum ||
                state->condition[team][roster] >
                    assets->condition_maximum) {
                return false;
            }
        }
    }
    return true;
}

bool tecmo_gameplay_fatigue_state_initialize(
    const TecmoGameplayFatigueAssets *assets,
    TecmoGameplayFatigueState *state,
    const TecmoGameplayFatigueRosterSeed
        seeds[TECMO_GAMEPLAY_FATIGUE_TEAM_COUNT]
             [TECMO_GAMEPLAY_FATIGUE_ROSTER_COUNT])
{
    TecmoGameplayFatigueState initialized;
    if (!assets_valid(assets) || state == NULL || seeds == NULL) {
        return false;
    }
    memset(&initialized, 0, sizeof(initialized));
    initialized.contract_tag = TECMO_GAMEPLAY_FATIGUE_STATE_TAG;
    for (size_t team = 0U; team < TECMO_GAMEPLAY_FATIGUE_TEAM_COUNT;
         ++team) {
        for (size_t roster = 0U;
             roster < TECMO_GAMEPLAY_FATIGUE_ROSTER_COUNT; ++roster) {
            uint8_t maximum = seeds[team][roster].maximum_capacity;
            if (seeds[team][roster].condition >
                    assets->condition_maximum ||
                maximum < 10U || maximum > assets->condition_maximum) {
                return false;
            }
            initialized.condition[team][roster] =
                seeds[team][roster].condition;
            initialized.capacity[team][roster] = maximum;
            initialized.countdown[team][roster] = maximum;
            initialized.maximum_capacity[team][roster] = maximum;
        }
    }
    if (!tecmo_gameplay_fatigue_state_valid(assets, &initialized)) {
        return false;
    }
    *state = initialized;
    return true;
}

static bool step_input_valid(const TecmoGameplayFatigueAssets *assets,
                             const TecmoGameplayFatigueStepInput *input)
{
    if (!assets_valid(assets) || input == NULL ||
        input->difficulty >= TECMO_GAMEPLAY_FATIGUE_DIFFICULTY_COUNT) {
        return false;
    }
    for (size_t team = 0U; team < TECMO_GAMEPLAY_FATIGUE_TEAM_COUNT;
         ++team) {
        bool seen[TECMO_GAMEPLAY_FATIGUE_ROSTER_COUNT] = {false};
        for (size_t slot = 0U; slot < TECMO_GAMEPLAY_FATIGUE_ACTIVE_COUNT;
             ++slot) {
            uint8_t roster = input->active_roster[team][slot];
            if (roster >= TECMO_GAMEPLAY_FATIGUE_ROSTER_COUNT ||
                seen[roster]) {
                return false;
            }
            seen[roster] = true;
        }
    }
    return true;
}

static bool roster_is_active(const TecmoGameplayFatigueStepInput *input,
                             size_t team,
                             size_t roster)
{
    for (size_t slot = 0U; slot < TECMO_GAMEPLAY_FATIGUE_ACTIVE_COUNT;
         ++slot) {
        if (input->active_roster[team][slot] == roster) return true;
    }
    return false;
}

static uint8_t add_capped(uint8_t value, uint8_t increment, uint8_t maximum)
{
    uint16_t sum = (uint16_t)value + increment;
    return sum < maximum ? (uint8_t)sum : maximum;
}

bool tecmo_gameplay_fatigue_step(
    const TecmoGameplayFatigueAssets *assets,
    TecmoGameplayFatigueState *state,
    const TecmoGameplayFatigueStepInput *input)
{
    TecmoGameplayFatigueState next;
    if (!tecmo_gameplay_fatigue_state_valid(assets, state) ||
        !step_input_valid(assets, input)) {
        return false;
    }
    next = *state;
    if (next.cadence_counter == 0U) {
        next.cadence_counter = assets->cadence_reload[input->difficulty];
        for (size_t team = 0U;
             team < TECMO_GAMEPLAY_FATIGUE_TEAM_COUNT; ++team) {
            for (size_t slot = 0U;
                 slot < TECMO_GAMEPLAY_FATIGUE_ACTIVE_COUNT; ++slot) {
                uint8_t roster = input->active_roster[team][slot];
                --next.countdown[team][roster];
                if (next.countdown[team][roster] != 0U) continue;
                if (next.capacity[team][roster] >= 10U) {
                    --next.capacity[team][roster];
                }
                next.countdown[team][roster] =
                    next.capacity[team][roster];
                if (next.condition[team][roster] >= 10U) {
                    --next.condition[team][roster];
                }
            }
        }
    }
    --next.cadence_counter;

    for (size_t team = 0U; team < TECMO_GAMEPLAY_FATIGUE_TEAM_COUNT;
         ++team) {
        for (size_t reverse = TECMO_GAMEPLAY_FATIGUE_ROSTER_COUNT;
             reverse > 0U; --reverse) {
            size_t roster = reverse - 1U;
            if (roster_is_active(input, team, roster)) continue;
            if (next.countdown[team][roster] != 0U) {
                --next.countdown[team][roster];
                continue;
            }
            next.countdown[team][roster] = assets->recovery_reload;
            if (next.condition[team][roster] != 0U) {
                next.condition[team][roster] = add_capped(
                    next.condition[team][roster],
                    assets->recovery_increment,
                    assets->condition_maximum);
            }
            next.capacity[team][roster] = add_capped(
                next.capacity[team][roster], assets->recovery_increment,
                next.maximum_capacity[team][roster]);
            /* The second roster path's final two stores are both present in
               Rev1: recovered capacity also becomes its recovery countdown. */
            if (team == 1U) {
                next.countdown[team][roster] =
                    next.capacity[team][roster];
            }
        }
    }
    if (!tecmo_gameplay_fatigue_state_valid(assets, &next)) return false;
    *state = next;
    return true;
}

bool tecmo_gameplay_fatigue_self_test(const char *asset_pack_path,
                                      char *message,
                                      size_t message_size)
{
    TecmoGameplayFatigueAssets assets;
    TecmoGameplayFatigueRosterSeed
        seeds[TECMO_GAMEPLAY_FATIGUE_TEAM_COUNT]
             [TECMO_GAMEPLAY_FATIGUE_ROSTER_COUNT];
    TecmoGameplayFatigueState state;
    TecmoGameplayFatigueState before;
    TecmoGameplayFatigueStepInput input;
    bool ok = false;
    tecmo_gameplay_fatigue_assets_init(&assets);
    if (!tecmo_gameplay_fatigue_assets_load(&assets, asset_pack_path)) {
        (void)snprintf(message, message_size, "%s", assets.status);
        goto cleanup;
    }
    memset(seeds, 0, sizeof(seeds));
    memset(&input, 0, sizeof(input));
    for (size_t team = 0U; team < TECMO_GAMEPLAY_FATIGUE_TEAM_COUNT;
         ++team) {
        for (size_t roster = 0U;
             roster < TECMO_GAMEPLAY_FATIGUE_ROSTER_COUNT; ++roster) {
            seeds[team][roster].condition = 100U;
            seeds[team][roster].maximum_capacity = 30U;
        }
        for (size_t slot = 0U; slot < TECMO_GAMEPLAY_FATIGUE_ACTIVE_COUNT;
             ++slot) {
            input.active_roster[team][slot] = (uint8_t)slot;
        }
    }
    if (!tecmo_gameplay_fatigue_state_initialize(
            &assets, &state, seeds)) {
        (void)snprintf(message, message_size,
                       "TGFT-1 state initialization failed");
        goto cleanup;
    }
    input.difficulty = 2U;
    state.capacity[0U][0U] = 10U;
    state.countdown[0U][0U] = 1U;
    state.condition[0U][0U] = 10U;
    state.capacity[0U][5U] = 20U;
    state.countdown[0U][5U] = 0U;
    state.condition[0U][5U] = 96U;
    state.capacity[1U][5U] = 20U;
    state.countdown[1U][5U] = 0U;
    state.condition[1U][5U] = 96U;
    if (!tecmo_gameplay_fatigue_step(&assets, &state, &input) ||
        state.cadence_counter != 0U ||
        state.capacity[0U][0U] != 9U ||
        state.countdown[0U][0U] != 9U ||
        state.condition[0U][0U] != 9U ||
        state.capacity[0U][5U] != 24U ||
        state.countdown[0U][5U] != 30U ||
        state.condition[0U][5U] != 100U ||
        state.capacity[1U][5U] != 24U ||
        state.countdown[1U][5U] != 24U ||
        state.condition[1U][5U] != 100U) {
        (void)snprintf(message, message_size,
                       "TGFT-1 decay/recovery vector failed");
        goto cleanup;
    }
    input.difficulty = 0U;
    state.cadence_counter = 0U;
    state.countdown[0U][0U] = 20U;
    if (!tecmo_gameplay_fatigue_step(&assets, &state, &input) ||
        state.cadence_counter != 5U ||
        state.countdown[0U][0U] != 19U) {
        (void)snprintf(message, message_size,
                       "TGFT-1 difficulty cadence vector failed");
        goto cleanup;
    }
    before = state;
    input.active_roster[0U][4U] = input.active_roster[0U][3U];
    if (tecmo_gameplay_fatigue_step(&assets, &state, &input) ||
        memcmp(&state, &before, sizeof(state)) != 0) {
        (void)snprintf(message, message_size,
                       "TGFT-1 transactional active-list rejection failed");
        goto cleanup;
    }
    input.active_roster[0U][4U] = 4U;
    state.contract_tag ^= 1U;
    before = state;
    if (tecmo_gameplay_fatigue_step(&assets, &state, &input) ||
        memcmp(&state, &before, sizeof(state)) != 0) {
        (void)snprintf(message, message_size,
                       "TGFT-1 malformed-state rejection failed");
        goto cleanup;
    }
    ok = true;
    (void)snprintf(message, message_size,
                   "TGFT-1 fatigue parser/evolution self-test passed");
cleanup:
    tecmo_gameplay_fatigue_assets_destroy(&assets);
    return ok;
}
