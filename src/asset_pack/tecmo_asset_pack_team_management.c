#include "tecmo_asset_pack_team_management.h"

#include "tecmo_asset_pack_d9f6.h"
#include "tecmo_asset_pack_util.h"

#include <stdbool.h>
#include <string.h>

typedef struct TeamManagementSpan {
    uint8_t bank;
    uint16_t cpu;
    uint16_t size;
    uint32_t fingerprint;
} TeamManagementSpan;

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
                       const TeamManagementSpan *span,
                       int enforce,
                       uint64_t *offset_out)
{
    uint64_t offset = bank_cpu_offset(prg_offset, span->bank, span->cpu);
    if (!range_ok(offset, span->size, rom_size) ||
        (enforce != 0 &&
         tecmo_asset_pack_fnv1a32(rom + (size_t)offset, span->size) !=
             span->fingerprint))
        return -1;
    if (offset_out != NULL) *offset_out = offset;
    return 0;
}

static void store_cell(uint8_t *destination,
                       uint8_t tile,
                       uint8_t palette,
                       uint8_t r0,
                       uint8_t r1)
{
    destination[0] = tile;
    destination[1] = palette;
    tecmo_asset_pack_store_u32(
        destination + 2U,
        tecmo_asset_pack_bg_chr_offset(tile, r0, r1));
}

int tecmo_asset_pack_team_management_self_test(char *message,
                                                size_t message_size)
{
    static const size_t expected[8] = {
        256U, 6016U, 17536U, 17568U, 20640U, 20656U, 20800U, 21061U
    };
    const size_t actual[8] = {
        TECMO_ASSET_PACK_TEAM_MANAGEMENT_STARTERS_CELLS_OFFSET,
        TECMO_ASSET_PACK_TEAM_MANAGEMENT_PLAYBOOK_CELLS_OFFSET,
        TECMO_ASSET_PACK_TEAM_MANAGEMENT_PALETTES_OFFSET,
        TECMO_ASSET_PACK_TEAM_MANAGEMENT_DIAGRAMS_OFFSET,
        TECMO_ASSET_PACK_TEAM_MANAGEMENT_MARKER_OFFSET,
        TECMO_ASSET_PACK_TEAM_MANAGEMENT_NAMES_OFFSET,
        TECMO_ASSET_PACK_TEAM_MANAGEMENT_DEFAULTS_OFFSET,
        TECMO_ASSET_PACK_TEAM_MANAGEMENT_SIZE
    };
    for (size_t i = 0U; i < 8U; ++i) {
        if (actual[i] != expected[i]) {
            tecmo_asset_pack_set_message(
                message, message_size, "TTMG-1 layout self-test failed.");
            return -1;
        }
    }
    tecmo_asset_pack_set_message(
        message, message_size, "TTMG-1 layout self-test passed.");
    return 0;
}

int tecmo_asset_pack_build_team_management(
    const uint8_t *rom,
    uint64_t rom_size,
    uint64_t prg_offset,
    uint32_t prg_banks,
    uint64_t chr_size,
    int enforce_revision_fingerprints,
    uint8_t *payload,
    size_t payload_size,
    TecmoTeamManagementProvenance *provenance,
    char *message,
    size_t message_size)
{
    static const TeamManagementSpan spans[] = {
        {3U, 0x8C99U, 6U, 0xE51885D7U},
        {3U, 0x8E4FU, 696U, 0x12AB26E8U},
        {2U, 0xA631U, 443U, 0xEDD2525EU},
        {3U, 0x9EC6U, 469U, 0x91DC81F3U},
        {3U, 0x9107U, 334U, 0x122C684FU},
        {3U, 0x9255U, 160U, 0xB26B5DA6U},
        {3U, 0xAD0BU, 1217U, 0xE5088534U},
        {3U, 0xAF01U, 16U, 0x7214CFEBU},
        {3U, 0xAFABU, 113U, 0x86E58601U},
        {0U, 0xA0E3U, 512U, 0xC6C032A0U},
        {0U, 0xA2E3U, 16U, 0x2AF12A2BU},
        {1U, 0xBED0U, 79U, 0xFF4433C8U}
    };
    static const uint8_t descriptors[2][7] = {
        {0x7DU, 0x7DU, 0xE0U, 0xB5U, 0x18U, 0xB5U, 0x00U},
        {0x7DU, 0x7DU, 0x23U, 0xB9U, 0xF2U, 0xB8U, 0x01U}
    };
    static const uint32_t descriptor_fingerprints[2] = {
        0xEEC27A7DU, 0xEDA72C5CU
    };
    static const uint32_t stream_fingerprints[2] = {
        0xC869A670U, 0x3111C9BFU
    };
    static const uint32_t decoded_fingerprints[2] = {
        0x483171E7U, 0x9667821DU
    };
    static const uint32_t palette_fingerprints[2] = {
        0x98634D94U, 0x0242ED20U
    };
    static const uint8_t screen_banks[2] = {0U, 1U};
    static const uint16_t screen_cpus[2] = {0xB518U, 0xB8F2U};
    static const uint16_t screen_sizes[2] = {201U, 50U};
    uint64_t offsets[sizeof(spans) / sizeof(spans[0])];
    uint64_t chr_offset;
    uint8_t decoded[2048];
    uint8_t visible[1920];

    if (rom == NULL || payload == NULL || provenance == NULL ||
        prg_banks < 8U || chr_size != TECMO_ASSET_PACK_TEAM_MANAGEMENT_CHR_SIZE ||
        payload_size != TECMO_ASSET_PACK_TEAM_MANAGEMENT_SIZE) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TTMG-1 import requires the exact Rev1 payload contract.");
        return -1;
    }
    memset(provenance, 0, sizeof(*provenance));
    chr_offset = prg_offset +
                 (uint64_t)prg_banks * TECMO_ASSET_PACK_PRG_BANK_BYTES;
    if (!range_ok(chr_offset, chr_size, rom_size) ||
        (enforce_revision_fingerprints != 0 &&
         tecmo_asset_pack_fnv1a32(rom + (size_t)chr_offset,
                                  (size_t)chr_size) !=
             TECMO_ASSET_PACK_TEAM_MANAGEMENT_CHR_FNV1A32)) {
        tecmo_asset_pack_set_message(
            message, message_size, "TTMG-1 full CHR fingerprint mismatch.");
        return -1;
    }
    for (size_t i = 0U; i < sizeof(spans) / sizeof(spans[0]); ++i) {
        if (verify_span(rom, rom_size, prg_offset, &spans[i],
                        enforce_revision_fingerprints, &offsets[i]) != 0) {
            tecmo_asset_pack_set_messagef(
                message, message_size,
                "TTMG-1 Bank%02u $%04X source fingerprint mismatch.",
                spans[i].bank, spans[i].cpu);
            return -1;
        }
    }
    {
        TeamManagementSpan fixed_input = {
            0U, 0xD714U, 212U, 0xBC71E228U
        };
        uint64_t offset = fixed_cpu_offset(prg_offset, prg_banks,
                                           fixed_input.cpu);
        if (!range_ok(offset, fixed_input.size, rom_size) ||
            (enforce_revision_fingerprints != 0 &&
             tecmo_asset_pack_fnv1a32(rom + (size_t)offset,
                                      fixed_input.size) !=
                 fixed_input.fingerprint)) {
            tecmo_asset_pack_set_message(
                message, message_size,
                "TTMG-1 fixed input source fingerprint mismatch.");
            return -1;
        }
        provenance->fixed_input_offset = offset;
    }

    memset(payload, 0, payload_size);
    memcpy(payload, "TTMG", 4U);
    tecmo_asset_pack_store_u16(payload + 4U, 1U);
    tecmo_asset_pack_store_u16(
        payload + 6U, TECMO_ASSET_PACK_TEAM_MANAGEMENT_HEADER_SIZE);
    tecmo_asset_pack_store_u16(payload + 8U, 32U);
    tecmo_asset_pack_store_u16(payload + 10U, 30U);
    tecmo_asset_pack_store_u16(payload + 12U, 1U);
    tecmo_asset_pack_store_u16(payload + 14U, 2U);
    tecmo_asset_pack_store_u16(
        payload + 16U, TECMO_ASSET_PACK_TEAM_MANAGEMENT_TEAM_COUNT);
    tecmo_asset_pack_store_u16(payload + 18U, 12U);
    tecmo_asset_pack_store_u32(
        payload + 20U,
        TECMO_ASSET_PACK_TEAM_MANAGEMENT_STARTERS_CELLS_OFFSET);
    tecmo_asset_pack_store_u32(
        payload + 24U,
        TECMO_ASSET_PACK_TEAM_MANAGEMENT_PLAYBOOK_CELLS_OFFSET);
    tecmo_asset_pack_store_u32(
        payload + 28U, TECMO_ASSET_PACK_TEAM_MANAGEMENT_PALETTES_OFFSET);
    tecmo_asset_pack_store_u32(
        payload + 32U, TECMO_ASSET_PACK_TEAM_MANAGEMENT_DIAGRAMS_OFFSET);
    tecmo_asset_pack_store_u32(
        payload + 36U, TECMO_ASSET_PACK_TEAM_MANAGEMENT_MARKER_OFFSET);
    tecmo_asset_pack_store_u32(
        payload + 40U, TECMO_ASSET_PACK_TEAM_MANAGEMENT_NAMES_OFFSET);
    tecmo_asset_pack_store_u32(
        payload + 44U, TECMO_ASSET_PACK_TEAM_MANAGEMENT_DEFAULTS_OFFSET);
    tecmo_asset_pack_store_u32(
        payload + 48U, TECMO_ASSET_PACK_TEAM_MANAGEMENT_SIZE);
    tecmo_asset_pack_store_u32(
        payload + 52U, TECMO_ASSET_PACK_TEAM_MANAGEMENT_CHR_SIZE);
    tecmo_asset_pack_store_u32(
        payload + 56U, TECMO_ASSET_PACK_TEAM_MANAGEMENT_CHR_FNV1A32);
    tecmo_asset_pack_store_u32(payload + 60U, TECMO_ASSET_PACK_TEAM_DATA_SIZE);
    tecmo_asset_pack_store_u32(
        payload + 64U, TECMO_ASSET_PACK_TEAM_DATA_FNV1A32);
    payload[68U] = TECMO_ASSET_PACK_TEAM_MANAGEMENT_DIAGRAM_COUNT;
    payload[69U] = 8U;
    payload[70U] = 8U;
    payload[71U] = TECMO_ASSET_PACK_TEAM_MANAGEMENT_PLAY_NAME_COUNT;
    payload[72U] = TECMO_ASSET_PACK_TEAM_MANAGEMENT_STARTER_COUNT;
    payload[73U] = TECMO_ASSET_PACK_TEAM_MANAGEMENT_PLAYBOOK_SLOT_COUNT;
    payload[74U] = 6U;
    payload[75U] = 7U;
    payload[76U] = 8U;
    payload[77U] = 8U;
    payload[78U] = 8U;
    payload[79U] = 1U;
    payload[80U] = 1U;
    payload[81U] = 2U;
    payload[82U] = 15U;
    payload[83U] = 16U;
    payload[84U] = 0xFAU;
    payload[85U] = 0xFAU;
    payload[86U] = 0xFAU;
    payload[87U] = 0xFAU;
    payload[88U] = 40U;
    payload[89U] = 44U;
    payload[90U] = 8U;
    payload[91U] = 40U;
    payload[92U] = 64U;
    payload[93U] = 136U;
    payload[94U] = 144U;
    payload[95U] = 64U;
    payload[96U] = 160U;

    for (size_t screen = 0U; screen < 2U; ++screen) {
        uint64_t descriptor_offset = fixed_cpu_offset(
            prg_offset, prg_banks, 0xDCEEU + (uint32_t)screen * 7U);
        uint64_t stream_offset = bank_cpu_offset(
            prg_offset, screen_banks[screen], screen_cpus[screen]);
        uint16_t palette_cpu = (uint16_t)descriptors[screen][2] |
            (uint16_t)((uint16_t)descriptors[screen][3] << 8U);
        uint64_t palette_offset = bank_cpu_offset(
            prg_offset, screen_banks[screen], palette_cpu);
        size_t encoded_size = 0U;
        size_t decoded_size = screen == 0U ? 1024U : 2048U;
        size_t visible_count = screen == 0U ? 960U : 1920U;
        uint8_t *destination = payload +
            (screen == 0U
                 ? TECMO_ASSET_PACK_TEAM_MANAGEMENT_STARTERS_CELLS_OFFSET
                 : TECMO_ASSET_PACK_TEAM_MANAGEMENT_PLAYBOOK_CELLS_OFFSET);
        if (!range_ok(descriptor_offset, 7U, rom_size) ||
            !range_ok(stream_offset, screen_sizes[screen], rom_size) ||
            !range_ok(palette_offset, 16U, rom_size) ||
            memcmp(rom + (size_t)descriptor_offset, descriptors[screen], 7U) != 0 ||
            tecmo_asset_pack_decode_d9f6_stream(
                rom + (size_t)bank_cpu_offset(prg_offset, screen_banks[screen],
                                              0x8000U),
                TECMO_ASSET_PACK_PRG_BANK_BYTES,
                screen_cpus[screen] - 0x8000U,
                decoded, decoded_size, &encoded_size,
                message, message_size) != 0 ||
            encoded_size != screen_sizes[screen]) {
            tecmo_asset_pack_set_message(
                message, message_size,
                "TTMG-1 screen descriptor or stream was rejected.");
            return -1;
        }
        for (size_t i = 0U; i < visible_count; ++i) {
            size_t page = i / 960U;
            size_t cell = i % 960U;
            visible[i] = decoded[page * 1024U + cell];
        }
        if (enforce_revision_fingerprints != 0 &&
            (tecmo_asset_pack_fnv1a32(
                 rom + (size_t)descriptor_offset, 7U) !=
                 descriptor_fingerprints[screen] ||
             tecmo_asset_pack_fnv1a32(
                 rom + (size_t)stream_offset, encoded_size) !=
                 stream_fingerprints[screen] ||
             tecmo_asset_pack_fnv1a32(decoded, decoded_size) !=
                 decoded_fingerprints[screen] ||
             tecmo_asset_pack_fnv1a32(
                 rom + (size_t)palette_offset, 16U) !=
                 palette_fingerprints[screen])) {
            tecmo_asset_pack_set_messagef(
                message, message_size,
                "TTMG-1 decoded screen %u fingerprint mismatch (descriptor=%08X stream=%08X visible=%08X full=%08X palette=%08X encoded=%u).",
                (unsigned)screen,
                tecmo_asset_pack_fnv1a32(
                    rom + (size_t)descriptor_offset, 7U),
                tecmo_asset_pack_fnv1a32(
                    rom + (size_t)stream_offset, encoded_size),
                tecmo_asset_pack_fnv1a32(visible, visible_count),
                tecmo_asset_pack_fnv1a32(decoded, decoded_size),
                tecmo_asset_pack_fnv1a32(
                    rom + (size_t)palette_offset, 16U),
                (unsigned)encoded_size);
            return -1;
        }
        for (size_t i = 0U; i < visible_count; ++i) {
            size_t page = i / 960U;
            size_t cell = i % 960U;
            uint8_t palette = tecmo_asset_pack_decoded_palette_index(
                decoded + page * 1024U,
                (unsigned)(cell / 32U), (unsigned)(cell % 32U));
            store_cell(destination + i *
                           TECMO_ASSET_PACK_TEAM_MANAGEMENT_CELL_STRIDE,
                       visible[i], palette, 0xFAU, 0xFAU);
        }
        for (size_t i = 0U; i < 16U; ++i) {
            uint8_t color = rom[(size_t)palette_offset + i];
            if (color > 0x3FU) return -1;
            payload[TECMO_ASSET_PACK_TEAM_MANAGEMENT_PALETTES_OFFSET +
                    screen * 16U + i] = color;
        }
        provenance->descriptor_offsets[screen] = descriptor_offset;
        provenance->stream_offsets[screen] = stream_offset;
        provenance->stream_sizes[screen] = encoded_size;
        provenance->palette_offsets[screen] = palette_offset;
    }

    for (size_t i = 0U; i < 512U; ++i) {
        store_cell(payload + TECMO_ASSET_PACK_TEAM_MANAGEMENT_DIAGRAMS_OFFSET +
                       i * TECMO_ASSET_PACK_TEAM_MANAGEMENT_CELL_STRIDE,
                   rom[(size_t)offsets[9] + i], 0U, 0xFAU, 0xFAU);
    }
    memcpy(payload + TECMO_ASSET_PACK_TEAM_MANAGEMENT_MARKER_OFFSET,
           rom + (size_t)offsets[10],
           TECMO_ASSET_PACK_TEAM_MANAGEMENT_MARKER_SIZE);
    for (size_t play = 0U;
         play < TECMO_ASSET_PACK_TEAM_MANAGEMENT_PLAY_NAME_COUNT; ++play) {
        uint16_t string_cpu = tecmo_asset_pack_read_u16(
            rom + (size_t)offsets[5] + play * 2U);
        uint16_t expected_cpu = (uint16_t)(0x9265U + play * 18U);
        uint64_t string_offset = bank_cpu_offset(prg_offset, 3U, string_cpu);
        uint8_t *destination =
            payload + TECMO_ASSET_PACK_TEAM_MANAGEMENT_NAMES_OFFSET +
            play * TECMO_ASSET_PACK_TEAM_MANAGEMENT_PLAY_NAME_STRIDE;
        if (string_cpu != expected_cpu ||
            !range_ok(string_offset, 18U, rom_size) ||
            rom[(size_t)string_offset + 17U] != 0xFFU)
            return -1;
        for (size_t i = 0U; i < 17U; ++i) {
            uint8_t c = rom[(size_t)string_offset + i];
            if (!((c >= 'A' && c <= 'Z') || c == ' ' || c == '&'))
                return -1;
            destination[i] = c;
        }
        destination[17U] = 0U;
    }
    for (size_t team = 0U;
         team < TECMO_ASSET_PACK_TEAM_MANAGEMENT_TEAM_COUNT; ++team) {
        uint8_t *defaults =
            payload + TECMO_ASSET_PACK_TEAM_MANAGEMENT_DEFAULTS_OFFSET +
            team * (TECMO_ASSET_PACK_TEAM_MANAGEMENT_STARTER_COUNT +
                    TECMO_ASSET_PACK_TEAM_MANAGEMENT_PLAYBOOK_SLOT_COUNT);
        for (uint8_t i = 0U;
             i < TECMO_ASSET_PACK_TEAM_MANAGEMENT_STARTER_COUNT; ++i)
            defaults[i] = i;
        for (uint8_t i = 0U;
             i < TECMO_ASSET_PACK_TEAM_MANAGEMENT_PLAYBOOK_SLOT_COUNT; ++i)
            defaults[TECMO_ASSET_PACK_TEAM_MANAGEMENT_STARTER_COUNT + i] = i;
    }

    provenance->chr_offset = chr_offset;
    provenance->route_vector_offset = offsets[0];
    provenance->starters_flow_offset = offsets[1];
    provenance->lineup_renderer_offset = offsets[2];
    provenance->generic_input_offset = offsets[3];
    provenance->playbook_flow_offset = offsets[4];
    provenance->playbook_names_offset = offsets[5];
    provenance->playbook_helper_offset = offsets[6];
    provenance->playbook_pointer_offset = offsets[7];
    provenance->playbook_oam_offset = offsets[8];
    provenance->playbook_diagrams_offset = offsets[9];
    provenance->playbook_marker_offset = offsets[10];
    provenance->defaults_offset = offsets[11];

    if (tecmo_asset_pack_fnv1a32(payload, payload_size) !=
            TECMO_ASSET_PACK_TEAM_MANAGEMENT_FNV1A32) {
        tecmo_asset_pack_set_messagef(
            message, message_size,
            "TTMG-1 canonical payload fingerprint mismatch (got %08X).",
            tecmo_asset_pack_fnv1a32(payload, payload_size));
        return -1;
    }
    tecmo_asset_pack_set_message(
        message, message_size,
        "Built strict TTMG-1 TEAM DATA management payload from Rev1 ROM data.");
    return 0;
}
