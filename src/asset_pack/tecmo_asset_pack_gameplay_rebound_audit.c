#include "tecmo_asset_pack_gameplay_rebound_audit.h"

#include "tecmo_asset_pack_import_layout.h"
#include "tecmo_asset_pack_util.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define REBOUND_AUDIT_PRG_BANK_COUNT 8U
#define REBOUND_AUDIT_REV1_ROM_SIZE 393232U

_Static_assert(
    TECMO_ASSET_PACK_GAMEPLAY_REBOUND_AUDIT_SOURCES_OFFSET +
            TECMO_GAMEPLAY_REBOUND_AUDIT_SOURCE_COUNT *
                TECMO_ASSET_PACK_GAMEPLAY_REBOUND_AUDIT_SOURCE_STRIDE ==
        TECMO_ASSET_PACK_GAMEPLAY_REBOUND_AUDIT_RAW_OFFSET,
    "TGRB-1 source records must end at raw bytes");
_Static_assert(
    TECMO_ASSET_PACK_GAMEPLAY_REBOUND_AUDIT_RAW_OFFSET +
            TECMO_ASSET_PACK_GAMEPLAY_REBOUND_AUDIT_RAW_SIZE ==
        TECMO_ASSET_PACK_GAMEPLAY_REBOUND_AUDIT_SIZE,
    "TGRB-1 raw bytes must end at payload boundary");

static const uint8_t rebound_audit_rev1_ines_header[16] = {
    'N','E','S',0x1AU,0x08U,0x20U,0x42U,0x00U,
    0x00U,0x00U,0x00U,0x00U,0x00U,0x00U,0x00U,0x00U
};

const TecmoGameplayReboundAuditExpectedSource
    tecmo_gameplay_rebound_audit_expected_sources[
        TECMO_GAMEPLAY_REBOUND_AUDIT_SOURCE_COUNT] = {
        {TECMO_GAMEPLAY_REBOUND_AUDIT_SOURCE_DIRECT_CAROM_PRODUCER,
         5U,0U,0xA8E9U,241U,0x8A09C556U,0xA6C3631AD90C94D6ULL},
        {TECMO_GAMEPLAY_REBOUND_AUDIT_SOURCE_DIRECT_CAROM_GATES,
         5U,0U,0xB6E5U,89U,0x787E0E1EU,0x03CBE443242CD8DEULL},
        {TECMO_GAMEPLAY_REBOUND_AUDIT_SOURCE_CLAIMANT_CONSUMER,
         5U,0U,0xBA56U,107U,0x097B9C78U,0x14B4446D08966498ULL},
        {TECMO_GAMEPLAY_REBOUND_AUDIT_SOURCE_COUNTER_ENTRY,
         7U,1U,0xC042U,3U,0x5CC2CBD5U,0x26810E19B9D6CDD5ULL},
        {TECMO_GAMEPLAY_REBOUND_AUDIT_SOURCE_COUNTER_PLANE,
         7U,1U,0xCC00U,48U,0x93ACD23FU,0x32D90859496DF23FULL}
    };

static bool range_ok(uint64_t offset, uint64_t count, uint64_t total)
{
    return offset <= total && count <= total - offset;
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

static void store_u64(uint8_t *bytes, uint64_t value)
{
    size_t index;
    for (index = 0U; index < 8U; ++index) {
        bytes[index] = (uint8_t)(value >> (index * 8U));
    }
}

static uint64_t source_offset(
    uint64_t prg_offset,
    uint32_t prg_banks,
    const TecmoGameplayReboundAuditExpectedSource *source)
{
    uint16_t base;
    uint32_t bank;
    if (source == NULL || prg_banks == 0U) return 0U;
    if (source->fixed_bank != 0U) {
        base = 0xC000U;
        bank = prg_banks - 1U;
    } else {
        base = TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE;
        bank = source->bank;
    }
    if (bank >= prg_banks || source->cpu_start < base) return 0U;
    return prg_offset + (uint64_t)bank * TECMO_ASSET_PACK_PRG_BANK_BYTES +
           (uint64_t)(source->cpu_start - base);
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

static uint32_t source_raw_offset(size_t index)
{
    size_t other;
    uint32_t offset = TECMO_ASSET_PACK_GAMEPLAY_REBOUND_AUDIT_RAW_OFFSET;
    for (other = 0U; other < index; ++other) {
        offset += tecmo_gameplay_rebound_audit_expected_sources[other]
                      .byte_count;
    }
    return offset;
}

static bool validate_raw_semantics(const uint8_t *payload)
{
    static const uint8_t producer_gate[] = {
        0x20U,0xDAU,0xA9U,0xA5U,0xBAU,0x29U,0x03U,0xD0U,
        0x45U,0xADU,0x88U,0x05U,0x09U,0x80U,0x8DU,0x88U,0x05U
    };
    static const uint8_t b6e5_gate[] = {
        0xA5U,0xBAU,0x29U,0x03U,0xF0U
    };
    static const uint8_t b6e5_ready[] = {
        0xADU,0x88U,0x05U,0x29U,0x80U,0xF0U
    };
    static const uint8_t ba56_consumer[] = {
        0x20U,0x7CU,0xB8U,0x20U,0xB6U,0x96U,
        0xADU,0x88U,0x05U,0x29U,0xBFU,0x8DU,0x88U,0x05U,
        0x29U,0x80U,0xF0U,0x14U,0xADU,0x88U,0x05U,
        0x29U,0x76U,0x09U,0x02U,0x8DU,0x88U,0x05U,
        0xA9U,0x10U,0x20U,0x11U,0xC7U,0xA2U,0x08U,0x4CU,0x42U,0xC0U
    };
    static const uint8_t counter_entry[] = {0x4CU,0x12U,0xCCU};
    static const uint8_t counter_prefix[] = {
        0x84U,0x9EU,0xA4U,0xBFU,0xB9U,0xA9U,0x05U,
        0xA4U,0xBEU,0x18U,0x79U,0x10U,0xCCU
    };
    static const uint8_t counter_increment[] = {
        0x20U,0x1EU,0xCCU,0x20U,0x13U,0xC4U,0xFEU,0x58U,0x7BU
    };
    static const uint8_t counter_offsets[] = {
        0x00U,0x18U,0x30U,0x48U,0x60U,0x78U,0x90U,0xA8U,0xC0U
    };
    const uint8_t *producer;
    const uint8_t *gates;
    const uint8_t *consumer;
    const uint8_t *entry;
    const uint8_t *plane;

    if (payload == NULL) return false;
    producer = payload + source_raw_offset(0U);
    gates = payload + source_raw_offset(1U);
    consumer = payload + source_raw_offset(2U);
    entry = payload + source_raw_offset(3U);
    plane = payload + source_raw_offset(4U);
    return contains_sequence(
               producer,
               tecmo_gameplay_rebound_audit_expected_sources[0U].byte_count,
               producer_gate, sizeof(producer_gate)) &&
           memcmp(gates, b6e5_gate, sizeof(b6e5_gate)) == 0 &&
           contains_sequence(
               gates,
               tecmo_gameplay_rebound_audit_expected_sources[1U].byte_count,
               b6e5_ready, sizeof(b6e5_ready)) &&
           contains_sequence(
               consumer,
               tecmo_gameplay_rebound_audit_expected_sources[2U].byte_count,
               ba56_consumer, sizeof(ba56_consumer)) &&
           memcmp(entry, counter_entry, sizeof(counter_entry)) == 0 &&
           memcmp(plane, counter_prefix, sizeof(counter_prefix)) == 0 &&
           contains_sequence(
               plane,
               tecmo_gameplay_rebound_audit_expected_sources[4U].byte_count,
               counter_increment, sizeof(counter_increment)) &&
           contains_sequence(
               plane,
               tecmo_gameplay_rebound_audit_expected_sources[4U].byte_count,
               counter_offsets, sizeof(counter_offsets));
}

int tecmo_asset_pack_build_gameplay_rebound_audit(
    const uint8_t *rom,
    uint64_t rom_size,
    uint64_t prg_offset,
    uint32_t prg_banks,
    int enforce_revision_fingerprints,
    uint8_t *payload,
    size_t payload_size,
    TecmoGameplayReboundAuditProvenance *provenance,
    char *message,
    size_t message_size)
{
    size_t index;
    if (rom == NULL || payload == NULL || provenance == NULL ||
        payload_size != TECMO_ASSET_PACK_GAMEPLAY_REBOUND_AUDIT_SIZE ||
        prg_offset != sizeof(rebound_audit_rev1_ines_header) ||
        prg_banks != REBOUND_AUDIT_PRG_BANK_COUNT ||
        enforce_revision_fingerprints == 0 ||
        rom_size != REBOUND_AUDIT_REV1_ROM_SIZE ||
        memcmp(rom, rebound_audit_rev1_ines_header,
               sizeof(rebound_audit_rev1_ines_header)) != 0) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TGRB-1 import requires the exact Rev1 iNES layout.");
        return -1;
    }

    memset(payload, 0, payload_size);
    memset(provenance, 0, sizeof(*provenance));
    for (index = 0U; index < TECMO_GAMEPLAY_REBOUND_AUDIT_SOURCE_COUNT;
         ++index) {
        const TecmoGameplayReboundAuditExpectedSource *expected =
            &tecmo_gameplay_rebound_audit_expected_sources[index];
        uint64_t offset = source_offset(prg_offset, prg_banks, expected);
        uint32_t end = (uint32_t)expected->cpu_start +
                       expected->byte_count - 1U;
        uint32_t actual32;
        uint64_t actual64;
        uint32_t raw_offset = source_raw_offset(index);
        uint8_t *record = payload +
            TECMO_ASSET_PACK_GAMEPLAY_REBOUND_AUDIT_SOURCES_OFFSET +
            index * TECMO_ASSET_PACK_GAMEPLAY_REBOUND_AUDIT_SOURCE_STRIDE;
        if (!range_ok(offset, expected->byte_count, rom_size) ||
            raw_offset < TECMO_ASSET_PACK_GAMEPLAY_REBOUND_AUDIT_RAW_OFFSET ||
            raw_offset + expected->byte_count >
                TECMO_ASSET_PACK_GAMEPLAY_REBOUND_AUDIT_SIZE) {
            tecmo_asset_pack_set_message(
                message, message_size, "TGRB-1 source range rejected.");
            return -1;
        }
        actual32 = tecmo_asset_pack_fnv1a32(
            rom + (size_t)offset, expected->byte_count);
        actual64 = fnv1a64(rom + (size_t)offset, expected->byte_count);
        if (actual32 != expected->fingerprint_fnv1a32 ||
            actual64 != expected->fingerprint_fnv1a64) {
            tecmo_asset_pack_set_messagef(
                message, message_size,
                "TGRB-1 %s $%04X-$%04X revision fingerprint mismatch.",
                expected->fixed_bank != 0U ? "fixed" : "Bank05",
                (unsigned)expected->cpu_start, (unsigned)end);
            return -1;
        }
        tecmo_asset_pack_store_u16(record, (uint16_t)expected->kind);
        record[2U] = expected->bank;
        record[3U] = expected->fixed_bank;
        tecmo_asset_pack_store_u16(record + 4U, expected->cpu_start);
        tecmo_asset_pack_store_u16(record + 6U, (uint16_t)end);
        tecmo_asset_pack_store_u32(record + 8U, expected->byte_count);
        tecmo_asset_pack_store_u32(record + 12U, raw_offset);
        tecmo_asset_pack_store_u32(record + 16U, actual32);
        store_u64(record + 20U, actual64);
        tecmo_asset_pack_store_u16(record + 28U, (uint16_t)index);
        memcpy(payload + raw_offset, rom + (size_t)offset,
               expected->byte_count);
        provenance->source_offsets[index] = offset;
    }

    memcpy(payload, "TGRB", 4U);
    tecmo_asset_pack_store_u16(
        payload + 4U, TECMO_ASSET_PACK_GAMEPLAY_REBOUND_AUDIT_VERSION);
    tecmo_asset_pack_store_u16(
        payload + 6U, TECMO_ASSET_PACK_GAMEPLAY_REBOUND_AUDIT_HEADER_SIZE);
    tecmo_asset_pack_store_u32(
        payload + 8U, TECMO_ASSET_PACK_GAMEPLAY_REBOUND_AUDIT_SIZE);
    tecmo_asset_pack_store_u16(
        payload + 12U, TECMO_GAMEPLAY_REBOUND_AUDIT_SOURCE_COUNT);
    tecmo_asset_pack_store_u16(
        payload + 14U,
        TECMO_ASSET_PACK_GAMEPLAY_REBOUND_AUDIT_SOURCE_STRIDE);
    tecmo_asset_pack_store_u32(
        payload + 16U,
        TECMO_ASSET_PACK_GAMEPLAY_REBOUND_AUDIT_SOURCES_OFFSET);
    tecmo_asset_pack_store_u32(
        payload + 20U, TECMO_ASSET_PACK_GAMEPLAY_REBOUND_AUDIT_RAW_OFFSET);
    tecmo_asset_pack_store_u32(
        payload + 24U,
        TECMO_ASSET_PACK_GAMEPLAY_REBOUND_AUDIT_RAW_SIZE);
    payload[28U] = TECMO_ASSET_PACK_GAMEPLAY_REBOUND_AUDIT_DIRECT_CAROM_MASK;
    payload[29U] = TECMO_ASSET_PACK_GAMEPLAY_REBOUND_AUDIT_CAROM_READY_MASK;
    payload[30U] = 0xBEU;
    payload[31U] = 0xBFU;
    tecmo_asset_pack_store_u16(payload + 32U, 0xA977U);
    tecmo_asset_pack_store_u16(payload + 34U, 0xBA8CU);
    tecmo_asset_pack_store_u16(payload + 36U, 0xC042U);
    tecmo_asset_pack_store_u16(payload + 38U, 0xCC00U);
    payload[40U] = TECMO_ASSET_PACK_GAMEPLAY_REBOUND_AUDIT_COUNTER_INDEX;
    payload[41U] = TECMO_ASSET_PACK_GAMEPLAY_REBOUND_AUDIT_COUNTER_OFFSET;
    tecmo_asset_pack_store_u16(
        payload + 42U, TECMO_ASSET_PACK_GAMEPLAY_REBOUND_AUDIT_COUNTER_BASE);

    if (!validate_raw_semantics(payload)) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TGRB-1 producer/consumer/counter semantics rejected.");
        return -1;
    }
    if (tecmo_asset_pack_fnv1a32(payload, payload_size) !=
            TECMO_ASSET_PACK_GAMEPLAY_REBOUND_AUDIT_FNV1A32 ||
        fnv1a64(payload, payload_size) !=
            TECMO_ASSET_PACK_GAMEPLAY_REBOUND_AUDIT_FNV1A64) {
        tecmo_asset_pack_set_messagef(
            message, message_size,
            "TGRB-1 canonical payload fingerprint mismatch (got %08X/%016llX).",
            tecmo_asset_pack_fnv1a32(payload, payload_size),
            (unsigned long long)fnv1a64(payload, payload_size));
        return -1;
    }
    tecmo_asset_pack_set_message(
        message, message_size,
        "Built strict ROM-derived TGRB-1 rebound-audit evidence asset.");
    return 0;
}

int tecmo_asset_pack_gameplay_rebound_audit_source_test(
    const char *rom_path,
    char *message,
    size_t message_size)
{
    uint8_t *rom = NULL;
    uint8_t *mutated = NULL;
    uint64_t rom_size = 0U;
    uint8_t payload[TECMO_ASSET_PACK_GAMEPLAY_REBOUND_AUDIT_SIZE];
    TecmoGameplayReboundAuditProvenance provenance;
    uint64_t mutation_offsets[TECMO_GAMEPLAY_REBOUND_AUDIT_SOURCE_COUNT];
    size_t index;
    int result;
    if (rom_path == NULL ||
        tecmo_asset_pack_read_file(rom_path, &rom, &rom_size) != 0) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TGRB-1 direct source test could not read the ROM.");
        return -1;
    }
    result = tecmo_asset_pack_build_gameplay_rebound_audit(
        rom, rom_size, sizeof(rebound_audit_rev1_ines_header),
        REBOUND_AUDIT_PRG_BANK_COUNT, 1, payload, sizeof(payload),
        &provenance, message, message_size);
    if (result != 0) {
        free(rom);
        return result;
    }
    mutated = (uint8_t *)malloc((size_t)rom_size);
    if (mutated == NULL) {
        free(mutated);
        free(rom);
        tecmo_asset_pack_set_message(
            message, message_size,
            "TGRB-1 direct source test allocation rejected.");
        return -1;
    }
    memcpy(mutation_offsets, provenance.source_offsets,
           sizeof(mutation_offsets));
    for (index = 0U; index < TECMO_GAMEPLAY_REBOUND_AUDIT_SOURCE_COUNT;
         ++index) {
        char mutation_message[192];
        char location[8];
        const TecmoGameplayReboundAuditExpectedSource *expected =
            &tecmo_gameplay_rebound_audit_expected_sources[index];
        if (mutation_offsets[index] >= rom_size) {
            free(mutated);
            free(rom);
            tecmo_asset_pack_set_message(
                message, message_size,
                "TGRB-1 direct source mutation offset rejected.");
            return -1;
        }
        memcpy(mutated, rom, (size_t)rom_size);
        mutated[(size_t)mutation_offsets[index]] ^= 0x01U;
        result = tecmo_asset_pack_build_gameplay_rebound_audit(
            mutated, rom_size, sizeof(rebound_audit_rev1_ines_header),
            REBOUND_AUDIT_PRG_BANK_COUNT, 1, payload, sizeof(payload),
            &provenance, mutation_message, sizeof(mutation_message));
        (void)snprintf(location, sizeof(location), "$%04X",
                       (unsigned)expected->cpu_start);
        if (result == 0 || strstr(mutation_message, location) == NULL) {
            free(mutated);
            free(rom);
            tecmo_asset_pack_set_messagef(
                message, message_size,
                "TGRB-1 source mutation did not reject/report span $%04X.",
                (unsigned)expected->cpu_start);
            return -1;
        }
    }
    free(mutated);
    free(rom);
    tecmo_asset_pack_set_message(
        message, message_size,
        "TGRB-1 direct Rev1 source and all five span-mutation tests passed.");
    return 0;
}

int tecmo_asset_pack_gameplay_rebound_audit_self_test(
    char *message,
    size_t message_size)
{
    uint8_t truncated_rom[16] = {0U};
    uint8_t payload[TECMO_ASSET_PACK_GAMEPLAY_REBOUND_AUDIT_SIZE];
    TecmoGameplayReboundAuditProvenance provenance;
    if (TECMO_ASSET_PACK_GAMEPLAY_REBOUND_AUDIT_RAW_SIZE != 488U ||
        TECMO_ASSET_PACK_GAMEPLAY_REBOUND_AUDIT_COUNTER_OFFSET != 0xC0U ||
        tecmo_asset_pack_build_gameplay_rebound_audit(
            truncated_rom, sizeof(truncated_rom), 16U,
            REBOUND_AUDIT_PRG_BANK_COUNT, 1, payload, sizeof(payload),
            &provenance, NULL, 0U) == 0) {
        tecmo_asset_pack_set_message(
            message, message_size, "TGRB-1 importer self-test failed.");
        return -1;
    }
    tecmo_asset_pack_set_message(
        message, message_size, "TGRB-1 importer self-test passed.");
    return 0;
}
