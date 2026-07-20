#include "tecmo_asset_pack_gameplay_penalties.h"

#include "tecmo_asset_pack_gameplay.h"
#include "tecmo_asset_pack_import_layout.h"
#include "tecmo_asset_pack_util.h"
#include "tecmo_gameplay_audio.h"

#include <stdbool.h>
#include <string.h>

#define PENALTY_PRG_BANK_COUNT 8U
#define PENALTY_FIXED_BANK 7U
#define PENALTY_PRESENTATION_COUNT 2U

static const uint8_t penalty_rev1_sha256[32] = {
    0x07U,0x6AU,0x6BU,0xEBU,0x27U,0x3FU,0xABU,0x39U,
    0x19U,0x8CU,0x87U,0xAEU,0x6AU,0xF6U,0x9FU,0x80U,
    0xAAU,0x54U,0x8DU,0x68U,0x17U,0x75U,0x38U,0x29U,
    0xF2U,0xC2U,0xBDU,0xE1U,0xF9U,0x74U,0x75U,0xC4U
};

const TecmoGameplayPenaltyExpectedSource
    tecmo_gameplay_penalty_expected_sources[
        TECMO_GAMEPLAY_PENALTY_SOURCE_COUNT] = {
        {TECMO_GAMEPLAY_PENALTY_SOURCE_FOUL_COMMIT,
         5U, 0U, 0x9571U, 217U, 0xC83877F7U},
        {TECMO_GAMEPLAY_PENALTY_SOURCE_FOUL_RULES_PRESENTATION,
         2U, 0U, 0xB0F8U, 673U, 0xA06E397CU},
        {TECMO_GAMEPLAY_PENALTY_SOURCE_FOUL_PRESENTATION_SCRIPT,
         PENALTY_FIXED_BANK, 1U, 0xE95EU, 180U, 0x9AFB64FEU},
        {TECMO_GAMEPLAY_PENALTY_SOURCE_RELEASE_GATE,
         PENALTY_FIXED_BANK, 1U, 0xEA14U, 28U, 0xD9D3C9CEU},
        {TECMO_GAMEPLAY_PENALTY_SOURCE_VIOLATION_SCRIPT,
         PENALTY_FIXED_BANK, 1U, 0xEC5BU, 186U, 0x288C2162U},
        {TECMO_GAMEPLAY_PENALTY_SOURCE_RELEASE_INPUT_HELPER,
         PENALTY_FIXED_BANK, 1U, 0xD2B9U, 22U, 0x0DDA3C9AU},
        {TECMO_GAMEPLAY_PENALTY_SOURCE_VIOLATION_SELECTOR_PRESENTATION,
         3U, 0U, 0xBE87U, 290U, 0xC8FFCCEDU}
    };

const uint8_t tecmo_gameplay_penalty_expected_rules[
    TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_RULES_SIZE] = {
    6U, 5U, 5U, 4U, 10U,
    1U, 2U, 3U,
    1U, 1U, 0U, 2U,
    1U, 0U, 0U, 2U, 0U,
    8U, 2U, 3U,
    4U, 0x80U, 2U, 6U, 0U, 5U
};

const uint8_t tecmo_gameplay_penalty_expected_classes[
    TECMO_GAMEPLAY_PENALTY_FOUL_CLASS_COUNT *
        TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_CLASS_STRIDE] = {
    1U, 1U, 0U, 1U, 0U,
    0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,
    2U, 2U, 1U, 0U, 1U,
    0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,
    3U, 3U, 1U, 0U, 2U,
    0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0U
};

const uint8_t tecmo_gameplay_penalty_expected_selectors[
    TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_SELECTORS_SIZE] = {
    8U, 2U,
    0x01U, 0x05U, 0x0BU, 0x0CU, 0x0DU, 0x0EU, 0x0FU, 0x12U,
    0x08U, 0x09U,
    0U, 0U, 0U, 0U
};

#define PENALTY_VIOLATION_RECORD(selector, five_seconds) \
    selector, selector, five_seconds, 2U, \
    4U, 0U, 120U, 0U, 0x80U, 2U, 6U, 5U, 5U, 0U, 0U, 0U

const uint8_t tecmo_gameplay_penalty_expected_violations[
    TECMO_GAMEPLAY_PENALTY_VIOLATION_COUNT *
        TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_VIOLATION_STRIDE] = {
    PENALTY_VIOLATION_RECORD(1U, 0U),
    PENALTY_VIOLATION_RECORD(2U, 0U),
    PENALTY_VIOLATION_RECORD(3U, 1U),
    PENALTY_VIOLATION_RECORD(4U, 0U),
    PENALTY_VIOLATION_RECORD(5U, 0U),
    PENALTY_VIOLATION_RECORD(6U, 0U),
    PENALTY_VIOLATION_RECORD(7U, 0U)
};

#undef PENALTY_VIOLATION_RECORD

const uint8_t tecmo_gameplay_penalty_expected_presentations[
    PENALTY_PRESENTATION_COUNT *
        TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_PRESENTATION_STRIDE] = {
    1U, 0U, 0U, 0U, 5U, 5U, 1U, 0U,
    4U, 0U, 160U, 0U, 0x80U, 2U,
    0U,0U,0U,0U,0U,0U,0U,0U,0U,0U,
    2U, 1U, 7U, 6U, 5U, 5U, 1U, 0U,
    4U, 0U, 120U, 0U, 0x80U, 2U,
    0U,0U,0U,0U,0U,0U,0U,0U,0U,0U
};

static bool range_ok(uint64_t offset, uint64_t count, uint64_t total)
{
    return offset <= total && count <= total - offset;
}

static uint64_t source_offset(uint64_t prg_offset,
                              uint32_t prg_banks,
                              const TecmoGameplayPenaltyExpectedSource *source)
{
    uint32_t cpu_base = source->fixed_bank != 0U ? 0xC000U : 0x8000U;
    uint32_t bank = source->fixed_bank != 0U
        ? prg_banks - 1U
        : source->bank;
    return prg_offset + (uint64_t)bank * TECMO_ASSET_PACK_PRG_BANK_BYTES +
           (uint64_t)(source->cpu_start - cpu_base);
}

static int validate_semantic_anchors(const uint8_t *rom,
                                     uint64_t rom_size,
                                     uint64_t prg_offset,
                                     uint32_t prg_banks,
                                     char *message,
                                     size_t message_size)
{
    static const uint8_t individual_cap[] = {
        0xBDU,0x30U,0x7CU,0x18U,0x69U,0x01U,0xC9U,0x07U,
        0xB0U,0x03U,0x9DU,0x30U,0x7CU
    };
    static const uint8_t offensive_selector[] = {
        0xACU,0xE3U,0x07U,0xF0U,0x02U,0xA0U,0x01U
    };
    static const uint8_t defensive_selector[] = {
        0xADU,0x78U,0x04U,0xF0U,0x04U,0xA9U,0x01U,0x85U,0x84U
    };
    static const uint8_t foul_wait[] = {0xA0U,0xA0U,0x20U,0x14U,0xEAU};
    static const uint8_t violation_wait[] = {0xA0U,0x78U,0x20U,0x14U,0xEAU};
    static const uint8_t release_gate[] = {
        0xA9U,0x04U,0x20U,0xFAU,0xE3U,0xA9U,0x01U,0x20U,
        0xFAU,0xE3U
    };
    static const uint8_t release_a[] = {0xA9U,0x80U,0xD0U,0x02U,0xA9U,0x40U};
    uint64_t bank05 = prg_offset + 5ULL * TECMO_ASSET_PACK_PRG_BANK_BYTES;
    uint64_t bank02 = prg_offset + 2ULL * TECMO_ASSET_PACK_PRG_BANK_BYTES;
    uint64_t fixed = prg_offset +
        (uint64_t)(prg_banks - 1U) * TECMO_ASSET_PACK_PRG_BANK_BYTES;
    uint64_t individual_offset = bank05 + (0x95BEU - 0x8000U);
    uint64_t offensive_offset = bank02 + (0xB148U - 0x8000U);
    uint64_t defensive_offset = bank02 + (0xB185U - 0x8000U);
    uint64_t foul_wait_offset = fixed + (0xE971U - 0xC000U);
    uint64_t violation_wait_offset = fixed + (0xECBAU - 0xC000U);
    uint64_t release_gate_offset = fixed + (0xEA14U - 0xC000U);
    uint64_t release_a_offset = fixed + (0xD2B9U - 0xC000U);
    if (!range_ok(individual_offset, sizeof(individual_cap), rom_size) ||
        !range_ok(offensive_offset, sizeof(offensive_selector), rom_size) ||
        !range_ok(defensive_offset, sizeof(defensive_selector), rom_size) ||
        !range_ok(foul_wait_offset, sizeof(foul_wait), rom_size) ||
        !range_ok(violation_wait_offset, sizeof(violation_wait), rom_size) ||
        !range_ok(release_gate_offset, sizeof(release_gate), rom_size) ||
        !range_ok(release_a_offset, sizeof(release_a), rom_size) ||
        memcmp(rom + (size_t)individual_offset,
               individual_cap, sizeof(individual_cap)) != 0 ||
        memcmp(rom + (size_t)offensive_offset,
               offensive_selector, sizeof(offensive_selector)) != 0 ||
        memcmp(rom + (size_t)defensive_offset,
               defensive_selector, sizeof(defensive_selector)) != 0 ||
        memcmp(rom + (size_t)foul_wait_offset,
               foul_wait, sizeof(foul_wait)) != 0 ||
        memcmp(rom + (size_t)violation_wait_offset,
               violation_wait, sizeof(violation_wait)) != 0 ||
        memcmp(rom + (size_t)release_gate_offset,
               release_gate, sizeof(release_gate)) != 0 ||
        memcmp(rom + (size_t)release_a_offset,
               release_a, sizeof(release_a)) != 0) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TPNL-1 ROM instruction anchors do not derive the encoded rules.");
        return -1;
    }
    return 0;
}

int tecmo_asset_pack_build_gameplay_penalties(
    const uint8_t *rom,
    uint64_t rom_size,
    uint64_t prg_offset,
    uint32_t prg_banks,
    int enforce_revision_fingerprints,
    uint8_t *payload,
    size_t payload_size,
    TecmoGameplayPenaltyProvenance *provenance,
    char *message,
    size_t message_size)
{
    if (rom == NULL || payload == NULL || provenance == NULL ||
        payload_size != TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_SIZE ||
        prg_banks != PENALTY_PRG_BANK_COUNT ||
        enforce_revision_fingerprints == 0) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TPNL-1 import requires the exact Rev1 ROM contract.");
        return -1;
    }
    memset(payload, 0, payload_size);
    memset(provenance, 0, sizeof(*provenance));
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_PENALTY_SOURCE_COUNT; ++index) {
        const TecmoGameplayPenaltyExpectedSource *expected =
            &tecmo_gameplay_penalty_expected_sources[index];
        uint64_t offset = source_offset(prg_offset, prg_banks, expected);
        uint32_t cpu_end = (uint32_t)expected->cpu_start +
                           expected->byte_count - 1U;
        uint8_t *record = payload +
            TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_SOURCES_OFFSET +
            index * TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_SOURCE_STRIDE;
        if (expected->bank >= prg_banks || cpu_end > 0xFFFFU ||
            !range_ok(offset, expected->byte_count, rom_size) ||
            tecmo_asset_pack_fnv1a32(
                rom + (size_t)offset, expected->byte_count) !=
                    expected->fingerprint) {
            tecmo_asset_pack_set_messagef(
                message, message_size,
                "TPNL-1 %s Bank%02u $%04X-$%04X fingerprint mismatch.",
                expected->fixed_bank != 0U ? "fixed" : "switched",
                (unsigned)expected->bank,
                (unsigned)expected->cpu_start, (unsigned)cpu_end);
            return -1;
        }
        tecmo_asset_pack_store_u16(record, (uint16_t)expected->kind);
        record[2U] = expected->bank;
        record[3U] = expected->fixed_bank;
        tecmo_asset_pack_store_u16(record + 4U, expected->cpu_start);
        tecmo_asset_pack_store_u16(record + 6U, (uint16_t)cpu_end);
        tecmo_asset_pack_store_u32(record + 8U, expected->byte_count);
        tecmo_asset_pack_store_u32(record + 12U, expected->fingerprint);
        tecmo_asset_pack_store_u16(record + 16U, (uint16_t)index);
        provenance->source_offsets[index] = offset;
    }
    if (validate_semantic_anchors(rom, rom_size, prg_offset, prg_banks,
                                  message, message_size) != 0) {
        return -1;
    }

    memcpy(payload + TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_RULES_OFFSET,
           tecmo_gameplay_penalty_expected_rules,
           sizeof(tecmo_gameplay_penalty_expected_rules));
    memcpy(payload + TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_CLASSES_OFFSET,
           tecmo_gameplay_penalty_expected_classes,
           sizeof(tecmo_gameplay_penalty_expected_classes));
    memcpy(payload + TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_SELECTORS_OFFSET,
           tecmo_gameplay_penalty_expected_selectors,
           sizeof(tecmo_gameplay_penalty_expected_selectors));
    memcpy(payload + TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_VIOLATIONS_OFFSET,
           tecmo_gameplay_penalty_expected_violations,
           sizeof(tecmo_gameplay_penalty_expected_violations));
    memcpy(payload + TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_PRESENTATIONS_OFFSET,
           tecmo_gameplay_penalty_expected_presentations,
           sizeof(tecmo_gameplay_penalty_expected_presentations));

    memcpy(payload, "TPNL", 4U);
    tecmo_asset_pack_store_u16(
        payload + 4U, TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_VERSION);
    tecmo_asset_pack_store_u16(
        payload + 6U, TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_HEADER_SIZE);
    tecmo_asset_pack_store_u32(
        payload + 8U, TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_SIZE);
    tecmo_asset_pack_store_u16(
        payload + 12U, TECMO_GAMEPLAY_PENALTY_SOURCE_COUNT);
    tecmo_asset_pack_store_u16(
        payload + 14U, TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_SOURCE_STRIDE);
    tecmo_asset_pack_store_u32(
        payload + 16U, TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_SOURCES_OFFSET);
    tecmo_asset_pack_store_u32(
        payload + 20U, TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_RULES_OFFSET);
    tecmo_asset_pack_store_u32(
        payload + 24U, TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_RULES_SIZE);
    tecmo_asset_pack_store_u32(
        payload + 28U, TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_CLASSES_OFFSET);
    tecmo_asset_pack_store_u16(
        payload + 32U, TECMO_GAMEPLAY_PENALTY_FOUL_CLASS_COUNT);
    tecmo_asset_pack_store_u16(
        payload + 34U, TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_CLASS_STRIDE);
    tecmo_asset_pack_store_u32(
        payload + 36U, TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_SELECTORS_OFFSET);
    tecmo_asset_pack_store_u32(
        payload + 40U, TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_SELECTORS_SIZE);
    tecmo_asset_pack_store_u32(
        payload + 44U, TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_VIOLATIONS_OFFSET);
    tecmo_asset_pack_store_u16(
        payload + 48U, TECMO_GAMEPLAY_PENALTY_VIOLATION_COUNT);
    tecmo_asset_pack_store_u16(
        payload + 50U, TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_VIOLATION_STRIDE);
    tecmo_asset_pack_store_u32(
        payload + 52U, TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_PRESENTATIONS_OFFSET);
    tecmo_asset_pack_store_u16(payload + 56U, PENALTY_PRESENTATION_COUNT);
    tecmo_asset_pack_store_u16(
        payload + 58U, TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_PRESENTATION_STRIDE);
    tecmo_asset_pack_store_u16(payload + 60U, 2U);
    tecmo_asset_pack_store_u32(payload + 64U,
                               TECMO_ASSET_PACK_GAMEPLAY_SIZE);
    tecmo_asset_pack_store_u32(payload + 68U,
                               TECMO_ASSET_PACK_GAMEPLAY_FNV1A32);
    tecmo_asset_pack_store_u32(payload + 72U,
                               TECMO_GAMEPLAY_SFX_PAYLOAD_SIZE);
    tecmo_asset_pack_store_u32(payload + 76U,
                               TECMO_GAMEPLAY_SFX_PAYLOAD_FNV1A32);
    tecmo_asset_pack_store_u32(
        payload + 80U, TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_SOURCES_FNV1A32);
    tecmo_asset_pack_store_u32(
        payload + 84U, TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_RULES_FNV1A32);
    tecmo_asset_pack_store_u32(
        payload + 88U, TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_CLASSES_FNV1A32);
    tecmo_asset_pack_store_u32(
        payload + 92U, TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_SELECTORS_FNV1A32);
    tecmo_asset_pack_store_u32(
        payload + 96U, TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_VIOLATIONS_FNV1A32);
    tecmo_asset_pack_store_u32(
        payload + 100U,
        TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_PRESENTATIONS_FNV1A32);
    memcpy(payload + 104U, penalty_rev1_sha256, sizeof(penalty_rev1_sha256));

    if (tecmo_asset_pack_fnv1a32(payload, payload_size) !=
            TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_FNV1A32) {
        tecmo_asset_pack_set_messagef(
            message, message_size,
            "TPNL-1 canonical payload fingerprint mismatch (got %08X).",
            tecmo_asset_pack_fnv1a32(payload, payload_size));
        return -1;
    }
    tecmo_asset_pack_set_message(
        message, message_size,
        "Built strict ROM-derived TPNL-1 penalty rules asset.");
    return 0;
}

int tecmo_asset_pack_gameplay_penalties_self_test(char *message,
                                                  size_t message_size)
{
    uint8_t truncated_rom[16] = {0};
    uint8_t payload[TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_SIZE];
    TecmoGameplayPenaltyProvenance provenance;
    if (sizeof(tecmo_gameplay_penalty_expected_rules) !=
            TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_RULES_SIZE ||
        sizeof(tecmo_gameplay_penalty_expected_classes) !=
            TECMO_GAMEPLAY_PENALTY_FOUL_CLASS_COUNT *
                TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_CLASS_STRIDE ||
        sizeof(tecmo_gameplay_penalty_expected_violations) !=
            TECMO_GAMEPLAY_PENALTY_VIOLATION_COUNT *
                TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_VIOLATION_STRIDE ||
        sizeof(tecmo_gameplay_penalty_expected_presentations) !=
            PENALTY_PRESENTATION_COUNT *
                TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_PRESENTATION_STRIDE ||
        TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_SOURCES_OFFSET +
                TECMO_GAMEPLAY_PENALTY_SOURCE_COUNT *
                    TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_SOURCE_STRIDE !=
            TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_RULES_OFFSET ||
        TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_PRESENTATIONS_OFFSET +
                sizeof(tecmo_gameplay_penalty_expected_presentations) !=
            TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_SIZE ||
        tecmo_asset_pack_fnv1a32(
            tecmo_gameplay_penalty_expected_rules,
            sizeof(tecmo_gameplay_penalty_expected_rules)) !=
            TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_RULES_FNV1A32 ||
        tecmo_asset_pack_fnv1a32(
            tecmo_gameplay_penalty_expected_classes,
            sizeof(tecmo_gameplay_penalty_expected_classes)) !=
            TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_CLASSES_FNV1A32 ||
        tecmo_asset_pack_fnv1a32(
            tecmo_gameplay_penalty_expected_selectors,
            sizeof(tecmo_gameplay_penalty_expected_selectors)) !=
            TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_SELECTORS_FNV1A32 ||
        tecmo_asset_pack_fnv1a32(
            tecmo_gameplay_penalty_expected_violations,
            sizeof(tecmo_gameplay_penalty_expected_violations)) !=
            TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_VIOLATIONS_FNV1A32 ||
        tecmo_asset_pack_fnv1a32(
            tecmo_gameplay_penalty_expected_presentations,
            sizeof(tecmo_gameplay_penalty_expected_presentations)) !=
            TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_PRESENTATIONS_FNV1A32 ||
        tecmo_asset_pack_build_gameplay_penalties(
            truncated_rom, sizeof(truncated_rom), 16U,
            PENALTY_PRG_BANK_COUNT, 1, payload, sizeof(payload),
            &provenance, NULL, 0U) == 0) {
        tecmo_asset_pack_set_message(message, message_size,
                                     "TPNL-1 layout self-test failed.");
        return -1;
    }
    tecmo_asset_pack_set_message(message, message_size,
                                 "TPNL-1 layout self-test passed.");
    return 0;
}
