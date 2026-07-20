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
#define PENALTY_REV1_ROM_SIZE 393232ULL

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
         3U, 0U, 0xBE87U, 290U, 0xC8FFCCEDU},
        {TECMO_GAMEPLAY_PENALTY_SOURCE_PRESENTATION_CUE_REQUEST,
         4U, 0U, 0xBA1FU, 32U, 0xF56AD5D8U}
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
    4U, 0U, 120U, 0U, 0x80U, 2U, 6U, 5U, 5U, 0U, 16U, 0U

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
    1U, 0U, 0U, 6U, 5U, 5U, 1U, 0U,
    4U, 0U, 160U, 0U, 0x80U, 2U,
    16U,0U,0U,0U,0U,0U,0U,0U,0U,0U,
    2U, 1U, 7U, 6U, 5U, 5U, 1U, 0U,
    4U, 0U, 120U, 0U, 0x80U, 2U,
    16U,0U,0U,0U,0U,0U,0U,0U,0U,0U
};

static uint32_t sha256_rotate_right(uint32_t value, unsigned shift)
{
    return (value >> shift) | (value << (32U - shift));
}

static void sha256_transform(uint32_t state[8], const uint8_t block[64])
{
    static const uint32_t constants[64] = {
        0x428A2F98U,0x71374491U,0xB5C0FBCFU,0xE9B5DBA5U,
        0x3956C25BU,0x59F111F1U,0x923F82A4U,0xAB1C5ED5U,
        0xD807AA98U,0x12835B01U,0x243185BEU,0x550C7DC3U,
        0x72BE5D74U,0x80DEB1FEU,0x9BDC06A7U,0xC19BF174U,
        0xE49B69C1U,0xEFBE4786U,0x0FC19DC6U,0x240CA1CCU,
        0x2DE92C6FU,0x4A7484AAU,0x5CB0A9DCU,0x76F988DAU,
        0x983E5152U,0xA831C66DU,0xB00327C8U,0xBF597FC7U,
        0xC6E00BF3U,0xD5A79147U,0x06CA6351U,0x14292967U,
        0x27B70A85U,0x2E1B2138U,0x4D2C6DFCU,0x53380D13U,
        0x650A7354U,0x766A0ABBU,0x81C2C92EU,0x92722C85U,
        0xA2BFE8A1U,0xA81A664BU,0xC24B8B70U,0xC76C51A3U,
        0xD192E819U,0xD6990624U,0xF40E3585U,0x106AA070U,
        0x19A4C116U,0x1E376C08U,0x2748774CU,0x34B0BCB5U,
        0x391C0CB3U,0x4ED8AA4AU,0x5B9CCA4FU,0x682E6FF3U,
        0x748F82EEU,0x78A5636FU,0x84C87814U,0x8CC70208U,
        0x90BEFFFAU,0xA4506CEBU,0xBEF9A3F7U,0xC67178F2U
    };
    uint32_t words[64];
    uint32_t a;
    uint32_t b;
    uint32_t c;
    uint32_t d;
    uint32_t e;
    uint32_t f;
    uint32_t g;
    uint32_t h;
    for (size_t index = 0U; index < 16U; ++index) {
        size_t offset = index * 4U;
        words[index] = ((uint32_t)block[offset] << 24U) |
                       ((uint32_t)block[offset + 1U] << 16U) |
                       ((uint32_t)block[offset + 2U] << 8U) |
                       (uint32_t)block[offset + 3U];
    }
    for (size_t index = 16U; index < 64U; ++index) {
        uint32_t prior = words[index - 15U];
        uint32_t recent = words[index - 2U];
        uint32_t sigma0 = sha256_rotate_right(prior, 7U) ^
                          sha256_rotate_right(prior, 18U) ^ (prior >> 3U);
        uint32_t sigma1 = sha256_rotate_right(recent, 17U) ^
                          sha256_rotate_right(recent, 19U) ^ (recent >> 10U);
        words[index] = words[index - 16U] + sigma0 +
                       words[index - 7U] + sigma1;
    }
    a = state[0U];
    b = state[1U];
    c = state[2U];
    d = state[3U];
    e = state[4U];
    f = state[5U];
    g = state[6U];
    h = state[7U];
    for (size_t index = 0U; index < 64U; ++index) {
        uint32_t choice = (e & f) ^ ((~e) & g);
        uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
        uint32_t sum0 = sha256_rotate_right(a, 2U) ^
                        sha256_rotate_right(a, 13U) ^
                        sha256_rotate_right(a, 22U);
        uint32_t sum1 = sha256_rotate_right(e, 6U) ^
                        sha256_rotate_right(e, 11U) ^
                        sha256_rotate_right(e, 25U);
        uint32_t temp1 = h + sum1 + choice + constants[index] + words[index];
        uint32_t temp2 = sum0 + majority;
        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }
    state[0U] += a;
    state[1U] += b;
    state[2U] += c;
    state[3U] += d;
    state[4U] += e;
    state[5U] += f;
    state[6U] += g;
    state[7U] += h;
}

static bool sha256_digest(const uint8_t *bytes,
                          size_t byte_count,
                          uint8_t digest[32])
{
    uint32_t state[8] = {
        0x6A09E667U,0xBB67AE85U,0x3C6EF372U,0xA54FF53AU,
        0x510E527FU,0x9B05688CU,0x1F83D9ABU,0x5BE0CD19U
    };
    uint8_t tail[128];
    size_t full_bytes = byte_count - byte_count % 64U;
    size_t remainder = byte_count - full_bytes;
    size_t padded_size;
    uint64_t bit_count;
    if (bytes == NULL || digest == NULL ||
        (uint64_t)byte_count > UINT64_MAX / 8ULL) {
        return false;
    }
    for (size_t offset = 0U; offset < full_bytes; offset += 64U) {
        sha256_transform(state, bytes + offset);
    }
    memset(tail, 0, sizeof(tail));
    memcpy(tail, bytes + full_bytes, remainder);
    tail[remainder] = 0x80U;
    padded_size = remainder < 56U ? 64U : 128U;
    bit_count = (uint64_t)byte_count * 8ULL;
    for (size_t index = 0U; index < 8U; ++index) {
        tail[padded_size - 1U - index] =
            (uint8_t)(bit_count >> (index * 8U));
    }
    sha256_transform(state, tail);
    if (padded_size == 128U) {
        sha256_transform(state, tail + 64U);
    }
    for (size_t index = 0U; index < 8U; ++index) {
        digest[index * 4U] = (uint8_t)(state[index] >> 24U);
        digest[index * 4U + 1U] = (uint8_t)(state[index] >> 16U);
        digest[index * 4U + 2U] = (uint8_t)(state[index] >> 8U);
        digest[index * 4U + 3U] = (uint8_t)state[index];
    }
    return true;
}

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
    static const uint8_t screen22_dispatch[] = {
        0xA9U,0x22U,0x20U,0x11U,0xC7U
    };
    static const uint8_t delayed_cue_request[] = {
        0xA9U,0x10U,0x20U,0x00U,0xC0U,
        0xA9U,0x06U,0x20U,0x09U,0xC0U
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
    uint64_t bank03 = prg_offset + 3ULL * TECMO_ASSET_PACK_PRG_BANK_BYTES;
    uint64_t bank04 = prg_offset + 4ULL * TECMO_ASSET_PACK_PRG_BANK_BYTES;
    uint64_t fixed = prg_offset +
        (uint64_t)(prg_banks - 1U) * TECMO_ASSET_PACK_PRG_BANK_BYTES;
    uint64_t individual_offset = bank05 + (0x95BEU - 0x8000U);
    uint64_t offensive_offset = bank02 + (0xB148U - 0x8000U);
    uint64_t defensive_offset = bank02 + (0xB185U - 0x8000U);
    uint64_t foul_screen_offset = fixed + (0xE96CU - 0xC000U);
    uint64_t violation_screen_offset = bank03 + (0xBE9CU - 0x8000U);
    uint64_t delayed_cue_offset = bank04 + (0xBA35U - 0x8000U);
    uint64_t foul_wait_offset = fixed + (0xE971U - 0xC000U);
    uint64_t violation_wait_offset = fixed + (0xECBAU - 0xC000U);
    uint64_t release_gate_offset = fixed + (0xEA14U - 0xC000U);
    uint64_t release_a_offset = fixed + (0xD2B9U - 0xC000U);
    if (!range_ok(individual_offset, sizeof(individual_cap), rom_size) ||
        !range_ok(offensive_offset, sizeof(offensive_selector), rom_size) ||
        !range_ok(defensive_offset, sizeof(defensive_selector), rom_size) ||
        !range_ok(foul_screen_offset, sizeof(screen22_dispatch), rom_size) ||
        !range_ok(violation_screen_offset,
                  sizeof(screen22_dispatch), rom_size) ||
        !range_ok(delayed_cue_offset,
                  sizeof(delayed_cue_request), rom_size) ||
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
        memcmp(rom + (size_t)foul_screen_offset,
               screen22_dispatch, sizeof(screen22_dispatch)) != 0 ||
        memcmp(rom + (size_t)violation_screen_offset,
               screen22_dispatch, sizeof(screen22_dispatch)) != 0 ||
        memcmp(rom + (size_t)delayed_cue_offset,
               delayed_cue_request, sizeof(delayed_cue_request)) != 0 ||
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
    uint8_t input_sha256[32];
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
    if (rom_size != PENALTY_REV1_ROM_SIZE || rom_size > (uint64_t)SIZE_MAX ||
        !sha256_digest(rom, (size_t)rom_size, input_sha256) ||
        memcmp(input_sha256, penalty_rev1_sha256,
               sizeof(input_sha256)) != 0) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TPNL-1 full-ROM SHA-256 mismatch for target Rev1.");
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
    static const uint8_t sha256_abc[32] = {
        0xBAU,0x78U,0x16U,0xBFU,0x8FU,0x01U,0xCFU,0xEAU,
        0x41U,0x41U,0x40U,0xDEU,0x5DU,0xAEU,0x22U,0x23U,
        0xB0U,0x03U,0x61U,0xA3U,0x96U,0x17U,0x7AU,0x9CU,
        0xB4U,0x10U,0xFFU,0x61U,0xF2U,0x00U,0x15U,0xADU
    };
    uint8_t truncated_rom[16] = {0};
    uint8_t payload[TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_SIZE];
    uint8_t digest[32];
    TecmoGameplayPenaltyProvenance provenance;
    if (!sha256_digest((const uint8_t *)"abc", 3U, digest) ||
        memcmp(digest, sha256_abc, sizeof(digest)) != 0 ||
        sizeof(tecmo_gameplay_penalty_expected_rules) !=
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
