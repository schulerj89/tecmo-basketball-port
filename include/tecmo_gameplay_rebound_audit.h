#ifndef TECMO_GAMEPLAY_REBOUND_AUDIT_H
#define TECMO_GAMEPLAY_REBOUND_AUDIT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * TGRB-1 is deliberately an audit boundary, not a rebound-stat emitter.
 *
 * Bank05 $A977 only marks a direct carom when raw $BA&3 is zero and then
 * writes $0588 bit 7.  $BA56 consumes that bit after $B87C/$96B6 and calls
 * fixed $C042 with X=8.  The current native scene does not retain the raw
 * $BA/$0588 lifetime or a fresh $BE/$BF owner at that exact call boundary,
 * so this module cannot legally mutate the mutable player ledger yet.
 */
#define TECMO_GAMEPLAY_REBOUND_AUDIT_SOURCE_COUNT 5U

typedef enum TecmoGameplayReboundAuditSourceKind {
    TECMO_GAMEPLAY_REBOUND_AUDIT_SOURCE_DIRECT_CAROM_PRODUCER = 1,
    TECMO_GAMEPLAY_REBOUND_AUDIT_SOURCE_DIRECT_CAROM_GATES,
    TECMO_GAMEPLAY_REBOUND_AUDIT_SOURCE_CLAIMANT_CONSUMER,
    TECMO_GAMEPLAY_REBOUND_AUDIT_SOURCE_COUNTER_ENTRY,
    TECMO_GAMEPLAY_REBOUND_AUDIT_SOURCE_COUNTER_PLANE
} TecmoGameplayReboundAuditSourceKind;

typedef struct TecmoGameplayReboundAuditSourceSpan {
    TecmoGameplayReboundAuditSourceKind kind;
    uint8_t bank;
    bool fixed_bank;
    uint16_t cpu_start;
    uint16_t cpu_end;
    uint32_t byte_count;
    uint32_t fingerprint_fnv1a32;
    uint64_t fingerprint_fnv1a64;
} TecmoGameplayReboundAuditSourceSpan;

typedef struct TecmoGameplayReboundAuditAssets {
    uint32_t lifecycle_tag;
    bool available;
    char status[160];
    uint8_t *storage;
    size_t storage_size;
    TecmoGameplayReboundAuditSourceSpan
        sources[TECMO_GAMEPLAY_REBOUND_AUDIT_SOURCE_COUNT];
} TecmoGameplayReboundAuditAssets;

typedef enum TecmoGameplayReboundAuditRelation {
    TECMO_GAMEPLAY_REBOUND_AUDIT_RELATION_UNAVAILABLE = 0,
    TECMO_GAMEPLAY_REBOUND_AUDIT_RELATION_SAME_TEAM = 1,
    TECMO_GAMEPLAY_REBOUND_AUDIT_RELATION_OTHER_TEAM = 2
} TecmoGameplayReboundAuditRelation;

/* Every raw field must be contemporaneous with the Bank05 $BA8C consumer.
 * `be_bf_identity_fresh` is a caller-established proof that the stored
 * $BE/$BF identity still resolves to claimant_actor/team/roster at $C042;
 * the resolver never guesses that mapping. */
typedef struct TecmoGameplayReboundAuditInput {
    bool terminal_miss_observed;
    bool direct_carom_route_observed;
    bool raw_ba_available;
    uint8_t raw_ba;
    bool raw_0588_available;
    uint8_t raw_0588;
    bool be_bf_identity_fresh;
    bool claimant_settlement_valid;
    TecmoGameplayReboundAuditRelation claimant_relation;
    uint32_t claimant_event_serial;
    uint32_t last_emitted_event_serial;
    uint8_t claimant_actor;
    uint8_t claimant_team;
    uint8_t claimant_roster_index;
} TecmoGameplayReboundAuditInput;

typedef enum TecmoGameplayReboundAuditReason {
    TECMO_GAMEPLAY_REBOUND_AUDIT_DEFER_ASSETS_UNAVAILABLE = 0,
    TECMO_GAMEPLAY_REBOUND_AUDIT_DEFER_RAW_BA_UNAVAILABLE,
    TECMO_GAMEPLAY_REBOUND_AUDIT_DEFER_RAW_BA_LOW_BITS_NONZERO,
    TECMO_GAMEPLAY_REBOUND_AUDIT_DEFER_RAW_0588_UNAVAILABLE,
    TECMO_GAMEPLAY_REBOUND_AUDIT_DEFER_RAW_0588_BIT80_CLEAR,
    TECMO_GAMEPLAY_REBOUND_AUDIT_DEFER_IDENTITY_NOT_FRESH,
    TECMO_GAMEPLAY_REBOUND_AUDIT_DEFER_CLAIMANT_INVALID,
    TECMO_GAMEPLAY_REBOUND_AUDIT_DEFER_CLAIMANT_RELATION_UNAVAILABLE,
    TECMO_GAMEPLAY_REBOUND_AUDIT_DEFER_CLAIMANT_SERIAL_ZERO,
    TECMO_GAMEPLAY_REBOUND_AUDIT_DEFER_CLAIMANT_SERIAL_DUPLICATE,
    TECMO_GAMEPLAY_REBOUND_AUDIT_DEFER_NOT_TERMINAL_MISS,
    TECMO_GAMEPLAY_REBOUND_AUDIT_DEFER_NOT_DIRECT_CAROM_ROUTE,
    /* The source gate is complete, but still makes no ledger write. */
    TECMO_GAMEPLAY_REBOUND_AUDIT_SOURCE_GATE_NON_EMITTING
} TecmoGameplayReboundAuditReason;

typedef struct TecmoGameplayReboundAuditDecision {
    bool source_gate_eligible;
    bool ledger_write_enabled;
    TecmoGameplayReboundAuditReason reason;
    uint8_t claimant_actor;
    uint8_t claimant_team;
    uint8_t claimant_roster_index;
    uint32_t claimant_event_serial;
} TecmoGameplayReboundAuditDecision;

void tecmo_gameplay_rebound_audit_init(
    TecmoGameplayReboundAuditAssets *assets);
void tecmo_gameplay_rebound_audit_destroy(
    TecmoGameplayReboundAuditAssets *assets);

bool tecmo_gameplay_rebound_audit_parse(
    TecmoGameplayReboundAuditAssets *assets,
    const uint8_t *payload,
    size_t payload_size);
bool tecmo_gameplay_rebound_audit_load(
    TecmoGameplayReboundAuditAssets *assets,
    const char *asset_pack_path);

const TecmoGameplayReboundAuditSourceSpan *
tecmo_gameplay_rebound_audit_find_source(
    const TecmoGameplayReboundAuditAssets *assets,
    TecmoGameplayReboundAuditSourceKind kind);

/* Pure fail-closed source-gate evaluation. This function intentionally never
 * accepts a ledger pointer and therefore cannot increment REB. */
bool tecmo_gameplay_rebound_audit_resolve(
    const TecmoGameplayReboundAuditAssets *assets,
    const TecmoGameplayReboundAuditInput *input,
    TecmoGameplayReboundAuditDecision *decision);
const char *tecmo_gameplay_rebound_audit_reason_name(
    TecmoGameplayReboundAuditReason reason);

#endif
