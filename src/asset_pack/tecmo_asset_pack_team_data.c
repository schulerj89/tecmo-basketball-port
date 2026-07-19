#include "tecmo_asset_pack_team_data.h"

#include "tecmo_asset_pack_d9f6.h"
#include "tecmo_asset_pack_util.h"

#include <stdbool.h>
#include <string.h>

#define TEAM_DATA_BANK 3U
#define TEAM_DATA_ROSTER_BANK 2U
#define TEAM_DATA_ART_BANK 6U

typedef struct TeamDataSpan {
    uint32_t bank;
    uint32_t cpu;
    size_t size;
    uint32_t fingerprint;
} TeamDataSpan;

static bool range_ok(uint64_t offset, uint64_t size, uint64_t total)
{
    return offset <= total && size <= total - offset;
}

static uint64_t bank_cpu_offset(uint64_t prg_offset,
                                uint32_t bank,
                                uint32_t cpu)
{
    return prg_offset + (uint64_t)bank * TECMO_ASSET_PACK_PRG_BANK_BYTES +
           (uint64_t)(cpu - 0x8000U);
}

static uint64_t fixed_cpu_offset(uint64_t prg_offset,
                                 uint32_t prg_banks,
                                 uint32_t cpu)
{
    return prg_offset + (uint64_t)(prg_banks - 1U) *
                            TECMO_ASSET_PACK_PRG_BANK_BYTES +
           (uint64_t)(cpu - 0xC000U);
}

static int verify_span(const uint8_t *rom,
                       uint64_t rom_size,
                       uint64_t prg_offset,
                       uint32_t prg_banks,
                       const TeamDataSpan *span,
                       int fixed,
                       int enforce,
                       uint64_t *offset_out)
{
    uint64_t offset = fixed != 0
        ? fixed_cpu_offset(prg_offset, prg_banks, span->cpu)
        : bank_cpu_offset(prg_offset, span->bank, span->cpu);
    if (!range_ok(offset, span->size, rom_size)) return -1;
    if (enforce != 0 &&
        tecmo_asset_pack_fnv1a32(rom + (size_t)offset, span->size) !=
            span->fingerprint)
        return -1;
    if (offset_out != NULL) *offset_out = offset;
    return 0;
}

static void store_cell(uint8_t *cell,
                       uint8_t tile,
                       uint8_t palette_index,
                       uint8_t r0,
                       uint8_t r1)
{
    cell[0] = tile;
    cell[1] = palette_index;
    tecmo_asset_pack_store_u32(
        cell + 2U, tecmo_asset_pack_bg_chr_offset(tile, r0, r1));
}

static int copy_length_string(const uint8_t *rom,
                              uint64_t rom_size,
                              uint64_t prg_offset,
                              uint32_t table_cpu,
                              size_t index,
                              uint8_t dest[16],
                              char *message,
                              size_t message_size)
{
    uint64_t pointer_offset = bank_cpu_offset(
        prg_offset, TEAM_DATA_ART_BANK, table_cpu + (uint32_t)index * 2U);
    uint16_t string_cpu;
    uint64_t string_offset;
    uint8_t length;

    if (!range_ok(pointer_offset, 2U, rom_size)) goto reject;
    string_cpu = tecmo_asset_pack_read_u16(rom + (size_t)pointer_offset);
    if (string_cpu < 0x8000U || string_cpu >= 0xC000U) goto reject;
    string_offset = bank_cpu_offset(
        prg_offset, TEAM_DATA_ART_BANK, string_cpu);
    if (!range_ok(string_offset, 1U, rom_size)) goto reject;
    length = rom[(size_t)string_offset];
    if (length == 0U || length > 15U ||
        !range_ok(string_offset + 1U, length, rom_size))
        goto reject;
    memset(dest, 0, 16U);
    for (size_t i = 0U; i < length; ++i) {
        uint8_t c = rom[(size_t)string_offset + 1U + i];
        if (!((c >= 'A' && c <= 'Z') || c == ' ' || c == '-' || c == '.'))
            goto reject;
        dest[i] = c;
    }
    return 0;

reject:
    tecmo_asset_pack_set_messagef(
        message, message_size,
        "TTDT-1 team string table $%04X index %u was rejected.",
        table_cpu, (unsigned)index);
    return -1;
}

static int read_player(const uint8_t *rom,
                       uint64_t rom_size,
                       uint64_t prg_offset,
                       size_t team,
                       size_t player,
                       uint8_t name[20],
                       uint8_t attributes[7],
                       uint16_t *player_cpu_out,
                       char *message,
                       size_t message_size)
{
    uint64_t master_offset = bank_cpu_offset(
        prg_offset, TEAM_DATA_ROSTER_BANK, 0x8001U + (uint32_t)team * 2U);
    uint16_t pointer_table_cpu;
    uint64_t pointer_offset;
    uint16_t player_cpu;
    uint64_t player_offset;
    size_t name_length = 0U;

    if (team >= TECMO_ASSET_PACK_TEAM_DATA_TEAM_COUNT ||
        player >= TECMO_ASSET_PACK_TEAM_DATA_PLAYERS_PER_TEAM ||
        !range_ok(master_offset, 2U, rom_size))
        goto reject;
    pointer_table_cpu = tecmo_asset_pack_read_u16(rom + (size_t)master_offset);
    if (pointer_table_cpu < 0x8000U || pointer_table_cpu >= 0xC000U)
        goto reject;
    pointer_offset = bank_cpu_offset(
        prg_offset, TEAM_DATA_ROSTER_BANK,
        pointer_table_cpu + (uint32_t)player * 2U);
    if (!range_ok(pointer_offset, 2U, rom_size)) goto reject;
    player_cpu = tecmo_asset_pack_read_u16(rom + (size_t)pointer_offset);
    if (player_cpu < 0x8000U || player_cpu >= 0xC000U) goto reject;
    player_offset = bank_cpu_offset(
        prg_offset, TEAM_DATA_ROSTER_BANK, player_cpu);
    if (!range_ok(player_offset, 8U, rom_size)) goto reject;

    memset(name, 0, 20U);
    while (name_length < 20U &&
           range_ok(player_offset + name_length, 1U, rom_size)) {
        uint8_t c = rom[(size_t)player_offset + name_length];
        if (c < 0x20U || c > 0x5AU) break;
        name[name_length++] = c;
    }
    if (name_length == 0U ||
        !range_ok(player_offset + name_length, 7U, rom_size) ||
        rom[(size_t)player_offset + name_length] < 0x80U)
        goto reject;
    memcpy(attributes, rom + (size_t)player_offset + name_length, 7U);
    if (player_cpu_out != NULL) *player_cpu_out = player_cpu;
    return 0;

reject:
    tecmo_asset_pack_set_message(
        message, message_size, "TTDT-1 roster pointer or player record was rejected.");
    return -1;
}

static int find_real_player_source(const uint8_t *rom,
                                   uint64_t rom_size,
                                   uint64_t prg_offset,
                                   uint16_t player_cpu,
                                   uint8_t *source_team,
                                   uint8_t *source_player)
{
    for (size_t team = 0U;
         team < TECMO_ASSET_PACK_TEAM_DATA_REAL_TEAM_COUNT; ++team) {
        uint64_t master_offset = bank_cpu_offset(
            prg_offset, TEAM_DATA_ROSTER_BANK, 0x8001U + (uint32_t)team * 2U);
        uint16_t table_cpu;
        if (!range_ok(master_offset, 2U, rom_size)) return -1;
        table_cpu = tecmo_asset_pack_read_u16(rom + (size_t)master_offset);
        if (table_cpu < 0x8000U || table_cpu >= 0xC000U) return -1;
        for (size_t player = 0U;
             player < TECMO_ASSET_PACK_TEAM_DATA_PLAYERS_PER_TEAM; ++player) {
            uint64_t pointer_offset = bank_cpu_offset(
                prg_offset, TEAM_DATA_ROSTER_BANK,
                table_cpu + (uint32_t)player * 2U);
            if (!range_ok(pointer_offset, 2U, rom_size)) return -1;
            if (tecmo_asset_pack_read_u16(rom + (size_t)pointer_offset) ==
                player_cpu) {
                *source_team = (uint8_t)team;
                *source_player = (uint8_t)player;
                return 0;
            }
        }
    }
    return -1;
}

static int read_profile(const uint8_t *rom,
                        uint64_t rom_size,
                        uint64_t prg_offset,
                        size_t team,
                        size_t player,
                        uint8_t profile[6],
                        char *message,
                        size_t message_size)
{
    uint64_t pointer_offset = bank_cpu_offset(
        prg_offset, TEAM_DATA_ROSTER_BANK, 0x9DC0U + (uint32_t)team * 2U);
    uint16_t profile_cpu;
    uint64_t profile_offset;

    if (team >= TECMO_ASSET_PACK_TEAM_DATA_TEAM_COUNT ||
        player >= TECMO_ASSET_PACK_TEAM_DATA_PLAYERS_PER_TEAM ||
        !range_ok(pointer_offset, 2U, rom_size))
        goto reject;
    profile_cpu = tecmo_asset_pack_read_u16(rom + (size_t)pointer_offset);
    if (profile_cpu < 0x8000U || profile_cpu >= 0xC000U) goto reject;
    profile_offset = bank_cpu_offset(
        prg_offset, TEAM_DATA_ROSTER_BANK,
        profile_cpu + (uint32_t)player * 6U);
    if (!range_ok(profile_offset, 6U, rom_size)) goto reject;
    memcpy(profile, rom + (size_t)profile_offset, 6U);
    return 0;

reject:
    tecmo_asset_pack_set_message(
        message, message_size, "TTDT-1 player profile pointer was rejected.");
    return -1;
}

static int build_cursor(const uint8_t record[5],
                        uint8_t dest[TECMO_ASSET_PACK_TEAM_DATA_CURSOR_STRIDE],
                        char *message,
                        size_t message_size)
{
    uint32_t top;
    if (record[0] != 0x11U) {
        tecmo_asset_pack_set_message(message, message_size,
                                     "TTDT-1 cursor record was rejected.");
        return -1;
    }
    top = (uint32_t)record[3] * 1024U + (uint32_t)record[4] * 16U;
    if (top + 32U > TECMO_ASSET_PACK_TEAM_DATA_CHR_SIZE) {
        tecmo_asset_pack_set_message(message, message_size,
                                     "TTDT-1 cursor CHR range was rejected.");
        return -1;
    }
    tecmo_asset_pack_store_u16(dest, (uint16_t)(int16_t)(int8_t)record[1]);
    tecmo_asset_pack_store_u16(dest + 2U,
                               (uint16_t)(int16_t)(int8_t)record[2]);
    tecmo_asset_pack_store_u32(dest + 4U, top);
    tecmo_asset_pack_store_u32(dest + 8U, top + 16U);
    dest[12U] = record[3];
    dest[13U] = record[4];
    return 0;
}

int tecmo_asset_pack_team_data_self_test(char *message, size_t message_size)
{
    uint8_t cursor[TECMO_ASSET_PACK_TEAM_DATA_CURSOR_STRIDE];
    const uint8_t good[5] = {0x11U, 0xFFU, 0U, 0x30U, 0x24U};
    const uint8_t bad[5] = {0x10U, 0U, 0U, 0U, 0U};
    memset(cursor, 0, sizeof(cursor));
    if (build_cursor(good, cursor, NULL, 0U) != 0 ||
        tecmo_asset_pack_read_u32(cursor + 4U) != 0xC240U ||
        tecmo_asset_pack_read_u32(cursor + 8U) != 0xC250U ||
        build_cursor(bad, cursor, NULL, 0U) == 0) {
        tecmo_asset_pack_set_message(message, message_size,
                                     "TTDT-1 cursor bounds self-test failed.");
        return -1;
    }
    tecmo_asset_pack_set_message(message, message_size,
                                 "TTDT-1 cursor bounds self-test passed.");
    return 0;
}

int tecmo_asset_pack_build_team_data(const uint8_t *rom,
                                     uint64_t rom_size,
                                     uint64_t prg_offset,
                                     uint32_t prg_banks,
                                     uint64_t chr_size,
                                     int enforce_revision_fingerprints,
                                     uint8_t *payload,
                                     size_t payload_size,
                                     TecmoTeamDataProvenance *provenance,
                                     char *message,
                                     size_t message_size)
{
    static const TeamDataSpan bank_spans[] = {
        {3U, 0x81B8U, 14U, 0xDCA1F834U},
        {3U, 0x841AU, 12U, 0x2CD2DE1CU},
        {3U, 0x8BFBU, 56U, 0xB67478D8U},
        {3U, 0x8C33U, 102U, 0xC831DFE1U},
        {3U, 0x8C99U, 6U, 0xE51885D7U},
        {3U, 0x8C9FU, 1462U, 0xB9232256U},
        {3U, 0x9EC6U, 469U, 0x91DC81F3U},
        {3U, 0xA09BU, 451U, 0xA60D4C69U},
        {3U, 0xA1BDU, 5U, 0x547D7A6FU},
        {3U, 0xA1C2U, 81U, 0xA10F8F03U},
        {3U, 0xA23FU, 31U, 0x879A9020U},
        {1U, 0x8031U, 5U, 0x7D5835D4U},
        {2U, 0x8001U, 7022U, 0x93B76340U},
        {2U, 0x9B6FU, 593U, 0x427BE255U},
        {2U, 0x9DC0U, 2146U, 0xECFDCBCBU},
        {3U, 0xA6C4U, 2546U, 0x2CE4F713U},
        {3U, 0xADFDU, 160U, 0x6A9DBB07U},
        {3U, 0xA9A8U, 98U, 0xB43C1AA6U},
        {3U, 0xAFE6U, 128U, 0x3038CF12U},
        {6U, 0xA2E4U, 486U, 0x91339DD9U},
        {0U, 0xBE37U, 16U, 0xF85BA74AU},
        {3U, 0xA25CU, 2U, 0x0AE8D0EAU},
        {3U, 0xB432U, 192U, 0x58566055U},
        {2U, 0xAA89U, 722U, 0xBEFE9E46U},
        {0U, 0x8001U, 108U, 0xA3D60DC1U},
        {0U, 0x8071U, 27U, 0x427070B7U},
        {1U, 0xBF1FU, 13U, 0x555E360CU},
        {6U, 0xA4CFU, 1916U, 0xD27CA55EU},
        {3U, 0x8017U, 29U, 0x6A54CD12U},
        {6U, 0xAC0BU, 64U, 0xDC51B191U},
        {3U, 0x8D5CU, 136U, 0x4866441BU},
        {2U, 0xAD5BU, 32U, 0x8B098364U}
    };
    static const TeamDataSpan fixed_spans[] = {
        {0U, 0xD714U, 212U, 0xBC71E228U},
        {0U, 0xD92BU, 492U, 0xE07A8EB7U},
        {0U, 0xDB25U, 171U, 0xC59694B5U},
        {0U, 0xDCD9U, 35U, 0xF42287FAU},
        {0U, 0xC42EU, 13U, 0xDB6A6AEEU},
        {0U, 0xCAF1U, 4U, 0x2D477AB7U},
        {0U, 0xD5C5U, 147U, 0x24E23095U},
        {0U, 0xDC19U, 29U, 0x1451114FU}
    };
    static const uint32_t screen_banks[3] = {1U, 0U, 0U};
    static const uint32_t screen_cpus[3] = {0xBB55U, 0xB492U, 0x877DU};
    static const size_t screen_sizes[3] = {318U, 135U, 284U};
    static const uint32_t descriptor_fingerprints[3] = {
        0x64B5020CU, 0x5967A0DAU, 0x7CE3E553U
    };
    static const uint32_t stream_fingerprints[3] = {
        0xA90DA6A3U, 0xEA330745U, 0x12CF0CA2U
    };
    static const uint32_t decoded_fingerprints[3] = {
        0x9265F597U, 0xF6D644A6U, 0x69A3DB3EU
    };
    static const uint32_t palette_fingerprints[3] = {
        0x913CE83EU, 0x98634D94U, 0xF49FA2BCU
    };
    static const uint8_t descriptors[3][7] = {
        {0x7DU, 0x7DU, 0x92U, 0xBCU, 0x55U, 0xBBU, 0x01U},
        {0x7DU, 0x7DU, 0xE0U, 0xB5U, 0x92U, 0xB4U, 0x00U},
        {0x66U, 0x7DU, 0x8DU, 0x80U, 0x7DU, 0x87U, 0x00U}
    };
    static const uint8_t selector_x[3] = {0x10U, 0x60U, 0xB0U};
    static const uint8_t division_offsets[5] = {0U, 7U, 14U, 20U, 27U};
    uint64_t offsets[sizeof(bank_spans) / sizeof(bank_spans[0])];
    uint64_t fixed_offsets[sizeof(fixed_spans) / sizeof(fixed_spans[0])];
    uint64_t chr_offset;
    uint8_t decoded[1024];
    size_t encoded_size;

    if (rom == NULL || payload == NULL || provenance == NULL || prg_banks < 8U ||
        chr_size != TECMO_ASSET_PACK_TEAM_DATA_CHR_SIZE ||
        payload_size != TECMO_ASSET_PACK_TEAM_DATA_SIZE) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TTDT-1 import requires the exact Rev1 payload contract.");
        return -1;
    }
    memset(provenance, 0, sizeof(*provenance));
    if ((uint64_t)prg_banks * TECMO_ASSET_PACK_PRG_BANK_BYTES >
        UINT64_MAX - prg_offset) {
        tecmo_asset_pack_set_message(message, message_size,
                                     "TTDT-1 CHR source offset overflowed.");
        return -1;
    }
    chr_offset = prg_offset +
                 (uint64_t)prg_banks * TECMO_ASSET_PACK_PRG_BANK_BYTES;
    if (!range_ok(chr_offset, chr_size, rom_size)) {
        tecmo_asset_pack_set_message(message, message_size,
                                     "TTDT-1 CHR source is outside the ROM.");
        return -1;
    }
    if (enforce_revision_fingerprints != 0 &&
        tecmo_asset_pack_fnv1a32(rom + (size_t)chr_offset, (size_t)chr_size) !=
            TECMO_ASSET_PACK_TEAM_DATA_CHR_FNV1A32) {
        tecmo_asset_pack_set_message(message, message_size,
                                     "TTDT-1 full CHR fingerprint mismatch.");
        return -1;
    }
    for (size_t i = 0U; i < sizeof(bank_spans) / sizeof(bank_spans[0]); ++i) {
        if (verify_span(rom, rom_size, prg_offset, prg_banks, &bank_spans[i], 0,
                        enforce_revision_fingerprints, &offsets[i]) != 0) {
            tecmo_asset_pack_set_messagef(
                message, message_size,
                "TTDT-1 Bank%02u $%04X source fingerprint mismatch (got %08X).",
                bank_spans[i].bank, bank_spans[i].cpu,
                tecmo_asset_pack_fnv1a32(
                    rom + (size_t)bank_cpu_offset(
                        prg_offset, bank_spans[i].bank, bank_spans[i].cpu),
                    bank_spans[i].size));
            return -1;
        }
    }
    for (size_t i = 0U; i < sizeof(fixed_spans) / sizeof(fixed_spans[0]); ++i) {
        if (verify_span(rom, rom_size, prg_offset, prg_banks, &fixed_spans[i], 1,
                        enforce_revision_fingerprints, &fixed_offsets[i]) != 0) {
            tecmo_asset_pack_set_message(
                message, message_size,
                "TTDT-1 fixed-bank source fingerprint mismatch.");
            return -1;
        }
    }

    memset(payload, 0, payload_size);
    memcpy(payload, "TTDT", 4U);
    tecmo_asset_pack_store_u16(payload + 4U, 1U);
    tecmo_asset_pack_store_u16(payload + 6U, TECMO_ASSET_PACK_TEAM_DATA_HEADER_SIZE);
    tecmo_asset_pack_store_u16(payload + 8U, 32U);
    tecmo_asset_pack_store_u16(payload + 10U, 30U);
    tecmo_asset_pack_store_u16(payload + 12U, TECMO_ASSET_PACK_TEAM_DATA_SCREEN_COUNT);
    tecmo_asset_pack_store_u16(payload + 14U, TECMO_ASSET_PACK_TEAM_DATA_SELECTOR_COUNT);
    tecmo_asset_pack_store_u16(payload + 16U, TECMO_ASSET_PACK_TEAM_DATA_TEAM_COUNT);
    tecmo_asset_pack_store_u16(payload + 18U, TECMO_ASSET_PACK_TEAM_DATA_PLAYERS_PER_TEAM);
    tecmo_asset_pack_store_u32(payload + 20U, TECMO_ASSET_PACK_TEAM_DATA_CELLS_OFFSET);
    tecmo_asset_pack_store_u32(payload + 24U, TECMO_ASSET_PACK_TEAM_DATA_PALETTES_OFFSET);
    tecmo_asset_pack_store_u32(payload + 28U, TECMO_ASSET_PACK_TEAM_DATA_SPRITE_PALETTE_OFFSET);
    tecmo_asset_pack_store_u32(payload + 32U, TECMO_ASSET_PACK_TEAM_DATA_CURSORS_OFFSET);
    tecmo_asset_pack_store_u32(payload + 36U, TECMO_ASSET_PACK_TEAM_DATA_SELECTORS_OFFSET);
    tecmo_asset_pack_store_u32(payload + 40U, TECMO_ASSET_PACK_TEAM_DATA_FONT_OFFSET);
    tecmo_asset_pack_store_u32(payload + 44U, TECMO_ASSET_PACK_TEAM_DATA_TEAMS_OFFSET);
    tecmo_asset_pack_store_u32(payload + 48U, TECMO_ASSET_PACK_TEAM_DATA_LOGOS_OFFSET);
    tecmo_asset_pack_store_u32(payload + 52U, TECMO_ASSET_PACK_TEAM_DATA_PLAYERS_OFFSET);
    tecmo_asset_pack_store_u32(payload + 56U, TECMO_ASSET_PACK_TEAM_DATA_SIZE);
    tecmo_asset_pack_store_u32(payload + 60U, TECMO_ASSET_PACK_TEAM_DATA_CHR_SIZE);
    tecmo_asset_pack_store_u32(payload + 64U, TECMO_ASSET_PACK_TEAM_DATA_CHR_FNV1A32);
    payload[68U] = 16U;
    payload[69U] = 5U;
    payload[70U] = 5U;
    payload[71U] = 8U;
    payload[72U] = 32U;
    payload[73U] = 8U;
    payload[74U] = 135U;
    payload[75U] = 80U;
    payload[76U] = 8U;
    payload[77U] = 40U;
    payload[78U] = 143U;
    payload[79U] = 8U;
    payload[80U] = 0xFAU;
    payload[81U] = 0xFAU;
    payload[82U] = 0xCCU;
    payload[83U] = 0xFAU;
    payload[84U] = 0xFAU;
    payload[85U] = 0xFAU;
    payload[86U] = 0x0CU;
    payload[87U] = 0x0DU;
    payload[88U] = 0x0EU;
    payload[89U] = TECMO_ASSET_PACK_TEAM_DATA_FONT_FIRST;
    payload[90U] = TECMO_ASSET_PACK_TEAM_DATA_FONT_COUNT;
    payload[91U] = 3U;
    payload[92U] = 6U;
    payload[93U] = 2U;
    payload[94U] = 6U;
    payload[95U] = 4U;
    payload[96U] = 32U;
    payload[97U] = 32U;
    payload[98U] = 0x67U;
    payload[99U] = 0x5FU;
    payload[100U] = 0x64U;
    payload[101U] = 48U;
    payload[102U] = 8U;
    payload[103U] = 10U;
    payload[104U] = 16U;
    payload[105U] = 19U;
    payload[106U] = 4U;
    payload[107U] = 32U;
    payload[108U] = 8U;
    payload[109U] = 10U;
    payload[110U] = 15U;
    payload[111U] = 18U;
    payload[112U] = 4U;
    payload[113U] = 31U;
    payload[114U] = 4U;
    payload[115U] = 7U;
    payload[116U] = 4U;
    payload[117U] = 20U;

    {
        const uint8_t *palette_pointer_low = rom + (size_t)bank_cpu_offset(
            prg_offset, TEAM_DATA_ART_BANK, 0xA3A5U);
        const uint8_t *palette_pointer_high = rom + (size_t)bank_cpu_offset(
            prg_offset, TEAM_DATA_ART_BANK, 0xA3A9U);
        for (size_t group = 0U;
             group < TECMO_ASSET_PACK_TEAM_DATA_PROFILE_PALETTE_COUNT;
             ++group) {
            uint16_t palette_cpu = (uint16_t)palette_pointer_low[group] |
                (uint16_t)((uint16_t)palette_pointer_high[group] << 8U);
            uint64_t palette_offset = bank_cpu_offset(
                prg_offset, TEAM_DATA_ART_BANK, palette_cpu);
            uint8_t *destination = payload +
                TECMO_ASSET_PACK_TEAM_DATA_PROFILE_PALETTES_HEADER_OFFSET +
                group * 16U;
            if (palette_cpu < 0xAC0BU || palette_cpu > 0xAC3BU ||
                !range_ok(palette_offset, 16U, rom_size))
                return -1;
            for (size_t color = 0U; color < 16U; ++color) {
                destination[color] = rom[(size_t)palette_offset + color];
                if (destination[color] > 0x3FU) return -1;
            }
        }
    }

    for (size_t screen = 0U; screen < 3U; ++screen) {
        uint64_t descriptor_offset = fixed_cpu_offset(
            prg_offset, prg_banks, 0xDCD9U + (uint32_t)screen * 7U);
        uint64_t stream_offset = bank_cpu_offset(
            prg_offset, screen_banks[screen], screen_cpus[screen]);
        uint16_t palette_cpu = (uint16_t)descriptors[screen][2] |
                               (uint16_t)((uint16_t)descriptors[screen][3] << 8U);
        uint64_t palette_offset = bank_cpu_offset(
            prg_offset, screen_banks[screen], palette_cpu);
        uint8_t r0 = (uint8_t)(descriptors[screen][0] * 2U);
        uint8_t r1 = (uint8_t)(descriptors[screen][1] * 2U);
        if (!range_ok(descriptor_offset, 7U, rom_size) ||
            !range_ok(stream_offset, screen_sizes[screen], rom_size) ||
            !range_ok(palette_offset, 16U, rom_size) ||
            memcmp(rom + (size_t)descriptor_offset, descriptors[screen], 7U) != 0 ||
            tecmo_asset_pack_validate_chr_pair(r0, r1, chr_size,
                                               "TEAM DATA screen", message,
                                               message_size) != 0 ||
            tecmo_asset_pack_decode_d9f6_stream(
                rom + (size_t)bank_cpu_offset(prg_offset, screen_banks[screen], 0x8000U),
                TECMO_ASSET_PACK_PRG_BANK_BYTES,
                screen_cpus[screen] - 0x8000U,
                decoded, sizeof(decoded), &encoded_size,
                message, message_size) != 0 ||
            encoded_size != screen_sizes[screen]) {
            tecmo_asset_pack_set_message(
                message, message_size,
                "TTDT-1 screen descriptor, stream, or CHR pair was rejected.");
            return -1;
        }
        if (enforce_revision_fingerprints != 0 &&
            (tecmo_asset_pack_fnv1a32(
                 rom + (size_t)descriptor_offset, 7U) !=
                 descriptor_fingerprints[screen] ||
             tecmo_asset_pack_fnv1a32(
                 rom + (size_t)stream_offset, encoded_size) !=
                 stream_fingerprints[screen] ||
             tecmo_asset_pack_fnv1a32(decoded, sizeof(decoded)) !=
                 decoded_fingerprints[screen] ||
             tecmo_asset_pack_fnv1a32(
                 rom + (size_t)palette_offset, 16U) !=
                 palette_fingerprints[screen])) {
            tecmo_asset_pack_set_message(message, message_size,
                                         "TTDT-1 screen fingerprint mismatch.");
            return -1;
        }
        for (size_t i = 0U; i < 960U; ++i) {
            uint8_t *cell = payload + TECMO_ASSET_PACK_TEAM_DATA_CELLS_OFFSET +
                            (screen * 960U + i) *
                                TECMO_ASSET_PACK_TEAM_DATA_CELL_STRIDE;
            store_cell(cell, decoded[i],
                       tecmo_asset_pack_decoded_palette_index(
                           decoded, (unsigned)(i / 32U), (unsigned)(i % 32U)),
                       r0, r1);
        }
        for (size_t i = 0U; i < 16U; ++i) {
            uint8_t color = rom[(size_t)palette_offset + i];
            if (color > 0x3FU) {
                tecmo_asset_pack_set_message(message, message_size,
                                             "TTDT-1 palette color was rejected.");
                return -1;
            }
            payload[TECMO_ASSET_PACK_TEAM_DATA_PALETTES_OFFSET +
                    screen * 16U + i] = color;
        }
        provenance->descriptor_offsets[screen] = descriptor_offset;
        provenance->stream_offsets[screen] = stream_offset;
        provenance->stream_sizes[screen] = encoded_size;
        provenance->palette_offsets[screen] = palette_offset;
    }

    {
        uint64_t sprite_palette_offset = bank_cpu_offset(
            prg_offset, 0U, 0xBE37U);
        if (!range_ok(sprite_palette_offset, 16U, rom_size)) return -1;
        for (size_t i = 0U; i < 16U; ++i) {
            uint8_t color = rom[(size_t)sprite_palette_offset + i];
            if (color > 0x3FU) return -1;
            payload[TECMO_ASSET_PACK_TEAM_DATA_SPRITE_PALETTE_OFFSET + i] = color;
        }
        if (build_cursor(
                rom + (size_t)bank_cpu_offset(prg_offset, 3U, 0xA1BDU),
                payload + TECMO_ASSET_PACK_TEAM_DATA_CURSORS_OFFSET,
                message, message_size) != 0 ||
            build_cursor(
                rom + (size_t)bank_cpu_offset(prg_offset, 1U, 0x8031U),
                payload + TECMO_ASSET_PACK_TEAM_DATA_CURSORS_OFFSET +
                    TECMO_ASSET_PACK_TEAM_DATA_CURSOR_STRIDE,
                message, message_size) != 0)
            return -1;
    }

    {
        const uint8_t *map = rom + (size_t)bank_cpu_offset(
            prg_offset, TEAM_DATA_BANK, 0xA23FU);
        size_t selector = 0U;
        for (size_t column = 0U; column < 3U; ++column) {
            size_t count = column == 0U ? 11U : 9U;
            for (size_t row = 0U; row < count; ++row, ++selector) {
                uint8_t *record = payload + TECMO_ASSET_PACK_TEAM_DATA_SELECTORS_OFFSET +
                                  selector * TECMO_ASSET_PACK_TEAM_DATA_SELECTOR_STRIDE;
                record[0] = selector_x[column];
                record[1] = row < (column == 0U ? 2U : 0U)
                    ? (uint8_t)(0x20U + row * 0x10U)
                    : (uint8_t)(0x48U +
                        (row - (column == 0U ? 2U : 0U)) * 0x10U);
                record[2] = map[selector];
                if (record[2] >= TECMO_ASSET_PACK_TEAM_DATA_TEAM_COUNT)
                    return -1;
            }
        }
        if (selector != TECMO_ASSET_PACK_TEAM_DATA_SELECTOR_COUNT ||
            map[0] != 28U || map[1] != 27U) {
            tecmo_asset_pack_set_message(message, message_size,
                                         "TTDT-1 selector map was rejected.");
            return -1;
        }
    }

    {
        const uint8_t *font = rom + (size_t)bank_cpu_offset(
            prg_offset, TEAM_DATA_BANK, 0x9AC8U);
        for (size_t i = 0U; i < TECMO_ASSET_PACK_TEAM_DATA_FONT_COUNT; ++i) {
            uint8_t *record = payload + TECMO_ASSET_PACK_TEAM_DATA_FONT_OFFSET +
                              i * TECMO_ASSET_PACK_TEAM_DATA_FONT_STRIDE;
            uint8_t tile = font[i];
            record[0] = (uint8_t)(TECMO_ASSET_PACK_TEAM_DATA_FONT_FIRST + i);
            record[1] = tile;
            record[2] = 0xFAU;
            record[3] = 0xFAU;
            tecmo_asset_pack_store_u32(
                record + 4U,
                tecmo_asset_pack_bg_chr_offset(tile, 0xFAU, 0xFAU));
        }
    }

    {
        uint8_t team_division[27];
        uint8_t team_conference[27];
        const uint8_t *division_teams = rom + (size_t)bank_cpu_offset(
            prg_offset, TEAM_DATA_BANK, 0xB287U);
        const uint8_t *dims = rom + (size_t)bank_cpu_offset(
            prg_offset, TEAM_DATA_ART_BANK, 0xA3CAU);
        const uint8_t *logo_palettes = rom + (size_t)bank_cpu_offset(
            prg_offset, TEAM_DATA_ART_BANK, 0xA3E7U);
        const uint8_t *selectors = rom + (size_t)bank_cpu_offset(
            prg_offset, TEAM_DATA_ART_BANK, 0xA478U);
        const uint8_t *layout_pointers = rom + (size_t)bank_cpu_offset(
            prg_offset, TEAM_DATA_ART_BANK, 0xA495U);
        const uint8_t *tile_table_pointers = rom + (size_t)bank_cpu_offset(
            prg_offset, TEAM_DATA_ART_BANK, 0xA404U);
        const uint8_t *attribute_table_pointers = rom + (size_t)bank_cpu_offset(
            prg_offset, TEAM_DATA_ART_BANK, 0xA43EU);
        const uint8_t *origins = rom + (size_t)offsets[28];
        const uint8_t *profile_palette_groups = rom + (size_t)bank_cpu_offset(
            prg_offset, TEAM_DATA_ART_BANK, 0xA3ADU);
        memset(team_division, 0xFF, sizeof(team_division));
        memset(team_conference, 0xFF, sizeof(team_conference));
        for (size_t division = 0U; division < 4U; ++division) {
            for (size_t slot = division_offsets[division];
                 slot < division_offsets[division + 1U]; ++slot) {
                uint8_t team = division_teams[slot];
                if (team >= 27U || team_division[team] != 0xFFU) return -1;
                team_division[team] = (uint8_t)division;
                team_conference[team] = division < 2U ? 0U : 1U;
            }
        }
        for (size_t team = 0U; team < TECMO_ASSET_PACK_TEAM_DATA_TEAM_COUNT; ++team) {
            uint8_t *record = payload + TECMO_ASSET_PACK_TEAM_DATA_TEAMS_OFFSET +
                              team * TECMO_ASSET_PACK_TEAM_DATA_TEAM_STRIDE;
            if (copy_length_string(rom, rom_size, prg_offset, 0xAECBU, team,
                                   record, message, message_size) != 0 ||
                copy_length_string(rom, rom_size, prg_offset, 0xAD60U, team,
                                   record + 16U, message, message_size) != 0)
                return -1;
            if (profile_palette_groups[team] >=
                    TECMO_ASSET_PACK_TEAM_DATA_PROFILE_PALETTE_COUNT ||
                origins[team] < 1U || origins[team] > 2U)
                return -1;
            record[39U] = (uint8_t)((profile_palette_groups[team] << 4U) |
                                    origins[team]);
            if (team < 27U) {
                uint8_t source_width = (uint8_t)(dims[team] & 0x0FU);
                uint8_t source_height = (uint8_t)(dims[team] >> 4U);
                uint8_t width = (uint8_t)(source_width * 2U);
                uint8_t height = (uint8_t)(source_height * 2U);
                uint8_t raw_selector = selectors[team];
                uint8_t r0 = (uint8_t)((raw_selector & 0x7FU) * 2U);
                uint8_t r1 = 0xFAU;
                uint16_t layout_cpu = tecmo_asset_pack_read_u16(
                    layout_pointers + team * 2U);
                uint16_t tile_table_cpu = tecmo_asset_pack_read_u16(
                    tile_table_pointers + team * 2U);
                uint16_t attribute_table_cpu = tecmo_asset_pack_read_u16(
                    attribute_table_pointers + team * 2U);
                uint64_t layout_offset;
                uint64_t tile_table_offset;
                uint64_t attribute_table_offset;
                size_t source_count = (size_t)source_width * source_height;
                size_t count = (size_t)width * height;
                if (team_division[team] == 0xFFU || width == 0U || height == 0U ||
                    count > TECMO_ASSET_PACK_TEAM_DATA_LOGO_CELL_LIMIT ||
                    layout_cpu < 0x8000U || layout_cpu >= 0xC000U ||
                    tile_table_cpu < 0x8000U || tile_table_cpu >= 0xC000U ||
                    attribute_table_cpu < 0x8000U ||
                    attribute_table_cpu >= 0xC000U)
                    return -1;
                layout_offset = bank_cpu_offset(
                    prg_offset, TEAM_DATA_ART_BANK, layout_cpu);
                tile_table_offset = bank_cpu_offset(
                    prg_offset, TEAM_DATA_ART_BANK, tile_table_cpu);
                attribute_table_offset = bank_cpu_offset(
                    prg_offset, TEAM_DATA_ART_BANK, attribute_table_cpu);
                if (!range_ok(layout_offset, source_count, rom_size) ||
                    tecmo_asset_pack_validate_chr_pair(
                        r0, r1, chr_size, "TEAM DATA logo", message,
                        message_size) != 0 ||
                    logo_palettes[team] > 3U)
                    return -1;
                record[32U] = team_conference[team];
                record[33U] = team_division[team];
                record[34U] = width;
                record[35U] = height;
                record[36U] = r0;
                record[37U] = (uint8_t)(raw_selector & 0x80U);
                record[38U] = (uint8_t)count;
                for (size_t source_cell = 0U; source_cell < source_count;
                     ++source_cell) {
                    uint8_t metatile = rom[(size_t)layout_offset + source_cell];
                    uint64_t metatile_offset = tile_table_offset +
                                               (uint64_t)metatile * 4U;
                    uint64_t attribute_offset = attribute_table_offset + metatile;
                    size_t source_col = source_cell % source_width;
                    size_t source_row = source_cell / source_width;
                    uint8_t palette;
                    if (!range_ok(metatile_offset, 4U, rom_size) ||
                        !range_ok(attribute_offset, 1U, rom_size))
                        return -1;
                    palette = (uint8_t)((rom[(size_t)attribute_offset] & 0x0CU) >> 2U);
                    for (size_t quadrant = 0U; quadrant < 4U; ++quadrant) {
                        size_t output_col = source_col * 2U + (quadrant & 1U);
                        size_t output_row = source_row * 2U + (quadrant >> 1U);
                        size_t output_cell = output_row * width + output_col;
                        uint8_t tile = (uint8_t)(
                            rom[(size_t)metatile_offset + quadrant] +
                            (raw_selector & 0x80U));
                        uint8_t *logo = payload +
                            TECMO_ASSET_PACK_TEAM_DATA_LOGOS_OFFSET +
                            (team * TECMO_ASSET_PACK_TEAM_DATA_LOGO_CELL_LIMIT +
                             output_cell) *
                                TECMO_ASSET_PACK_TEAM_DATA_LOGO_CELL_STRIDE;
                        logo[0U] = tile;
                        logo[1U] = palette;
                        tecmo_asset_pack_store_u32(
                            logo + 4U,
                            tecmo_asset_pack_bg_chr_offset(tile, r0, r1));
                    }
                }
            } else {
                record[32U] = team == 28U ? 0U : 1U;
                record[33U] = 0xFFU;
            }
        }
    }

    for (size_t team = 0U; team < TECMO_ASSET_PACK_TEAM_DATA_TEAM_COUNT; ++team) {
        for (size_t player = 0U;
             player < TECMO_ASSET_PACK_TEAM_DATA_PLAYERS_PER_TEAM; ++player) {
            uint8_t *record = payload + TECMO_ASSET_PACK_TEAM_DATA_PLAYERS_OFFSET +
                              (team * TECMO_ASSET_PACK_TEAM_DATA_PLAYERS_PER_TEAM + player) *
                                  TECMO_ASSET_PACK_TEAM_DATA_PLAYER_STRIDE;
            uint8_t attributes[7];
            uint8_t profile[6];
            uint8_t source_team = (uint8_t)team;
            uint8_t source_player = (uint8_t)player;
            uint16_t player_cpu;
            uint8_t portrait_index;
            uint8_t r0;
            uint8_t r1;
            const uint8_t *portrait_layouts = rom + (size_t)offsets[22];
            const uint8_t *metatile_tiles = rom + (size_t)offsets[24];
            const uint8_t *metatile_attributes = rom + (size_t)offsets[25];
            if (read_player(rom, rom_size, prg_offset, team, player,
                            record, attributes, &player_cpu,
                            message, message_size) != 0 ||
                read_profile(rom, rom_size, prg_offset, team, player, profile,
                             message, message_size) != 0)
                return -1;
            if (team >= TECMO_ASSET_PACK_TEAM_DATA_REAL_TEAM_COUNT &&
                find_real_player_source(rom, rom_size, prg_offset, player_cpu,
                                        &source_team, &source_player) != 0) {
                tecmo_asset_pack_set_message(
                    message, message_size,
                    "TTDT-1 all-star player pointer did not map to a real roster slot.");
                return -1;
            }
            memcpy(record + 20U, attributes, 7U);
            memcpy(record + 27U, profile, 6U);
            record[33U] = source_team;
            record[34U] = source_player;
            portrait_index = (uint8_t)((profile[5] >> 4U) +
                                       ((profile[2] & 0x80U) >> 3U));
            if (portrait_index >= 32U) return -1;
            r0 = 0xCCU;
            r1 = 0xFAU;
            if (tecmo_asset_pack_validate_chr_pair(
                    r0, r1, chr_size, "TEAM DATA portrait", message,
                    message_size) != 0)
                return -1;
            record[35U] = r0;
            record[36U] = r1;
            record[37U] = rom[(size_t)offsets[26] + 3U];
            if (record[37U] != 0x64U) return -1;
            for (size_t metatile_slot = 0U; metatile_slot < 6U;
                 ++metatile_slot) {
                size_t metatile_col = metatile_slot % 3U;
                size_t metatile_row = metatile_slot / 3U;
                uint8_t metatile = portrait_layouts[
                    (size_t)portrait_index * 6U + metatile_slot];
                uint8_t attribute;
                if (metatile >= 27U) return -1;
                attribute = metatile_attributes[metatile];
                for (size_t quadrant = 0U; quadrant < 4U; ++quadrant) {
                    size_t output_col = metatile_col * 2U + (quadrant & 1U);
                    size_t output_row = metatile_row * 2U + (quadrant >> 1U);
                    size_t output_cell = output_row * 6U + output_col;
                    uint8_t tile = metatile_tiles[(size_t)metatile * 4U + quadrant];
                    uint8_t palette = (uint8_t)((attribute & 0x0CU) >> 2U);
                    store_cell(
                        record + TECMO_ASSET_PACK_TEAM_DATA_PLAYER_PORTRAIT_OFFSET +
                            output_cell * TECMO_ASSET_PACK_TEAM_DATA_CELL_STRIDE,
                        tile, palette, r0, r1);
                }
            }
        }
    }

    provenance->chr_offset = chr_offset;
    provenance->root_vector_offset = offsets[0];
    provenance->season_vector_offset = offsets[1];
    provenance->entry_return_offset = offsets[2];
    provenance->core_flow_offset = offsets[3];
    provenance->route_vector_offset = offsets[4];
    provenance->team_data_flow_offset = offsets[5];
    provenance->generic_input_offset = offsets[6];
    provenance->selector_flow_offset = offsets[7];
    provenance->selector_cursor_offset = offsets[8];
    provenance->generic_cursor_offset = offsets[11];
    provenance->roster_data_offset = offsets[12];
    provenance->profile_data_offset = offsets[14];
    provenance->team_string_offset = bank_cpu_offset(
        prg_offset, TEAM_DATA_ART_BANK, 0xAC4BU);
    provenance->logo_layout_offset = offsets[19];
    provenance->profile_palette_offset = offsets[29];
    provenance->portrait_selector_offset = offsets[21];
    provenance->portrait_layout_offset = offsets[22];
    provenance->portrait_flow_offset = offsets[30];
    provenance->profile_detail_flow_offset = offsets[23];
    provenance->meter_flow_offset = offsets[31];
    provenance->metatile_tiles_offset = offsets[24];
    provenance->metatile_attributes_offset = offsets[25];
    provenance->condition_seed_offset = offsets[26];
    provenance->logo_expansion_offset = offsets[27];
    provenance->logo_origin_offset = offsets[28];
    provenance->sprite_palette_offset = offsets[20];
    provenance->fixed_input_offset = fixed_offsets[0];
    provenance->fixed_loader_offset = fixed_offsets[1];
    provenance->fixed_fade_offset = fixed_offsets[2];
    provenance->descriptor_table_offset = fixed_offsets[3];
    provenance->fixed_metatile_tiles_offset = fixed_offsets[4];
    provenance->fixed_metatile_attribute_offset = fixed_offsets[5];
    provenance->fixed_compositor_offset = fixed_offsets[6];
    provenance->fixed_portrait_selector_offset = fixed_offsets[7];

    if (tecmo_asset_pack_fnv1a32(payload, payload_size) !=
        TECMO_ASSET_PACK_TEAM_DATA_FNV1A32) {
        tecmo_asset_pack_set_messagef(
            message, message_size,
            "TTDT-1 canonical payload fingerprint mismatch (got %08X, expected %08X).",
            tecmo_asset_pack_fnv1a32(payload, payload_size),
            TECMO_ASSET_PACK_TEAM_DATA_FNV1A32);
        return -1;
    }
    tecmo_asset_pack_set_message(message, message_size,
                                 "Built strict TTDT-1 TEAM DATA payload from Rev1 ROM data.");
    return 0;
}
