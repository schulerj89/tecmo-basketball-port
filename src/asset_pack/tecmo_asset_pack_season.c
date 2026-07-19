#include "tecmo_asset_pack_season.h"

#include "tecmo_asset_pack_d9f6.h"
#include "tecmo_asset_pack_util.h"

#include <stdbool.h>
#include <string.h>

#define SEASON_BANK 3U
#define SEASON_ART_BANK 0U
#define SEASON_DEFAULTS_BANK 1U

typedef struct SeasonSpan {
    uint32_t bank;
    uint32_t cpu;
    size_t size;
    uint32_t fingerprint;
    bool fixed;
} SeasonSpan;

typedef struct SeasonScreenSource {
    uint8_t id;
    uint8_t descriptor[7];
    uint16_t decoded_size;
    uint16_t encoded_size;
    uint32_t descriptor_fingerprint;
    uint32_t stream_fingerprint;
    uint32_t decoded_fingerprint;
    uint32_t palette_fingerprint;
} SeasonScreenSource;

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

static uint64_t span_offset(uint64_t prg_offset,
                            uint32_t prg_banks,
                            const SeasonSpan *span)
{
    return span->fixed
        ? fixed_cpu_offset(prg_offset, prg_banks, span->cpu)
        : bank_cpu_offset(prg_offset, span->bank, span->cpu);
}

static int verify_span(const uint8_t *rom,
                       uint64_t rom_size,
                       uint64_t prg_offset,
                       uint32_t prg_banks,
                       const SeasonSpan *span,
                       int enforce,
                       uint64_t *offset_out,
                       char *message,
                       size_t message_size)
{
    uint64_t offset = span_offset(prg_offset, prg_banks, span);
    uint32_t got;
    if (!range_ok(offset, span->size, rom_size)) {
        tecmo_asset_pack_set_message(message, message_size,
                                     "TSNS-1 source span is outside the ROM.");
        return -1;
    }
    got = tecmo_asset_pack_fnv1a32(rom + (size_t)offset, span->size);
    if (enforce != 0 && got != span->fingerprint) {
        tecmo_asset_pack_set_messagef(
            message, message_size,
            "TSNS-1 %s $%04X/%u fingerprint mismatch (got %08X, expected %08X).",
            span->fixed ? "fixed" : "banked", span->cpu,
            (unsigned)span->size, got, span->fingerprint);
        return -1;
    }
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

static int build_cursor(const uint8_t source[5], uint8_t destination[16])
{
    uint32_t top;
    if (source[0] != 0x11U || source[3] != 0x30U || source[4] != 0x24U)
        return -1;
    top = (uint32_t)source[3] * 1024U + (uint32_t)source[4] * 16U;
    if (top + 32U > TECMO_ASSET_PACK_SEASON_CHR_SIZE) return -1;
    tecmo_asset_pack_store_u16(
        destination, (uint16_t)(int16_t)(int8_t)source[1]);
    tecmo_asset_pack_store_u16(
        destination + 2U, (uint16_t)(int16_t)(int8_t)source[2]);
    tecmo_asset_pack_store_u32(destination + 4U, top);
    tecmo_asset_pack_store_u32(destination + 8U, top + 16U);
    destination[12U] = source[3];
    destination[13U] = source[4];
    return 0;
}

static int import_leader_labels(const uint8_t *rom,
                                uint64_t rom_size,
                                uint64_t prg_offset,
                                uint8_t *payload,
                                char *message,
                                size_t message_size)
{
    static const char *expected[TECMO_ASSET_PACK_SEASON_LEADER_COUNT] = {
        "FIELD GOALS", "BLOCKED SHOTS", "REBOUNDS", "TOTAL POINTS",
        "STEALS", "3 POINT SHOTS", "FREE THROWS"
    };
    uint64_t pointers_offset = bank_cpu_offset(
        prg_offset, SEASON_ART_BANK, 0xB348U);
    if (!range_ok(pointers_offset, 14U, rom_size)) return -1;
    for (size_t label = 0U;
         label < TECMO_ASSET_PACK_SEASON_LEADER_COUNT;
         ++label) {
        uint16_t cpu = tecmo_asset_pack_read_u16(
            rom + (size_t)pointers_offset + label * 2U);
        uint64_t offset;
        char value[TECMO_ASSET_PACK_SEASON_LEADER_LABEL_SIZE];
        size_t length = 0U;
        size_t first = 0U;
        size_t last;
        if (cpu < 0xB356U || cpu > 0xB3D7U) goto reject;
        offset = bank_cpu_offset(prg_offset, SEASON_ART_BANK, cpu);
        memset(value, 0, sizeof(value));
        for (size_t i = 0U; i < 48U; ++i) {
            uint8_t c;
            if (!range_ok(offset + i, 1U, rom_size)) goto reject;
            c = rom[(size_t)offset + i];
            if (c == 0x24U) break;
            if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                c == ' ') {
                if (length + 1U >= sizeof(value)) goto reject;
                value[length++] = (char)c;
            }
        }
        while (first < length && value[first] == ' ') ++first;
        last = length;
        while (last > first && value[last - 1U] == ' ') --last;
        memmove(value, value + first, last - first);
        value[last - first] = '\0';
        if (strcmp(value, expected[label]) != 0) goto reject;
        memcpy(payload + TECMO_ASSET_PACK_SEASON_LEADERS_OFFSET +
                   label * TECMO_ASSET_PACK_SEASON_LEADER_LABEL_SIZE,
               value, strlen(value) + 1U);
    }
    return 0;

reject:
    tecmo_asset_pack_set_message(message, message_size,
                                 "TSNS-1 leader label source was rejected.");
    return -1;
}

static int import_menu_record(const uint8_t *source,
                              size_t source_size,
                              const uint8_t expected_header[4],
                              const char *const *expected_segments,
                              size_t segment_count,
                              uint8_t *destination,
                              size_t destination_stride,
                              char *message,
                              size_t message_size)
{
    size_t source_index = 4U;
    if (source == NULL || expected_header == NULL ||
        expected_segments == NULL || destination == NULL ||
        source_size < 5U || destination_stride == 0U ||
        memcmp(source, expected_header, 4U) != 0) {
        goto reject;
    }
    for (size_t segment = 0U; segment < segment_count; ++segment) {
        size_t destination_index = 0U;
        while (source_index < source_size && source[source_index] != 0xFFU &&
               !(source_index + 1U < source_size &&
                 source[source_index] == 0x26U &&
                 source[source_index + 1U] == 0x26U)) {
            uint8_t c = source[source_index++];
            if (c < 0x20U || c > 0x7EU ||
                destination_index + 1U >= destination_stride)
                goto reject;
            destination[segment * destination_stride + destination_index++] = c;
        }
        if (strcmp((const char *)destination + segment * destination_stride,
                   expected_segments[segment]) != 0)
            goto reject;
        if (segment + 1U < segment_count) {
            if (source_index + 1U >= source_size ||
                source[source_index] != 0x26U ||
                source[source_index + 1U] != 0x26U)
                goto reject;
            source_index += 2U;
            if (source_index < source_size && source[source_index] == 0x20U)
                ++source_index;
        } else if (source_index >= source_size ||
                   source[source_index++] != 0xFFU) {
            goto reject;
        }
    }
    if (source_index != source_size) goto reject;
    return 0;

reject:
    tecmo_asset_pack_set_message(message, message_size,
                                 "TSNS-1 menu text record was rejected.");
    return -1;
}

int tecmo_asset_pack_season_self_test(char *message, size_t message_size)
{
    const uint8_t cursor_source[5] = {0x11U, 0x00U, 0xFCU, 0x30U, 0x24U};
    uint8_t cursor[16] = {0};
    if (TECMO_ASSET_PACK_SEASON_SIZE != 101708U ||
        TECMO_ASSET_PACK_SEASON_SCHEDULE_COUNT != 27U * 82U / 2U ||
        build_cursor(cursor_source, cursor) != 0 ||
        tecmo_asset_pack_read_u32(cursor + 4U) != 0xC240U ||
        tecmo_asset_pack_read_u32(cursor + 8U) != 0xC250U) {
        tecmo_asset_pack_set_message(message, message_size,
                                     "TSNS-1 importer layout self-test failed.");
        return -1;
    }
    tecmo_asset_pack_set_message(message, message_size,
                                 "TSNS-1 importer layout self-test passed.");
    return 0;
}

int tecmo_asset_pack_build_season_menu(const uint8_t *rom,
                                       uint64_t rom_size,
                                       uint64_t prg_offset,
                                       uint32_t prg_banks,
                                       uint64_t chr_size,
                                       int enforce_revision_fingerprints,
                                       uint8_t *payload,
                                       size_t payload_size,
                                       TecmoSeasonMenuProvenance *provenance,
                                       char *message,
                                       size_t message_size)
{
    static const SeasonSpan spans[] = {
        {3U, 0x83B3U, 115U, 0xFAA5DF69U, false},
        {3U, 0x841AU, 12U, 0x2CD2DE1CU, false},
        {3U, 0x8426U, 12U, 0x5FF8449FU, false},
        {3U, 0x8514U, 133U, 0x3A0F404FU, false},
        {3U, 0x8599U, 63U, 0xE8D5B0ACU, false},
        {3U, 0xB27FU, 4U, 0x8145527EU, false},
        {3U, 0x891FU, 192U, 0x8289F925U, false},
        {3U, 0x8A15U, 510U, 0x8AA17312U, false},
        {3U, 0x93A7U, 467U, 0xED427D23U, false},
        {3U, 0x957AU, 904U, 0xA0B59AF2U, false},
        {3U, 0x9E00U, 198U, 0x693C1CB1U, false},
        {3U, 0x9AC8U, 59U, 0x286D27BBU, false},
        {3U, 0xB52FU, 2214U, 0x24112737U, false},
        {1U, 0xBED0U, 79U, 0xFF4433C8U, false},
        {1U, 0x8031U, 5U, 0x7D5835D4U, false},
        {0U, 0xBE37U, 16U, 0xF85BA74AU, false},
        {0U, 0xB348U, 165U, 0x7CFF9EFBU, false},
        {0U, 0xD714U, 212U, 0xBC71E228U, true},
        {0U, 0xD92BU, 492U, 0xE07A8EB7U, true},
        {0U, 0xDCFCU, 35U, 0x76CD31FFU, true},
        {3U, 0xB283U, 4U, 0x3B420652U, false},
        {3U, 0xB287U, 27U, 0xB33FB72AU, false},
        {0U, 0xAD3DU, 28U, 0xEB6AA6B8U, false},
        {0U, 0xAE15U, 7U, 0x0FB6BBEDU, false},
        {0U, 0xACB2U, 362U, 0x9C715947U, false},
        {0U, 0xB0CCU, 180U, 0xFFADC10AU, false},
        {0U, 0xB430U, 128U, 0xAC47E9DBU, false}
    };
    static const SeasonScreenSource screens[TECMO_ASSET_PACK_SEASON_SCREEN_COUNT] = {
        {17U, {0x7DU,0x7DU,0xEBU,0x84U,0x9DU,0x80U,0x00U},
         2048U,702U,0x0D624597U,0xCE7BD109U,0x062E4176U,0xB389D1A4U},
        {18U, {0x6DU,0x7DU,0x1BU,0xBAU,0xDAU,0xB8U,0x02U},
         1024U,322U,0x23D90656U,0x4C78CE45U,0x810099C6U,0x7F07658BU},
        {19U, {0x7DU,0x7DU,0xEBU,0x84U,0x65U,0x84U,0x00U},
         2048U,135U,0x4EA1E05BU,0x1298E5F8U,0xABA7EEEEU,0xB389D1A4U},
        {20U, {0x3BU,0x7DU,0xEBU,0x84U,0x71U,0x83U,0x00U},
         1024U,245U,0x9373DC72U,0x33EB31D7U,0x8DD03F9FU,0xB389D1A4U},
        {21U, {0x3BU,0x7DU,0xEBU,0x84U,0x5AU,0x83U,0x00U},
         2048U,24U,0x4EB8B3ABU,0x3C7D9D61U,0xDBF66A45U,0xB389D1A4U}
    };
    static const SeasonScreenSource leader_screens
        [TECMO_ASSET_PACK_SEASON_LEADER_SCREEN_COUNT] = {
        {38U, {0x7DU,0x7DU,0xE0U,0xBDU,0xA2U,0xBCU,0x01U},
         1024U,319U,0xF981E49DU,0x1CE88C44U,0x15DCE7D6U,0x14D2E107U},
        {39U, {0x7DU,0x17U,0xCCU,0xABU,0x86U,0xAAU,0x00U},
         1024U,51U,0x0E226C60U,0x8A23D17DU,0x7CB8DDAAU,0x72B80255U},
        {40U, {0x7DU,0x17U,0xCCU,0xABU,0xB8U,0xAAU,0x00U},
         1024U,51U,0x6372358AU,0x79815D52U,0x598C37D2U,0x72B80255U},
        {41U, {0x7DU,0x17U,0xDCU,0xABU,0xEAU,0xAAU,0x00U},
         1024U,55U,0x71A6D7FCU,0xAC3F7A71U,0xC601FF0AU,0x4D920E47U},
        {42U, {0x7DU,0x17U,0xCCU,0xABU,0x20U,0xABU,0x00U},
         1024U,65U,0x24D27A4BU,0xC33278ADU,0x20B9E9DCU,0x72B80255U},
        {43U, {0x7DU,0x17U,0xDCU,0xABU,0x60U,0xABU,0x00U},
         1024U,55U,0xD59A23BBU,0x206FC3A7U,0x4E7A5262U,0x4D920E47U},
        {44U, {0x7DU,0x17U,0xCCU,0xABU,0x96U,0xABU,0x00U},
         1024U,55U,0xA1F18139U,0xE3851387U,0x4BC8AF56U,0x72B80255U}
    };
    uint64_t offsets[sizeof(spans) / sizeof(spans[0])];
    uint64_t chr_offset;
    uint8_t decoded[2048];

    if (rom == NULL || payload == NULL || provenance == NULL || prg_banks < 8U ||
        chr_size != TECMO_ASSET_PACK_SEASON_CHR_SIZE ||
        payload_size != TECMO_ASSET_PACK_SEASON_SIZE) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TSNS-1 import requires the exact Rev1 payload contract.");
        return -1;
    }
    memset(provenance, 0, sizeof(*provenance));
    chr_offset = prg_offset +
                 (uint64_t)prg_banks * TECMO_ASSET_PACK_PRG_BANK_BYTES;
    if (!range_ok(chr_offset, chr_size, rom_size) ||
        (enforce_revision_fingerprints != 0 &&
         tecmo_asset_pack_fnv1a32(rom + (size_t)chr_offset,
                                  (size_t)chr_size) !=
             TECMO_ASSET_PACK_SEASON_CHR_FNV1A32)) {
        tecmo_asset_pack_set_message(message, message_size,
                                     "TSNS-1 full CHR contract was rejected.");
        return -1;
    }
    for (size_t i = 0U; i < sizeof(spans) / sizeof(spans[0]); ++i)
        if (verify_span(rom, rom_size, prg_offset, prg_banks, &spans[i],
                        enforce_revision_fingerprints, &offsets[i], message,
                        message_size) != 0)
            return -1;

    memset(payload, 0, payload_size);
    memcpy(payload, "TSNS", 4U);
    tecmo_asset_pack_store_u16(payload + 4U, 1U);
    tecmo_asset_pack_store_u16(payload + 6U, TECMO_ASSET_PACK_SEASON_HEADER_SIZE);
    tecmo_asset_pack_store_u16(payload + 8U, 32U);
    tecmo_asset_pack_store_u16(payload + 10U, 30U);
    tecmo_asset_pack_store_u16(payload + 12U, TECMO_ASSET_PACK_SEASON_SCREEN_COUNT);
    tecmo_asset_pack_store_u16(payload + 14U, TECMO_ASSET_PACK_SEASON_SCREEN_CELL_COUNT);
    tecmo_asset_pack_store_u16(payload + 16U, TECMO_ASSET_PACK_SEASON_FONT_COUNT);
    tecmo_asset_pack_store_u16(payload + 18U, TECMO_ASSET_PACK_SEASON_SCHEDULE_COUNT);
    tecmo_asset_pack_store_u32(payload + 20U, TECMO_ASSET_PACK_SEASON_CELLS_OFFSET);
    tecmo_asset_pack_store_u32(payload + 24U, TECMO_ASSET_PACK_SEASON_PALETTES_OFFSET);
    tecmo_asset_pack_store_u32(payload + 28U, TECMO_ASSET_PACK_SEASON_FONT_OFFSET);
    tecmo_asset_pack_store_u32(payload + 32U, TECMO_ASSET_PACK_SEASON_CURSOR_OFFSET);
    tecmo_asset_pack_store_u32(payload + 36U, TECMO_ASSET_PACK_SEASON_SPRITE_PALETTE_OFFSET);
    tecmo_asset_pack_store_u32(payload + 40U, TECMO_ASSET_PACK_SEASON_SCHEDULE_OFFSET);
    tecmo_asset_pack_store_u32(payload + 44U, TECMO_ASSET_PACK_SEASON_CONTROL_CELLS_OFFSET);
    tecmo_asset_pack_store_u32(payload + 48U, TECMO_ASSET_PACK_SEASON_LEADERS_OFFSET);
    tecmo_asset_pack_store_u32(payload + 52U, TECMO_ASSET_PACK_SEASON_SIZE);
    tecmo_asset_pack_store_u32(payload + 56U, TECMO_ASSET_PACK_SEASON_CHR_SIZE);
    tecmo_asset_pack_store_u32(payload + 60U, TECMO_ASSET_PACK_SEASON_CHR_FNV1A32);
    payload[64U] = 2U; payload[65U] = 1U; payload[66U] = 2U;
    payload[67U] = 1U; payload[68U] = 2U;
    for (size_t i = 0U; i < TECMO_ASSET_PACK_SEASON_SCREEN_COUNT; ++i)
        payload[69U + i] = screens[i].id;
    payload[74U] = 0xFAU; payload[75U] = 0xFAU;
    payload[76U] = 0xDAU; payload[77U] = 0xFAU;
    payload[78U] = 0xFAU; payload[79U] = 0xFAU;
    payload[80U] = 0x76U; payload[81U] = 0xFAU;
    payload[82U] = 0x76U; payload[83U] = 0xFAU;
    payload[84U] = 8U;
    payload[85U] = 5U;
    payload[86U] = 27U;
    payload[87U] = 4U;
    payload[88U] = 4U;
    payload[89U] = 2U;
    payload[90U] = 8U;
    payload[91U] = TECMO_ASSET_PACK_SEASON_LEADER_COUNT;
    tecmo_asset_pack_store_u16(payload + 92U, 1107U);
    tecmo_asset_pack_store_u16(payload + 94U, 567U);
    tecmo_asset_pack_store_u16(payload + 96U, 351U);
    tecmo_asset_pack_store_u16(payload + 98U, 1107U);
    tecmo_asset_pack_store_u16(
        payload + 100U, TECMO_ASSET_PACK_SEASON_GAME_LAUNCH_BOUNDARY_CPU);
    tecmo_asset_pack_store_u16(
        payload + 102U, TECMO_ASSET_PACK_SEASON_GAME_LAUNCH_TARGET_CPU);
    tecmo_asset_pack_store_u16(payload + 104U, 1U);
    payload[106U] = 40U; payload[107U] = 120U; payload[108U] = 200U;
    payload[109U] = 80U; payload[110U] = 16U;
    tecmo_asset_pack_store_u32(payload + 112U,
                               TECMO_ASSET_PACK_SEASON_MENU_TEXT_OFFSET);
    tecmo_asset_pack_store_u32(payload + 116U,
                               TECMO_ASSET_PACK_SEASON_DIVISION_STARTS_OFFSET);
    tecmo_asset_pack_store_u32(payload + 120U,
                               TECMO_ASSET_PACK_SEASON_DIVISION_TEAMS_OFFSET);
    tecmo_asset_pack_store_u32(payload + 124U,
                               TECMO_ASSET_PACK_SEASON_LEADER_NAV_OFFSET);
    tecmo_asset_pack_store_u32(payload + 128U,
                               TECMO_ASSET_PACK_SEASON_LEADER_TEMPLATE_OFFSET);
    tecmo_asset_pack_store_u32(payload + 132U,
                               TECMO_ASSET_PACK_SEASON_LEADER_SCREEN_CELLS_OFFSET);
    tecmo_asset_pack_store_u32(payload + 136U,
                               TECMO_ASSET_PACK_SEASON_LEADER_PALETTES_OFFSET);
    tecmo_asset_pack_store_u16(
        payload + 140U, TECMO_ASSET_PACK_SEASON_LEADER_SCREEN_COUNT);
    tecmo_asset_pack_store_u16(
        payload + 142U, TECMO_ASSET_PACK_SEASON_LEADER_SCREEN_CELL_COUNT);
    tecmo_asset_pack_store_u16(
        payload + 144U, TECMO_ASSET_PACK_SEASON_MENU_TEXT_SIZE);
    for (size_t i = 0U;
         i < TECMO_ASSET_PACK_SEASON_LEADER_SCREEN_COUNT; ++i) {
        payload[148U + i] = leader_screens[i].id;
        payload[155U + i * 2U] =
            (uint8_t)(leader_screens[i].descriptor[0] * 2U);
        payload[156U + i * 2U] =
            (uint8_t)(leader_screens[i].descriptor[1] * 2U);
    }

    for (size_t screen = 0U;
         screen < TECMO_ASSET_PACK_SEASON_SCREEN_COUNT;
         ++screen) {
        const SeasonScreenSource *source = &screens[screen];
        uint64_t descriptor_offset = fixed_cpu_offset(
            prg_offset, prg_banks,
            0xDC85U + (uint32_t)source->id * 7U);
        uint16_t palette_cpu = (uint16_t)source->descriptor[2] |
            (uint16_t)((uint16_t)source->descriptor[3] << 8U);
        uint16_t stream_cpu = (uint16_t)source->descriptor[4] |
            (uint16_t)((uint16_t)source->descriptor[5] << 8U);
        uint32_t bank = source->descriptor[6];
        uint64_t stream_offset = bank_cpu_offset(prg_offset, bank, stream_cpu);
        uint64_t palette_offset = bank_cpu_offset(prg_offset, bank, palette_cpu);
        uint8_t r0 = (uint8_t)(source->descriptor[0] * 2U);
        uint8_t r1 = (uint8_t)(source->descriptor[1] * 2U);
        size_t encoded_size = 0U;
        if (!range_ok(descriptor_offset, 7U, rom_size) ||
            !range_ok(stream_offset, source->encoded_size, rom_size) ||
            !range_ok(palette_offset, 16U, rom_size) ||
            memcmp(rom + (size_t)descriptor_offset,
                   source->descriptor, 7U) != 0 ||
            tecmo_asset_pack_validate_chr_pair(
                r0, r1, chr_size, "season screen", message,
                message_size) != 0 ||
            tecmo_asset_pack_decode_d9f6_stream(
                rom + (size_t)bank_cpu_offset(prg_offset, bank, 0x8000U),
                TECMO_ASSET_PACK_PRG_BANK_BYTES,
                stream_cpu - 0x8000U, decoded, source->decoded_size,
                &encoded_size, message, message_size) != 0 ||
            encoded_size != source->encoded_size) {
            tecmo_asset_pack_set_message(
                message, message_size,
                "TSNS-1 screen descriptor/stream contract was rejected.");
            return -1;
        }
        if (enforce_revision_fingerprints != 0 &&
            (tecmo_asset_pack_fnv1a32(
                 rom + (size_t)descriptor_offset, 7U) !=
                 source->descriptor_fingerprint ||
             tecmo_asset_pack_fnv1a32(
                 rom + (size_t)stream_offset, encoded_size) !=
                 source->stream_fingerprint ||
             tecmo_asset_pack_fnv1a32(decoded, source->decoded_size) !=
                 source->decoded_fingerprint ||
             tecmo_asset_pack_fnv1a32(
                 rom + (size_t)palette_offset, 16U) !=
                 source->palette_fingerprint)) {
            tecmo_asset_pack_set_message(message, message_size,
                                         "TSNS-1 screen fingerprint mismatch.");
            return -1;
        }
        for (size_t page = 0U; page < 2U; ++page) {
            size_t decoded_page = source->decoded_size == 2048U ? page : 0U;
            for (size_t cell_index = 0U; cell_index < 960U; ++cell_index) {
                size_t source_index = decoded_page * 1024U + cell_index;
                uint8_t *cell = payload + TECMO_ASSET_PACK_SEASON_CELLS_OFFSET +
                    (screen * TECMO_ASSET_PACK_SEASON_SCREEN_CELL_COUNT +
                     page * 960U + cell_index) *
                        TECMO_ASSET_PACK_SEASON_CELL_STRIDE;
                store_cell(
                    cell, decoded[source_index],
                    tecmo_asset_pack_decoded_palette_index(
                        decoded + decoded_page * 1024U,
                        (unsigned)(cell_index / 32U),
                        (unsigned)(cell_index % 32U)),
                    r0, r1);
            }
        }
        for (size_t color = 0U; color < 16U; ++color) {
            uint8_t value = rom[(size_t)palette_offset + color];
            if (value > 0x3FU) return -1;
            payload[TECMO_ASSET_PACK_SEASON_PALETTES_OFFSET +
                    screen * 16U + color] = value;
        }
        provenance->descriptor_offsets[screen] = descriptor_offset;
        provenance->stream_offsets[screen] = stream_offset;
        provenance->stream_sizes[screen] = encoded_size;
        provenance->palette_offsets[screen] = palette_offset;
    }

    for (size_t screen = 0U;
         screen < TECMO_ASSET_PACK_SEASON_LEADER_SCREEN_COUNT;
         ++screen) {
        const SeasonScreenSource *source = &leader_screens[screen];
        uint64_t descriptor_offset = fixed_cpu_offset(
            prg_offset, prg_banks,
            0xDC85U + (uint32_t)source->id * 7U);
        uint16_t palette_cpu = (uint16_t)source->descriptor[2] |
            (uint16_t)((uint16_t)source->descriptor[3] << 8U);
        uint16_t stream_cpu = (uint16_t)source->descriptor[4] |
            (uint16_t)((uint16_t)source->descriptor[5] << 8U);
        uint32_t bank = source->descriptor[6];
        uint64_t stream_offset = bank_cpu_offset(prg_offset, bank, stream_cpu);
        uint64_t palette_offset = bank_cpu_offset(prg_offset, bank, palette_cpu);
        uint8_t r0 = (uint8_t)(source->descriptor[0] * 2U);
        uint8_t r1 = (uint8_t)(source->descriptor[1] * 2U);
        size_t encoded_size = 0U;
        if (!range_ok(descriptor_offset, 7U, rom_size) ||
            !range_ok(stream_offset, source->encoded_size, rom_size) ||
            !range_ok(palette_offset, 16U, rom_size) ||
            memcmp(rom + (size_t)descriptor_offset,
                   source->descriptor, 7U) != 0 ||
            tecmo_asset_pack_validate_chr_pair(
                r0, r1, chr_size, "season leaders screen", message,
                message_size) != 0 ||
            tecmo_asset_pack_decode_d9f6_stream(
                rom + (size_t)bank_cpu_offset(prg_offset, bank, 0x8000U),
                TECMO_ASSET_PACK_PRG_BANK_BYTES,
                stream_cpu - 0x8000U, decoded, source->decoded_size,
                &encoded_size, message, message_size) != 0 ||
            encoded_size != source->encoded_size ||
            (enforce_revision_fingerprints != 0 &&
             (tecmo_asset_pack_fnv1a32(
                  rom + (size_t)descriptor_offset, 7U) !=
                  source->descriptor_fingerprint ||
              tecmo_asset_pack_fnv1a32(
                  rom + (size_t)stream_offset, encoded_size) !=
                  source->stream_fingerprint ||
              tecmo_asset_pack_fnv1a32(decoded, source->decoded_size) !=
                  source->decoded_fingerprint ||
              tecmo_asset_pack_fnv1a32(
                  rom + (size_t)palette_offset, 16U) !=
                  source->palette_fingerprint))) {
            tecmo_asset_pack_set_message(
                message, message_size,
                "TSNS-1 leaders screen contract was rejected.");
            return -1;
        }
        for (size_t cell_index = 0U; cell_index < 960U; ++cell_index) {
            uint8_t *cell = payload +
                TECMO_ASSET_PACK_SEASON_LEADER_SCREEN_CELLS_OFFSET +
                (screen * 960U + cell_index) *
                    TECMO_ASSET_PACK_SEASON_CELL_STRIDE;
            store_cell(
                cell, decoded[cell_index],
                tecmo_asset_pack_decoded_palette_index(
                    decoded, (unsigned)(cell_index / 32U),
                    (unsigned)(cell_index % 32U)),
                r0, r1);
        }
        for (size_t color = 0U; color < 16U; ++color) {
            uint8_t value = rom[(size_t)palette_offset + color];
            if (value > 0x3FU) return -1;
            payload[TECMO_ASSET_PACK_SEASON_LEADER_PALETTES_OFFSET +
                    screen * 16U + color] = value;
        }
        provenance->leader_descriptor_offsets[screen] = descriptor_offset;
        provenance->leader_stream_offsets[screen] = stream_offset;
        provenance->leader_stream_sizes[screen] = encoded_size;
        provenance->leader_palette_offsets[screen] = palette_offset;
    }

    {
        const uint8_t *font = rom + (size_t)offsets[11];
        for (size_t i = 0U; i < TECMO_ASSET_PACK_SEASON_FONT_COUNT; ++i) {
            uint8_t *record = payload + TECMO_ASSET_PACK_SEASON_FONT_OFFSET +
                i * TECMO_ASSET_PACK_SEASON_FONT_STRIDE;
            record[0] = (uint8_t)(0x20U + i);
            record[1] = font[i];
            record[2] = 0xFAU;
            record[3] = 0xFAU;
            tecmo_asset_pack_store_u32(
                record + 4U,
                tecmo_asset_pack_bg_chr_offset(font[i], 0xFAU, 0xFAU));
        }
    }
    if (build_cursor(rom + (size_t)offsets[14],
                     payload + TECMO_ASSET_PACK_SEASON_CURSOR_OFFSET) != 0)
        return -1;
    memcpy(payload + TECMO_ASSET_PACK_SEASON_SPRITE_PALETTE_OFFSET,
           rom + (size_t)offsets[15], 16U);
    for (size_t i = 0U; i < 16U; ++i)
        if (payload[TECMO_ASSET_PACK_SEASON_SPRITE_PALETTE_OFFSET + i] > 0x3FU)
            return -1;

    memcpy(payload + TECMO_ASSET_PACK_SEASON_SCHEDULE_OFFSET,
           rom + (size_t)offsets[12], TECMO_ASSET_PACK_SEASON_SCHEDULE_SIZE);
    for (size_t game = 0U; game < TECMO_ASSET_PACK_SEASON_SCHEDULE_COUNT; ++game) {
        uint8_t away = payload[TECMO_ASSET_PACK_SEASON_SCHEDULE_OFFSET + game * 2U];
        uint8_t home = payload[TECMO_ASSET_PACK_SEASON_SCHEDULE_OFFSET + game * 2U + 1U];
        if ((away & 0x3FU) >= 27U || (home & 0x3FU) >= 27U ||
            (away & 0x40U) != 0U || (home & 0x40U) != 0U)
            return -1;
    }
    {
        const uint8_t *tiles = rom + (size_t)bank_cpu_offset(
            prg_offset, SEASON_BANK, 0x89C0U);
        const uint8_t *screen20 = decoded; /* overwritten below for palette only */
        uint8_t screen20_decoded[1024];
        size_t encoded_size = 0U;
        (void)screen20;
        if (tecmo_asset_pack_decode_d9f6_stream(
                rom + (size_t)bank_cpu_offset(prg_offset, 0U, 0x8000U),
                TECMO_ASSET_PACK_PRG_BANK_BYTES, 0x8371U - 0x8000U,
                screen20_decoded, sizeof(screen20_decoded), &encoded_size,
                message, message_size) != 0 || encoded_size != 245U)
            return -1;
        for (size_t mode = 0U; mode < 4U; ++mode) {
            for (size_t glyph = 0U; glyph < 3U; ++glyph) {
                size_t index = mode * 3U + glyph;
                size_t column = 5U + glyph;
                size_t row = 10U;
                uint8_t *cell = payload +
                    TECMO_ASSET_PACK_SEASON_CONTROL_CELLS_OFFSET +
                    index * TECMO_ASSET_PACK_SEASON_CELL_STRIDE;
                store_cell(cell, tiles[index],
                           tecmo_asset_pack_decoded_palette_index(
                               screen20_decoded, (unsigned)row,
                               (unsigned)column),
                           0x76U, 0xFAU);
            }
        }
    }
    if (import_leader_labels(rom, rom_size, prg_offset, payload,
                             message, message_size) != 0)
        return -1;
    {
        static const uint8_t schedule_header[4] = {0x0DU,0x06U,0x01U,0x08U};
        static const uint8_t reset_header[4] = {0x18U,0x0CU,0x03U,0x0CU};
        static const uint8_t type_header[4] = {0x0DU,0x0AU,0x03U,0x0CU};
        static const char *const schedule_segments[3] = {
            "SCHEDULE", "PLAYOFF", "RESET"
        };
        static const char *const reset_segments[6] = {
            "RESET", "THE DATA OF ALL THE", "GAMES THAT YOU HAVE",
            "PLAYED WILL BE ERASED", "    ERASE    NO",
            "             YES"
        };
        static const char *const type_segments[5] = {
            "SEASON", "REGULAR", "REDUCED", "SHORT", "PROGRAMMED"
        };
        const uint8_t *records = rom + (size_t)offsets[10];
        uint8_t *texts = payload + TECMO_ASSET_PACK_SEASON_MENU_TEXT_OFFSET;
        if (import_menu_record(
                records, 31U, schedule_header, schedule_segments, 3U,
                texts, TECMO_ASSET_PACK_SEASON_SCHEDULE_TEXT_STRIDE,
                message, message_size) != 0 ||
            import_menu_record(
                records + 31U, 115U, reset_header, reset_segments, 6U,
                texts + TECMO_ASSET_PACK_SEASON_SCHEDULE_TEXT_COUNT *
                            TECMO_ASSET_PACK_SEASON_SCHEDULE_TEXT_STRIDE,
                TECMO_ASSET_PACK_SEASON_RESET_TEXT_STRIDE,
                message, message_size) != 0 ||
            import_menu_record(
                records + 146U, 52U, type_header, type_segments, 5U,
                texts + TECMO_ASSET_PACK_SEASON_SCHEDULE_TEXT_COUNT *
                            TECMO_ASSET_PACK_SEASON_SCHEDULE_TEXT_STRIDE +
                        TECMO_ASSET_PACK_SEASON_RESET_TEXT_COUNT *
                            TECMO_ASSET_PACK_SEASON_RESET_TEXT_STRIDE,
                TECMO_ASSET_PACK_SEASON_TYPE_TEXT_STRIDE,
                message, message_size) != 0)
            return -1;
    }
    memcpy(payload + TECMO_ASSET_PACK_SEASON_DIVISION_STARTS_OFFSET,
           rom + (size_t)offsets[20],
           TECMO_ASSET_PACK_SEASON_DIVISION_START_COUNT);
    memcpy(payload + TECMO_ASSET_PACK_SEASON_DIVISION_TEAMS_OFFSET,
           rom + (size_t)offsets[21], 27U);
    memcpy(payload + TECMO_ASSET_PACK_SEASON_LEADER_NAV_OFFSET,
           rom + (size_t)offsets[22],
           TECMO_ASSET_PACK_SEASON_LEADER_NAV_SIZE);
    memcpy(payload + TECMO_ASSET_PACK_SEASON_LEADER_TEMPLATE_OFFSET,
           rom + (size_t)offsets[23],
           TECMO_ASSET_PACK_SEASON_LEADER_COUNT);

    provenance->chr_offset = chr_offset;
    provenance->dispatch_offset = offsets[0];
    provenance->route_table_offset = offsets[1];
    provenance->leaders_offset = offsets[2];
    provenance->game_prelaunch_offset = offsets[3];
    provenance->game_terminal_offset = offsets[4];
    provenance->game_launch_target_offset = offsets[5];
    provenance->team_control_offset = offsets[6];
    provenance->standings_offset = offsets[7];
    provenance->schedule_core_offset = offsets[8];
    provenance->schedule_helpers_offset = offsets[9];
    provenance->popup_records_offset = offsets[10];
    provenance->font_offset = offsets[11];
    provenance->schedule_offset = offsets[12];
    provenance->defaults_offset = offsets[13];
    provenance->cursor_offset = offsets[14];
    provenance->sprite_palette_offset = offsets[15];
    provenance->leader_records_offset = offsets[16];
    provenance->division_starts_offset = offsets[20];
    provenance->division_teams_offset = offsets[21];
    provenance->leader_navigation_offset = offsets[22];
    provenance->leader_template_offset = offsets[23];
    provenance->fixed_input_offset = offsets[17];
    provenance->fixed_loader_offset = offsets[18];

    if (tecmo_asset_pack_fnv1a32(payload, payload_size) !=
            TECMO_ASSET_PACK_SEASON_FNV1A32) {
        tecmo_asset_pack_set_messagef(
            message, message_size,
            "TSNS-1 canonical payload fingerprint mismatch (got %08X, expected %08X).",
            tecmo_asset_pack_fnv1a32(payload, payload_size),
            TECMO_ASSET_PACK_SEASON_FNV1A32);
        return -1;
    }
    tecmo_asset_pack_set_message(
        message, message_size,
        "Built strict TSNS-1 season-management payload from Rev1 ROM data.");
    return 0;
}
