#include "tecmo_asset_pack_gameplay_pretip.h"

#include "tecmo_asset_pack_d9f6.h"
#include "tecmo_asset_pack_gameplay.h"
#include "tecmo_asset_pack_import_layout.h"
#include "tecmo_asset_pack_util.h"

#include <string.h>

#define PRETIP_REV1_ROM_SIZE 393232U
#define PRETIP_REV1_ROM_FNV1A32 0x0650F5B0U
#define PRETIP_PRG_BANK_COUNT 8U
#define PRETIP_DECODED_SIZE 2048U
#define PRETIP_ENCODED_SIZE 24U

static const uint8_t pretip_rev1_ines_header[16] = {
    'N','E','S',0x1AU,0x08U,0x20U,0x42U,0x00U,
    0x00U,0x00U,0x00U,0x00U,0x00U,0x00U,0x00U,0x00U
};

static const uint8_t pretip_rev1_sha256[32] = {
    0x07U,0x6AU,0x6BU,0xEBU,0x27U,0x3FU,0xABU,0x39U,
    0x19U,0x8CU,0x87U,0xAEU,0x6AU,0xF6U,0x9FU,0x80U,
    0xAAU,0x54U,0x8DU,0x68U,0x17U,0x75U,0x38U,0x29U,
    0xF2U,0xC2U,0xBDU,0xE1U,0xF9U,0x74U,0x75U,0xC4U
};

static void build_mechanics_block(uint8_t *block,
                                  const uint8_t *distance_table)
{
    if (block == NULL) return;
    memset(block, 0, TECMO_GAMEPLAY_PRETIP_TPM2_SIZE);
    memcpy(block, "TPM2", 4U);
    tecmo_asset_pack_store_u16(block + 4U,
                               TECMO_GAMEPLAY_PRETIP_TPM2_VERSION);
    tecmo_asset_pack_store_u16(block + 6U,
                               TECMO_GAMEPLAY_PRETIP_TPM2_SIZE);
    block[8U] = TECMO_GAMEPLAY_PRETIP_INPUT_MASK;
    block[9U] = TECMO_GAMEPLAY_PRETIP_NO_SAMPLE_ERROR;
    block[10U] = TECMO_GAMEPLAY_PRETIP_MAX_SAMPLE_ERROR;
    block[11U] = TECMO_GAMEPLAY_PRETIP_AUTO_THRESHOLD_BASE;
    block[12U] = TECMO_GAMEPLAY_PRETIP_AUTO_THRESHOLD_MASK;
    block[13U] = TECMO_GAMEPLAY_PRETIP_AUTO_THRESHOLD_SHIFT;
    block[14U] = TECMO_GAMEPLAY_PRETIP_CLAIM_BALL_HIGH_MIN;
    block[15U] = TECMO_GAMEPLAY_PRETIP_CLAIM_BALL_MINUS_JUMPER_LIMIT;
    block[16U] = TECMO_GAMEPLAY_PRETIP_ACTOR_JUMP_COMMIT_STATE;
    block[17U] = TECMO_GAMEPLAY_PRETIP_SLOT10_CLAIM_COMMIT_STATE;
    tecmo_asset_pack_store_u16(block + 18U, 0x0380U);
    tecmo_asset_pack_store_u16(block + 20U, 0x037FU);
    tecmo_asset_pack_store_u16(block + 22U, 0xA2D5U);
    tecmo_asset_pack_store_u16(block + 24U, 0xCD96U);
    tecmo_asset_pack_store_u16(block + 26U, 22U);
    tecmo_asset_pack_store_u16(block + 28U, 0xE56EU);
    tecmo_asset_pack_store_u16(block + 30U, 1U);
    block[32U] = TECMO_GAMEPLAY_PRETIP_SOURCE_CAPTURE_ERROR;
    block[33U] = TECMO_GAMEPLAY_PRETIP_SOURCE_ACTOR_DISPATCHER;
    block[34U] = TECMO_GAMEPLAY_PRETIP_SOURCE_AUTOMATIC_ACTOR_PATH;
    block[35U] = TECMO_GAMEPLAY_PRETIP_SOURCE_OPPOSING_DISPATCHER;
    block[36U] = TECMO_GAMEPLAY_PRETIP_SOURCE_OPPOSING_ACTOR_PATH;
    block[37U] = TECMO_GAMEPLAY_PRETIP_SOURCE_JUMP_COMMIT;
    block[38U] = TECMO_GAMEPLAY_PRETIP_SOURCE_SLOT10_CLAIM;
    block[39U] = TECMO_GAMEPLAY_PRETIP_SOURCE_E56E_HOOK_ANCHOR;
    block[40U] = TECMO_GAMEPLAY_PRETIP_SOURCE_RNG_MIX;
    block[41U] = 0x5AU; /* deterministic native bridge $53 seed */
    block[42U] = 0U;    /* deterministic native bridge $6A seed */
    block[43U] = 0U;    /* equality remains deferred, never tie-away */
    block[44U] = TECMO_GAMEPLAY_PRETIP_RAW_SELECTOR_0380_SEED;
    block[45U] = TECMO_GAMEPLAY_PRETIP_RAW_SELECTOR_037F_SEED;
    if (distance_table != NULL) {
        memcpy(block + TECMO_GAMEPLAY_PRETIP_BALL_DISTANCE_TABLE_OFFSET,
               distance_table,
               TECMO_GAMEPLAY_PRETIP_BALL_DISTANCE_TABLE_SIZE);
    }
}

const TecmoGameplayPreTipExpectedSource
    tecmo_gameplay_pretip_expected_sources[
        TECMO_GAMEPLAY_PRETIP_SOURCE_COUNT] = {
        {TECMO_GAMEPLAY_PRETIP_SOURCE_SCREEN_DESCRIPTOR,7U,1U,0xDD18U,7U,
         0x4EB8B3ABU,0xDCACA0877194976BULL,
         TECMO_ASSET_PACK_GAMEPLAY_PRETIP_DESCRIPTOR_OFFSET},
        {TECMO_GAMEPLAY_PRETIP_SOURCE_SCREEN_STREAM,0U,0U,0x835AU,24U,
         0x3C7D9D61U,0xC62FDDDD1AC3F401ULL,
         TECMO_ASSET_PACK_GAMEPLAY_PRETIP_ENCODED_OFFSET},
        {TECMO_GAMEPLAY_PRETIP_SOURCE_SCREEN_PALETTE,0U,0U,0x84EBU,16U,
         0xB389D1A4U,0xBB47DDCD587B4544ULL,
         TECMO_ASSET_PACK_GAMEPLAY_PRETIP_PALETTE_OFFSET},
        {TECMO_GAMEPLAY_PRETIP_SOURCE_WAIT_HELPERS,6U,0U,0xA0F4U,49U,
         0x3E73FFECU,0x1FE3F2EA43FEDF8CULL,
         TECMO_ASSET_PACK_GAMEPLAY_PRETIP_WAIT_OFFSET},
        {TECMO_GAMEPLAY_PRETIP_SOURCE_MATCHUP_SEQUENCE,6U,0U,0xA125U,299U,
         0xA87E338CU,0x085FC7A4CB8426ACULL,
         TECMO_ASSET_PACK_GAMEPLAY_PRETIP_SEQUENCE_OFFSET},
        {TECMO_GAMEPLAY_PRETIP_SOURCE_MODE_STRINGS,6U,0U,0xA250U,56U,
         0xD417BD44U,0x82D12878D6971084ULL,
         TECMO_ASSET_PACK_GAMEPLAY_PRETIP_STRINGS_OFFSET},
        {TECMO_GAMEPLAY_PRETIP_SOURCE_MODE_POINTERS,6U,0U,0xA288U,8U,
         0x9AB7D59FU,0x54ED9EC8D6CFF0DFULL,
         TECMO_ASSET_PACK_GAMEPLAY_PRETIP_POINTERS_OFFSET},
        {TECMO_GAMEPLAY_PRETIP_SOURCE_CHARACTER_MAP,6U,0U,0xA290U,63U,
         0xA482CC3DU,0x66FB75B9E18FD1FDULL,
         TECMO_ASSET_PACK_GAMEPLAY_PRETIP_CHARMAP_OFFSET},
        {TECMO_GAMEPLAY_PRETIP_SOURCE_CHARACTER_TILES,6U,0U,0xAF05U,152U,
         0xD541FFD5U,0xA1891DD2D3930395ULL,
         TECMO_ASSET_PACK_GAMEPLAY_PRETIP_CHARACTER_TILES_OFFSET},
        {TECMO_GAMEPLAY_PRETIP_SOURCE_TEXT_CHR_SELECTORS,6U,0U,0x9FA8U,10U,
         0xCE41F776U,0xB68811BCD0FAF076ULL,
         TECMO_ASSET_PACK_GAMEPLAY_PRETIP_TEXT_CHR_SELECTORS_OFFSET},
        {TECMO_GAMEPLAY_PRETIP_SOURCE_CLOSEUP_ENTRY,4U,0U,0x86ABU,510U,
         0x5EF44845U,0xCB66AABD4429C9E5ULL,
         TECMO_ASSET_PACK_GAMEPLAY_PRETIP_CLOSEUP_ENTRY_OFFSET},
        {TECMO_GAMEPLAY_PRETIP_SOURCE_CLOSEUP_PALETTE,4U,0U,0x89DDU,80U,
         0x15BD4D46U,0xD08622F7737FBB06ULL,
         TECMO_ASSET_PACK_GAMEPLAY_PRETIP_CLOSEUP_PALETTE_OFFSET},
        {TECMO_GAMEPLAY_PRETIP_SOURCE_CLOSEUP_CONTROL,4U,0U,0xAC76U,268U,
         0xA343EF88U,0x3B7E9B558EE6B4C8ULL,
         TECMO_ASSET_PACK_GAMEPLAY_PRETIP_CLOSEUP_CONTROL_OFFSET},
        {TECMO_GAMEPLAY_PRETIP_SOURCE_CLOSEUP_TIMING,4U,0U,0xAD82U,94U,
         0xC1C42D30U,0x3208F695BCBC76F0ULL,
         TECMO_ASSET_PACK_GAMEPLAY_PRETIP_CLOSEUP_TIMING_OFFSET},
        {TECMO_GAMEPLAY_PRETIP_SOURCE_CLOSEUP_SPRITE_STAGING,7U,1U,
         0xD861U,202U,0xA1491ACFU,0x40C7703A9FBB96CFULL,
         TECMO_ASSET_PACK_GAMEPLAY_PRETIP_CLOSEUP_SPRITE_STAGING_OFFSET},
        {TECMO_GAMEPLAY_PRETIP_SOURCE_TIP_SETUP,5U,0U,0x985BU,52U,
         0xF372E57CU,0x096F86BBD7A42ABCULL,
         TECMO_ASSET_PACK_GAMEPLAY_PRETIP_TIP_SETUP_OFFSET},
        {TECMO_GAMEPLAY_PRETIP_SOURCE_LATER_GENERAL_COLLISION_SETTLEMENT,
         5U,0U,0x98E1U,383U,
         0x0A2F945AU,0x6CB90FF6825E2A5AULL,
         TECMO_ASSET_PACK_GAMEPLAY_PRETIP_TIP_UPDATE_OFFSET},
        {TECMO_GAMEPLAY_PRETIP_SOURCE_LAUNCH_BRIDGE,7U,1U,0xE4A5U,146U,
         0x345D5CF8U,0x065BC86AA14C4478ULL,
         TECMO_ASSET_PACK_GAMEPLAY_PRETIP_LAUNCH_BRIDGE_OFFSET},
        {TECMO_GAMEPLAY_PRETIP_SOURCE_LIVE_HANDOFF,7U,1U,0xEB8DU,121U,
         0x32E920E6U,0x379F98E049CAED86ULL,
         TECMO_ASSET_PACK_GAMEPLAY_PRETIP_HANDOFF_OFFSET},
        {TECMO_GAMEPLAY_PRETIP_SOURCE_ORIENTATION_ORDERING,7U,1U,0xE537U,12U,
         0x23EB540BU,0xF47061919680B38BULL,
         TECMO_ASSET_PACK_GAMEPLAY_PRETIP_ORIENTATION_OFFSET},
        {TECMO_GAMEPLAY_PRETIP_SOURCE_CAPTURE_ERROR,4U,0U,0x86E1U,311U,
         0xF62D7C02U,0x3940397C1774CFE2ULL,
         TECMO_ASSET_PACK_GAMEPLAY_PRETIP_CAPTURE_ERROR_OFFSET},
        {TECMO_GAMEPLAY_PRETIP_SOURCE_ACTOR_DISPATCHER,5U,0U,0x8351U,78U,
         0x49B11D9AU,0xA90A6C174BE0F15AULL,
         TECMO_ASSET_PACK_GAMEPLAY_PRETIP_ACTOR_DISPATCHER_OFFSET},
        {TECMO_GAMEPLAY_PRETIP_SOURCE_AUTOMATIC_ACTOR_PATH,5U,0U,0x839FU,100U,
         0x39E37AD9U,0x7591953DB1443C99ULL,
         TECMO_ASSET_PACK_GAMEPLAY_PRETIP_AUTOMATIC_ACTOR_OFFSET},
        {TECMO_GAMEPLAY_PRETIP_SOURCE_OPPOSING_DISPATCHER,5U,0U,0x9824U,44U,
         0x40A15F8AU,0x115114FC51CF97EAULL,
         TECMO_ASSET_PACK_GAMEPLAY_PRETIP_OPPOSING_DISPATCHER_OFFSET},
        {TECMO_GAMEPLAY_PRETIP_SOURCE_OPPOSING_ACTOR_PATH,5U,0U,0x985EU,49U,
         0x1A6CF134U,0x3EE3400C4DD54BB4ULL,
         TECMO_ASSET_PACK_GAMEPLAY_PRETIP_OPPOSING_ACTOR_OFFSET},
        {TECMO_GAMEPLAY_PRETIP_SOURCE_JUMP_COMMIT,5U,0U,0x9C79U,80U,
         0x76854C2BU,0x12DAA5698409484BULL,
         TECMO_ASSET_PACK_GAMEPLAY_PRETIP_JUMP_COMMIT_OFFSET},
        {TECMO_GAMEPLAY_PRETIP_SOURCE_SLOT10_CLAIM,5U,0U,0xA214U,203U,
         0xED30348DU,0x0E50CF92E469E26DULL,
         TECMO_ASSET_PACK_GAMEPLAY_PRETIP_SLOT10_CLAIM_OFFSET},
        {TECMO_GAMEPLAY_PRETIP_SOURCE_E56E_HOOK_ANCHOR,7U,1U,0xE56EU,1U,
         0xAC0BD104U,0xAF64244C860266E4ULL,
         TECMO_ASSET_PACK_GAMEPLAY_PRETIP_E56E_HOOK_OFFSET},
        {TECMO_GAMEPLAY_PRETIP_SOURCE_RNG_MIX,7U,1U,0xCD96U,22U,
         0x299318D0U,0xFB37A5940B9FFA10ULL,
         TECMO_ASSET_PACK_GAMEPLAY_PRETIP_RNG_MIX_OFFSET}
    };

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

static bool source_span_contains(size_t outer_offset, size_t outer_size,
                                 size_t inner_offset, size_t inner_size)
{
    return inner_offset >= outer_offset &&
           inner_offset - outer_offset <= outer_size &&
           inner_size <= outer_size - (inner_offset - outer_offset);
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

static void store_u64(uint8_t *bytes, uint64_t value)
{
    unsigned index;
    for (index = 0U; index < 8U; ++index)
        bytes[index] = (uint8_t)(value >> (index * 8U));
}

static bool range_ok(uint64_t offset, uint64_t count, uint64_t total)
{
    return offset <= total && count <= total - offset;
}

static uint64_t source_offset(
    uint64_t prg_offset,
    const TecmoGameplayPreTipExpectedSource *source)
{
    uint16_t base = source->fixed_bank != 0U ? 0xC000U : 0x8000U;
    return prg_offset +
           (uint64_t)source->bank * TECMO_ASSET_PACK_PRG_BANK_BYTES +
           (uint64_t)(source->cpu_start - base);
}

int tecmo_asset_pack_build_gameplay_pretip(
    const uint8_t *rom,
    uint64_t rom_size,
    uint64_t prg_offset,
    uint32_t prg_banks,
    int enforce_revision_fingerprints,
    uint8_t *payload,
    size_t payload_size,
    TecmoGameplayPreTipProvenance *provenance,
    char *message,
    size_t message_size)
{
    uint8_t sha256[32];
    size_t encoded_size = 0U;
    size_t index;
    if (rom == NULL || payload == NULL || provenance == NULL ||
        payload_size != TECMO_ASSET_PACK_GAMEPLAY_PRETIP_SIZE ||
        enforce_revision_fingerprints == 0 ||
        rom_size != PRETIP_REV1_ROM_SIZE ||
        prg_offset != sizeof(pretip_rev1_ines_header) ||
        prg_banks != PRETIP_PRG_BANK_COUNT ||
        memcmp(rom, pretip_rev1_ines_header,
               sizeof(pretip_rev1_ines_header)) != 0 ||
        tecmo_asset_pack_fnv1a32(rom, (size_t)rom_size) !=
            PRETIP_REV1_ROM_FNV1A32 ||
        tecmo_asset_pack_sha256_digest(
            rom, (size_t)rom_size, sha256) != 0 ||
        memcmp(sha256, pretip_rev1_sha256, sizeof(sha256)) != 0) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TPTI-2 import requires the exact Rev1 ROM fingerprint.");
        return -1;
    }
    memset(payload, 0, payload_size);
    memset(provenance, 0, sizeof(*provenance));
    for (index = 0U; index < TECMO_GAMEPLAY_PRETIP_SOURCE_COUNT; ++index) {
        const TecmoGameplayPreTipExpectedSource *expected =
            &tecmo_gameplay_pretip_expected_sources[index];
        uint64_t offset = source_offset(prg_offset, expected);
        uint32_t end =
            (uint32_t)expected->cpu_start + expected->byte_count - 1U;
        uint8_t *record =
            payload + TECMO_ASSET_PACK_GAMEPLAY_PRETIP_SOURCES_OFFSET +
            index * TECMO_ASSET_PACK_GAMEPLAY_PRETIP_SOURCE_STRIDE;
        if (expected->bank >= prg_banks || end > 0xFFFFU ||
            (expected->fixed_bank == 0U && end >= 0xC000U) ||
            !range_ok(offset, expected->byte_count, rom_size) ||
            tecmo_asset_pack_fnv1a32(
                rom + (size_t)offset, expected->byte_count) !=
                    expected->fingerprint_fnv1a32 ||
            fnv1a64(rom + (size_t)offset, expected->byte_count) !=
                    expected->fingerprint_fnv1a64 ||
            !range_ok(expected->payload_offset, expected->byte_count,
                      payload_size)) {
            tecmo_asset_pack_set_messagef(
                message, message_size,
                "TPTI-2 Bank%02u $%04X source fingerprint rejected.",
                (unsigned)expected->bank, (unsigned)expected->cpu_start);
            return -1;
        }
        tecmo_asset_pack_store_u16(record, (uint16_t)expected->kind);
        record[2U] = expected->bank;
        record[3U] = expected->fixed_bank;
        tecmo_asset_pack_store_u16(record + 4U, expected->cpu_start);
        tecmo_asset_pack_store_u16(record + 6U, (uint16_t)end);
        tecmo_asset_pack_store_u32(record + 8U, expected->byte_count);
        tecmo_asset_pack_store_u32(
            record + 12U, expected->fingerprint_fnv1a32);
        store_u64(record + 16U, expected->fingerprint_fnv1a64);
        tecmo_asset_pack_store_u32(
            record + 24U, expected->payload_offset);
        memcpy(payload + expected->payload_offset,
               rom + (size_t)offset, expected->byte_count);
        provenance->source_offsets[index] = offset;
    }
    if (tecmo_asset_pack_fnv1a32(
            payload + TECMO_ASSET_PACK_GAMEPLAY_PRETIP_TIP_INPUT_OFFSET,
            TECMO_ASSET_PACK_GAMEPLAY_PRETIP_TIP_INPUT_SIZE) !=
            TECMO_ASSET_PACK_GAMEPLAY_PRETIP_TIP_INPUT_FNV1A32 ||
        fnv1a64(
            payload + TECMO_ASSET_PACK_GAMEPLAY_PRETIP_TIP_INPUT_OFFSET,
            TECMO_ASSET_PACK_GAMEPLAY_PRETIP_TIP_INPUT_SIZE) !=
            TECMO_ASSET_PACK_GAMEPLAY_PRETIP_TIP_INPUT_FNV1A64) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TPTI-2 Bank05 $985E-$986A exact input subspan rejected.");
        return -1;
    }
    if (memcmp(payload + TECMO_ASSET_PACK_GAMEPLAY_PRETIP_DESCRIPTOR_OFFSET,
               "\x3B\x7D\xEB\x84\x5A\x83\x00", 7U) != 0 ||
        tecmo_asset_pack_decode_d9f6_stream(
            rom + (size_t)(prg_offset +
                0U * TECMO_ASSET_PACK_PRG_BANK_BYTES),
            (size_t)TECMO_ASSET_PACK_PRG_BANK_BYTES,
            0x835AU - 0x8000U,
            payload + TECMO_ASSET_PACK_GAMEPLAY_PRETIP_DECODED_OFFSET,
            PRETIP_DECODED_SIZE, &encoded_size,
            message, message_size) != 0 ||
        encoded_size != PRETIP_ENCODED_SIZE ||
        tecmo_asset_pack_fnv1a32(
            payload + TECMO_ASSET_PACK_GAMEPLAY_PRETIP_DECODED_OFFSET,
            PRETIP_DECODED_SIZE) != 0xDBF66A45U ||
        fnv1a64(
            payload + TECMO_ASSET_PACK_GAMEPLAY_PRETIP_DECODED_OFFSET,
            PRETIP_DECODED_SIZE) != 0xD1B369CF288E21A5ULL) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TPTI-2 screen $15 decode/descriptor contract rejected.");
        return -1;
    }

    if (!source_span_contains(
            TECMO_ASSET_PACK_GAMEPLAY_PRETIP_CLOSEUP_ENTRY_OFFSET, 510U,
            TECMO_ASSET_PACK_GAMEPLAY_PRETIP_CAPTURE_ERROR_OFFSET, 311U) ||
        !source_span_contains(
            TECMO_ASSET_PACK_GAMEPLAY_PRETIP_TIP_SETUP_OFFSET, 52U,
            TECMO_ASSET_PACK_GAMEPLAY_PRETIP_OPPOSING_ACTOR_OFFSET, 49U)) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TPTI-2 declared source containment contract rejected.");
        return -1;
    }

    build_mechanics_block(
        payload + TECMO_ASSET_PACK_GAMEPLAY_PRETIP_MECHANICS_OFFSET,
        rom + (size_t)(prg_offset +
            5U * TECMO_ASSET_PACK_PRG_BANK_BYTES + (0xBDF7U - 0x8000U)));

    memcpy(payload, "TPTI", 4U);
    tecmo_asset_pack_store_u16(
        payload + 4U, TECMO_ASSET_PACK_GAMEPLAY_PRETIP_VERSION);
    tecmo_asset_pack_store_u16(
        payload + 6U, TECMO_ASSET_PACK_GAMEPLAY_PRETIP_HEADER_SIZE);
    tecmo_asset_pack_store_u32(
        payload + 8U, TECMO_ASSET_PACK_GAMEPLAY_PRETIP_SIZE);
    tecmo_asset_pack_store_u16(
        payload + 12U, TECMO_GAMEPLAY_PRETIP_SOURCE_COUNT);
    tecmo_asset_pack_store_u16(
        payload + 14U, TECMO_ASSET_PACK_GAMEPLAY_PRETIP_SOURCE_STRIDE);
    tecmo_asset_pack_store_u32(
        payload + 16U, TECMO_ASSET_PACK_GAMEPLAY_PRETIP_SOURCES_OFFSET);
    payload[20U] = TECMO_GAMEPLAY_PRETIP_SCREEN_ID;
    payload[21U] = 0x3BU;
    payload[22U] = 0x7DU;
    payload[23U] = 8U;
    tecmo_asset_pack_store_u32(payload + 24U, 23416U);
    tecmo_asset_pack_store_u32(payload + 28U, 0x2047CCE0U);
    tecmo_asset_pack_store_u32(payload + 32U, 96372U);
    tecmo_asset_pack_store_u32(payload + 36U, 0x812628F0U);
    tecmo_asset_pack_store_u32(payload + 40U, 36784U);
    tecmo_asset_pack_store_u32(payload + 44U, 0x05C00ECBU);
    tecmo_asset_pack_store_u32(payload + 48U, 262144U);
    tecmo_asset_pack_store_u32(payload + 52U, 0xF6F6E854U);
    store_u64(payload + 56U, 0x96A64F53B240ABB4ULL);
    tecmo_asset_pack_store_u32(payload + 64U, PRETIP_REV1_ROM_SIZE);
    tecmo_asset_pack_store_u32(payload + 68U, PRETIP_REV1_ROM_FNV1A32);
    memcpy(payload + 72U, pretip_rev1_sha256, 32U);
    tecmo_asset_pack_store_u32(payload + 104U, PRETIP_ENCODED_SIZE);
    tecmo_asset_pack_store_u32(payload + 108U, PRETIP_DECODED_SIZE);
    tecmo_asset_pack_store_u32(
        payload + 112U, TECMO_ASSET_PACK_GAMEPLAY_PRETIP_DESCRIPTOR_OFFSET);
    tecmo_asset_pack_store_u32(
        payload + 116U, TECMO_ASSET_PACK_GAMEPLAY_PRETIP_ENCODED_OFFSET);
    tecmo_asset_pack_store_u32(
        payload + 120U, TECMO_ASSET_PACK_GAMEPLAY_PRETIP_DECODED_OFFSET);
    tecmo_asset_pack_store_u32(
        payload + 124U, TECMO_ASSET_PACK_GAMEPLAY_PRETIP_PALETTE_OFFSET);
    tecmo_asset_pack_store_u32(payload + 128U, 16U);
    tecmo_asset_pack_store_u32(payload + 132U, 0x4EB8B3ABU);
    tecmo_asset_pack_store_u32(payload + 136U, 0x3C7D9D61U);
    tecmo_asset_pack_store_u32(payload + 140U, 0xDBF66A45U);
    tecmo_asset_pack_store_u32(payload + 144U, 0xB389D1A4U);
    tecmo_asset_pack_store_u32(
        payload + 148U, TECMO_ASSET_PACK_GAMEPLAY_PRETIP_TWAR_SIZE);
    tecmo_asset_pack_store_u32(
        payload + 152U, TECMO_ASSET_PACK_GAMEPLAY_PRETIP_TWAR_FNV1A32);
    {
        static const uint16_t frames[TECMO_GAMEPLAY_PRETIP_PHASE_COUNT] = {
            61U,121U,61U,208U,30U,120U,60U,30U
        };
        for (index = 0U; index < TECMO_GAMEPLAY_PRETIP_PHASE_COUNT; ++index)
            tecmo_asset_pack_store_u16(payload + 156U + index * 2U,
                                       frames[index]);
    }
    payload[172U] = TECMO_GAMEPLAY_PRETIP_INPUT_MASK;
    payload[173U] = TECMO_GAMEPLAY_PRETIP_NO_SAMPLE_ERROR;
    payload[174U] = TECMO_GAMEPLAY_PRETIP_MAX_SAMPLE_ERROR;
    payload[175U] = TECMO_GAMEPLAY_PRETIP_PHASE_COUNT;
    payload[176U] = 0x82U; /* minimum raw timing seed */
    payload[177U] = 0xC1U; /* maximum raw timing seed */
    payload[TECMO_ASSET_PACK_GAMEPLAY_PRETIP_TIP_AWAY_ACTOR_OFFSET] =
        4U; /* Bank04-selected away tip actor */
    payload[TECMO_ASSET_PACK_GAMEPLAY_PRETIP_TIP_HOME_ACTOR_OFFSET] =
        9U; /* Bank04-selected home tip actor */
    payload[180U] = 0U;
    payload[181U] = 1U;
    payload[182U] = 10U;
    payload[183U] = 0x1AU;
    payload[184U] = 0x1AU; /* TWAR screen */
    payload[185U] = TECMO_GAMEPLAY_PRETIP_GLYPH_COUNT;
    payload[186U] = TECMO_GAMEPLAY_PRETIP_GLYPH_TILE_COUNT;
    payload[187U] = 33U;   /* capture-bounded motion-loop start */
    payload[188U] = 25U;   /* Bank04 $88 motion ceiling */
    payload[189U] = 0U;    /* equality is deferred, never a tie winner */
    payload[190U] = 0xC6U; /* Bank06 card-text background CHR r0 */
    payload[191U] = 0xFAU; /* Bank06 card-text background CHR r1 */

    tecmo_asset_pack_store_u16(
        payload + 192U, TECMO_ASSET_PACK_GAMEPLAY_PRETIP_DEPENDENCY_COUNT);
    tecmo_asset_pack_store_u16(
        payload + 194U, TECMO_GAMEPLAY_PRETIP_TPM2_VERSION);
    tecmo_asset_pack_store_u32(
        payload + TECMO_ASSET_PACK_GAMEPLAY_PRETIP_TPM2_OFFSET_FIELD,
        TECMO_ASSET_PACK_GAMEPLAY_PRETIP_MECHANICS_OFFSET);
    tecmo_asset_pack_store_u32(
        payload + TECMO_ASSET_PACK_GAMEPLAY_PRETIP_TPM2_SIZE_FIELD,
        TECMO_ASSET_PACK_GAMEPLAY_PRETIP_MECHANICS_SIZE);
    tecmo_asset_pack_store_u32(
        payload + TECMO_ASSET_PACK_GAMEPLAY_PRETIP_EXACT_SOURCE_BASE_FIELD,
        TECMO_ASSET_PACK_GAMEPLAY_PRETIP_EXACT_SOURCE_OFFSET);
    tecmo_asset_pack_store_u32(
        payload + TECMO_ASSET_PACK_GAMEPLAY_PRETIP_TGJS_SIZE_OFFSET,
        TECMO_ASSET_PACK_GAMEPLAY_PRETIP_TGJS_SIZE);
    tecmo_asset_pack_store_u32(
        payload + TECMO_ASSET_PACK_GAMEPLAY_PRETIP_TGJS_FNV32_OFFSET,
        TECMO_ASSET_PACK_GAMEPLAY_PRETIP_TGJS_FNV1A32);
    tecmo_asset_pack_store_u16(
        payload + TECMO_ASSET_PACK_GAMEPLAY_PRETIP_TGJS_VERSION_OFFSET,
        TECMO_ASSET_PACK_GAMEPLAY_PRETIP_TGJS_VERSION);
    tecmo_asset_pack_store_u16(
        payload + TECMO_ASSET_PACK_GAMEPLAY_PRETIP_TGJS_HEADER_OFFSET,
        TECMO_ASSET_PACK_GAMEPLAY_PRETIP_TGJS_HEADER_SIZE);
    tecmo_asset_pack_store_u32(
        payload + 232U,
        tecmo_asset_pack_fnv1a32(
            payload + TECMO_ASSET_PACK_GAMEPLAY_PRETIP_MECHANICS_OFFSET,
            TECMO_ASSET_PACK_GAMEPLAY_PRETIP_MECHANICS_SIZE));
    store_u64(
        payload + 236U,
        fnv1a64(
            payload + TECMO_ASSET_PACK_GAMEPLAY_PRETIP_MECHANICS_OFFSET,
            TECMO_ASSET_PACK_GAMEPLAY_PRETIP_MECHANICS_SIZE));

    if (full_payload_fingerprint32(payload, payload_size) !=
            TECMO_ASSET_PACK_GAMEPLAY_PRETIP_FNV1A32 ||
        full_payload_fingerprint64(payload, payload_size) !=
            TECMO_ASSET_PACK_GAMEPLAY_PRETIP_FNV1A64) {
        tecmo_asset_pack_set_messagef(
            message, message_size,
            "TPTI-2 ordinary full-payload fingerprint mismatch "
            "(got %08X/%016llX).",
            full_payload_fingerprint32(payload, payload_size),
            (unsigned long long)full_payload_fingerprint64(payload, payload_size));
        return -1;
    }
    tecmo_asset_pack_set_message(
        message, message_size,
        "Built strict ROM-derived TPTI-2 pre-tip asset.");
    return 0;
}
