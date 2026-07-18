#define _CRT_SECURE_NO_WARNINGS

#include "tecmo_asset_pack_source_map.h"

#include "tecmo_asset_pack.h"
#include "tecmo_asset_pack_util.h"

#include <stdio.h>
#include <stdlib.h>

static uint64_t source_map_prg_bank_cpu_source_offset(uint64_t prg_offset,
                                                       uint32_t prg_banks,
                                                       uint32_t bank,
                                                       uint32_t cpu_address)
{
    if (bank >= prg_banks ||
        cpu_address < TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE ||
        cpu_address >= TECMO_ASSET_PACK_SWITCHED_PRG_CPU_LIMIT) {
        return 0U;
    }

    return prg_offset +
           (uint64_t)bank * TECMO_ASSET_PACK_PRG_BANK_BYTES +
           (uint64_t)(cpu_address - TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE);
}

static int append_source_map_entry(char *buffer,
                                   size_t capacity,
                                   size_t *length,
                                   int *first,
                                   const char *id,
                                   const char *kind,
                                   uint64_t source_offset,
                                   uint64_t size,
                                   uint32_t bank,
                                   uint32_t cpu_address)
{
    const char *prefix = *first != 0 ? "" : ",\n";

    *first = 0;
    return tecmo_asset_pack_append_text(buffer,
                       capacity,
                       length,
                       "%s"
                       "    {\"id\":\"%s\",\"kind\":\"%s\",\"source_offset\":%llu,"
                       "\"size\":%llu,\"bank\":%u,\"cpu_address\":%u}",
                       prefix,
                       id,
                       kind,
                       (unsigned long long)source_offset,
                       (unsigned long long)size,
                       bank,
                       cpu_address);
}
static int append_logical_source_map_entry(char *buffer,
                                           size_t capacity,
                                           size_t *length,
                                           int *first,
                                           const char *id,
                                           const char *kind,
                                           const char *schema,
                                           const char *source_entry,
                                           uint64_t source_offset,
                                           uint32_t bank,
                                           uint32_t cpu_address,
                                           int source_bank_available)
{
    const char *prefix = *first != 0 ? "" : ",\n";

    *first = 0;
    return tecmo_asset_pack_append_text(buffer,
                       capacity,
                       length,
                       "%s"
                       "    {\"id\":\"%s\",\"kind\":\"%s\",\"schema\":\"%s\","
                       "\"source_entry\":\"%s\",\"source_offset\":%llu,"
                       "\"bank\":%u,\"cpu_address\":%u,\"source_bank_available\":%s}",
                       prefix,
                       id,
                       kind,
                       schema,
                       source_entry,
                       (unsigned long long)source_offset,
                       bank,
                       cpu_address,
                       source_bank_available ? "true" : "false");
}

static int append_opening_screen_source_map_entries(
    char *buffer,
    size_t capacity,
    size_t *length,
    int *first,
    const TecmoOpeningScreenProvenance provenance[2])
{
    const char *prefix = *first != 0 ? "" : ",\n";

    *first = 0;
    return tecmo_asset_pack_append_text(
        buffer,
        capacity,
        length,
        "%s"
        "    {\"id\":\"%s\",\"kind\":\"opening-tecmo-presents-native\","
        "\"schema\":\"tecmo.intro.screen/TISC-1\",\"screen_id\":0,"
        "\"duration_frames\":133,\"palette_stride\":32,"
        "\"palette_frames\":[0,4,8,12,16,123,125,127,129],"
        "\"sprite_visible_frames\":[0,131],\"sources\":["
        "{\"role\":\"descriptor\",\"source_entry\":\"prg/fixed\","
        "\"source_offset\":%llu,\"cpu_address\":%u,\"size\":7},"
        "{\"role\":\"compressed-screen\",\"source_entry\":\"prg/bank00\","
        "\"source_offset\":%llu,\"bank\":0,\"cpu_address\":%u,"
        "\"encoded_size\":%llu,\"decoded_size\":1024,\"visible_page\":0},"
        "{\"role\":\"background-palette\",\"source_entry\":\"prg/bank00\","
        "\"source_offset\":%llu,\"bank\":0,\"cpu_address\":%u,\"size\":16,"
        "\"fingerprint_fnv1a32\":\"761B572D\"},"
        "{\"role\":\"sprite-chr-selectors\",\"source_entry\":\"prg/bank00\","
        "\"source_offset\":%llu,\"bank\":0,\"cpu_address\":%u,\"size\":7,"
        "\"selectors\":[244,245]},"
        "{\"role\":\"sprite-records\",\"source_entry\":\"prg/bank00\","
        "\"source_offset\":%llu,\"bank\":0,\"cpu_address\":%u,"
        "\"size\":81,\"record_count\":20,\"record_stride\":4,"
        "\"record_fingerprint_fnv1a32\":\"A3E8B4F0\"},"
        "{\"role\":\"sprite-palette\",\"source_entry\":\"prg/bank00\","
        "\"source_offset\":%llu,\"bank\":0,\"cpu_address\":%u,\"size\":16,"
        "\"fingerprint_fnv1a32\":\"ACF5D9A1\"},"
        "{\"role\":\"sprite-layout-operands\",\"source_entry\":\"prg/bank00\","
        "\"source_offsets\":[%llu,%llu,%llu],\"bank\":0,"
        "\"cpu_addresses\":[%u,%u,%u],\"values\":[64,85,80]}],"
        "\"chr_resolution\":{\"entry\":\"chr/all\","
        "\"background_selectors\":[252,250],\"sprite_selectors\":[244,245]}},\n"
        "    {\"id\":\"%s\",\"kind\":\"opening-nba-license-native\","
        "\"schema\":\"tecmo.intro.screen/TISC-1\",\"screen_id\":2,"
        "\"duration_frames\":277,\"palette_stride\":16,"
        "\"palette_frames\":[0,36,40,44,48,275],\"sprite_count\":0,"
        "\"sources\":["
        "{\"role\":\"descriptor\",\"source_entry\":\"prg/fixed\","
        "\"source_offset\":%llu,\"cpu_address\":%u,\"size\":7},"
        "{\"role\":\"compressed-screen\",\"source_entry\":\"prg/bank00\","
        "\"source_offset\":%llu,\"bank\":0,\"cpu_address\":%u,"
        "\"encoded_size\":%llu,\"decoded_size\":1024,\"visible_page\":0},"
        "{\"role\":\"background-palette\",\"source_entry\":\"prg/bank00\","
        "\"source_offset\":%llu,\"bank\":0,\"cpu_address\":%u,\"size\":16,"
        "\"fingerprint_fnv1a32\":\"761B572D\"}],"
        "\"chr_resolution\":{\"entry\":\"chr/all\","
        "\"background_selectors\":[252,250]} }",
        prefix,
        TECMO_ASSET_PACK_PRESENTS_ID,
        (unsigned long long)provenance[0].descriptor_offset,
        TECMO_ASSET_PACK_SCREEN_DESCRIPTOR_CPU +
            TECMO_ASSET_PACK_PRESENTS_SCREEN_ID * TECMO_ASSET_PACK_SCREEN_DESCRIPTOR_SIZE,
        (unsigned long long)provenance[0].stream_offset,
        provenance[0].stream_cpu,
        (unsigned long long)provenance[0].stream_size,
        (unsigned long long)provenance[0].palette_offset,
        provenance[0].palette_cpu,
        (unsigned long long)provenance[0].selector_code_offset,
        TECMO_ASSET_PACK_PRESENTS_SELECTOR_CODE_CPU,
        (unsigned long long)provenance[0].sprite_table_offset,
        TECMO_ASSET_PACK_PRESENTS_SPRITE_TABLE_CPU,
        (unsigned long long)provenance[0].sprite_palette_offset,
        TECMO_ASSET_PACK_PRESENTS_SPRITE_PALETTE_CPU,
        (unsigned long long)provenance[0].y_base_operand_offset,
        (unsigned long long)provenance[0].tile_base_operand_offset,
        (unsigned long long)provenance[0].x_base_operand_offset,
        TECMO_ASSET_PACK_PRESENTS_Y_BASE_OPERAND_CPU,
        TECMO_ASSET_PACK_PRESENTS_TILE_BASE_OPERAND_CPU,
        TECMO_ASSET_PACK_PRESENTS_X_BASE_OPERAND_CPU,
        TECMO_ASSET_PACK_LICENSE_ID,
        (unsigned long long)provenance[1].descriptor_offset,
        TECMO_ASSET_PACK_SCREEN_DESCRIPTOR_CPU +
            TECMO_ASSET_PACK_LICENSE_SCREEN_ID * TECMO_ASSET_PACK_SCREEN_DESCRIPTOR_SIZE,
        (unsigned long long)provenance[1].stream_offset,
        provenance[1].stream_cpu,
        (unsigned long long)provenance[1].stream_size,
        (unsigned long long)provenance[1].palette_offset,
        provenance[1].palette_cpu);
}

static int append_arena_background_source_map_entry(
    char *buffer,
    size_t capacity,
    size_t *length,
    int *first,
    const TecmoArenaBackgroundProvenance *provenance)
{
    const char *prefix = *first != 0 ? "" : ",\n";

    *first = 0;
    return tecmo_asset_pack_append_text(
        buffer,
        capacity,
        length,
        "%s"
        "    {\"id\":\"%s\",\"kind\":\"arena-intro-background-layer\","
        "\"schema\":\"tecmo.arena-intro.background-layer/TATL-1\","
        "\"screen_id\":24,\"decoder_cpu_address\":%u,"
        "\"route\":{\"source_entry\":\"prg/bank%02u\",\"source_offset\":%llu,"
        "\"bank\":%u,\"cpu_address\":%u},"
        "\"descriptor\":{\"source_entry\":\"prg/fixed\",\"source_offset\":%llu,"
        "\"size\":7,\"bank\":%u,\"cpu_address\":%u},"
        "\"stream\":{\"source_entry\":\"prg/bank%02u\",\"source_offset\":%llu,"
        "\"encoded_size\":%llu,\"decoded_size\":2048,\"bank\":%u,"
        "\"cpu_address\":%u},"
        "\"palette\":{\"source_entry\":\"prg/bank%02u\",\"source_offset\":%llu,"
        "\"size\":16,\"bank\":%u,\"cpu_address\":%u},"
        "\"lower_chr_tables\":["
        "{\"source_entry\":\"prg/fixed\",\"source_offset\":%llu,\"bank\":%u,"
        "\"cpu_address\":%u,\"selector_cpu_address\":%u},"
        "{\"source_entry\":\"prg/fixed\",\"source_offset\":%llu,\"bank\":%u,"
        "\"cpu_address\":%u,\"selector_cpu_address\":%u}]}",
        prefix,
        TECMO_ASSET_PACK_ARENA_INTRO_BACKGROUND_ID,
        TECMO_ASSET_PACK_ARENA_DECODER_CPU,
        provenance->route_bank,
        (unsigned long long)provenance->route_source_offset,
        provenance->route_bank,
        provenance->route_cpu,
        (unsigned long long)provenance->descriptor_source_offset,
        provenance->descriptor_bank,
        provenance->descriptor_cpu,
        provenance->stream_bank,
        (unsigned long long)provenance->stream_source_offset,
        (unsigned long long)provenance->stream_encoded_size,
        provenance->stream_bank,
        provenance->stream_cpu,
        provenance->stream_bank,
        (unsigned long long)provenance->palette_source_offset,
        provenance->stream_bank,
        provenance->palette_cpu,
        (unsigned long long)provenance->lower_r0_table_source_offset,
        provenance->descriptor_bank,
        TECMO_ASSET_PACK_ARENA_LOWER_R0_TABLE_CPU,
        TECMO_ASSET_PACK_ARENA_LOWER_R0_TABLE_CPU +
            TECMO_ASSET_PACK_ARENA_LOWER_SELECTOR_INDEX,
        (unsigned long long)provenance->lower_r1_table_source_offset,
        provenance->descriptor_bank,
        TECMO_ASSET_PACK_ARENA_LOWER_R1_TABLE_CPU,
        TECMO_ASSET_PACK_ARENA_LOWER_R1_TABLE_CPU +
            TECMO_ASSET_PACK_ARENA_LOWER_SELECTOR_INDEX);
}

static int append_arena_sprite_groups_source_map_entry(
    char *buffer,
    size_t capacity,
    size_t *length,
    int *first,
    const TecmoArenaSpriteGroupsProvenance *provenance)
{
    const char *prefix = *first != 0 ? "" : ",\n";

    *first = 0;
    return tecmo_asset_pack_append_text(
        buffer,
        capacity,
        length,
        "%s"
        "    {\"id\":\"%s\",\"kind\":\"arena-intro-sprite-groups\","
        "\"schema\":\"tecmo.arena-intro.sprite-groups/TASG-2\","
        "\"palette\":{\"source_entry\":\"prg/bank04\",\"source_offset\":%llu,"
        "\"size\":16,\"bank\":4,\"cpu_address\":%u},"
        "\"pointer_table\":{\"source_entry\":\"prg/bank00\",\"source_offset\":%llu,"
        "\"size\":4,\"bank\":0,\"cpu_address\":%u},"
        "\"streams\":["
        "{\"kind\":\"jumbotron\",\"selector\":0,\"pointer_entry_cpu_address\":%u,"
        "\"source_entry\":\"prg/bank00\",\"source_offset\":%llu,\"size\":%u,"
        "\"bank\":0,\"cpu_address\":%u,\"record_count\":55},"
        "{\"kind\":\"goal\",\"selector\":1,\"pointer_entry_cpu_address\":%u,"
        "\"source_entry\":\"prg/bank00\",\"source_offset\":%llu,\"size\":%u,"
        "\"bank\":0,\"cpu_address\":%u,\"record_count\":16}],"
        "\"bank04\":{"
        "\"seeds\":{\"source_entry\":\"prg/bank04\",\"source_offset\":%llu,"
        "\"size\":4,\"bank\":4,\"cpu_address\":%u},"
        "\"emitter\":{\"source_entry\":\"prg/bank04\",\"source_offset\":%llu,"
        "\"size\":%u,\"bank\":4,\"cpu_address\":%u},"
        "\"params\":{\"source_entry\":\"prg/bank04\",\"source_offset\":%llu,"
        "\"size\":4,\"bank\":4,\"cpu_address\":%u}},"
        "\"mapper\":{\"sprite_size\":\"8x16\",\"r2\":8,\"r3\":9},"
        "\"chr_pages\":["
        "{\"source_entry\":\"chr/bank01\",\"source_offset\":%llu,\"size\":1024,"
        "\"mapper_register\":2,\"selector\":8,\"chr_offset\":8192},"
        "{\"source_entry\":\"chr/bank01\",\"source_offset\":%llu,\"size\":1024,"
        "\"mapper_register\":3,\"selector\":9,\"chr_offset\":9216}]}",
        prefix,
        TECMO_ASSET_PACK_ARENA_INTRO_SPRITE_GROUPS_ID,
        (unsigned long long)provenance->palette_source_offset,
        TECMO_ASSET_PACK_ARENA_PALETTE_CPU,
        (unsigned long long)provenance->pointer_table_source_offset,
        TECMO_ASSET_PACK_ARENA_POINTER_TABLE_CPU,
        TECMO_ASSET_PACK_ARENA_POINTER_TABLE_CPU,
        (unsigned long long)provenance->stream_source_offset[0],
        provenance->stream_size[0],
        provenance->stream_cpu[0],
        TECMO_ASSET_PACK_ARENA_POINTER_TABLE_CPU + 2U,
        (unsigned long long)provenance->stream_source_offset[1],
        provenance->stream_size[1],
        provenance->stream_cpu[1],
        (unsigned long long)provenance->seeds_source_offset,
        TECMO_ASSET_PACK_ARENA_SEEDS_CPU,
        (unsigned long long)provenance->emitter_source_offset,
        TECMO_ASSET_PACK_ARENA_SPRITE_EMIT_SIZE,
        TECMO_ASSET_PACK_ARENA_SPRITE_EMIT_CPU,
        (unsigned long long)provenance->params_source_offset,
        TECMO_ASSET_PACK_ARENA_PARAMS_CPU,
        (unsigned long long)provenance->chr_page_source_offset[0],
        (unsigned long long)provenance->chr_page_source_offset[1]);
}

static int append_post_arena_source_map_entries(char *buffer,
                                                size_t capacity,
                                                size_t *length,
                                                int *first,
                                                const TecmoPostArenaProvenance *provenance)
{
    const char *prefix = *first != 0 ? "" : ",\n";
    int result;

    *first = 0;
    result = tecmo_asset_pack_append_text(
        buffer,
        capacity,
        length,
        "%s"
        "    {\"id\":\"%s\",\"kind\":\"post-arena-ready-native\","
        "\"schema\":\"tecmo.intro.ready/TRDY-1\",\"sources\":["
        "{\"role\":\"route\",\"source_entry\":\"prg/bank04\",\"source_offset\":%llu,"
        "\"bank\":4,\"cpu_address\":%u,\"size\":28},"
        "{\"role\":\"descriptor\",\"source_entry\":\"prg/fixed\",\"source_offset\":%llu,"
        "\"cpu_address\":%u,\"size\":7},"
        "{\"role\":\"compressed-screen\",\"source_entry\":\"prg/bank04\","
        "\"source_offset\":%llu,\"bank\":4,\"cpu_address\":47858,\"size\":%llu},"
        "{\"role\":\"palette\",\"source_entry\":\"prg/bank04\",\"source_offset\":%llu,"
        "\"bank\":4,\"cpu_address\":48481,\"size\":16},"
        "{\"role\":\"reveal-script\",\"source_entry\":\"prg/bank04\","
        "\"source_offset\":%llu,\"bank\":4,\"cpu_address\":%u,\"size\":48}],"
        "\"chr_resolution\":\"resolved offsets into chr/all\"},\n"
        "    {\"id\":\"%s\",\"kind\":\"post-arena-warriors-native\","
        "\"schema\":\"tecmo.intro.warriors/TWAR-1\",\"sources\":["
        "{\"role\":\"route\",\"source_entry\":\"prg/bank04\",\"source_offset\":%llu,"
        "\"bank\":4,\"cpu_address\":%u,\"size\":459},"
        "{\"role\":\"descriptor\",\"source_entry\":\"prg/fixed\",\"source_offset\":%llu,"
        "\"cpu_address\":%u,\"size\":7},"
        "{\"role\":\"compressed-screen\",\"source_entry\":\"prg/bank01\","
        "\"source_offset\":%llu,\"bank\":1,\"cpu_address\":46277,\"size\":%llu},"
        "{\"role\":\"background-palette\",\"source_entry\":\"prg/bank01\","
        "\"source_offset\":%llu,\"bank\":1,\"cpu_address\":46555,\"size\":16},"
        "{\"role\":\"sprite-palette\",\"source_entry\":\"prg/bank04\","
        "\"source_offset\":%llu,\"bank\":4,\"cpu_address\":35309,\"size\":16},"
        "{\"role\":\"player-sprite-pointer\",\"source_entry\":\"prg/bank00\","
        "\"source_offset\":%llu,\"bank\":0,\"cpu_address\":43279,\"size\":2},"
        "{\"role\":\"player-sprite-stream\",\"source_entry\":\"prg/bank00\","
        "\"source_offset\":%llu,\"bank\":0,\"cpu_address\":43289,\"size\":185},"
        "{\"role\":\"wordmark-glyphs\",\"source_entry\":\"prg/bank06\","
        "\"bank\":6,\"cpu_addresses\":[44885,44805,44865,44865,44833,44857,44865,44869],"
        "\"source_offsets\":[%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu],"
        "\"glyph_count\":8,\"tile_count\":32},"
        "{\"role\":\"patch-one\",\"source_entry\":\"prg/bank01\","
        "\"source_offset\":%llu,\"bank\":1,\"cpu_address\":48300,\"size\":64},"
        "{\"role\":\"patch-two\",\"source_entry\":\"prg/bank01\","
        "\"source_offset\":%llu,\"bank\":1,\"cpu_address\":48364,\"size\":64}],"
        "\"chr_resolution\":\"moving/lower/sprite offsets into chr/all\"},\n"
        "    {\"id\":\"%s\",\"kind\":\"post-arena-clippers-native\","
        "\"schema\":\"tecmo.intro.clippers/TCLP-1\",\"sources\":["
        "{\"role\":\"route\",\"source_entry\":\"prg/bank04\",\"source_offset\":%llu,"
        "\"bank\":4,\"cpu_address\":%u},"
        "{\"role\":\"descriptor\",\"source_entry\":\"prg/fixed\",\"source_offset\":%llu,"
        "\"cpu_address\":%u,\"size\":7},"
        "{\"role\":\"compressed-screen\",\"source_entry\":\"prg/bank01\","
        "\"source_offset\":%llu,\"bank\":1,\"cpu_address\":46571,\"size\":%llu},"
        "{\"role\":\"palette\",\"source_entry\":\"prg/bank01\","
        "\"source_offset\":%llu,\"bank\":1,\"cpu_address\":47044,\"size\":16},"
        "{\"role\":\"irq-handler\",\"source_entry\":\"prg/fixed\","
        "\"source_offset\":%llu,\"cpu_address\":64900,\"size\":101},"
        "{\"role\":\"irq-split-table\",\"source_entry\":\"prg/fixed\","
        "\"source_offset\":%llu,\"cpu_address\":64983,\"size\":6},"
        "{\"role\":\"team-name\",\"source_entry\":\"prg/bank06\","
        "\"source_offset\":%llu,\"bank\":6,\"cpu_address\":44195,\"size\":9},"
        "{\"role\":\"wordmark-glyph-table\",\"source_entry\":\"prg/bank06\","
        "\"source_offset\":%llu,\"bank\":6,\"cpu_address\":44805}],"
        "\"chr_resolution\":\"base and fixed-lower-band offsets into chr/all\"},\n"
        "    {\"id\":\"%s\",\"kind\":\"post-arena-bucks-native\","
        "\"schema\":\"tecmo.intro.bucks/TBUC-1\",\"sources\":["
        "{\"role\":\"route\",\"source_entry\":\"prg/bank04\",\"source_offset\":%llu,"
        "\"bank\":4,\"cpu_address\":%u},"
        "{\"role\":\"descriptor\",\"source_entry\":\"prg/fixed\",\"source_offset\":%llu,"
        "\"cpu_address\":56628,\"size\":7},"
        "{\"role\":\"compressed-screen\",\"source_entry\":\"prg/bank01\","
        "\"source_offset\":%llu,\"bank\":1,\"cpu_address\":46081,\"size\":%llu},"
        "{\"role\":\"palette\",\"source_entry\":\"prg/bank01\",\"source_offset\":%llu,"
        "\"bank\":1,\"cpu_address\":46245,\"size\":16},"
        "{\"role\":\"flash-thresholds\",\"source_entry\":\"prg/bank04\","
        "\"source_offset\":%llu,\"bank\":4,\"cpu_address\":34979,\"size\":6},"
        "{\"role\":\"team-name\",\"source_entry\":\"prg/bank06\",\"source_offset\":%llu,"
        "\"bank\":6,\"cpu_address\":44216,\"size\":6},"
        "{\"role\":\"wordmark-glyph-table\",\"source_entry\":\"prg/bank06\","
        "\"source_offset\":%llu,\"bank\":6,\"cpu_address\":44805}],"
        "\"chr_resolution\":\"base and fixed-lower-band offsets into chr/all\"},\n"
        "    {\"id\":\"%s\",\"kind\":\"post-arena-pass-native\","
        "\"schema\":\"tecmo.intro.pass/TPAS-1\",\"sources\":["
        "{\"role\":\"route\",\"source_entry\":\"prg/bank04\",\"source_offset\":%llu,"
        "\"bank\":4,\"cpu_address\":%u},"
        "{\"role\":\"descriptor\",\"source_entry\":\"prg/fixed\",\"source_offset\":%llu,"
        "\"cpu_address\":56656,\"size\":7},"
        "{\"role\":\"compressed-screen\",\"source_entry\":\"prg/bank01\","
        "\"source_offset\":%llu,\"bank\":1,\"cpu_address\":47411,\"size\":%llu},"
        "{\"role\":\"background-palette\",\"source_entry\":\"prg/bank01\","
        "\"source_offset\":%llu,\"bank\":1,\"cpu_address\":47785,\"size\":16},"
        "{\"role\":\"helper-palette\",\"source_entry\":\"prg/bank04\","
        "\"source_offset\":%llu,\"bank\":4,\"cpu_address\":35325,\"size\":16},"
        "{\"role\":\"special-palette\",\"source_entry\":\"prg/bank04\","
        "\"source_offset\":%llu,\"bank\":4,\"cpu_address\":35357,\"size\":16},"
        "{\"role\":\"player-sprite-pointer\",\"source_entry\":\"prg/bank00\","
        "\"source_offset\":%llu,\"bank\":0,\"cpu_address\":43281,\"size\":2},"
        "{\"role\":\"player-ball-stream\",\"source_entry\":\"prg/bank00\","
        "\"source_offset\":%llu,\"bank\":0,\"cpu_address\":43474,\"size\":41}],"
        "\"chr_resolution\":\"background and sprite offsets into chr/all\"}",
        prefix,
        TECMO_ASSET_PACK_READY_ID,
        (unsigned long long)provenance->ready_route_offset,
        TECMO_ASSET_PACK_READY_ROUTE_CPU,
        (unsigned long long)provenance->ready_descriptor_offset,
        TECMO_ASSET_PACK_SCREEN_DESCRIPTOR_CPU +
            TECMO_ASSET_PACK_READY_SCREEN_ID * TECMO_ASSET_PACK_SCREEN_DESCRIPTOR_SIZE,
        (unsigned long long)provenance->ready_stream_offset,
        (unsigned long long)provenance->ready_stream_size,
        (unsigned long long)provenance->ready_palette_offset,
        (unsigned long long)provenance->ready_script_offset,
        TECMO_ASSET_PACK_READY_SCRIPT_CPU,
        TECMO_ASSET_PACK_WARRIORS_ID,
        (unsigned long long)provenance->warriors_route_offset,
        TECMO_ASSET_PACK_WARRIORS_ROUTE_CPU,
        (unsigned long long)provenance->warriors_descriptor_offset,
        TECMO_ASSET_PACK_SCREEN_DESCRIPTOR_CPU +
            TECMO_ASSET_PACK_WARRIORS_SCREEN_ID * TECMO_ASSET_PACK_SCREEN_DESCRIPTOR_SIZE,
        (unsigned long long)provenance->warriors_stream_offset,
        (unsigned long long)provenance->warriors_stream_size,
        (unsigned long long)provenance->warriors_palette_offset,
        (unsigned long long)provenance->warriors_sprite_palette_offset,
        (unsigned long long)provenance->warriors_pointer_offset,
        (unsigned long long)provenance->warriors_piece_stream_offset,
        (unsigned long long)provenance->warriors_wordmark_glyph_offset[0],
        (unsigned long long)provenance->warriors_wordmark_glyph_offset[1],
        (unsigned long long)provenance->warriors_wordmark_glyph_offset[2],
        (unsigned long long)provenance->warriors_wordmark_glyph_offset[3],
        (unsigned long long)provenance->warriors_wordmark_glyph_offset[4],
        (unsigned long long)provenance->warriors_wordmark_glyph_offset[5],
        (unsigned long long)provenance->warriors_wordmark_glyph_offset[6],
        (unsigned long long)provenance->warriors_wordmark_glyph_offset[7],
        (unsigned long long)provenance->warriors_patch_offset[0],
        (unsigned long long)provenance->warriors_patch_offset[1],
        TECMO_ASSET_PACK_CLIPPERS_ID,
        (unsigned long long)provenance->clippers_route_offset,
        TECMO_ASSET_PACK_CLIPPERS_ROUTE_CPU,
        (unsigned long long)provenance->clippers_descriptor_offset,
        TECMO_ASSET_PACK_SCREEN_DESCRIPTOR_CPU +
            TECMO_ASSET_PACK_CLIPPERS_SCREEN_ID * TECMO_ASSET_PACK_SCREEN_DESCRIPTOR_SIZE,
        (unsigned long long)provenance->clippers_stream_offset,
        (unsigned long long)provenance->clippers_stream_size,
        (unsigned long long)provenance->clippers_palette_offset,
        (unsigned long long)provenance->clippers_irq_table_offset[0],
        (unsigned long long)provenance->clippers_irq_table_offset[1],
        (unsigned long long)provenance->clippers_string_offset,
        (unsigned long long)provenance->clippers_glyph_table_offset,
        TECMO_ASSET_PACK_BUCKS_ID,
        (unsigned long long)provenance->bucks_route_offset,
        TECMO_ASSET_PACK_BUCKS_ROUTE_CPU,
        (unsigned long long)provenance->bucks_descriptor_offset,
        (unsigned long long)provenance->bucks_stream_offset,
        (unsigned long long)provenance->bucks_stream_size,
        (unsigned long long)provenance->bucks_palette_offset,
        (unsigned long long)provenance->bucks_threshold_offset,
        (unsigned long long)provenance->bucks_string_offset,
        (unsigned long long)provenance->bucks_glyph_table_offset,
        TECMO_ASSET_PACK_PASS_ID,
        (unsigned long long)provenance->pass_route_offset,
        TECMO_ASSET_PACK_PASS_ROUTE_CPU,
        (unsigned long long)provenance->pass_descriptor_offset,
        (unsigned long long)provenance->pass_stream_offset,
        (unsigned long long)provenance->pass_stream_size,
        (unsigned long long)provenance->pass_palette_offset,
        (unsigned long long)provenance->pass_helper_palette_offset,
        (unsigned long long)provenance->pass_special_palette_offset,
        (unsigned long long)provenance->pass_pointer_offset,
        (unsigned long long)provenance->pass_piece_stream_offset);
    return result;
}

static int append_finale_source_map_entry(char *buffer,
                                          size_t capacity,
                                          size_t *length,
                                          int *first,
                                          const TecmoFinaleProvenance *provenance)
{
    static const uint16_t descriptor_cpu[TECMO_ASSET_PACK_FINALE_SCREEN_COUNT] = {
        0xDD49U, 0xDD65U, 0xDD5EU, 0xDD73U, 0xDDC0U
    };
    static const uint16_t route_cpu[TECMO_ASSET_PACK_FINALE_ROUTE_COUNT] = {
        0x851CU, 0x83EAU, 0x852EU, 0x83AEU, 0x8310U
    };
    const char *prefix = *first != 0 ? "" : ",\n";
    int result;

    *first = 0;
    result = tecmo_asset_pack_append_text(
        buffer,
        capacity,
        length,
        "%s"
        "    {\"id\":\"%s\",\"kind\":\"intro-finale-native\","
        "\"schema\":\"tecmo.intro.finale/TFIN-1\",\"sources\":["
        "{\"role\":\"dispatch\",\"source_entry\":\"prg/bank04\","
        "\"source_offset\":%llu,\"bank\":4,\"cpu_address\":33487,\"size\":35},"
        "{\"role\":\"route-opening\",\"source_entry\":\"prg/bank04\","
        "\"source_offset\":%llu,\"bank\":4,\"cpu_address\":%u},"
        "{\"role\":\"route-short-loop\",\"source_entry\":\"prg/bank04\","
        "\"source_offset\":%llu,\"bank\":4,\"cpu_address\":%u},"
        "{\"role\":\"route-selector-two\",\"source_entry\":\"prg/bank04\","
        "\"source_offset\":%llu,\"bank\":4,\"cpu_address\":%u},"
        "{\"role\":\"route-staged\",\"source_entry\":\"prg/bank04\","
        "\"source_offset\":%llu,\"bank\":4,\"cpu_address\":%u},"
        "{\"role\":\"route-title\",\"source_entry\":\"prg/bank04\","
        "\"source_offset\":%llu,\"bank\":4,\"cpu_address\":%u},"
        "{\"role\":\"screen-descriptor-0\",\"source_entry\":\"prg/fixed\","
        "\"source_offset\":%llu,\"cpu_address\":%u,\"size\":7},"
        "{\"role\":\"screen-descriptor-1\",\"source_entry\":\"prg/fixed\","
        "\"source_offset\":%llu,\"cpu_address\":%u,\"size\":7},"
        "{\"role\":\"screen-descriptor-2\",\"source_entry\":\"prg/fixed\","
        "\"source_offset\":%llu,\"cpu_address\":%u,\"size\":7},"
        "{\"role\":\"screen-descriptor-3\",\"source_entry\":\"prg/fixed\","
        "\"source_offset\":%llu,\"cpu_address\":%u,\"size\":7},"
        "{\"role\":\"screen-descriptor-4\",\"source_entry\":\"prg/fixed\","
        "\"source_offset\":%llu,\"cpu_address\":%u,\"size\":7},"
        "{\"role\":\"compressed-screen-0\",\"source_entry\":\"prg/bank%02u\","
        "\"source_offset\":%llu,\"bank\":%u,\"cpu_address\":%u,\"size\":%llu},"
        "{\"role\":\"compressed-screen-1\",\"source_entry\":\"prg/bank%02u\","
        "\"source_offset\":%llu,\"bank\":%u,\"cpu_address\":%u,\"size\":%llu},"
        "{\"role\":\"compressed-screen-2\",\"source_entry\":\"prg/bank%02u\","
        "\"source_offset\":%llu,\"bank\":%u,\"cpu_address\":%u,\"size\":%llu},"
        "{\"role\":\"compressed-screen-3\",\"source_entry\":\"prg/bank%02u\","
        "\"source_offset\":%llu,\"bank\":%u,\"cpu_address\":%u,\"size\":%llu},"
        "{\"role\":\"compressed-screen-4\",\"source_entry\":\"prg/bank%02u\","
        "\"source_offset\":%llu,\"bank\":%u,\"cpu_address\":%u,\"size\":%llu},"
        "{\"role\":\"background-palette-0\",\"source_entry\":\"prg/bank%02u\","
        "\"source_offset\":%llu,\"bank\":%u,\"cpu_address\":%u,\"size\":16},"
        "{\"role\":\"background-palette-1\",\"source_entry\":\"prg/bank%02u\","
        "\"source_offset\":%llu,\"bank\":%u,\"cpu_address\":%u,\"size\":16},"
        "{\"role\":\"background-palette-2\",\"source_entry\":\"prg/bank%02u\","
        "\"source_offset\":%llu,\"bank\":%u,\"cpu_address\":%u,\"size\":16},"
        "{\"role\":\"background-palette-3\",\"source_entry\":\"prg/bank%02u\","
        "\"source_offset\":%llu,\"bank\":%u,\"cpu_address\":%u,\"size\":16},"
        "{\"role\":\"background-palette-4\",\"source_entry\":\"prg/bank%02u\","
        "\"source_offset\":%llu,\"bank\":%u,\"cpu_address\":%u,\"size\":16},"
        "{\"role\":\"short-anchor-tables\",\"source_entry\":\"prg/bank04\","
        "\"source_offset\":%llu,\"bank\":4,\"cpu_addresses\":[33891,33907],\"size_each\":16},"
        "{\"role\":\"selector-two-immediate\",\"source_entry\":\"prg/bank04\","
        "\"source_offset\":%llu,\"bank\":4,\"cpu_address\":34095,\"size\":1,\"selector\":2},"
        "{\"role\":\"selector-two-indexed-operands\",\"source_entry\":\"prg/bank04\","
        "\"source_offsets\":[%llu,%llu,%llu,%llu],"
        "\"bank\":4,\"cpu_addresses\":[34366,34368,34370,34372],\"selector\":2},"
        "{\"role\":\"sprite-pointer\",\"source_entry\":\"prg/bank00\","
        "\"source_offset\":%llu,\"bank\":0,\"cpu_address\":43281,\"size\":2},"
        "{\"role\":\"sprite-stream\",\"source_entry\":\"prg/bank00\","
        "\"source_offset\":%llu,\"bank\":0,\"cpu_address\":43474,\"size\":41},"
        "{\"role\":\"sprite-chr-selector-writes\",\"source_entry\":\"prg/bank04\","
        "\"source_offsets\":[%llu,%llu,%llu],\"bank\":4,"
        "\"cpu_addresses\":[34154,34158,34162],\"selectors\":[%u,%u,%u],"
        "\"validated_across\":[\"short-loop\",\"selector-two\",\"staged\"]},"
        "{\"role\":\"short-staged-sprite-palette\",\"source_entry\":\"prg/bank04\","
        "\"source_offset\":%llu,\"bank\":4,\"cpu_address\":35341,\"size\":16},"
        "{\"role\":\"selector-two-sprite-palette\",\"source_entry\":\"prg/bank04\","
        "\"source_offset\":%llu,\"bank\":4,\"cpu_address\":35325,\"size\":16},"
        "{\"role\":\"title-character-record\",\"source_entry\":\"prg/bank04\","
        "\"source_offset\":%llu,\"bank\":4,\"cpu_address\":33643,\"size\":26},"
        "{\"role\":\"glyph-map\",\"source_entry\":\"prg/bank06\","
        "\"source_offset\":%llu,\"bank\":6,\"cpu_address\":41587},"
        "{\"role\":\"glyph-quads\",\"source_entry\":\"prg/bank06\","
        "\"source_offset\":%llu,\"bank\":6,\"cpu_address\":44805},"
        "{\"role\":\"title-split\",\"source_entry\":\"prg/fixed\","
        "\"source_offset\":%llu,\"cpu_address\":65044,"
        "\"end_cpu_address\":65169,\"size\":126}],"
        "\"native_contract\":{\"screen_layers\":5,\"pages_per_layer\":2,"
        "\"one_page_mirror_layers\":[0,3],\"one_page_source_decoded_bytes\":1024,"
        "\"sprite_groups\":2,\"shared_piece_count\":10,"
        "\"title_slots\":44,\"title_source_slots\":26,\"blank_slots\":18,"
        "\"resolved_chr_entry\":\"chr/all\"}}",
        prefix,
        TECMO_ASSET_PACK_FINALE_ID,
        (unsigned long long)provenance->dispatch_offset,
        (unsigned long long)provenance->route_offsets[0], route_cpu[0],
        (unsigned long long)provenance->route_offsets[1], route_cpu[1],
        (unsigned long long)provenance->route_offsets[2], route_cpu[2],
        (unsigned long long)provenance->route_offsets[3], route_cpu[3],
        (unsigned long long)provenance->route_offsets[4], route_cpu[4],
        (unsigned long long)provenance->screens[0].descriptor_offset, descriptor_cpu[0],
        (unsigned long long)provenance->screens[1].descriptor_offset, descriptor_cpu[1],
        (unsigned long long)provenance->screens[2].descriptor_offset, descriptor_cpu[2],
        (unsigned long long)provenance->screens[3].descriptor_offset, descriptor_cpu[3],
        (unsigned long long)provenance->screens[4].descriptor_offset, descriptor_cpu[4],
        provenance->screens[0].stream_bank,
        (unsigned long long)provenance->screens[0].stream_offset,
        provenance->screens[0].stream_bank,
        provenance->screens[0].stream_cpu,
        (unsigned long long)provenance->screens[0].stream_size,
        provenance->screens[1].stream_bank,
        (unsigned long long)provenance->screens[1].stream_offset,
        provenance->screens[1].stream_bank,
        provenance->screens[1].stream_cpu,
        (unsigned long long)provenance->screens[1].stream_size,
        provenance->screens[2].stream_bank,
        (unsigned long long)provenance->screens[2].stream_offset,
        provenance->screens[2].stream_bank,
        provenance->screens[2].stream_cpu,
        (unsigned long long)provenance->screens[2].stream_size,
        provenance->screens[3].stream_bank,
        (unsigned long long)provenance->screens[3].stream_offset,
        provenance->screens[3].stream_bank,
        provenance->screens[3].stream_cpu,
        (unsigned long long)provenance->screens[3].stream_size,
        provenance->screens[4].stream_bank,
        (unsigned long long)provenance->screens[4].stream_offset,
        provenance->screens[4].stream_bank,
        provenance->screens[4].stream_cpu,
        (unsigned long long)provenance->screens[4].stream_size,
        provenance->screens[0].stream_bank,
        (unsigned long long)provenance->screens[0].palette_offset,
        provenance->screens[0].stream_bank,
        provenance->screens[0].palette_cpu,
        provenance->screens[1].stream_bank,
        (unsigned long long)provenance->screens[1].palette_offset,
        provenance->screens[1].stream_bank,
        provenance->screens[1].palette_cpu,
        provenance->screens[2].stream_bank,
        (unsigned long long)provenance->screens[2].palette_offset,
        provenance->screens[2].stream_bank,
        provenance->screens[2].palette_cpu,
        provenance->screens[3].stream_bank,
        (unsigned long long)provenance->screens[3].palette_offset,
        provenance->screens[3].stream_bank,
        provenance->screens[3].palette_cpu,
        provenance->screens[4].stream_bank,
        (unsigned long long)provenance->screens[4].palette_offset,
        provenance->screens[4].stream_bank,
        provenance->screens[4].palette_cpu,
        (unsigned long long)provenance->short_anchor_table_offset,
        (unsigned long long)provenance->reverse_selector_offset,
        (unsigned long long)provenance->reverse_indexed_operand_offset[0],
        (unsigned long long)provenance->reverse_indexed_operand_offset[1],
        (unsigned long long)provenance->reverse_indexed_operand_offset[2],
        (unsigned long long)provenance->reverse_indexed_operand_offset[3],
        (unsigned long long)provenance->sprite_pointer_offset,
        (unsigned long long)provenance->sprite_stream_offset,
        (unsigned long long)provenance->sprite_chr_selector_offset[0],
        (unsigned long long)provenance->sprite_chr_selector_offset[1],
        (unsigned long long)provenance->sprite_chr_selector_offset[2],
        provenance->sprite_chr_selector[0],
        provenance->sprite_chr_selector[1],
        provenance->sprite_chr_selector[2],
        (unsigned long long)provenance->sprite_palette_offset[0],
        (unsigned long long)provenance->sprite_palette_offset[1],
        (unsigned long long)provenance->title_source_offset,
        (unsigned long long)provenance->glyph_map_offset,
        (unsigned long long)provenance->glyph_table_offset,
        (unsigned long long)provenance->fixed_irq_offset);
    return result;
}

static int append_title_source_map_entries(char *buffer, size_t capacity,
                                           size_t *length, int *first,
                                           const TecmoTitleProvenance p[2])
{
    const char *prefix = *first != 0 ? "" : ",\n";
    *first = 0;
    return tecmo_asset_pack_append_text(
        buffer, capacity, length,
        "%s"
        "    {\"id\":\"%s\",\"kind\":\"title-attract-native\","
        "\"schema\":\"tecmo.title-attract/TATR-2\",\"sources\":["
        "{\"role\":\"descriptor\",\"source_entry\":\"prg/fixed\",\"source_offset\":%llu,\"cpu_address\":%u,\"size\":7},"
        "{\"role\":\"compressed-screen\",\"source_entry\":\"prg/bank00\",\"source_offset\":%llu,\"bank\":0,\"cpu_address\":%u,\"encoded_size\":%llu},"
        "{\"role\":\"initial-palettes\",\"source_entry\":\"prg/bank00\",\"source_offset\":%llu,\"bank\":0,\"cpu_address\":%u,\"size\":32},"
        "{\"role\":\"final-sprite-palette\",\"source_entry\":\"prg/bank00\",\"source_offset\":%llu,\"bank\":0,\"cpu_address\":%u,\"size\":16},"
        "{\"role\":\"sprite-records\",\"source_entry\":\"prg/bank01\",\"source_offset\":%llu,\"bank\":1,\"cpu_address\":%u,\"size\":197},"
        "{\"role\":\"attribute-state-a\",\"source_entry\":\"prg/bank00\",\"source_offset\":%llu,\"bank\":0,\"cpu_address\":%u,\"size\":16},"
        "{\"role\":\"attribute-state-b\",\"source_entry\":\"prg/bank00\",\"source_offset\":%llu,\"bank\":0,\"cpu_address\":%u,\"size\":16}]},\n"
        "    {\"id\":\"%s\",\"kind\":\"title-start-native\","
        "\"schema\":\"tecmo.title-start/TTLE-1\",\"sources\":["
        "{\"role\":\"descriptor\",\"source_entry\":\"prg/fixed\",\"source_offset\":%llu,\"cpu_address\":%u,\"size\":7},"
        "{\"role\":\"compressed-screen\",\"source_entry\":\"prg/bank00\",\"source_offset\":%llu,\"bank\":0,\"cpu_address\":%u,\"encoded_size\":%llu},"
        "{\"role\":\"palette\",\"source_entry\":\"prg/bank00\",\"source_offset\":%llu,\"bank\":0,\"cpu_address\":%u,\"size\":16},"
        "{\"role\":\"prompt-blank\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"bank\":3,\"cpu_address\":%u,\"size\":10},"
        "{\"role\":\"prompt-visible\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"bank\":3,\"cpu_address\":%u,\"size\":10}]}",
        prefix, TECMO_ASSET_PACK_TITLE_ATTRACT_ID,
        (unsigned long long)p[0].descriptor_offset, 0xDC8CU,
        (unsigned long long)p[0].stream_offset, p[0].stream_cpu,
        (unsigned long long)p[0].stream_size,
        (unsigned long long)p[0].palette_offset, TECMO_ASSET_PACK_TITLE_PALETTE_CPU,
        (unsigned long long)p[0].sprite_palette_offset, TECMO_ASSET_PACK_TITLE_SPRITE_PALETTE_CPU,
        (unsigned long long)p[0].sprite_table_offset, TECMO_ASSET_PACK_TITLE_SPRITE_TABLE_CPU,
        (unsigned long long)p[0].attr_a_offset, TECMO_ASSET_PACK_TITLE_ATTR_A_CPU,
        (unsigned long long)p[0].attr_b_offset, TECMO_ASSET_PACK_TITLE_ATTR_B_CPU,
        TECMO_ASSET_PACK_TITLE_SCREEN_ID,
        (unsigned long long)p[1].descriptor_offset, 0xDC9AU,
        (unsigned long long)p[1].stream_offset, p[1].stream_cpu,
        (unsigned long long)p[1].stream_size,
        (unsigned long long)p[1].palette_offset, TECMO_ASSET_PACK_TITLE_PALETTE_CPU,
        (unsigned long long)p[1].prompt_blank_offset, TECMO_ASSET_PACK_TITLE_PROMPT_BLANK_CPU,
        (unsigned long long)p[1].prompt_visible_offset, TECMO_ASSET_PACK_TITLE_PROMPT_VISIBLE_CPU);
}

static int append_start_menu_source_map_entry(char *buffer,
                                              size_t capacity,
                                              size_t *length,
                                              int *first,
                                              const TecmoStartGameMenuProvenance *p)
{
    const char *prefix = *first != 0 ? "" : ",\n";
    *first = 0;
    return tecmo_asset_pack_append_text(
        buffer, capacity, length,
        "%s"
        "    {\"id\":\"%s\",\"kind\":\"start-game-menu-native\","
        "\"schema\":\"tecmo.start-game-menu/TSGM-1\",\"screen_id\":4,"
        "\"input_contract\":\"ines-only\",\"sources\":["
        "{\"role\":\"descriptor\",\"source_entry\":\"prg/fixed\",\"source_offset\":%llu,\"cpu_address\":%u,\"size\":7,\"fingerprint_fnv1a32\":\"0A5B3B88\"},"
        "{\"role\":\"compressed-screen\",\"source_entry\":\"prg/bank00\",\"source_offset\":%llu,\"bank\":0,\"cpu_address\":%u,\"encoded_size\":%llu,\"decoded_size\":2048,\"encoded_fingerprint_fnv1a32\":\"8047E031\",\"decoded_fingerprint_fnv1a32\":\"E1840CFE\"},"
        "{\"role\":\"menu-background-palette\",\"source_entry\":\"prg/bank00\",\"source_offset\":%llu,\"bank\":0,\"cpu_address\":%u,\"size\":16,\"fingerprint_fnv1a32\":\"F16D31BF\"},"
        "{\"role\":\"title-background-palette\",\"source_entry\":\"prg/bank00\",\"source_offset\":%llu,\"bank\":0,\"cpu_address\":%u,\"size\":16,\"fingerprint_fnv1a32\":\"BBF7850B\"},"
        "{\"role\":\"title-sprite-palette\",\"source_entry\":\"prg/bank00\",\"source_offset\":%llu,\"bank\":0,\"cpu_address\":%u,\"size\":16,\"fingerprint_fnv1a32\":\"ACF5D9A1\"},"
        "{\"role\":\"menu-sprite-selectors\",\"source_entry\":\"prg/bank00\",\"source_offset\":%llu,\"bank\":0,\"cpu_address\":%u,\"size\":7,\"selectors\":[244,245],\"fingerprint_fnv1a32\":\"23BDC5CE\"},"
        "{\"role\":\"menu-sprite-palette\",\"source_entry\":\"prg/bank00\",\"source_offset\":%llu,\"bank\":0,\"cpu_address\":%u,\"size\":16,\"fingerprint_fnv1a32\":\"F85BA74A\"},"
        "{\"role\":\"nba-emblem\",\"source_entry\":\"prg/bank01\",\"source_offset\":%llu,\"bank\":1,\"cpu_address\":%u,\"size\":197,\"piece_count\":49,\"fingerprint_fnv1a32\":\"669E53D3\"},"
        "{\"role\":\"root-cursor\",\"source_entry\":\"prg/bank01\",\"source_offset\":%llu,\"bank\":1,\"cpu_address\":%u,\"size\":5,\"selector\":48,\"tile\":36,\"resolved_chr_entry\":\"chr/all\",\"resolved_chr_offset\":49728,\"resolved_chr_size\":32,\"resolved_chr_fingerprint_fnv1a32\":\"18F367C4\",\"fingerprint_fnv1a32\":\"7D5835D4\"},"
        "{\"role\":\"character-map\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"bank\":3,\"cpu_address\":%u,\"size\":91,\"fingerprint_fnv1a32\":\"724F80CE\"},"
        "{\"role\":\"root-menu-record\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"bank\":3,\"cpu_address\":%u,\"size\":110,\"fingerprint_fnv1a32\":\"8CFF0188\"},"
        "{\"role\":\"season-menu-record\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"bank\":3,\"cpu_address\":%u,\"size\":101,\"fingerprint_fnv1a32\":\"01620B02\"},"
        "{\"role\":\"music-popup-record\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"bank\":3,\"cpu_address\":%u,\"size\":21,\"fingerprint_fnv1a32\":\"A144CD78\"},"
        "{\"role\":\"speed-popup-record\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"bank\":3,\"cpu_address\":%u,\"size\":33,\"fingerprint_fnv1a32\":\"BE4B508D\"},"
        "{\"role\":\"period-popup-record\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"bank\":3,\"cpu_address\":%u,\"size\":37,\"fingerprint_fnv1a32\":\"30B68A59\"},"
        "{\"role\":\"period-values\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"bank\":3,\"cpu_address\":%u,\"size\":5,\"fingerprint_fnv1a32\":\"0F3A2C36\"},"
        "{\"role\":\"screen-loader\",\"source_entry\":\"prg/fixed\",\"source_offset\":%llu,\"cpu_address\":%u,\"size\":122,\"fingerprint_fnv1a32\":\"835283BE\"},"
        "{\"role\":\"fade-out\",\"source_entry\":\"prg/fixed\",\"source_offset\":%llu,\"cpu_address\":%u,\"size\":99,\"fingerprint_fnv1a32\":\"5D98AB7A\"},"
        "{\"role\":\"fade-in\",\"source_entry\":\"prg/fixed\",\"source_offset\":%llu,\"cpu_address\":%u,\"size\":72,\"fingerprint_fnv1a32\":\"D75D6EEA\"},"
        "{\"role\":\"season-transition\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"bank\":3,\"cpu_address\":%u,\"size\":161,\"fingerprint_fnv1a32\":\"7CBACC29\"},"
        "{\"role\":\"root-input-parameters\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"bank\":3,\"cpu_address\":%u,\"size\":145,\"fingerprint_fnv1a32\":\"DE568C7B\"},"
        "{\"role\":\"pointer-coordinate-tables\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"bank\":3,\"cpu_address\":%u,\"size\":247,\"fingerprint_fnv1a32\":\"218E8BCB\"}],"
        "\"native_contract\":{\"pages\":2,\"cells\":1920,\"payload_size\":14112,"
        "\"payload_fingerprint_fnv1a32\":\"A6C1E06B\","
        "\"palette_stages_fingerprint_fnv1a32\":\"F83B6C17\","
        "\"composed_fingerprint_fnv1a32\":\"661750F3\","
        "\"palette_stage_frames\":[0,2,4,6,8,20,24,28,32],\"root_items\":7,\"season_items\":6,"
        "\"direction_repeat_frames\":8,\"season_transition_frames\":32,\"period_value_count\":5,"
        "\"background_pixels_per_frame\":8,\"emblem_pixels_per_frame\":5,"
        "\"resolved_chr_entry\":\"chr/all\"}}",
        prefix, TECMO_ASSET_PACK_START_GAME_MENU_ID,
        (unsigned long long)p->descriptor_offset, TECMO_ASSET_PACK_START_MENU_DESCRIPTOR_CPU,
        (unsigned long long)p->stream_offset, TECMO_ASSET_PACK_START_MENU_STREAM_CPU,
        (unsigned long long)p->stream_size,
        (unsigned long long)p->background_palette_offset, TECMO_ASSET_PACK_START_MENU_BG_PALETTE_CPU,
        (unsigned long long)p->title_background_palette_offset, TECMO_ASSET_PACK_START_MENU_TITLE_BG_PALETTE_CPU,
        (unsigned long long)p->title_sprite_palette_offset, TECMO_ASSET_PACK_START_MENU_TITLE_SPRITE_PALETTE_CPU,
        (unsigned long long)p->sprite_setup_offset, TECMO_ASSET_PACK_START_MENU_SPRITE_SETUP_CPU,
        (unsigned long long)p->sprite_palette_offset, TECMO_ASSET_PACK_START_MENU_SPRITE_PALETTE_CPU,
        (unsigned long long)p->emblem_offset, TECMO_ASSET_PACK_START_MENU_EMBLEM_CPU,
        (unsigned long long)p->cursor_offset, TECMO_ASSET_PACK_START_MENU_CURSOR_CPU,
        (unsigned long long)p->char_map_offset, TECMO_ASSET_PACK_START_MENU_CHAR_MAP_CPU,
        (unsigned long long)p->main_record_offset, TECMO_ASSET_PACK_START_MENU_MAIN_RECORD_CPU,
        (unsigned long long)p->season_record_offset, TECMO_ASSET_PACK_START_MENU_SEASON_RECORD_CPU,
        (unsigned long long)p->music_record_offset, TECMO_ASSET_PACK_START_MENU_MUSIC_RECORD_CPU,
        (unsigned long long)p->speed_record_offset, TECMO_ASSET_PACK_START_MENU_SPEED_RECORD_CPU,
        (unsigned long long)p->period_record_offset, TECMO_ASSET_PACK_START_MENU_PERIOD_RECORD_CPU,
        (unsigned long long)p->period_values_offset, TECMO_ASSET_PACK_START_MENU_PERIOD_VALUES_CPU,
        (unsigned long long)p->loader_offset, TECMO_ASSET_PACK_START_MENU_LOADER_CPU,
        (unsigned long long)p->fade_out_offset, TECMO_ASSET_PACK_START_MENU_FADE_OUT_CPU,
        (unsigned long long)p->fade_in_offset, TECMO_ASSET_PACK_START_MENU_FADE_IN_CPU,
        (unsigned long long)p->season_route_offset, TECMO_ASSET_PACK_START_MENU_SEASON_ROUTE_CPU,
        (unsigned long long)p->input_params_offset, TECMO_ASSET_PACK_START_MENU_INPUT_PARAMS_CPU,
        (unsigned long long)p->pointer_coord_offset, TECMO_ASSET_PACK_START_MENU_POINTER_COORD_CPU);
}

char *tecmo_asset_pack_build_ines_source_map(uint32_t mapper,
                                   uint32_t trainer_bytes,
                                   uint32_t prg_banks,
                                   uint32_t chr_banks,
                                   uint64_t prg_offset,
                                   uint64_t chr_offset,
                                   uint64_t chr_size,
                                   const TecmoOpeningScreenProvenance opening_provenance[2],
                                   const TecmoArenaBackgroundProvenance *background_provenance,
                                   const TecmoArenaSpriteGroupsProvenance *sprite_groups_provenance,
                                   const TecmoPostArenaProvenance *post_arena_provenance,
                                   const TecmoFinaleProvenance *finale_provenance,
                                   const TecmoTitleProvenance title_provenance[2],
                                   const TecmoStartGameMenuProvenance *start_menu_provenance,
                                   size_t *source_map_size_out)
{
    size_t entry_count = (size_t)prg_banks + (size_t)chr_banks + 14U;
    size_t capacity;
    size_t length = 0U;
    char *source_map;
    uint64_t script_source_offset =
        source_map_prg_bank_cpu_source_offset(prg_offset,
                                   prg_banks,
                                   TECMO_ASSET_PACK_ARENA_BANK04,
                                   TECMO_ASSET_PACK_ARENA_ROUTE_CPU);
    uint64_t palette_source_offset =
        source_map_prg_bank_cpu_source_offset(prg_offset,
                                   prg_banks,
                                   TECMO_ASSET_PACK_ARENA_BANK04,
                                   TECMO_ASSET_PACK_ARENA_PALETTE_CPU);
    int bank04_available = prg_banks > TECMO_ASSET_PACK_ARENA_BANK04;
    int first = 1;
    int first_logical = 1;

    if (entry_count > (SIZE_MAX - 24576U) / 320U) {
        return NULL;
    }
    capacity = 32768U + entry_count * 320U;
    source_map = (char *)malloc(capacity);
    if (source_map == NULL) {
        return NULL;
    }

    if (tecmo_asset_pack_append_text(source_map,
                    capacity,
                    &length,
                    "{\n"
                    "  \"format\":\"tecmo.assetpack.source-map/1\",\n"
                    "  \"source\":{\n"
                    "    \"kind\":\"ines\",\n"
                    "    \"mapper\":%u,\n"
                    "    \"trainer_bytes\":%u,\n"
                    "    \"prg_offset\":%llu,\n"
                    "    \"prg_bank_bytes\":%llu,\n"
                    "    \"prg_banks\":%u,\n"
                    "    \"chr_offset\":%llu,\n"
                    "    \"chr_bank_bytes\":%llu,\n"
                    "    \"chr_banks\":%u\n"
                    "  },\n"
                    "  \"raw_entries\":[\n",
                    mapper,
                    trainer_bytes,
                    (unsigned long long)prg_offset,
                    (unsigned long long)TECMO_ASSET_PACK_PRG_BANK_BYTES,
                    prg_banks,
                    (unsigned long long)chr_offset,
                    (unsigned long long)TECMO_ASSET_PACK_CHR_BANK_BYTES,
                    chr_banks) != 0) {
        free(source_map);
        return NULL;
    }

    for (uint32_t bank = 0; bank < prg_banks; ++bank) {
        char id[TECMO_ASSET_PACK_ID_SIZE];
        uint64_t offset = prg_offset + (uint64_t)bank * TECMO_ASSET_PACK_PRG_BANK_BYTES;

        (void)snprintf(id, sizeof(id), "prg/bank%02u", bank);
        if (append_source_map_entry(source_map,
                                    capacity,
                                    &length,
                                    &first,
                                    id,
                                    "raw-prg-bank",
                                    offset,
                                    TECMO_ASSET_PACK_PRG_BANK_BYTES,
                                    bank,
                                    0x8000U) != 0) {
            free(source_map);
            return NULL;
        }
    }

    {
        uint32_t fixed_bank = prg_banks - 1U;
        uint64_t offset = prg_offset + (uint64_t)fixed_bank * TECMO_ASSET_PACK_PRG_BANK_BYTES;

        if (append_source_map_entry(source_map,
                                    capacity,
                                    &length,
                                    &first,
                                    "prg/fixed",
                                    "raw-prg-fixed-alias",
                                    offset,
                                    TECMO_ASSET_PACK_PRG_BANK_BYTES,
                                    fixed_bank,
                                    0xC000U) != 0) {
            free(source_map);
            return NULL;
        }
    }

    if (chr_size > 0U) {
        if (append_source_map_entry(source_map,
                                    capacity,
                                    &length,
                                    &first,
                                    "chr/all",
                                    "raw-chr-range",
                                    chr_offset,
                                    chr_size,
                                    0U,
                                    0U) != 0) {
            free(source_map);
            return NULL;
        }

        for (uint32_t bank = 0; bank < chr_banks; ++bank) {
            char id[TECMO_ASSET_PACK_ID_SIZE];
            uint64_t offset = chr_offset + (uint64_t)bank * TECMO_ASSET_PACK_CHR_BANK_BYTES;

            (void)snprintf(id, sizeof(id), "chr/bank%02u", bank);
            if (append_source_map_entry(source_map,
                                        capacity,
                                        &length,
                                        &first,
                                        id,
                                        "raw-chr-bank",
                                        offset,
                                        TECMO_ASSET_PACK_CHR_BANK_BYTES,
                                        bank,
                                        0U) != 0) {
                free(source_map);
                return NULL;
            }
        }
    }

    if (tecmo_asset_pack_append_text(source_map,
                    capacity,
                    &length,
                    "\n"
                    "  ],\n"
                    "  \"logical_entries\":[\n") != 0) {
        free(source_map);
        return NULL;
    }

    if (append_opening_screen_source_map_entries(source_map,
                                                 capacity,
                                                 &length,
                                                 &first_logical,
                                                 opening_provenance) != 0 ||
        append_logical_source_map_entry(source_map,
                                        capacity,
                                        &length,
                                        &first_logical,
                                        TECMO_ASSET_PACK_ARENA_INTRO_SCRIPT_ID,
                                        "arena-intro-native-script",
                                        "tecmo.arena-intro.script/1",
                                        bank04_available ? "prg/bank04" : "prg/fixed",
                                        script_source_offset,
                                        bank04_available ? TECMO_ASSET_PACK_ARENA_BANK04 : prg_banks - 1U,
                                        TECMO_ASSET_PACK_ARENA_ROUTE_CPU,
                                        bank04_available) != 0 ||
        append_arena_background_source_map_entry(source_map,
                                                 capacity,
                                                 &length,
                                                 &first_logical,
                                                 background_provenance) != 0 ||
        append_logical_source_map_entry(source_map,
                                        capacity,
                                        &length,
                                        &first_logical,
                                        TECMO_ASSET_PACK_ARENA_INTRO_PALETTE_ID,
                                        "arena-intro-palette-cycle",
                                        "tecmo.arena-intro.palette-cycle/1",
                                        bank04_available ? "prg/bank04" : "prg/fixed",
                                        palette_source_offset,
                                        bank04_available ? TECMO_ASSET_PACK_ARENA_BANK04 : prg_banks - 1U,
                                        TECMO_ASSET_PACK_ARENA_PALETTE_CPU,
                                        bank04_available) != 0 ||
        append_arena_sprite_groups_source_map_entry(source_map,
                                                    capacity,
                                                    &length,
                                                    &first_logical,
                                                    sprite_groups_provenance) != 0 ||
        append_post_arena_source_map_entries(source_map,
                                             capacity,
                                             &length,
                                             &first_logical,
                                             post_arena_provenance) != 0 ||
        append_finale_source_map_entry(source_map,
                                       capacity,
                                       &length,
                                       &first_logical,
                                       finale_provenance) != 0 ||
        (title_provenance[0].stream_cpu != 0U &&
         append_title_source_map_entries(source_map, capacity, &length,
                                         &first_logical, title_provenance) != 0) ||
        (start_menu_provenance->stream_size != 0U &&
         append_start_menu_source_map_entry(source_map, capacity, &length,
                                            &first_logical, start_menu_provenance) != 0)) {
        free(source_map);
        return NULL;
    }

    if (tecmo_asset_pack_append_text(source_map,
                    capacity,
                    &length,
                    "\n"
                    "  ],\n"
                    "  \"input_contract\":\"ines-only\",\n"
                    "  \"logical_entry_note\":\"ROM-only pack with sanitized native opening and arena intro entries; no decomp, capture, or loose-file entries are imported\"\n"
                    "}\n") != 0) {
        free(source_map);
        return NULL;
    }

    *source_map_size_out = length;
    return source_map;
}
