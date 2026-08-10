#define _CRT_SECURE_NO_WARNINGS

#include "tecmo_gameplay_rebound_audit.h"

#include "asset_pack/tecmo_asset_pack_gameplay_rebound_audit.h"
#include "tecmo_asset_pack.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TECMO_GAMEPLAY_REBOUND_AUDIT_LIFECYCLE_TAG 0x42524754U

static uint16_t read_u16(const uint8_t *bytes)
{
    return (uint16_t)((uint16_t)bytes[0U] |
                      ((uint16_t)bytes[1U] << 8U));
}

static uint32_t read_u32(const uint8_t *bytes)
{
    return (uint32_t)bytes[0U] | ((uint32_t)bytes[1U] << 8U) |
           ((uint32_t)bytes[2U] << 16U) | ((uint32_t)bytes[3U] << 24U);
}

static uint64_t read_u64(const uint8_t *bytes)
{
    uint64_t value = 0U;
    size_t index;
    for (index = 0U; index < 8U; ++index) {
        value |= (uint64_t)bytes[index] << (index * 8U);
    }
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

static bool bytes_are_zero(const uint8_t *bytes, size_t count)
{
    size_t index;
    for (index = 0U; index < count; ++index) {
        if (bytes[index] != 0U) return false;
    }
    return true;
}

static uint32_t source_raw_offset(size_t index)
{
    uint32_t offset = TECMO_ASSET_PACK_GAMEPLAY_REBOUND_AUDIT_RAW_OFFSET;
    size_t other;
    for (other = 0U; other < index; ++other) {
        offset += tecmo_gameplay_rebound_audit_expected_sources[other]
                      .byte_count;
    }
    return offset;
}

static bool contains_sequence(const uint8_t *bytes,
                              size_t count,
                              const uint8_t *needle,
                              size_t needle_count)
{
    size_t index;
    if (bytes == NULL || needle == NULL || needle_count == 0U ||
        needle_count > count) {
        return false;
    }
    for (index = 0U; index <= count - needle_count; ++index) {
        if (memcmp(bytes + index, needle, needle_count) == 0) return true;
    }
    return false;
}

static bool validate_header(const uint8_t *payload, size_t payload_size)
{
    return payload != NULL &&
           payload_size == TECMO_ASSET_PACK_GAMEPLAY_REBOUND_AUDIT_SIZE &&
           memcmp(payload, "TGRB", 4U) == 0 &&
           read_u16(payload + 4U) ==
               TECMO_ASSET_PACK_GAMEPLAY_REBOUND_AUDIT_VERSION &&
           read_u16(payload + 6U) ==
               TECMO_ASSET_PACK_GAMEPLAY_REBOUND_AUDIT_HEADER_SIZE &&
           read_u32(payload + 8U) ==
               TECMO_ASSET_PACK_GAMEPLAY_REBOUND_AUDIT_SIZE &&
           read_u16(payload + 12U) ==
               TECMO_GAMEPLAY_REBOUND_AUDIT_SOURCE_COUNT &&
           read_u16(payload + 14U) ==
               TECMO_ASSET_PACK_GAMEPLAY_REBOUND_AUDIT_SOURCE_STRIDE &&
           read_u32(payload + 16U) ==
               TECMO_ASSET_PACK_GAMEPLAY_REBOUND_AUDIT_SOURCES_OFFSET &&
           read_u32(payload + 20U) ==
               TECMO_ASSET_PACK_GAMEPLAY_REBOUND_AUDIT_RAW_OFFSET &&
           read_u32(payload + 24U) ==
               TECMO_ASSET_PACK_GAMEPLAY_REBOUND_AUDIT_RAW_SIZE &&
           payload[28U] ==
               TECMO_ASSET_PACK_GAMEPLAY_REBOUND_AUDIT_DIRECT_CAROM_MASK &&
           payload[29U] ==
               TECMO_ASSET_PACK_GAMEPLAY_REBOUND_AUDIT_CAROM_READY_MASK &&
           payload[30U] == 0xBEU && payload[31U] == 0xBFU &&
           read_u16(payload + 32U) == 0xA977U &&
           read_u16(payload + 34U) == 0xBA8CU &&
           read_u16(payload + 36U) == 0xC042U &&
           read_u16(payload + 38U) == 0xCC00U &&
           payload[40U] ==
               TECMO_ASSET_PACK_GAMEPLAY_REBOUND_AUDIT_COUNTER_INDEX &&
           payload[41U] ==
               TECMO_ASSET_PACK_GAMEPLAY_REBOUND_AUDIT_COUNTER_OFFSET &&
           read_u16(payload + 42U) ==
               TECMO_ASSET_PACK_GAMEPLAY_REBOUND_AUDIT_COUNTER_BASE &&
           bytes_are_zero(
               payload + 44U,
               TECMO_ASSET_PACK_GAMEPLAY_REBOUND_AUDIT_HEADER_SIZE - 44U);
}

static bool validate_sources(const uint8_t *payload)
{
    size_t index;
    for (index = 0U; index < TECMO_GAMEPLAY_REBOUND_AUDIT_SOURCE_COUNT;
         ++index) {
        const TecmoGameplayReboundAuditExpectedSource *expected =
            &tecmo_gameplay_rebound_audit_expected_sources[index];
        const uint8_t *record = payload +
            TECMO_ASSET_PACK_GAMEPLAY_REBOUND_AUDIT_SOURCES_OFFSET +
            index * TECMO_ASSET_PACK_GAMEPLAY_REBOUND_AUDIT_SOURCE_STRIDE;
        const uint8_t *raw = payload + source_raw_offset(index);
        uint32_t end = (uint32_t)expected->cpu_start +
                       expected->byte_count - 1U;
        if (read_u16(record) != (uint16_t)expected->kind ||
            record[2U] != expected->bank ||
            record[3U] != expected->fixed_bank ||
            read_u16(record + 4U) != expected->cpu_start ||
            read_u16(record + 6U) != (uint16_t)end ||
            read_u32(record + 8U) != expected->byte_count ||
            read_u32(record + 12U) != source_raw_offset(index) ||
            read_u32(record + 16U) != expected->fingerprint_fnv1a32 ||
            read_u64(record + 20U) != expected->fingerprint_fnv1a64 ||
            read_u16(record + 28U) != (uint16_t)index ||
            !bytes_are_zero(record + 30U, 10U) ||
            fnv1a32(raw, expected->byte_count) !=
                expected->fingerprint_fnv1a32 ||
            fnv1a64(raw, expected->byte_count) !=
                expected->fingerprint_fnv1a64) {
            return false;
        }
    }
    return true;
}

static bool validate_semantics(const uint8_t *payload)
{
    static const uint8_t producer_gate[] = {
        0x20U,0xDAU,0xA9U,0xA5U,0xBAU,0x29U,0x03U,0xD0U,
        0x45U,0xADU,0x88U,0x05U,0x09U,0x80U,0x8DU,0x88U,0x05U
    };
    static const uint8_t consumer_tail[] = {
        0x20U,0x7CU,0xB8U,0x20U,0xB6U,0x96U,
        0xADU,0x88U,0x05U,0x29U,0xBFU,0x8DU,0x88U,0x05U,
        0x29U,0x80U,0xF0U,0x14U,0xADU,0x88U,0x05U,
        0x29U,0x76U,0x09U,0x02U,0x8DU,0x88U,0x05U,
        0xA9U,0x10U,0x20U,0x11U,0xC7U,0xA2U,0x08U,0x4CU,0x42U,0xC0U
    };
    static const uint8_t counter_offsets[] = {
        0x00U,0x18U,0x30U,0x48U,0x60U,0x78U,0x90U,0xA8U,0xC0U
    };
    const uint8_t *producer = payload + source_raw_offset(0U);
    const uint8_t *gates = payload + source_raw_offset(1U);
    const uint8_t *consumer = payload + source_raw_offset(2U);
    const uint8_t *entry = payload + source_raw_offset(3U);
    const uint8_t *plane = payload + source_raw_offset(4U);
    return contains_sequence(producer, 241U, producer_gate,
                             sizeof(producer_gate)) &&
           gates[0U] == 0xA5U && gates[1U] == 0xBAU &&
           gates[2U] == 0x29U && gates[3U] == 0x03U &&
           contains_sequence(gates, 89U,
                             (const uint8_t[]){0xADU,0x88U,0x05U,0x29U,
                                               0x80U,0xF0U}, 6U) &&
           contains_sequence(consumer, 107U, consumer_tail,
                             sizeof(consumer_tail)) &&
           entry[0U] == 0x4CU && entry[1U] == 0x12U &&
           entry[2U] == 0xCCU &&
           plane[0U] == 0x84U && plane[1U] == 0x9EU &&
           contains_sequence(plane, 48U,
                             (const uint8_t[]){0xFEU,0x58U,0x7BU}, 3U) &&
           contains_sequence(plane, 48U, counter_offsets,
                             sizeof(counter_offsets));
}

static bool reject(TecmoGameplayReboundAuditAssets *candidate,
                   TecmoGameplayReboundAuditAssets *destination,
                   const char *status)
{
    bool publish = destination != NULL && !destination->available &&
                   destination->storage == NULL;
    free(candidate->storage);
    candidate->storage = NULL;
    candidate->storage_size = 0U;
    memset(candidate->sources, 0, sizeof(candidate->sources));
    candidate->available = false;
    (void)snprintf(candidate->status, sizeof(candidate->status), "%s",
                   status != NULL ? status : "TGRB-1 rejected");
    if (publish) {
        (void)snprintf(destination->status, sizeof(destination->status),
                       "%s", candidate->status);
    }
    return false;
}

void tecmo_gameplay_rebound_audit_init(
    TecmoGameplayReboundAuditAssets *assets)
{
    if (assets == NULL) return;
    memset(assets, 0, sizeof(*assets));
    assets->lifecycle_tag = TECMO_GAMEPLAY_REBOUND_AUDIT_LIFECYCLE_TAG;
}

void tecmo_gameplay_rebound_audit_destroy(
    TecmoGameplayReboundAuditAssets *assets)
{
    if (assets == NULL || assets->lifecycle_tag !=
                              TECMO_GAMEPLAY_REBOUND_AUDIT_LIFECYCLE_TAG) {
        return;
    }
    free(assets->storage);
    tecmo_gameplay_rebound_audit_init(assets);
}

bool tecmo_gameplay_rebound_audit_parse(
    TecmoGameplayReboundAuditAssets *assets,
    const uint8_t *payload,
    size_t payload_size)
{
    TecmoGameplayReboundAuditAssets candidate;
    TecmoGameplayReboundAuditAssets previous;
    uint8_t *storage;
    size_t index;
    if (assets == NULL || assets->lifecycle_tag !=
                              TECMO_GAMEPLAY_REBOUND_AUDIT_LIFECYCLE_TAG) {
        return false;
    }
    tecmo_gameplay_rebound_audit_init(&candidate);
    if (!validate_header(payload, payload_size)) {
        return reject(&candidate, assets,
                      "TGRB-1 header/size/reserved contract rejected");
    }
    if (fnv1a32(payload, payload_size) !=
            TECMO_ASSET_PACK_GAMEPLAY_REBOUND_AUDIT_FNV1A32 ||
        fnv1a64(payload, payload_size) !=
            TECMO_ASSET_PACK_GAMEPLAY_REBOUND_AUDIT_FNV1A64) {
        return reject(&candidate, assets,
                      "TGRB-1 canonical payload fingerprint rejected");
    }
    if (!validate_sources(payload) || !validate_semantics(payload)) {
        return reject(&candidate, assets,
                      "TGRB-1 source/semantic contract rejected");
    }
    storage = (uint8_t *)malloc(payload_size);
    if (storage == NULL) {
        return reject(&candidate, assets, "TGRB-1 allocation failed");
    }
    memcpy(storage, payload, payload_size);
    candidate.storage = storage;
    candidate.storage_size = payload_size;
    for (index = 0U; index < TECMO_GAMEPLAY_REBOUND_AUDIT_SOURCE_COUNT;
         ++index) {
        const TecmoGameplayReboundAuditExpectedSource *expected =
            &tecmo_gameplay_rebound_audit_expected_sources[index];
        TecmoGameplayReboundAuditSourceSpan *source =
            &candidate.sources[index];
        source->kind = expected->kind;
        source->bank = expected->bank;
        source->fixed_bank = expected->fixed_bank != 0U;
        source->cpu_start = expected->cpu_start;
        source->cpu_end = (uint16_t)((uint32_t)expected->cpu_start +
                                     expected->byte_count - 1U);
        source->byte_count = expected->byte_count;
        source->fingerprint_fnv1a32 = expected->fingerprint_fnv1a32;
        source->fingerprint_fnv1a64 = expected->fingerprint_fnv1a64;
    }
    candidate.available = true;
    (void)snprintf(candidate.status, sizeof(candidate.status),
                   "TGRB-1 rebound eligibility audit assetpack");
    previous = *assets;
    *assets = candidate;
    free(previous.storage);
    return true;
}

bool tecmo_gameplay_rebound_audit_load(
    TecmoGameplayReboundAuditAssets *assets,
    const char *asset_pack_path)
{
    uint8_t *payload = NULL;
    uint64_t payload_size = 0U;
    bool loaded;
    if (assets == NULL || assets->lifecycle_tag !=
                              TECMO_GAMEPLAY_REBOUND_AUDIT_LIFECYCLE_TAG) {
        return false;
    }
    if (asset_pack_path == NULL || tecmo_asset_pack_read_entry_exact(
            asset_pack_path, TECMO_ASSET_PACK_GAMEPLAY_REBOUND_AUDIT_ID,
            TECMO_ASSET_PACK_GAMEPLAY_REBOUND_AUDIT_SIZE,
            &payload, &payload_size) != 0) {
        if (!assets->available) {
            (void)snprintf(assets->status, sizeof(assets->status),
                           "TGRB-1 gameplay/rebound-audit entry missing or wrong-sized");
        }
        return false;
    }
    loaded = tecmo_gameplay_rebound_audit_parse(
        assets, payload, (size_t)payload_size);
    tecmo_asset_pack_free(payload);
    return loaded;
}

const TecmoGameplayReboundAuditSourceSpan *
tecmo_gameplay_rebound_audit_find_source(
    const TecmoGameplayReboundAuditAssets *assets,
    TecmoGameplayReboundAuditSourceKind kind)
{
    size_t index;
    if (assets == NULL || !assets->available) return NULL;
    for (index = 0U; index < TECMO_GAMEPLAY_REBOUND_AUDIT_SOURCE_COUNT;
         ++index) {
        if (assets->sources[index].kind == kind) return &assets->sources[index];
    }
    return NULL;
}

static void deferred(TecmoGameplayReboundAuditDecision *decision,
                     TecmoGameplayReboundAuditReason reason)
{
    memset(decision, 0, sizeof(*decision));
    decision->reason = reason;
    /* This is invariant by design: source-gate success is not permission to
       write REB until raw production data is retained at $C042. */
    decision->ledger_write_enabled = false;
}

bool tecmo_gameplay_rebound_audit_resolve(
    const TecmoGameplayReboundAuditAssets *assets,
    const TecmoGameplayReboundAuditInput *input,
    TecmoGameplayReboundAuditDecision *decision)
{
    if (decision == NULL) return false;
    if (assets == NULL || !assets->available) {
        deferred(decision, TECMO_GAMEPLAY_REBOUND_AUDIT_DEFER_ASSETS_UNAVAILABLE);
        return true;
    }
    if (input == NULL) {
        deferred(decision, TECMO_GAMEPLAY_REBOUND_AUDIT_DEFER_CLAIMANT_INVALID);
        return true;
    }
    /* Raw producer gates lead deliberately. A native miss/claimant bridge is
       not a substitute for the original $BA/$0588 state. */
    if (!input->raw_ba_available) {
        deferred(decision, TECMO_GAMEPLAY_REBOUND_AUDIT_DEFER_RAW_BA_UNAVAILABLE);
        return true;
    }
    if ((input->raw_ba & TECMO_ASSET_PACK_GAMEPLAY_REBOUND_AUDIT_DIRECT_CAROM_MASK) != 0U) {
        deferred(decision,
                 TECMO_GAMEPLAY_REBOUND_AUDIT_DEFER_RAW_BA_LOW_BITS_NONZERO);
        return true;
    }
    if (!input->raw_0588_available) {
        deferred(decision, TECMO_GAMEPLAY_REBOUND_AUDIT_DEFER_RAW_0588_UNAVAILABLE);
        return true;
    }
    if ((input->raw_0588 & TECMO_ASSET_PACK_GAMEPLAY_REBOUND_AUDIT_CAROM_READY_MASK) == 0U) {
        deferred(decision,
                 TECMO_GAMEPLAY_REBOUND_AUDIT_DEFER_RAW_0588_BIT80_CLEAR);
        return true;
    }
    if (!input->be_bf_identity_fresh) {
        deferred(decision, TECMO_GAMEPLAY_REBOUND_AUDIT_DEFER_IDENTITY_NOT_FRESH);
        return true;
    }
    if (!input->claimant_settlement_valid || input->claimant_actor >= 10U ||
        input->claimant_team >= 2U || input->claimant_roster_index >= 12U) {
        deferred(decision, TECMO_GAMEPLAY_REBOUND_AUDIT_DEFER_CLAIMANT_INVALID);
        return true;
    }
    if (input->claimant_relation != TECMO_GAMEPLAY_REBOUND_AUDIT_RELATION_SAME_TEAM &&
        input->claimant_relation != TECMO_GAMEPLAY_REBOUND_AUDIT_RELATION_OTHER_TEAM) {
        deferred(decision,
                 TECMO_GAMEPLAY_REBOUND_AUDIT_DEFER_CLAIMANT_RELATION_UNAVAILABLE);
        return true;
    }
    if (input->claimant_event_serial == 0U) {
        deferred(decision,
                 TECMO_GAMEPLAY_REBOUND_AUDIT_DEFER_CLAIMANT_SERIAL_ZERO);
        return true;
    }
    if (input->claimant_event_serial == input->last_emitted_event_serial) {
        deferred(decision,
                 TECMO_GAMEPLAY_REBOUND_AUDIT_DEFER_CLAIMANT_SERIAL_DUPLICATE);
        return true;
    }
    if (!input->terminal_miss_observed) {
        deferred(decision, TECMO_GAMEPLAY_REBOUND_AUDIT_DEFER_NOT_TERMINAL_MISS);
        return true;
    }
    if (!input->direct_carom_route_observed) {
        deferred(decision,
                 TECMO_GAMEPLAY_REBOUND_AUDIT_DEFER_NOT_DIRECT_CAROM_ROUTE);
        return true;
    }
    deferred(decision,
             TECMO_GAMEPLAY_REBOUND_AUDIT_SOURCE_GATE_NON_EMITTING);
    decision->source_gate_eligible = true;
    decision->claimant_actor = input->claimant_actor;
    decision->claimant_team = input->claimant_team;
    decision->claimant_roster_index = input->claimant_roster_index;
    decision->claimant_event_serial = input->claimant_event_serial;
    return true;
}

const char *tecmo_gameplay_rebound_audit_reason_name(
    TecmoGameplayReboundAuditReason reason)
{
    switch (reason) {
    case TECMO_GAMEPLAY_REBOUND_AUDIT_DEFER_ASSETS_UNAVAILABLE:
        return "assets-unavailable";
    case TECMO_GAMEPLAY_REBOUND_AUDIT_DEFER_RAW_BA_UNAVAILABLE:
        return "raw-ba-unavailable";
    case TECMO_GAMEPLAY_REBOUND_AUDIT_DEFER_RAW_BA_LOW_BITS_NONZERO:
        return "raw-ba-low-bits-nonzero";
    case TECMO_GAMEPLAY_REBOUND_AUDIT_DEFER_RAW_0588_UNAVAILABLE:
        return "raw-0588-unavailable";
    case TECMO_GAMEPLAY_REBOUND_AUDIT_DEFER_RAW_0588_BIT80_CLEAR:
        return "raw-0588-bit80-clear";
    case TECMO_GAMEPLAY_REBOUND_AUDIT_DEFER_IDENTITY_NOT_FRESH:
        return "be-bf-identity-not-fresh";
    case TECMO_GAMEPLAY_REBOUND_AUDIT_DEFER_CLAIMANT_INVALID:
        return "claimant-invalid";
    case TECMO_GAMEPLAY_REBOUND_AUDIT_DEFER_CLAIMANT_RELATION_UNAVAILABLE:
        return "claimant-relation-unavailable";
    case TECMO_GAMEPLAY_REBOUND_AUDIT_DEFER_CLAIMANT_SERIAL_ZERO:
        return "claimant-serial-zero";
    case TECMO_GAMEPLAY_REBOUND_AUDIT_DEFER_CLAIMANT_SERIAL_DUPLICATE:
        return "claimant-serial-duplicate";
    case TECMO_GAMEPLAY_REBOUND_AUDIT_DEFER_NOT_TERMINAL_MISS:
        return "terminal-miss-not-observed";
    case TECMO_GAMEPLAY_REBOUND_AUDIT_DEFER_NOT_DIRECT_CAROM_ROUTE:
        return "direct-carom-route-not-observed";
    case TECMO_GAMEPLAY_REBOUND_AUDIT_SOURCE_GATE_NON_EMITTING:
        return "source-gate-eligible-non-emitting";
    default:
        return "unknown";
    }
}
