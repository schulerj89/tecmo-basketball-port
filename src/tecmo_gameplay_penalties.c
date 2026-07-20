#define _CRT_SECURE_NO_WARNINGS

#include "tecmo_gameplay_penalties.h"

#include "asset_pack/tecmo_asset_pack_gameplay.h"
#include "asset_pack/tecmo_asset_pack_gameplay_penalties.h"
#include "asset_pack/tecmo_asset_pack_import_layout.h"
#include "tecmo_asset_pack.h"
#include "tecmo_gameplay_audio.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TECMO_GAMEPLAY_PENALTIES_LIFECYCLE_TAG 0x4C4E5054U
#define PENALTY_PRESENTATION_COUNT 2U

static const uint8_t penalty_rev1_sha256[32] = {
    0x07U,0x6AU,0x6BU,0xEBU,0x27U,0x3FU,0xABU,0x39U,
    0x19U,0x8CU,0x87U,0xAEU,0x6AU,0xF6U,0x9FU,0x80U,
    0xAAU,0x54U,0x8DU,0x68U,0x17U,0x75U,0x38U,0x29U,
    0xF2U,0xC2U,0xBDU,0xE1U,0xF9U,0x74U,0x75U,0xC4U
};

static uint16_t read_u16(const uint8_t *bytes)
{
    return (uint16_t)((uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8U));
}

static uint32_t read_u32(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8U) |
           ((uint32_t)bytes[2] << 16U) | ((uint32_t)bytes[3] << 24U);
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

static bool reject(TecmoGameplayPenaltyAssets *assets,
                   const char *message)
{
    free(assets->storage);
    assets->storage = NULL;
    assets->storage_size = 0U;
    assets->rules = NULL;
    assets->foul_classes = NULL;
    assets->attempt_selectors = NULL;
    assets->violations = NULL;
    assets->presentations = NULL;
    assets->gameplay_core_fingerprint = 0U;
    assets->gameplay_sfx_fingerprint = 0U;
    memset(assets->sources, 0, sizeof(assets->sources));
    assets->available = false;
    (void)snprintf(assets->status, sizeof(assets->status), "%s",
                   message != NULL ? message : "TPNL-1 rejected");
    return false;
}

void tecmo_gameplay_penalties_init(TecmoGameplayPenaltyAssets *assets)
{
    if (assets == NULL) return;
    memset(assets, 0, sizeof(*assets));
    assets->lifecycle_tag = TECMO_GAMEPLAY_PENALTIES_LIFECYCLE_TAG;
}

void tecmo_gameplay_penalties_destroy(TecmoGameplayPenaltyAssets *assets)
{
    if (assets == NULL ||
        assets->lifecycle_tag != TECMO_GAMEPLAY_PENALTIES_LIFECYCLE_TAG) {
        return;
    }
    free(assets->storage);
    tecmo_gameplay_penalties_init(assets);
}

static bool validate_header(const uint8_t *payload, size_t payload_size)
{
    return payload_size == TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_SIZE &&
           memcmp(payload, "TPNL", 4U) == 0 &&
           read_u16(payload + 4U) ==
               TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_VERSION &&
           read_u16(payload + 6U) ==
               TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_HEADER_SIZE &&
           read_u32(payload + 8U) ==
               TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_SIZE &&
           read_u16(payload + 12U) ==
               TECMO_GAMEPLAY_PENALTY_SOURCE_COUNT &&
           read_u16(payload + 14U) ==
               TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_SOURCE_STRIDE &&
           read_u32(payload + 16U) ==
               TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_SOURCES_OFFSET &&
           read_u32(payload + 20U) ==
               TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_RULES_OFFSET &&
           read_u32(payload + 24U) ==
               TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_RULES_SIZE &&
           read_u32(payload + 28U) ==
               TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_CLASSES_OFFSET &&
           read_u16(payload + 32U) ==
               TECMO_GAMEPLAY_PENALTY_FOUL_CLASS_COUNT &&
           read_u16(payload + 34U) ==
               TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_CLASS_STRIDE &&
           read_u32(payload + 36U) ==
               TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_SELECTORS_OFFSET &&
           read_u32(payload + 40U) ==
               TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_SELECTORS_SIZE &&
           read_u32(payload + 44U) ==
               TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_VIOLATIONS_OFFSET &&
           read_u16(payload + 48U) ==
               TECMO_GAMEPLAY_PENALTY_VIOLATION_COUNT &&
           read_u16(payload + 50U) ==
               TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_VIOLATION_STRIDE &&
           read_u32(payload + 52U) ==
               TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_PRESENTATIONS_OFFSET &&
           read_u16(payload + 56U) == PENALTY_PRESENTATION_COUNT &&
           read_u16(payload + 58U) ==
               TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_PRESENTATION_STRIDE &&
           read_u16(payload + 60U) == 2U &&
           read_u16(payload + 62U) == 0U &&
           read_u32(payload + 64U) == TECMO_ASSET_PACK_GAMEPLAY_SIZE &&
           read_u32(payload + 68U) == TECMO_ASSET_PACK_GAMEPLAY_FNV1A32 &&
           read_u32(payload + 72U) == TECMO_GAMEPLAY_SFX_PAYLOAD_SIZE &&
           read_u32(payload + 76U) ==
               TECMO_GAMEPLAY_SFX_PAYLOAD_FNV1A32 &&
           read_u32(payload + 80U) ==
               TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_SOURCES_FNV1A32 &&
           read_u32(payload + 84U) ==
               TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_RULES_FNV1A32 &&
           read_u32(payload + 88U) ==
               TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_CLASSES_FNV1A32 &&
           read_u32(payload + 92U) ==
               TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_SELECTORS_FNV1A32 &&
           read_u32(payload + 96U) ==
               TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_VIOLATIONS_FNV1A32 &&
           read_u32(payload + 100U) ==
               TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_PRESENTATIONS_FNV1A32 &&
           memcmp(payload + 104U, penalty_rev1_sha256,
                  sizeof(penalty_rev1_sha256)) == 0 &&
           bytes_are_zero(payload + 136U,
                          TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_HEADER_SIZE -
                              136U);
}

static bool validate_sources(const uint8_t *payload)
{
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_PENALTY_SOURCE_COUNT; ++index) {
        const TecmoGameplayPenaltyExpectedSource *expected =
            &tecmo_gameplay_penalty_expected_sources[index];
        const uint8_t *record = payload +
            TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_SOURCES_OFFSET +
            index * TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_SOURCE_STRIDE;
        uint32_t cpu_end = (uint32_t)expected->cpu_start +
                           expected->byte_count - 1U;
        if (read_u16(record) != (uint16_t)expected->kind ||
            record[2U] != expected->bank ||
            record[3U] != expected->fixed_bank ||
            read_u16(record + 4U) != expected->cpu_start ||
            read_u16(record + 6U) != (uint16_t)cpu_end ||
            read_u32(record + 8U) != expected->byte_count ||
            read_u32(record + 12U) != expected->fingerprint ||
            read_u16(record + 16U) != (uint16_t)index ||
            !bytes_are_zero(record + 18U, 14U)) {
            return false;
        }
    }
    return fnv1a32(
               payload + TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_SOURCES_OFFSET,
               TECMO_GAMEPLAY_PENALTY_SOURCE_COUNT *
                   TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_SOURCE_STRIDE) ==
           TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_SOURCES_FNV1A32;
}

static bool validate_semantics(const uint8_t *payload)
{
    const uint8_t *rules = payload +
        TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_RULES_OFFSET;
    const uint8_t *classes = payload +
        TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_CLASSES_OFFSET;
    const uint8_t *selectors = payload +
        TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_SELECTORS_OFFSET;
    const uint8_t *violations = payload +
        TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_VIOLATIONS_OFFSET;
    const uint8_t *presentations = payload +
        TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_PRESENTATIONS_OFFSET;
    return memcmp(rules, tecmo_gameplay_penalty_expected_rules,
                  sizeof(tecmo_gameplay_penalty_expected_rules)) == 0 &&
           memcmp(classes, tecmo_gameplay_penalty_expected_classes,
                  sizeof(tecmo_gameplay_penalty_expected_classes)) == 0 &&
           memcmp(selectors, tecmo_gameplay_penalty_expected_selectors,
                  sizeof(tecmo_gameplay_penalty_expected_selectors)) == 0 &&
           memcmp(violations, tecmo_gameplay_penalty_expected_violations,
                  sizeof(tecmo_gameplay_penalty_expected_violations)) == 0 &&
           memcmp(presentations,
                  tecmo_gameplay_penalty_expected_presentations,
                  sizeof(tecmo_gameplay_penalty_expected_presentations)) == 0 &&
           fnv1a32(rules,
                   TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_RULES_SIZE) ==
               TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_RULES_FNV1A32 &&
           fnv1a32(classes,
                   TECMO_GAMEPLAY_PENALTY_FOUL_CLASS_COUNT *
                       TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_CLASS_STRIDE) ==
               TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_CLASSES_FNV1A32 &&
           fnv1a32(selectors,
                   TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_SELECTORS_SIZE) ==
               TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_SELECTORS_FNV1A32 &&
           fnv1a32(violations,
                   TECMO_GAMEPLAY_PENALTY_VIOLATION_COUNT *
                       TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_VIOLATION_STRIDE) ==
               TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_VIOLATIONS_FNV1A32 &&
           fnv1a32(presentations,
                   PENALTY_PRESENTATION_COUNT *
                       TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_PRESENTATION_STRIDE) ==
               TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_PRESENTATIONS_FNV1A32;
}

static bool validate_dependencies(const uint8_t *gameplay_core,
                                  size_t gameplay_core_size,
                                  const uint8_t *gameplay_sfx,
                                  size_t gameplay_sfx_size)
{
    return gameplay_core != NULL &&
           gameplay_core_size == TECMO_ASSET_PACK_GAMEPLAY_SIZE &&
           memcmp(gameplay_core, "TGPL", 4U) == 0 &&
           read_u16(gameplay_core + 4U) == TECMO_ASSET_PACK_GAMEPLAY_VERSION &&
           read_u32(gameplay_core + 8U) == TECMO_ASSET_PACK_GAMEPLAY_SIZE &&
           fnv1a32(gameplay_core, gameplay_core_size) ==
               TECMO_ASSET_PACK_GAMEPLAY_FNV1A32 &&
           gameplay_sfx != NULL &&
           gameplay_sfx_size == TECMO_GAMEPLAY_SFX_PAYLOAD_SIZE &&
           memcmp(gameplay_sfx, "TSFX", 4U) == 0 &&
           read_u16(gameplay_sfx + 4U) == 1U &&
           read_u32(gameplay_sfx + 8U) == TECMO_GAMEPLAY_SFX_PAYLOAD_SIZE &&
           fnv1a32(gameplay_sfx, gameplay_sfx_size) ==
               TECMO_GAMEPLAY_SFX_PAYLOAD_FNV1A32;
}

bool tecmo_gameplay_penalties_parse(
    TecmoGameplayPenaltyAssets *assets,
    const uint8_t *payload,
    size_t payload_size,
    const uint8_t *gameplay_core,
    size_t gameplay_core_size,
    const uint8_t *gameplay_sfx,
    size_t gameplay_sfx_size)
{
    uint8_t *storage;
    if (assets == NULL ||
        assets->lifecycle_tag != TECMO_GAMEPLAY_PENALTIES_LIFECYCLE_TAG) {
        return false;
    }
    tecmo_gameplay_penalties_destroy(assets);
    if (payload == NULL || !validate_header(payload, payload_size)) {
        return reject(assets, "TPNL-1 header/size/reserved contract rejected");
    }
    if (fnv1a32(payload, payload_size) !=
            TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_FNV1A32) {
        return reject(assets, "TPNL-1 canonical payload fingerprint rejected");
    }
    if (!validate_sources(payload) || !validate_semantics(payload)) {
        return reject(assets, "TPNL-1 source/semantic contract rejected");
    }
    if (!validate_dependencies(gameplay_core, gameplay_core_size,
                               gameplay_sfx, gameplay_sfx_size)) {
        return reject(
            assets,
            "TPNL-1 same-pack gameplay/core and audio/gameplay-sfx dependencies rejected");
    }
    storage = (uint8_t *)malloc(payload_size);
    if (storage == NULL) return reject(assets, "TPNL-1 allocation failed");
    memcpy(storage, payload, payload_size);
    assets->storage = storage;
    assets->storage_size = payload_size;
    assets->rules = storage +
        TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_RULES_OFFSET;
    assets->foul_classes = storage +
        TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_CLASSES_OFFSET;
    assets->attempt_selectors = storage +
        TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_SELECTORS_OFFSET;
    assets->violations = storage +
        TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_VIOLATIONS_OFFSET;
    assets->presentations = storage +
        TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_PRESENTATIONS_OFFSET;
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_PENALTY_SOURCE_COUNT; ++index) {
        const TecmoGameplayPenaltyExpectedSource *expected =
            &tecmo_gameplay_penalty_expected_sources[index];
        TecmoGameplayPenaltySourceSpan *source = &assets->sources[index];
        source->kind = expected->kind;
        source->bank = expected->bank;
        source->fixed_bank = expected->fixed_bank != 0U;
        source->cpu_start = expected->cpu_start;
        source->cpu_end = (uint16_t)((uint32_t)expected->cpu_start +
                                     expected->byte_count - 1U);
        source->byte_count = expected->byte_count;
        source->fingerprint = expected->fingerprint;
    }
    assets->gameplay_core_fingerprint = TECMO_ASSET_PACK_GAMEPLAY_FNV1A32;
    assets->gameplay_sfx_fingerprint =
        TECMO_GAMEPLAY_SFX_PAYLOAD_FNV1A32;
    assets->available = true;
    (void)snprintf(assets->status, sizeof(assets->status),
                   "TPNL-1 gameplay penalty rules assetpack");
    return true;
}

bool tecmo_gameplay_penalties_load(TecmoGameplayPenaltyAssets *assets,
                                   const char *asset_pack_path)
{
    uint8_t *payload = NULL;
    uint8_t *gameplay_core = NULL;
    uint8_t *gameplay_sfx = NULL;
    uint64_t payload_size = 0U;
    uint64_t gameplay_core_size = 0U;
    uint64_t gameplay_sfx_size = 0U;
    bool loaded;
    if (assets == NULL ||
        assets->lifecycle_tag != TECMO_GAMEPLAY_PENALTIES_LIFECYCLE_TAG) {
        return false;
    }
    tecmo_gameplay_penalties_destroy(assets);
    if (asset_pack_path == NULL ||
        tecmo_asset_pack_read_entry_exact(
            asset_pack_path, TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_ID,
            TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_SIZE,
            &payload, &payload_size) != 0) {
        return reject(
            assets,
            "TPNL-1 gameplay/penalties entry missing or wrong-sized");
    }
    if (tecmo_asset_pack_read_entry_exact(
            asset_pack_path, TECMO_ASSET_PACK_GAMEPLAY_ID,
            TECMO_ASSET_PACK_GAMEPLAY_SIZE,
            &gameplay_core, &gameplay_core_size) != 0) {
        tecmo_asset_pack_free(payload);
        return reject(assets,
                      "TPNL-1 gameplay/core dependency missing or wrong-sized");
    }
    if (tecmo_asset_pack_read_entry_exact(
            asset_pack_path, TECMO_ASSET_PACK_GAMEPLAY_SFX_ID,
            TECMO_GAMEPLAY_SFX_PAYLOAD_SIZE,
            &gameplay_sfx, &gameplay_sfx_size) != 0) {
        tecmo_asset_pack_free(payload);
        tecmo_asset_pack_free(gameplay_core);
        return reject(
            assets,
            "TPNL-1 audio/gameplay-sfx dependency missing or wrong-sized");
    }
    loaded = tecmo_gameplay_penalties_parse(
        assets, payload, (size_t)payload_size,
        gameplay_core, (size_t)gameplay_core_size,
        gameplay_sfx, (size_t)gameplay_sfx_size);
    tecmo_asset_pack_free(payload);
    tecmo_asset_pack_free(gameplay_core);
    tecmo_asset_pack_free(gameplay_sfx);
    return loaded;
}

const TecmoGameplayPenaltySourceSpan *tecmo_gameplay_penalties_find_source(
    const TecmoGameplayPenaltyAssets *assets,
    TecmoGameplayPenaltySourceKind kind)
{
    if (assets == NULL || !assets->available) return NULL;
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_PENALTY_SOURCE_COUNT; ++index) {
        if (assets->sources[index].kind == kind) {
            return &assets->sources[index];
        }
    }
    return NULL;
}

static bool selector_in_list(const uint8_t *selectors,
                             size_t offset,
                             size_t count,
                             uint8_t value)
{
    for (size_t index = 0U; index < count; ++index) {
        if (selectors[offset + index] == value) return true;
    }
    return false;
}

bool tecmo_gameplay_penalties_classify(
    const TecmoGameplayPenaltyAssets *assets,
    const TecmoGameplayPenaltyContext *context,
    TecmoGameplayPenaltyResult *result)
{
    const uint8_t *rules;
    const uint8_t *selectors;
    uint8_t individual_after;
    uint8_t team_after;
    uint8_t bonus_threshold;
    bool offensive;
    bool bonus;
    if (assets == NULL || !assets->available || context == NULL ||
        result == NULL) {
        return false;
    }
    rules = assets->rules;
    selectors = assets->attempt_selectors;
    if (context->foul_actor >= rules[4U] ||
        context->offensive_primary_actor >= rules[4U] ||
        context->individual_fouls > rules[0U] ||
        context->team_fouls > rules[1U] ||
        (unsigned)context->period_kind >=
            TECMO_GAMEPLAY_PENALTY_PERIOD_KIND_COUNT ||
        selectors[0U] != rules[17U] || selectors[1U] != rules[18U]) {
        return false;
    }
    memset(result, 0, sizeof(*result));
    offensive = context->foul_actor == context->offensive_primary_actor;
    result->offensive_foul = offensive;
    result->turnover = offensive && rules[12U] != 0U;
    if (offensive) {
        result->foul_class = context->saved_route == rules[14U]
            ? (TecmoGameplayFoulClass)rules[5U]
            : (TecmoGameplayFoulClass)rules[7U];
    } else if (context->current_route != rules[16U] ||
               (uint8_t)(context->contact_selector >> rules[15U]) == 0U) {
        result->foul_class = (TecmoGameplayFoulClass)rules[7U];
    } else {
        result->foul_class = (TecmoGameplayFoulClass)rules[6U];
    }

    individual_after = context->individual_fouls;
    if (individual_after < rules[0U]) {
        individual_after = (uint8_t)(individual_after + rules[8U]);
    }
    team_after = context->team_fouls;
    if (result->foul_class != TECMO_GAMEPLAY_FOUL_CLASS_CHARGING &&
        team_after < rules[1U]) {
        team_after = (uint8_t)(team_after + rules[9U]);
    }
    result->individual_fouls_after = individual_after;
    result->team_fouls_after = team_after;
    result->individual_foul_delta =
        (uint8_t)(individual_after - context->individual_fouls);
    result->team_foul_delta =
        (uint8_t)(team_after - context->team_fouls);
    result->fouled_out = individual_after >= rules[0U];
    bonus_threshold = context->period_kind ==
            TECMO_GAMEPLAY_PENALTY_PERIOD_OVERTIME
        ? rules[3U]
        : rules[2U];
    bonus = team_after >= bonus_threshold;
    result->team_in_bonus = bonus;

    if (offensive) {
        result->free_throw_attempts = 0U;
    } else if (selector_in_list(selectors, 2U + selectors[0U],
                                selectors[1U], context->current_route)) {
        result->free_throw_attempts = 1U;
    } else if (selector_in_list(selectors, 2U, selectors[0U],
                                context->saved_route)) {
        result->free_throw_attempts = rules[11U];
    } else if (bonus) {
        result->free_throw_attempts = rules[11U];
        result->attempts_from_bonus = true;
    }
    return result->foul_class >= TECMO_GAMEPLAY_FOUL_CLASS_CHARGING &&
           result->foul_class <= TECMO_GAMEPLAY_FOUL_CLASS_PUSHING;
}

static void decode_presentation(const uint8_t *record,
                                TecmoGameplayPenaltyPresentation *presentation)
{
    presentation->kind =
        (TecmoGameplayPenaltyPresentationKind)record[0U];
    presentation->selector_min = record[1U];
    presentation->selector_max = record[2U];
    presentation->entry_sfx_id = record[3U];
    presentation->live_restart_sfx_id = record[4U];
    presentation->live_restart_music_id = record[5U];
    presentation->lead_in_frames = read_u16(record + 8U);
    presentation->maximum_wait_frames = read_u16(record + 10U);
    presentation->release_button_mask = record[12U];
    presentation->controller_count = record[13U];
}

bool tecmo_gameplay_penalties_get_presentation(
    const TecmoGameplayPenaltyAssets *assets,
    TecmoGameplayPenaltyPresentationKind kind,
    uint8_t selector,
    TecmoGameplayPenaltyPresentation *presentation)
{
    if (assets == NULL || !assets->available || presentation == NULL ||
        (kind != TECMO_GAMEPLAY_PENALTY_PRESENTATION_FOUL &&
         kind != TECMO_GAMEPLAY_PENALTY_PRESENTATION_VIOLATION)) {
        return false;
    }
    for (size_t index = 0U; index < PENALTY_PRESENTATION_COUNT; ++index) {
        const uint8_t *record = assets->presentations +
            index * TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_PRESENTATION_STRIDE;
        if (record[0U] != (uint8_t)kind) continue;
        if ((kind == TECMO_GAMEPLAY_PENALTY_PRESENTATION_FOUL &&
             selector != 0U) ||
            (kind == TECMO_GAMEPLAY_PENALTY_PRESENTATION_VIOLATION &&
             (selector < record[1U] || selector > record[2U]))) {
            return false;
        }
        decode_presentation(record, presentation);
        return true;
    }
    return false;
}

bool tecmo_gameplay_penalties_get_violation(
    const TecmoGameplayPenaltyAssets *assets,
    uint8_t selector,
    TecmoGameplayViolation *violation,
    TecmoGameplayPenaltyPresentation *presentation)
{
    const uint8_t *record;
    if (assets == NULL || !assets->available || violation == NULL ||
        presentation == NULL || selector == 0U ||
        selector > TECMO_GAMEPLAY_PENALTY_VIOLATION_COUNT) {
        return false;
    }
    record = assets->violations +
        (size_t)(selector - 1U) *
            TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_VIOLATION_STRIDE;
    if (record[0U] != selector || record[1U] < 1U || record[1U] > 7U ||
        record[2U] != (selector == 3U ? 1U : 0U) ||
        !tecmo_gameplay_penalties_get_presentation(
            assets, TECMO_GAMEPLAY_PENALTY_PRESENTATION_VIOLATION,
            selector, presentation) ||
        read_u16(record + 4U) != presentation->lead_in_frames ||
        read_u16(record + 6U) != presentation->maximum_wait_frames ||
        record[8U] != presentation->release_button_mask ||
        record[9U] != presentation->controller_count ||
        record[10U] != presentation->entry_sfx_id) {
        return false;
    }
    *violation = (TecmoGameplayViolation)record[1U];
    return true;
}

static bool self_test_expect(
    const TecmoGameplayPenaltyAssets *assets,
    const TecmoGameplayPenaltyContext *context,
    TecmoGameplayFoulClass foul_class,
    uint8_t individual_after,
    uint8_t team_after,
    uint8_t attempts,
    bool turnover,
    bool bonus,
    bool attempts_from_bonus)
{
    TecmoGameplayPenaltyResult result;
    return tecmo_gameplay_penalties_classify(assets, context, &result) &&
           result.foul_class == foul_class &&
           result.individual_fouls_after == individual_after &&
           result.team_fouls_after == team_after &&
           result.individual_foul_delta ==
               (uint8_t)(individual_after - context->individual_fouls) &&
           result.team_foul_delta ==
               (uint8_t)(team_after - context->team_fouls) &&
           result.free_throw_attempts == attempts &&
           result.turnover == turnover &&
           result.team_in_bonus == bonus &&
           result.attempts_from_bonus == attempts_from_bonus &&
           result.fouled_out == (individual_after >= 6U);
}

bool tecmo_gameplay_penalties_self_test(const char *asset_pack_path,
                                        char *message,
                                        size_t message_size)
{
    TecmoGameplayPenaltyAssets assets;
    TecmoGameplayPenaltyContext context;
    TecmoGameplayPenaltyResult result;
    TecmoGameplayPenaltyPresentation presentation;
    TecmoGameplayViolation violation;
    const TecmoGameplayPenaltySourceSpan *rules_source;
    bool passed = false;
    tecmo_gameplay_penalties_init(&assets);
    memset(&context, 0, sizeof(context));
    if (tecmo_gameplay_penalties_classify(&assets, &context, &result) ||
        tecmo_gameplay_penalties_get_violation(
            &assets, 3U, &violation, &presentation) ||
        tecmo_gameplay_penalties_find_source(
            &assets,
            TECMO_GAMEPLAY_PENALTY_SOURCE_FOUL_RULES_PRESENTATION) != NULL) {
        (void)snprintf(message, message_size,
                       "unavailable helper accepted input");
        goto cleanup;
    }
    if (asset_pack_path == NULL ||
        !tecmo_gameplay_penalties_load(&assets, asset_pack_path) ||
        !tecmo_gameplay_penalties_load(&assets, asset_pack_path)) {
        (void)snprintf(message, message_size, "%s",
                       asset_pack_path != NULL ? assets.status
                                               : "PACK path required");
        goto cleanup;
    }
    rules_source = tecmo_gameplay_penalties_find_source(
        &assets, TECMO_GAMEPLAY_PENALTY_SOURCE_FOUL_RULES_PRESENTATION);
    if (rules_source == NULL || rules_source->bank != 2U ||
        rules_source->fixed_bank || rules_source->cpu_start != 0xB0F8U ||
        rules_source->cpu_end != 0xB398U ||
        rules_source->byte_count != 673U ||
        rules_source->fingerprint != 0xA06E397CU ||
        tecmo_gameplay_penalties_find_source(
            &assets, (TecmoGameplayPenaltySourceKind)0) != NULL ||
        tecmo_gameplay_penalties_find_source(
            &assets, (TecmoGameplayPenaltySourceKind)8) != NULL) {
        (void)snprintf(message, message_size,
                       "source provenance contract failed");
        goto cleanup;
    }

    /* M01: defensive PUSHING, side 0 counters, two attempts for side 1. */
    context.foul_actor = 0U;
    context.offensive_primary_actor = 7U;
    context.saved_route = 5U;
    context.current_route = 5U;
    context.contact_selector = 0U;
    context.individual_fouls = 0U;
    context.team_fouls = 0U;
    context.period_kind = TECMO_GAMEPLAY_PENALTY_PERIOD_REGULATION;
    if (!self_test_expect(
            &assets, &context, TECMO_GAMEPLAY_FOUL_CLASS_PUSHING,
            1U, 1U, 2U, false, false, false)) {
        (void)snprintf(message, message_size,
                       "M01 defensive PUSHING context failed");
        goto cleanup;
    }
    /* M06 mirrors M01 with the other side's actor slots. */
    context.foul_actor = 6U;
    context.offensive_primary_actor = 1U;
    if (!self_test_expect(
            &assets, &context, TECMO_GAMEPLAY_FOUL_CLASS_PUSHING,
            1U, 1U, 2U, false, false, false)) {
        (void)snprintf(message, message_size,
                       "M06 defensive PUSHING context failed");
        goto cleanup;
    }
    /* M07: route-zero offensive primary is CHARGING and a turnover. */
    context.foul_actor = 0U;
    context.offensive_primary_actor = 0U;
    context.saved_route = 0U;
    context.current_route = 25U;
    context.individual_fouls = 1U;
    context.team_fouls = 2U;
    if (!self_test_expect(
            &assets, &context, TECMO_GAMEPLAY_FOUL_CLASS_CHARGING,
            2U, 2U, 0U, true, false, false)) {
        (void)snprintf(message, message_size,
                       "M07 offensive CHARGING context failed");
        goto cleanup;
    }

    /* A nonzero offensive route is PUSHING but still never awards attempts. */
    context.saved_route = 5U;
    if (!self_test_expect(
            &assets, &context, TECMO_GAMEPLAY_FOUL_CLASS_PUSHING,
            2U, 3U, 0U, true, false, false)) {
        (void)snprintf(message, message_size,
                       "offensive PUSHING context failed");
        goto cleanup;
    }

    context.foul_actor = 1U;
    context.offensive_primary_actor = 0U;
    context.saved_route = 0U;
    context.current_route = 0U;
    context.contact_selector = 4U;
    context.individual_fouls = 5U;
    context.team_fouls = 3U;
    if (!self_test_expect(
            &assets, &context, TECMO_GAMEPLAY_FOUL_CLASS_BLOCKING,
            6U, 4U, 0U, false, false, false)) {
        (void)snprintf(message, message_size,
                       "regulation pre-bonus/cap boundary failed");
        goto cleanup;
    }
    context.individual_fouls = 6U;
    context.team_fouls = 4U;
    if (!self_test_expect(
            &assets, &context, TECMO_GAMEPLAY_FOUL_CLASS_BLOCKING,
            6U, 5U, 2U, false, true, true)) {
        (void)snprintf(message, message_size,
                       "regulation bonus/cap boundary failed");
        goto cleanup;
    }
    context.individual_fouls = 0U;
    context.team_fouls = 2U;
    context.period_kind = TECMO_GAMEPLAY_PENALTY_PERIOD_OVERTIME;
    if (!self_test_expect(
            &assets, &context, TECMO_GAMEPLAY_FOUL_CLASS_BLOCKING,
            1U, 3U, 0U, false, false, false)) {
        (void)snprintf(message, message_size,
                       "overtime pre-bonus boundary failed");
        goto cleanup;
    }
    context.team_fouls = 3U;
    if (!self_test_expect(
            &assets, &context, TECMO_GAMEPLAY_FOUL_CLASS_BLOCKING,
            1U, 4U, 2U, false, true, true)) {
        (void)snprintf(message, message_size,
                       "overtime bonus boundary failed");
        goto cleanup;
    }
    context.period_kind = TECMO_GAMEPLAY_PENALTY_PERIOD_REGULATION;
    context.team_fouls = 0U;
    context.current_route = 8U;
    if (!self_test_expect(
            &assets, &context, TECMO_GAMEPLAY_FOUL_CLASS_PUSHING,
            1U, 1U, 1U, false, false, false)) {
        (void)snprintf(message, message_size,
                       "one-attempt current-route boundary failed");
        goto cleanup;
    }
    context.saved_route = 5U;
    if (!self_test_expect(
            &assets, &context, TECMO_GAMEPLAY_FOUL_CLASS_PUSHING,
            1U, 1U, 1U, false, false, false)) {
        (void)snprintf(message, message_size,
                       "one-attempt selector precedence failed");
        goto cleanup;
    }

    if (!tecmo_gameplay_penalties_get_violation(
            &assets, 3U, &violation, &presentation) ||
        violation != TECMO_GAMEPLAY_VIOLATION_FIVE_SECONDS ||
        presentation.lead_in_frames != 4U ||
        presentation.maximum_wait_frames != 120U ||
        presentation.release_button_mask != 0x80U ||
        presentation.controller_count != 2U ||
        presentation.entry_sfx_id != 6U ||
        !tecmo_gameplay_penalties_get_presentation(
            &assets, TECMO_GAMEPLAY_PENALTY_PRESENTATION_FOUL,
            0U, &presentation) ||
        presentation.lead_in_frames != 4U ||
        presentation.maximum_wait_frames != 160U ||
        presentation.entry_sfx_id != 0U ||
        tecmo_gameplay_penalties_get_violation(
            &assets, 0U, &violation, &presentation) ||
        tecmo_gameplay_penalties_get_violation(
            &assets, 8U, &violation, &presentation) ||
        tecmo_gameplay_penalties_get_presentation(
            &assets, TECMO_GAMEPLAY_PENALTY_PRESENTATION_FOUL,
            1U, &presentation)) {
        (void)snprintf(message, message_size,
                       "presentation/selector contract failed");
        goto cleanup;
    }

    context.foul_actor = TECMO_GAMEPLAY_PENALTY_ACTOR_SLOT_COUNT;
    if (tecmo_gameplay_penalties_classify(&assets, &context, &result)) {
        (void)snprintf(message, message_size,
                       "invalid actor accepted");
        goto cleanup;
    }
    context.foul_actor = 1U;
    context.individual_fouls = 7U;
    if (tecmo_gameplay_penalties_classify(&assets, &context, &result)) {
        (void)snprintf(message, message_size,
                       "invalid individual counter accepted");
        goto cleanup;
    }
    context.individual_fouls = 0U;
    context.team_fouls = 6U;
    if (tecmo_gameplay_penalties_classify(&assets, &context, &result)) {
        (void)snprintf(message, message_size,
                       "invalid team counter accepted");
        goto cleanup;
    }
    context.team_fouls = 0U;
    context.period_kind = TECMO_GAMEPLAY_PENALTY_PERIOD_KIND_COUNT;
    if (tecmo_gameplay_penalties_classify(&assets, &context, &result) ||
        tecmo_gameplay_penalties_classify(&assets, NULL, &result) ||
        tecmo_gameplay_penalties_classify(&assets, &context, NULL)) {
        (void)snprintf(message, message_size,
                       "invalid context accepted");
        goto cleanup;
    }

    (void)snprintf(
        message, message_size,
        "TPNL-1 penalty assets passed: sources=7 classes=3 violations=7 caps=6/5 bonus=5/4 selectors=2FT:8,1FT:2");
    passed = true;

cleanup:
    tecmo_gameplay_penalties_destroy(&assets);
    tecmo_gameplay_penalties_destroy(&assets);
    return passed;
}
