#include "tecmo_cli_internal.h"

#include "asset_pack/tecmo_asset_pack_gameplay_rebound_audit.h"
#include "tecmo_gameplay_rebound_audit.h"
#include "tecmo_player_stats.h"
#include "tecmo_team_data.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool expect_reason(const TecmoGameplayReboundAuditAssets *assets,
                          const TecmoGameplayReboundAuditInput *input,
                          TecmoGameplayReboundAuditReason expected)
{
    TecmoGameplayReboundAuditDecision decision;
    return tecmo_gameplay_rebound_audit_resolve(assets, input, &decision) &&
           decision.reason == expected && !decision.ledger_write_enabled;
}

static bool validate_resolver_fail_closed(
    const TecmoGameplayReboundAuditAssets *assets)
{
    TecmoGameplayReboundAuditInput input;
    TecmoGameplayReboundAuditDecision decision;
    TecmoPlayerStatsGameLedger ledger;
    TecmoPlayerStatsGameLedger before;

    memset(&input, 0, sizeof(input));
    if (!expect_reason(assets, &input,
                       TECMO_GAMEPLAY_REBOUND_AUDIT_DEFER_RAW_BA_UNAVAILABLE)) {
        return false;
    }
    input.raw_ba_available = true;
    input.raw_ba = 1U;
    if (!expect_reason(
            assets, &input,
            TECMO_GAMEPLAY_REBOUND_AUDIT_DEFER_RAW_BA_LOW_BITS_NONZERO)) {
        return false;
    }
    input.raw_ba = 0U;
    if (!expect_reason(
            assets, &input,
            TECMO_GAMEPLAY_REBOUND_AUDIT_DEFER_RAW_0588_UNAVAILABLE)) {
        return false;
    }
    input.raw_0588_available = true;
    input.raw_0588 = 0U;
    if (!expect_reason(
            assets, &input,
            TECMO_GAMEPLAY_REBOUND_AUDIT_DEFER_RAW_0588_BIT80_CLEAR)) {
        return false;
    }
    input.raw_0588 = 0x80U;
    if (!expect_reason(
            assets, &input,
            TECMO_GAMEPLAY_REBOUND_AUDIT_DEFER_IDENTITY_NOT_FRESH)) {
        return false;
    }
    input.be_bf_identity_fresh = true;
    if (!expect_reason(
            assets, &input,
            TECMO_GAMEPLAY_REBOUND_AUDIT_DEFER_CLAIMANT_INVALID)) {
        return false;
    }
    input.claimant_settlement_valid = true;
    input.claimant_actor = 2U;
    input.claimant_team = 1U;
    input.claimant_roster_index = 4U;
    if (!expect_reason(
            assets, &input,
            TECMO_GAMEPLAY_REBOUND_AUDIT_DEFER_CLAIMANT_RELATION_UNAVAILABLE)) {
        return false;
    }
    input.claimant_relation = TECMO_GAMEPLAY_REBOUND_AUDIT_RELATION_OTHER_TEAM;
    if (!expect_reason(
            assets, &input,
            TECMO_GAMEPLAY_REBOUND_AUDIT_DEFER_CLAIMANT_SERIAL_ZERO)) {
        return false;
    }
    input.claimant_event_serial = 7U;
    input.last_emitted_event_serial = 7U;
    if (!expect_reason(
            assets, &input,
            TECMO_GAMEPLAY_REBOUND_AUDIT_DEFER_CLAIMANT_SERIAL_DUPLICATE)) {
        return false;
    }
    input.last_emitted_event_serial = 6U;
    if (!expect_reason(
            assets, &input,
            TECMO_GAMEPLAY_REBOUND_AUDIT_DEFER_NOT_TERMINAL_MISS)) {
        return false;
    }
    input.terminal_miss_observed = true;
    if (!expect_reason(
            assets, &input,
            TECMO_GAMEPLAY_REBOUND_AUDIT_DEFER_NOT_DIRECT_CAROM_ROUTE)) {
        return false;
    }
    input.direct_carom_route_observed = true;
    tecmo_player_stats_game_ledger_initialize(&ledger);
    before = ledger;
    if (!tecmo_gameplay_rebound_audit_resolve(assets, &input, &decision) ||
        !decision.source_gate_eligible || decision.ledger_write_enabled ||
        decision.reason !=
            TECMO_GAMEPLAY_REBOUND_AUDIT_SOURCE_GATE_NON_EMITTING ||
        memcmp(&ledger, &before, sizeof(ledger)) != 0 ||
        (ledger.coverage & tecmo_player_stats_counter_bit(
            TECMO_PLAYER_STATS_COUNTER_REBOUNDS)) != 0U ||
        tecmo_player_stats_game_counter_add(
            &ledger, input.claimant_team, input.claimant_roster_index,
            TECMO_PLAYER_STATS_COUNTER_REBOUNDS, 1U) ||
        memcmp(&ledger, &before, sizeof(ledger)) != 0 ||
        !tecmo_player_stats_game_ledger_valid(&ledger)) {
        return false;
    }
    return true;
}

static uint32_t rebound_audit_raw_offset(size_t source_index)
{
    uint32_t offset = TECMO_ASSET_PACK_GAMEPLAY_REBOUND_AUDIT_RAW_OFFSET;
    size_t index;
    for (index = 0U; index < source_index; ++index) {
        offset += tecmo_gameplay_rebound_audit_expected_sources[index]
                      .byte_count;
    }
    return offset;
}

static bool parse_rejects_payload_mutation(
    const TecmoGameplayReboundAuditAssets *assets,
    uint8_t *payload,
    size_t payload_offset)
{
    TecmoGameplayReboundAuditAssets candidate;
    bool rejected;
    payload[payload_offset] ^= 0x01U;
    tecmo_gameplay_rebound_audit_init(&candidate);
    rejected = !tecmo_gameplay_rebound_audit_parse(
        &candidate, payload, assets->storage_size) && !candidate.available &&
        candidate.storage == NULL &&
        strcmp(candidate.status,
               "TGRB-1 canonical payload fingerprint rejected") == 0;
    tecmo_gameplay_rebound_audit_destroy(&candidate);
    return rejected;
}

static bool validate_serialized_mutations(
    const TecmoGameplayReboundAuditAssets *assets)
{
    uint8_t *payload;
    size_t index;
    if (assets == NULL || assets->storage == NULL ||
        assets->storage_size != TECMO_ASSET_PACK_GAMEPLAY_REBOUND_AUDIT_SIZE) {
        return false;
    }
    payload = (uint8_t *)malloc(assets->storage_size);
    if (payload == NULL) return false;
    /* One table-driven descriptor mutation and one raw-byte mutation for
       every imported span. The canonical whole-payload FNV must reject each
       before any partial source object can be published. */
    for (index = 0U; index < TECMO_GAMEPLAY_REBOUND_AUDIT_SOURCE_COUNT;
         ++index) {
        memcpy(payload, assets->storage, assets->storage_size);
        if (!parse_rejects_payload_mutation(
                assets, payload,
                TECMO_ASSET_PACK_GAMEPLAY_REBOUND_AUDIT_SOURCES_OFFSET +
                    index *
                        TECMO_ASSET_PACK_GAMEPLAY_REBOUND_AUDIT_SOURCE_STRIDE +
                    4U)) {
            free(payload);
            return false;
        }
        memcpy(payload, assets->storage, assets->storage_size);
        if (!parse_rejects_payload_mutation(
                assets, payload, rebound_audit_raw_offset(index))) {
            free(payload);
            return false;
        }
    }
    free(payload);
    return true;
}

int tecmo_cli_run_gameplay_rebound_audit_command(
    const TecmoCliContext *context)
{
    const char *pack_path;
    const char *rom_path;
    TecmoGameplayReboundAuditAssets assets;
    const TecmoGameplayReboundAuditSourceSpan *producer;
    const TecmoGameplayReboundAuditSourceSpan *consumer;
    const TecmoGameplayReboundAuditSourceSpan *counter_entry;
    const TecmoGameplayReboundAuditSourceSpan *counter_plane;
    char message[192];
    bool ok = false;

    if (context == NULL ||
        strcmp(context->command, "--gameplay-rebound-audit-test") != 0) {
        return TECMO_CLI_NOT_HANDLED;
    }
    pack_path = context->index < context->argc
        ? context->argv[context->index] : NULL;
    rom_path = context->index + 1 < context->argc
        ? context->argv[context->index + 1] : NULL;
    tecmo_gameplay_rebound_audit_init(&assets);
    if (pack_path == NULL ||
        !tecmo_gameplay_rebound_audit_load(&assets, pack_path) ||
        !tecmo_gameplay_rebound_audit_load(&assets, pack_path)) {
        printf("Rebound audit test failed: %s\n",
               pack_path != NULL ? assets.status : "PACK path required");
        goto cleanup;
    }
    producer = tecmo_gameplay_rebound_audit_find_source(
        &assets,
        TECMO_GAMEPLAY_REBOUND_AUDIT_SOURCE_DIRECT_CAROM_PRODUCER);
    consumer = tecmo_gameplay_rebound_audit_find_source(
        &assets, TECMO_GAMEPLAY_REBOUND_AUDIT_SOURCE_CLAIMANT_CONSUMER);
    counter_entry = tecmo_gameplay_rebound_audit_find_source(
        &assets, TECMO_GAMEPLAY_REBOUND_AUDIT_SOURCE_COUNTER_ENTRY);
    counter_plane = tecmo_gameplay_rebound_audit_find_source(
        &assets, TECMO_GAMEPLAY_REBOUND_AUDIT_SOURCE_COUNTER_PLANE);
    if (producer == NULL || producer->bank != 5U || producer->fixed_bank ||
        producer->cpu_start != 0xA8E9U || producer->cpu_end != 0xA9D9U ||
        producer->fingerprint_fnv1a32 != 0x8A09C556U ||
        consumer == NULL || consumer->cpu_start != 0xBA56U ||
        consumer->cpu_end != 0xBAC0U ||
        consumer->fingerprint_fnv1a64 != 0x14B4446D08966498ULL ||
        counter_entry == NULL || !counter_entry->fixed_bank ||
        counter_entry->cpu_start != 0xC042U ||
        counter_entry->byte_count != 3U ||
        counter_plane == NULL || !counter_plane->fixed_bank ||
        counter_plane->cpu_start != 0xCC00U ||
        counter_plane->cpu_end != 0xCC2FU ||
        counter_plane->fingerprint_fnv1a32 != 0x93ACD23FU ||
        !validate_serialized_mutations(&assets) ||
        !validate_resolver_fail_closed(&assets)) {
        printf("Rebound audit test failed: source/resolver/negative contract\n");
        goto cleanup;
    }
    if (rom_path != NULL &&
        tecmo_asset_pack_gameplay_rebound_audit_source_test(
            rom_path, message, sizeof(message)) != 0) {
        printf("Rebound audit source test failed: %s\n", message);
        goto cleanup;
    }
    if (!tecmo_team_data_self_test(message, sizeof(message))) {
        printf("Rebound audit test failed: TEAM DATA presentation: %s\n",
               message);
        goto cleanup;
    }
    ok = true;

cleanup:
    tecmo_gameplay_rebound_audit_destroy(&assets);
    if (!ok) return 1;
    printf("TGRB-1 rebound audit passed: strict A977/B6E5/BA56/C042/CC00 provenance; raw BA+0588+BE/BF claimant gates required; REB coverage bit8 remains off and TEAM DATA/leader output remains unsupported (---).\n");
    return 0;
}
