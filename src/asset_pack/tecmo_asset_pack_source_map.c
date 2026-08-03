#define _CRT_SECURE_NO_WARNINGS

#include "tecmo_asset_pack_source_map.h"

#include "tecmo_asset_pack.h"
#include "tecmo_asset_pack_util.h"
#include "tecmo_gameplay_audio.h"

#include <stdio.h>
#include <stdlib.h>

#define TECMO_ASSET_PACK_SOURCE_MAP_BASE_CAPACITY 131072U

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
        "\"input_contract\":\"ines-only\",\"runtime_dependencies\":["
        "{\"entry\":\"title/start-screen\",\"schema\":\"tecmo.title-start/TTLE-1\",\"payload_size\":5860,\"frame_start\":0,\"frame_end\":7},"
        "{\"entry\":\"chr/all\",\"size\":262144,\"fingerprint_fnv1a32\":\"F6F6E854\",\"fingerprint_fnv1a64\":\"96A64F53B240ABB4\"}],"
        "\"sources\":["
        "{\"role\":\"descriptor\",\"source_entry\":\"prg/fixed\",\"source_offset\":%llu,\"cpu_address\":%u,\"size\":7,\"fingerprint_fnv1a32\":\"0A5B3B88\"},"
        "{\"role\":\"compressed-screen\",\"source_entry\":\"prg/bank00\",\"source_offset\":%llu,\"bank\":0,\"cpu_address\":%u,\"encoded_size\":%llu,\"decoded_size\":2048,\"encoded_fingerprint_fnv1a32\":\"8047E031\",\"decoded_fingerprint_fnv1a32\":\"E1840CFE\"},"
        "{\"role\":\"menu-background-palette\",\"source_entry\":\"prg/bank00\",\"source_offset\":%llu,\"bank\":0,\"cpu_address\":%u,\"size\":16,\"fingerprint_fnv1a32\":\"F16D31BF\"},"
        "{\"role\":\"title-background-palette\",\"source_entry\":\"prg/bank00\",\"source_offset\":%llu,\"bank\":0,\"cpu_address\":%u,\"size\":16,\"fingerprint_fnv1a32\":\"BBF7850B\"},"
        "{\"role\":\"title-sprite-palette\",\"source_entry\":\"prg/bank00\",\"source_offset\":%llu,\"bank\":0,\"cpu_address\":%u,\"size\":16,\"fingerprint_fnv1a32\":\"ACF5D9A1\"},"
        "{\"role\":\"menu-sprite-selectors\",\"source_entry\":\"prg/bank00\",\"source_offset\":%llu,\"bank\":0,\"cpu_address\":%u,\"size\":7,\"selectors\":[244,245],\"fingerprint_fnv1a32\":\"23BDC5CE\"},"
        "{\"role\":\"menu-sprite-palette\",\"source_entry\":\"prg/bank00\",\"source_offset\":%llu,\"bank\":0,\"cpu_address\":%u,\"size\":16,\"fingerprint_fnv1a32\":\"F85BA74A\"},"
        "{\"role\":\"nba-emblem\",\"source_entry\":\"prg/bank01\",\"source_offset\":%llu,\"bank\":1,\"cpu_address\":%u,\"size\":197,\"piece_count\":49,\"fingerprint_fnv1a32\":\"669E53D3\"},"
        "{\"role\":\"root-cursor\",\"source_entry\":\"prg/bank01\",\"source_offset\":%llu,\"bank\":1,\"cpu_address\":%u,\"size\":5,\"selector\":48,\"tile\":36,\"resolved_chr_entry\":\"chr/all\",\"resolved_chr_offset\":49728,\"resolved_chr_size\":32,\"resolved_chr_fingerprint_fnv1a32\":\"18F367C4\",\"fingerprint_fnv1a32\":\"7D5835D4\"},"
        "{\"role\":\"full-chr\",\"source_entry\":\"chr/all\",\"source_offset\":%llu,\"size\":262144,\"fingerprint_fnv1a32\":\"F6F6E854\",\"fingerprint_fnv1a64\":\"96A64F53B240ABB4\"},"
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
        "{\"role\":\"pointer-coordinate-tables\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"bank\":3,\"cpu_address\":%u,\"size\":247,\"fingerprint_fnv1a32\":\"218E8BCB\"},"
        "{\"role\":\"music-popup-flow\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"bank\":3,\"cpu_address\":%u,\"size\":44,\"fingerprint_fnv1a32\":\"051E3038\"},"
        "{\"role\":\"speed-popup-flow\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"bank\":3,\"cpu_address\":%u,\"size\":47,\"fingerprint_fnv1a32\":\"223D3BB4\"},"
        "{\"role\":\"period-popup-flow\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"bank\":3,\"cpu_address\":%u,\"size\":136,\"fingerprint_fnv1a32\":\"4234F755\"},"
        "{\"role\":\"overlay-row-transfer\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"bank\":3,\"cpu_address\":%u,\"size\":164,\"fingerprint_fnv1a32\":\"4325EDF8\"},"
        "{\"role\":\"menu-record-render\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"bank\":3,\"cpu_address\":%u,\"size\":517,\"fingerprint_fnv1a32\":\"7590F31B\"},"
        "{\"role\":\"menu-input-wrapper\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"bank\":3,\"cpu_address\":%u,\"size\":77,\"fingerprint_fnv1a32\":\"6C2709EB\"},"
        "{\"role\":\"controller-poll\",\"source_entry\":\"prg/fixed\",\"source_offset\":%llu,\"cpu_address\":%u,\"size\":9,\"fingerprint_fnv1a32\":\"8868D9B5\"},"
        "{\"role\":\"menu-input-helper\",\"source_entry\":\"prg/fixed\",\"source_offset\":%llu,\"cpu_address\":%u,\"size\":273,\"fingerprint_fnv1a32\":\"AE47C4A0\"},"
        "{\"role\":\"title-call-and-menu-session-setup\",\"source_entry\":\"prg/fixed\",\"source_offset\":%llu,\"cpu_address\":%u,\"size\":44,\"fingerprint_fnv1a32\":\"4FBABE09\"},"
        "{\"role\":\"start-menu-call-and-post-return-exit-chain\",\"source_entry\":\"prg/fixed\",\"source_offset\":%llu,\"cpu_address\":%u,\"size\":41,\"fingerprint_fnv1a32\":\"76C592FC\"}],"
        "\"native_contract\":{\"pages\":2,\"cells\":1920,\"payload_size\":14112,"
        "\"payload_fingerprint_fnv1a32\":\"DF89006B\","
        "\"palette_stages_fingerprint_fnv1a32\":\"F83B6C17\","
        "\"composed_fingerprint_fnv1a32\":\"661750F3\","
        "\"runtime_title_dependency_entry\":\"title/start-screen\","
        "\"runtime_title_dependency_frames\":[0,7],\"runtime_chr_dependency_entry\":\"chr/all\","
        "\"palette_stage_frames\":[0,2,4,6,8,20,24,28,32],\"root_items\":7,\"season_items\":6,"
        "\"entry_music_track_id\":6,\"entry_music_queue_cpu_address\":58487,"
        "\"entry_music_queue_fingerprint_fnv1a32\":\"0ADC9176\","
        "\"entry_music_timing\":\"after-title-confirmation-before-root-setup\","
        "\"direction_repeat_frames\":8,\"season_transition_frames\":32,\"period_value_count\":5,"
        "\"background_pixels_per_frame\":8,\"emblem_pixels_per_frame\":5,"
        "\"input_gate_seed\":5,\"period_setup_extra_frames\":1,\"exit_palette_step_frames\":2,"
        "\"exit_black_frame\":8,\"exit_dispatch_frame\":11,\"root_input_mask\":128,"
        "\"generic_input_mask\":192,\"period_input_mask\":204,\"direction_mask\":12,"
        "\"initial_input_gate\":0,\"overlay_row_cadence\":1,\"setup_row_start\":0,"
        "\"teardown_row_start\":1,\"cursor_commit_delay_frames\":1,"
        "\"popup_cursor_anchors\":{\"music\":[47,200],\"speed\":[47,167],\"period\":[71,200]},"
        "\"popup_cursor_anchor_derivation\":{\"selector_indices\":[27,8,9],"
        "\"x_table_cpu_address\":40752,\"y_table_cpu_address\":40723,"
        "\"cursor_record_cpu_address\":32817,\"cursor_dy\":-4},"
        "\"resolved_chr_entry\":\"chr/all\","
        "\"resolved_chr_size\":262144,\"resolved_chr_fingerprint_fnv1a32\":\"F6F6E854\","
        "\"resolved_chr_fingerprint_fnv1a64\":\"96A64F53B240ABB4\"}}",
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
        (unsigned long long)p->chr_offset,
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
        (unsigned long long)p->pointer_coord_offset, TECMO_ASSET_PACK_START_MENU_POINTER_COORD_CPU,
        (unsigned long long)p->music_flow_offset, TECMO_ASSET_PACK_START_MENU_MUSIC_FLOW_CPU,
        (unsigned long long)p->speed_flow_offset, TECMO_ASSET_PACK_START_MENU_SPEED_FLOW_CPU,
        (unsigned long long)p->period_flow_offset, TECMO_ASSET_PACK_START_MENU_PERIOD_FLOW_CPU,
        (unsigned long long)p->overlay_transfer_offset, TECMO_ASSET_PACK_START_MENU_OVERLAY_TRANSFER_CPU,
        (unsigned long long)p->record_render_offset, TECMO_ASSET_PACK_START_MENU_RECORD_RENDER_CPU,
        (unsigned long long)p->input_wrapper_offset, TECMO_ASSET_PACK_START_MENU_INPUT_WRAPPER_CPU,
        (unsigned long long)p->controller_poll_offset, TECMO_ASSET_PACK_START_MENU_CONTROLLER_POLL_CPU,
        (unsigned long long)p->input_helper_offset, TECMO_ASSET_PACK_START_MENU_INPUT_HELPER_CPU,
        (unsigned long long)p->title_call_and_menu_session_setup_offset,
        TECMO_ASSET_PACK_START_MENU_TITLE_CALL_AND_MENU_SESSION_SETUP_CPU,
        (unsigned long long)p->start_menu_call_and_post_return_exit_chain_offset,
        TECMO_ASSET_PACK_START_MENU_CALL_AND_POST_RETURN_EXIT_CHAIN_CPU);
}

static int append_preseason_source_map_entry(char *buffer,
                                             size_t capacity,
                                             size_t *length,
                                             int *first,
                                             const TecmoPreseasonMenuProvenance *p)
{
    const char *prefix = *first != 0 ? "" : ",\n";
    *first = 0;
    return tecmo_asset_pack_append_text(
        buffer, capacity, length,
        "%s"
        "    {\"id\":\"%s\",\"kind\":\"preseason-menu-native\","
        "\"schema\":\"tecmo.preseason-menu/TPRE-1\",\"input_contract\":\"ines-only\","
        "\"runtime_dependencies\":["
        "{\"entry\":\"menu/start-game\",\"schema\":\"tecmo.start-game-menu/TSGM-1\",\"size\":14112,\"fingerprint_fnv1a32\":\"DF89006B\"},"
        "{\"entry\":\"chr/all\",\"size\":262144,\"fingerprint_fnv1a32\":\"F6F6E854\",\"fingerprint_fnv1a64\":\"96A64F53B240ABB4\"}],"
        "\"sources\":["
        "{\"role\":\"base-start-menu-descriptor\",\"source_entry\":\"prg/fixed\",\"source_offset\":%llu,\"cpu_address\":56481,\"size\":7,\"fingerprint_fnv1a32\":\"0A5B3B88\"},"
        "{\"role\":\"base-start-menu-stream\",\"source_entry\":\"prg/bank00\",\"source_offset\":%llu,\"bank\":0,\"cpu_address\":34968,\"encoded_size\":%llu,\"decoded_size\":2048,\"encoded_fingerprint_fnv1a32\":\"8047E031\",\"decoded_fingerprint_fnv1a32\":\"E1840CFE\"},"
        "{\"role\":\"preseason-root-vector\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"bank\":3,\"cpu_address\":33208,\"size\":14,\"fingerprint_fnv1a32\":\"DCA1F834\"},"
        "{\"role\":\"preseason-flow\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"bank\":3,\"cpu_address\":39270,\"size\":116,\"fingerprint_fnv1a32\":\"0520BA1D\"},"
        "{\"role\":\"control-ownership\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"bank\":3,\"cpu_address\":39386,\"size\":12,\"fingerprint_fnv1a32\":\"82A498DF\"},"
        "{\"role\":\"difficulty-flow\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"bank\":3,\"cpu_address\":39398,\"size\":62,\"fingerprint_fnv1a32\":\"8759504B\"},"
        "{\"role\":\"popup-row-transfer\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"bank\":3,\"cpu_address\":39460,\"size\":164,\"fingerprint_fnv1a32\":\"4325EDF8\"},"
        "{\"role\":\"character-map\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"bank\":3,\"cpu_address\":39592,\"size\":91,\"fingerprint_fnv1a32\":\"724F80CE\"},"
        "{\"role\":\"control-record\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"bank\":3,\"cpu_address\":40030,\"size\":91,\"fingerprint_fnv1a32\":\"C4F89AF6\"},"
        "{\"role\":\"division-record\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"bank\":3,\"cpu_address\":40121,\"size\":68,\"fingerprint_fnv1a32\":\"B92F396E\"},"
        "{\"role\":\"difficulty-record\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"bank\":3,\"cpu_address\":40226,\"size\":40,\"fingerprint_fnv1a32\":\"8861F7F3\"},"
        "{\"role\":\"menu-input-wrapper\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"bank\":3,\"cpu_address\":40646,\"size\":77,\"fingerprint_fnv1a32\":\"6C2709EB\"},"
        "{\"role\":\"input-parameters\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"bank\":3,\"cpu_address\":40723,\"size\":145,\"fingerprint_fnv1a32\":\"DE568C7B\"},"
        "{\"role\":\"coordinate-pointers\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"bank\":3,\"cpu_address\":40868,\"size\":116,\"fingerprint_fnv1a32\":\"AAF64989\"},"
        "{\"role\":\"coordinate-tables\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"bank\":3,\"cpu_address\":40984,\"size\":131,\"fingerprint_fnv1a32\":\"7529BA7F\"},"
        "{\"role\":\"team-state-driver\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"bank\":3,\"cpu_address\":45516,\"size\":167,\"fingerprint_fnv1a32\":\"46A305AD\"},"
        "{\"role\":\"player-pointers-confirmation-bridge-and-team-tables\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"bank\":3,\"cpu_address\":45683,\"size\":47,\"fingerprint_fnv1a32\":\"1F0402B0\"},"
        "{\"role\":\"team-confirmation-state-advance\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"bank\":3,\"cpu_address\":45687,\"size\":12,\"fingerprint_fnv1a32\":\"D6AFFBD2\",\"runtime_behavior\":\"unexecuted_at_milestone_boundary\"},"
        "{\"role\":\"division-offset-table\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"bank\":3,\"cpu_address\":45699,\"size\":4,\"fingerprint_fnv1a32\":\"3B420652\"},"
        "{\"role\":\"team-id-table\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"bank\":3,\"cpu_address\":45703,\"size\":27,\"fingerprint_fnv1a32\":\"B33FB72A\"},"
        "{\"role\":\"cursor-sprite-record\",\"source_entry\":\"prg/bank01\",\"source_offset\":%llu,\"bank\":1,\"cpu_address\":32817,\"size\":5,\"fingerprint_fnv1a32\":\"7D5835D4\",\"resolved_dx\":0,\"resolved_dy\":-4},"
        "{\"role\":\"player-markers\",\"source_entry\":\"prg/bank01\",\"source_offset\":%llu,\"bank\":1,\"cpu_address\":32822,\"size\":20,\"fingerprint_fnv1a32\":\"5E246059\",\"selector_source_record_offsets\":[3,13],\"resolved_chr_selector\":48,\"resolved_chr_pair_count\":7,\"resolved_chr_fingerprint_fnv1a32\":\"1E505537\"},"
        "{\"role\":\"team-screen-descriptors\",\"source_entry\":\"prg/fixed\",\"source_offsets\":[%llu,%llu,%llu,%llu],\"cpu_addresses\":[56495,56502,56509,56516],\"size_each\":7,\"fingerprints_fnv1a32\":[\"C62DA3E3\",\"5ABDD3EA\",\"53633C79\",\"4824AE3D\"]},"
        "{\"role\":\"team-screen-streams\",\"source_entry\":\"prg/bank00\",\"source_offsets\":[%llu,%llu,%llu,%llu],\"cpu_addresses\":[35187,35581,36034,36428],\"encoded_sizes\":[%llu,%llu,%llu,%llu],\"decoded_size_each\":1024,\"encoded_fingerprints_fnv1a32\":[\"A06C1E1F\",\"BB5D6EA8\",\"FAD48944\",\"5E41604D\"],\"decoded_fingerprints_fnv1a32\":[\"AF69FFC7\",\"727E1AD2\",\"41877C8F\",\"0B5CB115\"]},"
        "{\"role\":\"team-screen-palettes\",\"source_entry\":\"prg/bank00\",\"source_offsets\":[%llu,%llu,%llu,%llu],\"cpu_addresses\":[37766,37782,37798,37814],\"size_each\":16,\"fingerprints_fnv1a32\":[\"FAAF07AE\",\"34F6B8DC\",\"7574D664\",\"CE09A13A\"]},"
        "{\"role\":\"controller-poll\",\"source_entry\":\"prg/fixed\",\"source_offset\":%llu,\"cpu_address\":54039,\"size\":9,\"fingerprint_fnv1a32\":\"8868D9B5\"},"
        "{\"role\":\"menu-input-helper\",\"source_entry\":\"prg/fixed\",\"source_offset\":%llu,\"cpu_address\":55075,\"size\":273,\"fingerprint_fnv1a32\":\"AE47C4A0\"},"
        "{\"role\":\"screen-loader\",\"source_entry\":\"prg/fixed\",\"source_offset\":%llu,\"cpu_address\":55595,\"size\":122,\"fingerprint_fnv1a32\":\"835283BE\"},"
        "{\"role\":\"fade-out\",\"source_entry\":\"prg/fixed\",\"source_offset\":%llu,\"cpu_address\":56101,\"size\":99,\"fingerprint_fnv1a32\":\"5D98AB7A\"},"
        "{\"role\":\"fade-in\",\"source_entry\":\"prg/fixed\",\"source_offset\":%llu,\"cpu_address\":56200,\"size\":72,\"fingerprint_fnv1a32\":\"D75D6EEA\"},"
        "{\"role\":\"post-return-exit-chain\",\"source_entry\":\"prg/fixed\",\"source_offset\":%llu,\"cpu_address\":58497,\"size\":41,\"fingerprint_fnv1a32\":\"76C592FC\"},"
        "{\"role\":\"full-chr\",\"source_entry\":\"chr/all\",\"source_offset\":%llu,\"size\":262144,\"fingerprint_fnv1a32\":\"F6F6E854\",\"fingerprint_fnv1a64\":\"96A64F53B240ABB4\"}],"
        "\"native_contract\":{\"payload_size\":%u,\"payload_fingerprint_fnv1a32\":\"D9EE49F4\",\"overlay_records\":3,\"overlay_cell_count\":510,\"team_screens\":4,\"team_screen_cells\":3840,\"team_count\":27,\"control_rows\":7,\"difficulty_rows\":3,\"division_rows\":4,\"row_cadence_frames\":1,\"team_entry_display_stage_frames\":[3,5,7],\"team_entry_display_black_frame\":9,\"team_input_ready_frames\":16,\"team_memory_palette_full_frame\":31,\"team_display_first_visible_frame\":18,\"team_display_palette_step_frames\":4,\"team_display_full_frame\":30,\"team_exit_display_stage_frames\":[1,3,5],\"team_exit_display_black_frame\":7,\"team_exit_frames\":32,\"division_return_display_first_visible_frame\":1,\"division_return_display_palette_step_frames\":4,\"division_return_display_full_frame\":13,\"p2_terminal\":\"interactive-team-selection-before-confirm\"}}",
        prefix, TECMO_ASSET_PACK_PRESEASON_MENU_ID,
        (unsigned long long)p->base_descriptor_offset,
        (unsigned long long)p->base_stream_offset,
        (unsigned long long)p->base_stream_size,
        (unsigned long long)p->root_vector_offset,
        (unsigned long long)p->flow_offset,
        (unsigned long long)p->ownership_offset,
        (unsigned long long)p->difficulty_flow_offset,
        (unsigned long long)p->popup_builder_offset,
        (unsigned long long)p->char_map_offset,
        (unsigned long long)p->control_record_offset,
        (unsigned long long)p->division_record_offset,
        (unsigned long long)p->difficulty_record_offset,
        (unsigned long long)p->input_wrapper_offset,
        (unsigned long long)p->input_params_offset,
        (unsigned long long)p->input_coord_pointers_offset,
        (unsigned long long)p->coord_tables_offset,
        (unsigned long long)p->team_driver_offset,
        (unsigned long long)p->team_maps_offset,
        (unsigned long long)(p->team_maps_offset + 4U),
        (unsigned long long)(p->team_maps_offset + 16U),
        (unsigned long long)(p->team_maps_offset + 20U),
        (unsigned long long)p->cursor_record_offset,
        (unsigned long long)p->marker_records_offset,
        (unsigned long long)p->descriptor_offsets[0],
        (unsigned long long)p->descriptor_offsets[1],
        (unsigned long long)p->descriptor_offsets[2],
        (unsigned long long)p->descriptor_offsets[3],
        (unsigned long long)p->stream_offsets[0],
        (unsigned long long)p->stream_offsets[1],
        (unsigned long long)p->stream_offsets[2],
        (unsigned long long)p->stream_offsets[3],
        (unsigned long long)p->stream_sizes[0],
        (unsigned long long)p->stream_sizes[1],
        (unsigned long long)p->stream_sizes[2],
        (unsigned long long)p->stream_sizes[3],
        (unsigned long long)p->palette_offsets[0],
        (unsigned long long)p->palette_offsets[1],
        (unsigned long long)p->palette_offsets[2],
        (unsigned long long)p->palette_offsets[3],
        (unsigned long long)p->controller_poll_offset,
        (unsigned long long)p->input_helper_offset,
        (unsigned long long)p->screen_loader_offset,
        (unsigned long long)p->fade_out_offset,
        (unsigned long long)p->fade_in_offset,
        (unsigned long long)p->post_return_exit_chain_offset,
        (unsigned long long)p->chr_offset,
        (unsigned)TECMO_ASSET_PACK_PRESEASON_SIZE);
}

static int append_all_star_source_map_entry(char *buffer,
                                            size_t capacity,
                                            size_t *length,
                                            int *first,
                                            const TecmoAllStarMenuProvenance *p)
{
    const char *prefix = *first != 0 ? "" : ",\n";
    *first = 0;
    return tecmo_asset_pack_append_text(
        buffer, capacity, length,
        "%s"
        "    {\"id\":\"%s\",\"kind\":\"all-star-menu-native\","
        "\"schema\":\"tecmo.all-star-menu/TALL-1\",\"input_contract\":\"ines-only\","
        "\"runtime_dependencies\":["
        "{\"entry\":\"menu/preseason\",\"schema\":\"tecmo.preseason-menu/TPRE-1\",\"size\":26736,\"fingerprint_fnv1a32\":\"D9EE49F4\"},"
        "{\"entry\":\"menu/start-game\",\"schema\":\"tecmo.start-game-menu/TSGM-1\",\"size\":14112,\"fingerprint_fnv1a32\":\"DF89006B\"},"
        "{\"entry\":\"chr/all\",\"size\":262144,\"fingerprint_fnv1a32\":\"F6F6E854\",\"fingerprint_fnv1a64\":\"96A64F53B240ABB4\"}],"
        "\"sources\":["
        "{\"role\":\"base-start-menu-descriptor\",\"source_entry\":\"prg/fixed\",\"source_offset\":%llu,\"cpu_address\":56481,\"size\":7,\"fingerprint_fnv1a32\":\"0A5B3B88\"},"
        "{\"role\":\"base-start-menu-stream\",\"source_entry\":\"prg/bank00\",\"source_offset\":%llu,\"bank\":0,\"cpu_address\":34968,\"encoded_size\":%llu,\"decoded_size\":2048,\"encoded_fingerprint_fnv1a32\":\"8047E031\",\"decoded_fingerprint_fnv1a32\":\"E1840CFE\"},"
        "{\"role\":\"all-star-precommit-flow\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"bank\":3,\"cpu_address\":33313,\"size\":182,\"fingerprint_fnv1a32\":\"85DFCDE5\"},"
        "{\"role\":\"all-star-final-side-and-launch-commit\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"bank\":3,\"cpu_address\":33495,\"size\":21,\"fingerprint_fnv1a32\":\"369626B2\",\"runtime_behavior\":\"bounded-unexecuted\"},"
        "{\"role\":\"all-star-launch-routine\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"bank\":3,\"cpu_address\":45695,\"size\":4,\"fingerprint_fnv1a32\":\"8145527E\",\"runtime_behavior\":\"never-called\"},"
        "{\"role\":\"control-ownership\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"bank\":3,\"cpu_address\":39386,\"size\":12,\"fingerprint_fnv1a32\":\"82A498DF\"},"
        "{\"role\":\"difficulty-flow\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"bank\":3,\"cpu_address\":39398,\"size\":62,\"fingerprint_fnv1a32\":\"8759504B\"},"
        "{\"role\":\"popup-row-transfer\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"bank\":3,\"cpu_address\":39460,\"size\":164,\"fingerprint_fnv1a32\":\"4325EDF8\"},"
        "{\"role\":\"character-map\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"bank\":3,\"cpu_address\":39592,\"size\":91,\"fingerprint_fnv1a32\":\"724F80CE\"},"
        "{\"role\":\"team-side-record\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"bank\":3,\"cpu_address\":39742,\"size\":23,\"fingerprint_fnv1a32\":\"28D1A422\"},"
        "{\"role\":\"control-record\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"bank\":3,\"cpu_address\":40030,\"size\":91,\"fingerprint_fnv1a32\":\"C4F89AF6\"},"
        "{\"role\":\"difficulty-record\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"bank\":3,\"cpu_address\":40226,\"size\":40,\"fingerprint_fnv1a32\":\"8861F7F3\"},"
        "{\"role\":\"menu-input-wrapper\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"bank\":3,\"cpu_address\":40646,\"size\":77,\"fingerprint_fnv1a32\":\"6C2709EB\"},"
        "{\"role\":\"input-parameters\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"bank\":3,\"cpu_address\":40723,\"size\":145,\"fingerprint_fnv1a32\":\"DE568C7B\"},"
        "{\"role\":\"coordinate-pointers\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"bank\":3,\"cpu_address\":40868,\"size\":116,\"fingerprint_fnv1a32\":\"AAF64989\"},"
        "{\"role\":\"coordinate-tables\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"bank\":3,\"cpu_address\":40984,\"size\":131,\"fingerprint_fnv1a32\":\"7529BA7F\"},"
        "{\"role\":\"cursor-sprite-record\",\"source_entry\":\"prg/bank01\",\"source_offset\":%llu,\"bank\":1,\"cpu_address\":32817,\"size\":5,\"fingerprint_fnv1a32\":\"7D5835D4\",\"resolved_dx\":0,\"resolved_dy\":-4},"
        "{\"role\":\"controller-poll\",\"source_entry\":\"prg/fixed\",\"source_offset\":%llu,\"cpu_address\":54039,\"size\":9,\"fingerprint_fnv1a32\":\"8868D9B5\"},"
        "{\"role\":\"menu-input-helper\",\"source_entry\":\"prg/fixed\",\"source_offset\":%llu,\"cpu_address\":55075,\"size\":273,\"fingerprint_fnv1a32\":\"AE47C4A0\"},"
        "{\"role\":\"full-chr\",\"source_entry\":\"chr/all\",\"source_offset\":%llu,\"size\":262144,\"fingerprint_fnv1a32\":\"F6F6E854\",\"fingerprint_fnv1a64\":\"96A64F53B240ABB4\"}],"
        "\"native_contract\":{\"payload_size\":%u,\"payload_fingerprint_fnv1a32\":\"F02A2A45\",\"control_rows\":7,\"difficulty_rows\":3,\"team_side_rows\":2,\"team_side_codes\":[27,28],\"team_side_modes\":[1,4],\"direction_repeat_frames\":8,\"input_gate_seed\":5,\"row_cadence_frames\":1,\"cursor_commit_delay_frames\":1,\"terminal\":\"final-accept-consumed-no-game-launch\"}}",
        prefix, TECMO_ASSET_PACK_ALL_STAR_MENU_ID,
        (unsigned long long)p->base_descriptor_offset,
        (unsigned long long)p->base_stream_offset,
        (unsigned long long)p->base_stream_size,
        (unsigned long long)p->flow_offset,
        (unsigned long long)p->final_commit_offset,
        (unsigned long long)p->launch_routine_offset,
        (unsigned long long)p->ownership_offset,
        (unsigned long long)p->difficulty_flow_offset,
        (unsigned long long)p->popup_builder_offset,
        (unsigned long long)p->char_map_offset,
        (unsigned long long)p->team_record_offset,
        (unsigned long long)p->control_record_offset,
        (unsigned long long)p->difficulty_record_offset,
        (unsigned long long)p->input_wrapper_offset,
        (unsigned long long)p->input_params_offset,
        (unsigned long long)p->input_coord_pointers_offset,
        (unsigned long long)p->coord_tables_offset,
        (unsigned long long)p->cursor_record_offset,
        (unsigned long long)p->controller_poll_offset,
        (unsigned long long)p->input_helper_offset,
        (unsigned long long)p->chr_offset,
        (unsigned)TECMO_ASSET_PACK_ALL_STAR_SIZE);
}

static int append_music_source_map_entry(char *buffer,
                                         size_t capacity,
                                         size_t *length,
                                         int *first,
                                         const TecmoMusicProvenance *p)
{
    const char *prefix = *first != 0 ? "" : ",\n";
    *first = 0;
    return tecmo_asset_pack_append_text(
        buffer, capacity, length,
        "%s"
        "    {\"id\":\"%s\",\"kind\":\"native-nes-music\","
        "\"schema\":\"tecmo.music/TMUS-1\",\"input_contract\":\"ines-only\","
        "\"sources\":["
        "{\"role\":\"audio-bank-range\",\"source_entry\":\"prg/bank04\",\"source_offset\":%llu,\"bank\":4,\"cpu_address\":35492,\"size\":5218,\"fingerprint_fnv1a32\":\"06F2A750\"},"
        "{\"role\":\"music-directory\",\"source_entry\":\"prg/bank04\",\"source_offset\":%llu,\"bank\":4,\"cpu_address\":36048,\"size\":18,\"fingerprint_fnv1a32\":\"59366EC4\"},"
        "{\"role\":\"native-audio-engine\",\"source_entry\":\"prg/fixed\",\"source_offset\":%llu,\"cpu_address\":62194,\"size\":1759,\"fingerprint_fnv1a32\":\"FC6A0BC1\"},"
        "{\"role\":\"period-table\",\"source_entry\":\"prg/fixed\",\"source_offset\":%llu,\"cpu_address\":63803,\"size\":150,\"fingerprint_fnv1a32\":\"3F5A394D\"},"
        "{\"role\":\"opening-track-queue\",\"source_entry\":\"prg/bank04\",\"source_offset\":%llu,\"bank\":4,\"cpu_address\":%u,\"size\":%u,\"fingerprint_fnv1a32\":\"FCDCAFEF\"},"
        "{\"role\":\"opening-first-scene-route\",\"source_entry\":\"prg/bank04\",\"source_offset\":%llu,\"bank\":4,\"cpu_address\":%u,\"size\":%u,\"fingerprint_fnv1a32\":\"07FD2C8D\"},"
        "{\"role\":\"menu-track-queue\",\"source_entry\":\"prg/fixed\",\"source_offset\":%llu,\"cpu_address\":%u,\"size\":%u,\"fingerprint_fnv1a32\":\"0ADC9176\"},"
        "{\"role\":\"pregame-matchup-track-queue\",\"source_entry\":\"prg/bank06\",\"source_offset\":%llu,\"bank\":6,\"cpu_address\":%u,\"size\":%u,\"fingerprint_fnv1a32\":\"1E564AC0\"},"
        "{\"role\":\"gameplay-track-5\",\"source_entry\":\"prg/bank04\",\"source_offset\":%llu,\"bank\":4,\"cpu_address\":37620,\"size\":%u,\"fingerprint_fnv1a32\":\"1270498B\"},"
        "{\"role\":\"presentation-track-6\",\"source_entry\":\"prg/bank04\",\"source_offset\":%llu,\"bank\":4,\"cpu_address\":38595,\"size\":%u,\"fingerprint_fnv1a32\":\"BD91FCF1\"},"
        "{\"role\":\"opening-track-7\",\"source_entry\":\"prg/bank04\",\"source_offset\":%llu,\"bank\":4,\"cpu_address\":36066,\"size\":%u,\"fingerprint_fnv1a32\":\"69F85EC2\"},"
        "{\"role\":\"pregame-matchup-stinger-8\",\"source_entry\":\"prg/bank04\",\"source_offset\":%llu,\"bank\":4,\"cpu_address\":40467,\"size\":%u,\"fingerprint_fnv1a32\":\"8122C6CF\"}],"
        "\"native_contract\":{\"payload_size\":%u,\"payload_fingerprint_fnv1a32\":\"%08X\","
        "\"tracks\":[5,6,7,8],\"channels\":[\"pulse1\",\"pulse2\",\"triangle\",\"noise\"],"
        "\"voice_count\":%u,\"pitch_count\":75,\"instruction_count\":%u,"
        "\"semantic_instructions\":[\"note\",\"voice\",\"legato\",\"pitch_delta\",\"rest\",\"bounded_loop\",\"phrase_scope\",\"resolved_call\",\"return\",\"end\"],"
        "\"channel_loop_state_count\":1,"
        "\"runtime_raw_pointer_or_opcode_dependency\":false,"
        "\"tick_rate\":\"39375000/655171\",\"sample_rate\":44100,"
        "\"opening_track_id\":7,\"opening_queue_scene\":\"arena-entry\","
        "\"opening_queue_relation\":\"one-NMI-before-first-arena-route\","
        "\"opening_queue_to_clear_ticks\":2614,"
        "\"opening_tick_boundary\":\"F7EE-consume-through-first-063E-zero-inclusive\","
        "\"menu_track_id\":6,\"menu_queue_scene\":\"blue-start-game-menu-entry\","
        "\"menu_queue_timing\":\"after-title-confirmation-before-root-setup\","
        "\"queue_semantics\":\"pending-until-next-audio-tick\","
        "\"envelope_phase_transition\":\"same-tick-fallthrough\","
        "\"voice_timing_bits\":\"attack-7_decay-5-6_release-2-4\","
        "\"pitch_delta_zero_semantics\":\"reset-both-channel-delta-bytes\","
        "\"pregame_matchup_stinger_queue_to_clear_ticks\":396,"
        "\"long_loop_regression_ticks\":100000,"
        "\"game_music_setting\":\"gates-future-track-5-only\"}}",
        prefix, TECMO_ASSET_PACK_MUSIC_ID,
        (unsigned long long)p->source_offset,
        (unsigned long long)p->directory_offset,
        (unsigned long long)p->engine_offset,
        (unsigned long long)p->pitch_offset,
        (unsigned long long)p->opening_queue_offset,
        (unsigned)TECMO_ASSET_PACK_MUSIC_OPENING_QUEUE_CPU,
        (unsigned)TECMO_ASSET_PACK_MUSIC_OPENING_QUEUE_SIZE,
        (unsigned long long)p->opening_first_route_offset,
        (unsigned)TECMO_ASSET_PACK_MUSIC_OPENING_FIRST_ROUTE_CPU,
        (unsigned)TECMO_ASSET_PACK_MUSIC_OPENING_FIRST_ROUTE_SIZE,
        (unsigned long long)p->menu_queue_offset,
        (unsigned)TECMO_ASSET_PACK_MUSIC_MENU_QUEUE_CPU,
        (unsigned)TECMO_ASSET_PACK_MUSIC_MENU_QUEUE_SIZE,
        (unsigned long long)p->pregame_matchup_queue_offset,
        (unsigned)TECMO_ASSET_PACK_MUSIC_PREGAME_MATCHUP_QUEUE_CPU,
        (unsigned)TECMO_ASSET_PACK_MUSIC_PREGAME_MATCHUP_QUEUE_SIZE,
        (unsigned long long)p->track_offsets[0], p->track_sizes[0],
        (unsigned long long)p->track_offsets[1], p->track_sizes[1],
        (unsigned long long)p->track_offsets[2], p->track_sizes[2],
        (unsigned long long)p->track_offsets[3], p->track_sizes[3],
        p->payload_size, p->payload_fingerprint,
        (unsigned)p->voice_count, p->instruction_count);
}

static int append_gameplay_audio_source_map_entries(
    char *buffer, size_t capacity, size_t *length, int *first,
    const TecmoGameplayAudioProvenance *p)
{
    const char *prefix = *first != 0 ? "" : ",\n";
    *first = 0;
    if (tecmo_asset_pack_append_text(
            buffer, capacity, length,
            "%s"
            "    {\"id\":\"%s\",\"kind\":\"native-gameplay-sfx\","
            "\"schema\":\"tecmo.gameplay-audio/TSFX-1\","
            "\"input_contract\":\"ines-only\",\"sources\":["
            "{\"role\":\"sfx-directory\",\"source_entry\":\"prg/bank04\",\"source_offset\":%llu,\"bank\":4,\"cpu_address\":35492,\"size\":32,\"fingerprint_fnv1a32\":\"6283F255\"},"
            "{\"role\":\"sfx-core\",\"source_entry\":\"prg/bank04\",\"source_offset\":%llu,\"bank\":4,\"cpu_address\":35492,\"size\":556,\"fingerprint_fnv1a32\":\"548EED95\"},"
            "{\"role\":\"sfx-extension\",\"source_entry\":\"prg/bank04\",\"source_offset\":%llu,\"bank\":4,\"cpu_address\":40331,\"size\":136,\"fingerprint_fnv1a32\":\"838408D4\"},"
            "{\"role\":\"fixed-audio-engine\",\"source_entry\":\"prg/fixed\",\"source_offset\":%llu,\"cpu_address\":62194,\"size\":1759,\"fingerprint_fnv1a32\":\"FC6A0BC1\"},"
            "{\"role\":\"clock-buzzer-id-3-a\",\"source_entry\":\"prg/fixed\",\"source_offset\":%llu,\"cpu_address\":59355,\"request_site_cpu_address\":59357,\"size\":5,\"fingerprint_fnv1a32\":\"FA9A48DB\"},"
            "{\"role\":\"clock-buzzer-id-3-b\",\"source_entry\":\"prg/fixed\",\"source_offset\":%llu,\"cpu_address\":59501,\"request_site_cpu_address\":59503,\"size\":5,\"fingerprint_fnv1a32\":\"FA9A48DB\"},"
            "{\"role\":\"countdown-id-14\",\"source_entry\":\"prg/fixed\",\"source_offset\":%llu,\"cpu_address\":59491,\"request_site_cpu_address\":59493,\"size\":5,\"fingerprint_fnv1a32\":\"E30ADA62\"},"
            "{\"role\":\"gameplay-id-5\",\"source_entry\":\"prg/bank05\",\"source_offset\":%llu,\"bank\":5,\"cpu_address\":40940,\"size\":5,\"fingerprint_fnv1a32\":\"5824A080\"},"
            "{\"role\":\"crowd-response-id-11\",\"source_entry\":\"prg/bank05\",\"source_offset\":%llu,\"bank\":5,\"cpu_address\":44289,\"size\":14,\"fingerprint_fnv1a32\":\"B7141C72\"},"
            "{\"role\":\"side-result-ids-12-13\",\"source_entry\":\"prg/bank05\",\"source_offset\":%llu,\"bank\":5,\"cpu_address\":45521,\"size\":22,\"fingerprint_fnv1a32\":\"CFCD9759\"}],"
            "\"native_contract\":{\"payload_size\":%u,"
            "\"payload_fingerprint_fnv1a32\":\"%08X\","
            "\"revision_fingerprint_fnv1a32\":\"0650F5B0\","
            "\"effect_ids\":[3,5,6,11,12,13,14],"
            "\"semantic_events\":{\"3\":\"clock-buzzer\",\"5\":\"bank05-9fec-cue\",\"6\":\"violation-cue\",\"11\":\"crowd-response\",\"12\":\"side-result-12\",\"13\":\"side-result-13\",\"14\":\"countdown\"},"
            "\"event_conditions\":{\"3\":\"shot-or-period-expiry\",\"5\":\"restart-after-violation-foul-or-period-reset-caller-gated-by-game-music\",\"6\":\"bounded-dynamic-violation-cutaway-correlation\",\"14\":\"each-game-second-boundary-below-twelve\"},"
            "\"channels\":[\"pulse1\",\"pulse2\",\"triangle\",\"noise\"],"
            "\"priority_masks\":[16,32,64,128],"
            "\"mailbox_semantics\":\"last-write-wins-until-next-audio-tick\","
            "\"music_under_override\":\"sequencer-and-oscillators-advance\","
            "\"instruction_count\":%u,\"voice_count\":%u,"
            "\"runtime_raw_pointer_or_opcode_dependency\":false}},\n",
            prefix, TECMO_ASSET_PACK_GAMEPLAY_SFX_ID,
            (unsigned long long)p->sfx_directory_offset,
            (unsigned long long)p->sfx_core_offset,
            (unsigned long long)p->sfx_extension_offset,
            (unsigned long long)p->engine_offset,
            (unsigned long long)p->event_offsets[0],
            (unsigned long long)p->event_offsets[2],
            (unsigned long long)p->event_offsets[1],
            (unsigned long long)p->event_offsets[3],
            (unsigned long long)p->event_offsets[4],
            (unsigned long long)p->event_offsets[5],
            p->sfx_payload_size, p->sfx_payload_fingerprint,
            p->sfx_instruction_count, (unsigned)p->sfx_voice_count) != 0)
        return -1;
    return tecmo_asset_pack_append_text(
        buffer, capacity, length,
        "    {\"id\":\"%s\",\"kind\":\"native-gameplay-dmc\","
        "\"schema\":\"tecmo.gameplay-audio/TDMC-1\","
        "\"input_contract\":\"ines-only\",\"sources\":["
        "{\"role\":\"dmc-pool-c080-c280\",\"source_entry\":\"prg/fixed\",\"source_offset\":%llu,\"cpu_address\":49280,\"size\":513,\"fingerprint_fnv1a32\":\"33E109D7\"},"
        "{\"role\":\"dmc-pool-c440-c710\",\"source_entry\":\"prg/fixed\",\"source_offset\":%llu,\"cpu_address\":50240,\"size\":721,\"fingerprint_fnv1a32\":\"6ECC107C\"},"
        "{\"role\":\"dmc-pool-c740-caf0\",\"source_entry\":\"prg/fixed\",\"source_offset\":%llu,\"cpu_address\":51008,\"size\":945,\"fingerprint_fnv1a32\":\"F621FD7C\"},"
        "{\"role\":\"bank05-a8d6-trigger\",\"source_entry\":\"prg/bank05\",\"source_offset\":%llu,\"bank\":5,\"cpu_address\":43222,\"size\":19,\"fingerprint_fnv1a32\":\"EE75A82E\"},"
        "{\"role\":\"bank05-a9c5-trigger\",\"source_entry\":\"prg/bank05\",\"source_offset\":%llu,\"bank\":5,\"cpu_address\":43461,\"size\":21,\"fingerprint_fnv1a32\":\"567D5B90\"},"
        "{\"role\":\"layup-sequence-abf5-trigger\",\"source_entry\":\"prg/bank05\",\"source_offset\":%llu,\"bank\":5,\"cpu_address\":44021,\"size\":21,\"fingerprint_fnv1a32\":\"1C158FAB\"},"
        "{\"role\":\"held-ball-dribble-trigger\",\"source_entry\":\"prg/bank05\",\"source_offset\":%llu,\"bank\":5,\"cpu_address\":46507,\"size\":21,\"fingerprint_fnv1a32\":\"F6CEC8DB\"},"
        "{\"role\":\"gameplay-music-gate\",\"source_entry\":\"prg/fixed\",\"source_offset\":%llu,\"cpu_address\":60203,\"size\":11,\"fingerprint_fnv1a32\":\"181D6897\"},"
        "{\"role\":\"restart-audio-flow\",\"source_entry\":\"prg/fixed\",\"source_offset\":%llu,\"cpu_address\":59207,\"size\":30,\"fingerprint_fnv1a32\":\"AC51064E\"},"
        "{\"role\":\"period-audio-flow\",\"source_entry\":\"prg/fixed\",\"source_offset\":%llu,\"cpu_address\":58765,\"size\":92,\"fingerprint_fnv1a32\":\"1185F342\"},"
        "{\"role\":\"final-audio-flow\",\"source_entry\":\"prg/fixed\",\"source_offset\":%llu,\"cpu_address\":59039,\"size\":41,\"fingerprint_fnv1a32\":\"03BFDFA0\"}],"
        "\"native_contract\":{\"payload_size\":%u,"
        "\"payload_fingerprint_fnv1a32\":\"%08X\","
        "\"revision_fingerprint_fnv1a32\":\"0650F5B0\","
        "\"clip_count\":5,\"pool_count\":3,"
        "\"rates\":[14,14,14,15,15],"
        "\"loop\":false,\"irq\":false,\"direct_dac_write\":false,"
        "\"delta_counter_persistence\":\"retrigger-end-and-clear\","
        "\"inactive_output\":\"held-dac-level\","
        "\"independent_from_music_and_sfx\":true,"
        "\"clip_names\":{\"0\":\"bank05-a8d6-short\","
        "\"1\":\"bank05-a8d6-long\","
        "\"2\":\"bank05-a9c5\","
        "\"3\":\"layup-sequence-abf5\","
        "\"4\":\"held-ball-dribble\"},"
        "\"unresolved_clip_ids\":[0,1,2],"
        "\"semantic_boundary\":\"clip IDs 0, 1, and 2 remain address-bound and unresolved; ABF5 has sequence-level correlation only; no impact, rim, or exclusivity claim\"}}",
        TECMO_ASSET_PACK_GAMEPLAY_DMC_ID,
        (unsigned long long)p->dmc_pool_offsets[0],
        (unsigned long long)p->dmc_pool_offsets[1],
        (unsigned long long)p->dmc_pool_offsets[2],
        (unsigned long long)p->dmc_trigger_offsets[0],
        (unsigned long long)p->dmc_trigger_offsets[1],
        (unsigned long long)p->dmc_trigger_offsets[2],
        (unsigned long long)p->dmc_trigger_offsets[3],
        (unsigned long long)p->gameplay_gate_offset,
        (unsigned long long)p->restart_offset,
        (unsigned long long)p->period_offset,
        (unsigned long long)p->final_offset,
        p->dmc_payload_size, p->dmc_payload_fingerprint);
}

static int append_frontend_audio_source_map_entry(
    char *buffer, size_t capacity, size_t *length, int *first,
    const TecmoFrontendAudioProvenance *p)
{
    const char *prefix = *first != 0 ? "" : ",\n";
    *first = 0;
    return tecmo_asset_pack_append_text(
        buffer, capacity, length,
        "%s"
        "    {\"id\":\"%s\",\"kind\":\"native-frontend-sfx\","
        "\"schema\":\"tecmo.frontend-audio/TFSX-1\","
        "\"input_contract\":\"ines-only\",\"runtime_dependencies\":["
        "{\"entry\":\"audio/music\",\"schema\":\"TMUS-1\","
        "\"same_pack_required\":true}],\"sources\":["
        "{\"role\":\"sfx-directory\",\"source_entry\":\"prg/bank04\",\"source_offset\":%llu,\"bank\":4,\"cpu_address\":35492,\"size\":32,\"fingerprint_fnv1a32\":\"6283F255\"},"
        "{\"role\":\"accepted-menu-a-release-sfx-8\",\"source_entry\":\"prg/bank04\",\"source_offset\":%llu,\"bank\":4,\"cpu_address\":35831,\"size\":51,\"fingerprint_fnv1a32\":\"AC9D4C1F\"},"
        "{\"role\":\"title-confirm-sfx-10\",\"source_entry\":\"prg/bank04\",\"source_offset\":%llu,\"bank\":4,\"cpu_address\":35735,\"size\":96,\"fingerprint_fnv1a32\":\"963DC35E\"},"
        "{\"role\":\"title-setup\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"bank\":3,\"cpu_address\":32854,\"size\":32,\"fingerprint_fnv1a32\":\"4A97C61D\"},"
        "{\"role\":\"title-confirm-input-and-queue\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"bank\":3,\"cpu_address\":32886,\"size\":27,\"fingerprint_fnv1a32\":\"0C902C97\"},"
        "{\"role\":\"title-setup-transition-bridge\",\"source_entry\":\"prg/fixed\",\"source_offset\":%llu,\"cpu_address\":49155,\"size\":3,\"fingerprint_fnv1a32\":\"0F4103F2\"},"
        "{\"role\":\"title-setup-three-yield-flow\",\"source_entry\":\"prg/fixed\",\"source_offset\":%llu,\"cpu_address\":55598,\"size\":119,\"fingerprint_fnv1a32\":\"105B38A7\"},"
        "{\"role\":\"title-setup-zero-state-two-yield-flow\",\"source_entry\":\"prg/fixed\",\"source_offset\":%llu,\"cpu_address\":56101,\"size\":99,\"fingerprint_fnv1a32\":\"5D98AB7A\"},"
        "{\"role\":\"task-frame-yield-helper\",\"source_entry\":\"prg/fixed\",\"source_offset\":%llu,\"cpu_address\":58362,\"size\":32,\"fingerprint_fnv1a32\":\"B8BA175B\"},"
        "{\"role\":\"title-stop-bridge\",\"source_entry\":\"prg/fixed\",\"source_offset\":%llu,\"cpu_address\":49188,\"size\":6,\"fingerprint_fnv1a32\":\"B4100BD2\"},"
        "{\"role\":\"title-stop-dispatch\",\"source_entry\":\"prg/fixed\",\"source_offset\":%llu,\"cpu_address\":52143,\"size\":12,\"fingerprint_fnv1a32\":\"AB3677A6\"},"
        "{\"role\":\"title-stop-and-wait\",\"source_entry\":\"prg/fixed\",\"source_offset\":%llu,\"cpu_address\":60422,\"size\":32,\"fingerprint_fnv1a32\":\"F1BCC8E2\"},"
        "{\"role\":\"menu-accepted-release-dispatch\",\"source_entry\":\"prg/fixed\",\"source_offset\":%llu,\"cpu_address\":55144,\"size\":43,\"fingerprint_fnv1a32\":\"68D40771\"},"
        "{\"role\":\"menu-track-transition-flow\",\"source_entry\":\"prg/fixed\",\"source_offset\":%llu,\"cpu_address\":58487,\"size\":42,\"fingerprint_fnv1a32\":\"71B4A4A8\"},"
        "{\"role\":\"audio-mailboxes\",\"source_entry\":\"prg/fixed\",\"source_offset\":%llu,\"cpu_address\":62194,\"size\":8,\"fingerprint_fnv1a32\":\"17DE7030\"}],"
        "\"native_contract\":{\"payload_size\":%u,"
        "\"payload_fingerprint_fnv1a32\":\"%08X\","
        "\"effect_ids\":[8,10],\"instruction_count\":%u,"
        "\"voice_count\":%u,\"channels\":[\"pulse1\",\"pulse2\",\"triangle\",\"noise\"],"
        "\"title_stop_frame\":5,\"title_setup_yield_count\":5,"
        "\"title_setup_yield_proof_fnv1a32\":\"CA4CA88A\","
        "\"title_confirmation_cue_frame\":1,"
        "\"title_handoff_frame\":127,\"title_animation_frames\":126,"
        "\"menu_music_track\":6,"
        "\"menu_cue_condition\":\"accepted-player1-a-release-only\","
        "\"runtime_raw_pointer_or_opcode_dependency\":false}}",
        prefix, TECMO_ASSET_PACK_FRONTEND_SFX_ID,
        (unsigned long long)p->sfx_directory_offset,
        (unsigned long long)p->effect_offsets[0],
        (unsigned long long)p->effect_offsets[1],
        (unsigned long long)p->title_setup_offset,
        (unsigned long long)p->title_confirm_offset,
        (unsigned long long)p->title_setup_bridge_offset,
        (unsigned long long)p->title_setup_transition_offset,
        (unsigned long long)p->title_setup_fade_offset,
        (unsigned long long)p->frame_yield_helper_offset,
        (unsigned long long)p->title_stop_bridge_offset,
        (unsigned long long)p->title_stop_dispatch_offset,
        (unsigned long long)p->title_stop_offset,
        (unsigned long long)p->menu_accept_offset,
        (unsigned long long)p->menu_flow_offset,
        (unsigned long long)p->mailboxes_offset,
        p->payload_size, p->payload_fingerprint, p->instruction_count,
        (unsigned)p->voice_count);
}

static int append_team_data_source_map_entry(char *buffer,
                                             size_t capacity,
                                             size_t *length,
                                             int *first,
                                             const TecmoTeamDataProvenance *p)
{
    const char *prefix = *first != 0 ? "" : ",\n";
    *first = 0;
    return tecmo_asset_pack_append_text(
        buffer, capacity, length,
        "%s"
        "    {\"id\":\"%s\",\"kind\":\"team-data-native\","
        "\"schema\":\"tecmo.team-data/TTDT-1\",\"input_contract\":\"ines-only\","
        "\"runtime_dependencies\":["
        "{\"entry\":\"chr/all\",\"size\":262144,\"fingerprint_fnv1a32\":\"F6F6E854\",\"fingerprint_fnv1a64\":\"96A64F53B240ABB4\"}],"
        "\"sources\":["
        "{\"role\":\"root-dispatch-vector\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"bank\":3,\"cpu_address\":33208,\"size\":14,\"fingerprint_fnv1a32\":\"DCA1F834\"},"
        "{\"role\":\"season-dispatch-vector\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"bank\":3,\"cpu_address\":33818,\"size\":12,\"fingerprint_fnv1a32\":\"2CD2DE1C\"},"
        "{\"role\":\"entry-and-return-routes\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"bank\":3,\"cpu_address\":35835,\"size\":56,\"fingerprint_fnv1a32\":\"B67478D8\"},"
        "{\"role\":\"team-data-core-flow\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"bank\":3,\"cpu_address\":35891,\"size\":102,\"fingerprint_fnv1a32\":\"C831DFE1\"},"
        "{\"role\":\"profile-route-vector\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"bank\":3,\"cpu_address\":35993,\"size\":6,\"fingerprint_fnv1a32\":\"E51885D7\"},"
        "{\"role\":\"profile-roster-player-flow\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"bank\":3,\"cpu_address\":35999,\"size\":1462,\"fingerprint_fnv1a32\":\"B9232256\"},"
        "{\"role\":\"generic-input-and-coordinate-flow\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"bank\":3,\"cpu_address\":40646,\"size\":469,\"fingerprint_fnv1a32\":\"91DC81F3\"},"
        "{\"role\":\"team-selector-flow-and-tables\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"bank\":3,\"cpu_address\":41115,\"size\":451,\"fingerprint_fnv1a32\":\"A60D4C69\"},"
        "{\"role\":\"selector-cursor-record\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"bank\":3,\"cpu_address\":41405,\"size\":5,\"fingerprint_fnv1a32\":\"547D7A6F\",\"resolved_chr_offsets\":[49728,49744]},"
        "{\"role\":\"generic-cursor-record\",\"source_entry\":\"prg/bank01\",\"source_offset\":%llu,\"bank\":1,\"cpu_address\":32817,\"size\":5,\"fingerprint_fnv1a32\":\"7D5835D4\",\"resolved_chr_offsets\":[49728,49744]},"
        "{\"role\":\"team-rosters-and-player-records\",\"source_entry\":\"prg/bank02\",\"source_offset\":%llu,\"bank\":2,\"cpu_address\":32769,\"size\":7022,\"fingerprint_fnv1a32\":\"93B76340\"},"
        "{\"role\":\"team-profile-records\",\"source_entry\":\"prg/bank02\",\"source_offset\":%llu,\"bank\":2,\"cpu_address\":40384,\"size\":2146,\"fingerprint_fnv1a32\":\"ECFDCBCB\"},"
        "{\"role\":\"team-city-and-nickname-strings\",\"source_entry\":\"prg/bank06\",\"source_offset\":%llu,\"bank\":6,\"cpu_address\":44107,\"pointer_tables\":[44384,44747],\"encoding\":\"length-prefixed-uppercase\"},"
        "{\"role\":\"team-logo-layout-and-selector-tables\",\"source_entry\":\"prg/bank06\",\"source_offset\":%llu,\"bank\":6,\"cpu_address\":41700,\"size\":486,\"fingerprint_fnv1a32\":\"91339DD9\"},"
        "{\"role\":\"team-profile-palette-groups\",\"source_entry\":\"prg/bank06\",\"source_offset\":%llu,\"bank\":6,\"cpu_address\":44043,\"size\":64,\"fingerprint_fnv1a32\":\"DC51B191\",\"pointer_order_cpu_addresses\":[44059,44043,44075,44091]},"
        "{\"role\":\"portrait-selector\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"bank\":3,\"cpu_address\":41564,\"size\":2,\"fingerprint_fnv1a32\":\"0AE8D0EA\"},"
        "{\"role\":\"portrait-layouts\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"bank\":3,\"cpu_address\":46130,\"size\":192,\"fingerprint_fnv1a32\":\"58566055\"},"
        "{\"role\":\"portrait-compositor-flow\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"bank\":3,\"cpu_address\":36188,\"size\":136,\"fingerprint_fnv1a32\":\"4866441B\"},"
        "{\"role\":\"player-detail-and-direct-all-star-flow\",\"source_entry\":\"prg/bank02\",\"source_offset\":%llu,\"bank\":2,\"cpu_address\":43657,\"size\":722,\"fingerprint_fnv1a32\":\"BEFE9E46\"},"
        "{\"role\":\"ability-meter-flow\",\"source_entry\":\"prg/bank02\",\"source_offset\":%llu,\"bank\":2,\"cpu_address\":44379,\"size\":32,\"fingerprint_fnv1a32\":\"8B098364\"},"
        "{\"role\":\"portrait-metatile-tiles\",\"source_entry\":\"prg/bank00\",\"source_offset\":%llu,\"bank\":0,\"cpu_address\":32769,\"size\":108,\"fingerprint_fnv1a32\":\"A3D60DC1\"},"
        "{\"role\":\"portrait-metatile-attributes\",\"source_entry\":\"prg/bank00\",\"source_offset\":%llu,\"bank\":0,\"cpu_address\":32881,\"size\":27,\"fingerprint_fnv1a32\":\"427070B7\"},"
        "{\"role\":\"player-condition-seed-flow\",\"source_entry\":\"prg/bank01\",\"source_offset\":%llu,\"bank\":1,\"cpu_address\":48927,\"size\":13,\"fingerprint_fnv1a32\":\"555E360C\"},"
        "{\"role\":\"team-logo-metatile-expansion\",\"source_entry\":\"prg/bank06\",\"source_offset\":%llu,\"bank\":6,\"cpu_address\":42191,\"size\":1916,\"fingerprint_fnv1a32\":\"D27CA55E\"},"
        "{\"role\":\"team-logo-origins\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"bank\":3,\"cpu_address\":32791,\"size\":29,\"fingerprint_fnv1a32\":\"6A54CD12\"},"
        "{\"role\":\"sprite-palette\",\"source_entry\":\"prg/bank00\",\"source_offset\":%llu,\"bank\":0,\"cpu_address\":48695,\"size\":16,\"fingerprint_fnv1a32\":\"F85BA74A\"},"
        "{\"role\":\"screen-descriptors\",\"source_entry\":\"prg/fixed\",\"source_offsets\":[%llu,%llu,%llu],\"cpu_addresses\":[56537,56544,56551],\"size_each\":7,\"fingerprints_fnv1a32\":[\"64B5020C\",\"5967A0DA\",\"7CE3E553\"]},"
        "{\"role\":\"screen-streams\",\"source_entries\":[\"prg/bank01\",\"prg/bank00\",\"prg/bank00\"],\"source_offsets\":[%llu,%llu,%llu],\"cpu_addresses\":[47957,46226,34685],\"encoded_sizes\":[%llu,%llu,%llu],\"decoded_size_each\":1024,\"encoded_fingerprints_fnv1a32\":[\"A90DA6A3\",\"EA330745\",\"12CF0CA2\"],\"decoded_fingerprints_fnv1a32\":[\"9265F597\",\"F6D644A6\",\"69A3DB3E\"]},"
        "{\"role\":\"screen-palettes\",\"source_entries\":[\"prg/bank01\",\"prg/bank00\",\"prg/bank00\"],\"source_offsets\":[%llu,%llu,%llu],\"cpu_addresses\":[48274,46560,32909],\"size_each\":16,\"fingerprints_fnv1a32\":[\"913CE83E\",\"98634D94\",\"F49FA2BC\"]},"
        "{\"role\":\"fixed-input-helpers\",\"source_entry\":\"prg/fixed\",\"source_offset\":%llu,\"cpu_address\":55060,\"size\":212,\"fingerprint_fnv1a32\":\"BC71E228\"},"
        "{\"role\":\"fixed-screen-loader\",\"source_entry\":\"prg/fixed\",\"source_offset\":%llu,\"cpu_address\":55595,\"size\":492,\"fingerprint_fnv1a32\":\"E07A8EB7\"},"
        "{\"role\":\"fixed-fade-flow\",\"source_entry\":\"prg/fixed\",\"source_offset\":%llu,\"cpu_address\":56101,\"size\":171,\"fingerprint_fnv1a32\":\"C59694B5\"},"
        "{\"role\":\"fixed-metatile-tiles\",\"source_entry\":\"prg/fixed\",\"source_offset\":%llu,\"cpu_address\":50222,\"size\":13,\"fingerprint_fnv1a32\":\"DB6A6AEE\"},"
        "{\"role\":\"fixed-metatile-attribute-helper\",\"source_entry\":\"prg/fixed\",\"source_offset\":%llu,\"cpu_address\":51953,\"size\":4,\"fingerprint_fnv1a32\":\"2D477AB7\"},"
        "{\"role\":\"fixed-metatile-compositor\",\"source_entry\":\"prg/fixed\",\"source_offset\":%llu,\"cpu_address\":54725,\"size\":147,\"fingerprint_fnv1a32\":\"24E23095\"},"
        "{\"role\":\"fixed-home-uniform-color-table\",\"source_entry\":\"prg/fixed\",\"source_offset\":%llu,\"cpu_address\":56345,\"size\":29,\"fingerprint_fnv1a32\":\"1451114F\"},"
        "{\"role\":\"fixed-gameplay-uniform-color-setup\",\"source_entry\":\"prg/fixed\",\"source_offset\":%llu,\"cpu_address\":57003,\"size\":53,\"fingerprint_fnv1a32\":\"B2808BB3\"},"
        "{\"role\":\"full-chr\",\"source_entry\":\"chr/all\",\"source_offset\":%llu,\"size\":262144,\"fingerprint_fnv1a32\":\"F6F6E854\",\"fingerprint_fnv1a64\":\"96A64F53B240ABB4\"}],"
        "\"native_contract\":{\"payload_size\":%u,\"payload_fingerprint_fnv1a32\":\"812628F0\",\"screens\":3,\"selector_entries\":29,\"teams\":29,\"real_team_logos\":27,\"logo_cell_limit\":60,\"profile_palette_groups\":4,\"players_per_team\":12,\"player_stride\":184,\"portrait_cells\":24,\"gameplay_uniform_colors\":{\"home_table_entries\":29,\"away_default\":48,\"away_lakers\":56,\"lakers_team_id\":12},\"roster_rows\":6,\"roster_pages\":2,\"slide_frames\":32,\"slide_pixels_per_frame\":8,\"entry_transition\":{\"render_on\":4,\"first_visible\":7,\"palette_step\":4,\"stable\":20},\"selector_profile_transition\":{\"black\":8,\"render_off\":10,\"render_on\":16,\"first_visible\":19,\"palette_step\":4,\"stable\":32},\"roster_detail_transition\":{\"black\":8,\"render_off\":10,\"render_on\":15,\"first_visible\":18,\"palette_step\":4,\"stable\":31},\"profile_roster_transition\":\"oam-only-stable-next-frame\",\"all_star_name_mapping\":\"direct-player-pointer-to-real-roster-slot\",\"terminal\":\"player-detail-no-gameplay\"}}",
        prefix, TECMO_ASSET_PACK_TEAM_DATA_ID,
        (unsigned long long)p->root_vector_offset,
        (unsigned long long)p->season_vector_offset,
        (unsigned long long)p->entry_return_offset,
        (unsigned long long)p->core_flow_offset,
        (unsigned long long)p->route_vector_offset,
        (unsigned long long)p->team_data_flow_offset,
        (unsigned long long)p->generic_input_offset,
        (unsigned long long)p->selector_flow_offset,
        (unsigned long long)p->selector_cursor_offset,
        (unsigned long long)p->generic_cursor_offset,
        (unsigned long long)p->roster_data_offset,
        (unsigned long long)p->profile_data_offset,
        (unsigned long long)p->team_string_offset,
        (unsigned long long)p->logo_layout_offset,
        (unsigned long long)p->profile_palette_offset,
        (unsigned long long)p->portrait_selector_offset,
        (unsigned long long)p->portrait_layout_offset,
        (unsigned long long)p->portrait_flow_offset,
        (unsigned long long)p->profile_detail_flow_offset,
        (unsigned long long)p->meter_flow_offset,
        (unsigned long long)p->metatile_tiles_offset,
        (unsigned long long)p->metatile_attributes_offset,
        (unsigned long long)p->condition_seed_offset,
        (unsigned long long)p->logo_expansion_offset,
        (unsigned long long)p->logo_origin_offset,
        (unsigned long long)p->sprite_palette_offset,
        (unsigned long long)p->descriptor_offsets[0],
        (unsigned long long)p->descriptor_offsets[1],
        (unsigned long long)p->descriptor_offsets[2],
        (unsigned long long)p->stream_offsets[0],
        (unsigned long long)p->stream_offsets[1],
        (unsigned long long)p->stream_offsets[2],
        (unsigned long long)p->stream_sizes[0],
        (unsigned long long)p->stream_sizes[1],
        (unsigned long long)p->stream_sizes[2],
        (unsigned long long)p->palette_offsets[0],
        (unsigned long long)p->palette_offsets[1],
        (unsigned long long)p->palette_offsets[2],
        (unsigned long long)p->fixed_input_offset,
        (unsigned long long)p->fixed_loader_offset,
        (unsigned long long)p->fixed_fade_offset,
        (unsigned long long)p->fixed_metatile_tiles_offset,
        (unsigned long long)p->fixed_metatile_attribute_offset,
        (unsigned long long)p->fixed_compositor_offset,
        (unsigned long long)p->fixed_home_uniform_colors_offset,
        (unsigned long long)p->fixed_gameplay_uniform_setup_offset,
        (unsigned long long)p->chr_offset,
        (unsigned)TECMO_ASSET_PACK_TEAM_DATA_SIZE);
}

static int append_team_management_source_map_entry(
    char *buffer,
    size_t capacity,
    size_t *length,
    int *first,
    const TecmoTeamManagementProvenance *p)
{
    const char *prefix = *first != 0 ? "" : ",\n";
    *first = 0;
    return tecmo_asset_pack_append_text(
        buffer, capacity, length,
        "%s"
        "    {\"id\":\"%s\",\"kind\":\"team-management-native\","
        "\"schema\":\"tecmo.team-management/TTMG-1\",\"input_contract\":\"ines-only\","
        "\"runtime_dependencies\":["
        "{\"entry\":\"menu/team-data\",\"size\":96372,\"fingerprint_fnv1a32\":\"812628F0\"},"
        "{\"entry\":\"chr/all\",\"size\":262144,\"fingerprint_fnv1a32\":\"F6F6E854\",\"fingerprint_fnv1a64\":\"96A64F53B240ABB4\"}],"
        "\"sources\":["
        "{\"role\":\"profile-route-vector\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"bank\":3,\"cpu_address\":35993,\"size\":6,\"fingerprint_fnv1a32\":\"E51885D7\"},"
        "{\"role\":\"starters-flow\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"bank\":3,\"cpu_address\":36431,\"size\":696,\"fingerprint_fnv1a32\":\"12AB26E8\"},"
        "{\"role\":\"lineup-renderer\",\"source_entry\":\"prg/bank02\",\"source_offset\":%llu,\"bank\":2,\"cpu_address\":42545,\"size\":443,\"fingerprint_fnv1a32\":\"EDD2525E\"},"
        "{\"role\":\"generic-selectors\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"bank\":3,\"cpu_address\":40646,\"size\":469,\"fingerprint_fnv1a32\":\"91DC81F3\"},"
        "{\"role\":\"fixed-input\",\"source_entry\":\"prg/fixed\",\"source_offset\":%llu,\"cpu_address\":55060,\"size\":212,\"fingerprint_fnv1a32\":\"BC71E228\"},"
        "{\"role\":\"playbook-flow\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"bank\":3,\"cpu_address\":37127,\"size\":334,\"fingerprint_fnv1a32\":\"122C684F\"},"
        "{\"role\":\"playbook-helper-renderer\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"bank\":3,\"cpu_address\":44299,\"size\":1217,\"fingerprint_fnv1a32\":\"E5088534\"},"
        "{\"role\":\"play-names\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"bank\":3,\"cpu_address\":37461,\"size\":160,\"fingerprint_fnv1a32\":\"B26B5DA6\"},"
        "{\"role\":\"playbook-pointers\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"bank\":3,\"cpu_address\":44801,\"size\":16,\"fingerprint_fnv1a32\":\"7214CFEB\"},"
        "{\"role\":\"playbook-oam\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"bank\":3,\"cpu_address\":44971,\"size\":113,\"fingerprint_fnv1a32\":\"86E58601\"},"
        "{\"role\":\"play-diagrams\",\"source_entry\":\"prg/bank00\",\"source_offset\":%llu,\"bank\":0,\"cpu_address\":41187,\"size\":512,\"fingerprint_fnv1a32\":\"C6C032A0\"},"
        "{\"role\":\"play-marker\",\"source_entry\":\"prg/bank00\",\"source_offset\":%llu,\"bank\":0,\"cpu_address\":41699,\"size\":16,\"fingerprint_fnv1a32\":\"2AF12A2B\"},"
        "{\"role\":\"session-default-initializer\",\"source_entry\":\"prg/bank01\",\"source_offset\":%llu,\"bank\":1,\"cpu_address\":48848,\"size\":79,\"fingerprint_fnv1a32\":\"FF4433C8\"},"
        "{\"role\":\"screen-descriptors\",\"source_entry\":\"prg/fixed\",\"source_offsets\":[%llu,%llu],\"cpu_addresses\":[56558,56565],\"size_each\":7,\"fingerprints_fnv1a32\":[\"EEC27A7D\",\"EDA72C5C\"]},"
        "{\"role\":\"screen-streams\",\"source_entries\":[\"prg/bank00\",\"prg/bank01\"],\"source_offsets\":[%llu,%llu],\"encoded_sizes\":[%llu,%llu],\"encoded_fingerprints_fnv1a32\":[\"C869A670\",\"3111C9BF\"]},"
        "{\"role\":\"screen-palettes\",\"source_entries\":[\"prg/bank00\",\"prg/bank01\"],\"source_offsets\":[%llu,%llu],\"size_each\":16,\"fingerprints_fnv1a32\":[\"98634D94\",\"0242ED20\"]},"
        "{\"role\":\"full-chr\",\"source_entry\":\"chr/all\",\"source_offset\":%llu,\"size\":262144,\"fingerprint_fnv1a32\":\"F6F6E854\",\"fingerprint_fnv1a64\":\"96A64F53B240ABB4\"}],"
        "\"native_contract\":{\"payload_size\":%u,\"payload_fingerprint_fnv1a32\":\"D192EAC6\",\"teams\":29,\"starters\":5,\"bench_choices\":7,\"playbook_slots\":4,\"plays\":8,\"carousel_frames\":8,\"terminal\":\"management-only-no-gameplay\"}}",
        prefix, TECMO_ASSET_PACK_TEAM_MANAGEMENT_ID,
        (unsigned long long)p->route_vector_offset,
        (unsigned long long)p->starters_flow_offset,
        (unsigned long long)p->lineup_renderer_offset,
        (unsigned long long)p->generic_input_offset,
        (unsigned long long)p->fixed_input_offset,
        (unsigned long long)p->playbook_flow_offset,
        (unsigned long long)p->playbook_helper_offset,
        (unsigned long long)p->playbook_names_offset,
        (unsigned long long)p->playbook_pointer_offset,
        (unsigned long long)p->playbook_oam_offset,
        (unsigned long long)p->playbook_diagrams_offset,
        (unsigned long long)p->playbook_marker_offset,
        (unsigned long long)p->defaults_offset,
        (unsigned long long)p->descriptor_offsets[0],
        (unsigned long long)p->descriptor_offsets[1],
        (unsigned long long)p->stream_offsets[0],
        (unsigned long long)p->stream_offsets[1],
        (unsigned long long)p->stream_sizes[0],
        (unsigned long long)p->stream_sizes[1],
        (unsigned long long)p->palette_offsets[0],
        (unsigned long long)p->palette_offsets[1],
        (unsigned long long)p->chr_offset,
        (unsigned)TECMO_ASSET_PACK_TEAM_MANAGEMENT_SIZE);
}

static int append_season_source_map_entry(char *buffer,
                                          size_t capacity,
                                          size_t *length,
                                          int *first,
                                          const TecmoSeasonMenuProvenance *p)
{
    const char *prefix = *first != 0 ? "" : ",\n";
    *first = 0;
    return tecmo_asset_pack_append_text(
        buffer, capacity, length,
        "%s"
        "    {\"id\":\"%s\",\"kind\":\"season-management-native\","
        "\"schema\":\"tecmo.season/TSNS-1\",\"input_contract\":\"ines-only\","
        "\"runtime_dependencies\":["
        "{\"entry\":\"chr/all\",\"size\":262144,\"fingerprint_fnv1a32\":\"F6F6E854\",\"fingerprint_fnv1a64\":\"96A64F53B240ABB4\"},"
        "{\"entry\":\"menu/team-data\",\"schema\":\"tecmo.team-data/TTDT-1\",\"size\":96372,\"fingerprint_fnv1a32\":\"812628F0\"}],"
        "\"sources\":["
        "{\"role\":\"season-dispatch\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"cpu_address\":33715,\"size\":115,\"fingerprint_fnv1a32\":\"FAA5DF69\"},"
        "{\"role\":\"route-table\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"cpu_address\":33818,\"size\":12,\"fingerprint_fnv1a32\":\"2CD2DE1C\"},"
        "{\"role\":\"leaders-route\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"cpu_address\":33830,\"size\":12,\"fingerprint_fnv1a32\":\"5FF8449F\"},"
        "{\"role\":\"game-start-prelaunch\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"cpu_address\":34068,\"size\":133,\"fingerprint_fnv1a32\":\"3A0F404F\"},"
        "{\"role\":\"game-launch-terminal-unexecuted\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"cpu_address\":34201,\"size\":63,\"fingerprint_fnv1a32\":\"E8D5B0AC\"},"
        "{\"role\":\"game-launch-target-unexecuted\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"cpu_address\":45695,\"size\":4,\"fingerprint_fnv1a32\":\"8145527E\"},"
        "{\"role\":\"team-control-flow\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"cpu_address\":35103,\"size\":192,\"fingerprint_fnv1a32\":\"8289F925\"},"
        "{\"role\":\"standings-and-programmed-editor-flow\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"cpu_address\":35349,\"size\":510,\"fingerprint_fnv1a32\":\"8AA17312\"},"
        "{\"role\":\"schedule-flow\",\"source_entry\":\"prg/bank03\",\"source_offsets\":[%llu,%llu],\"cpu_addresses\":[37799,38266],\"sizes\":[467,904],\"fingerprints_fnv1a32\":[\"ED427D23\",\"A0B59AF2\"]},"
        "{\"role\":\"semantic-menu-records-and-box-descriptors\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"cpu_address\":40448,\"size\":198,\"fingerprint_fnv1a32\":\"693C1CB1\"},"
        "{\"role\":\"popup-cursor-coordinate-tables\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"cpu_address\":40723,\"size\":58,\"fingerprint_fnv1a32\":\"E5A66DF5\"},"
        "{\"role\":\"standings-games-behind-renderer\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"cpu_address\":42299,\"size\":42,\"fingerprint_fnv1a32\":\"8BD60DDE\",\"zero_map_cpu_addresses\":[39624,39625],\"resolved_zero_tiles\":[255,0],\"resolved_half_tile\":248,\"resolved_half_chr_offset\":257920},"
        "{\"role\":\"font-map\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"cpu_address\":39624,\"size\":59,\"fingerprint_fnv1a32\":\"286D27BB\"},"
        "{\"role\":\"regular-season-schedule\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"cpu_address\":46383,\"size\":2214,\"records\":1107,\"fingerprint_fnv1a32\":\"24112737\"},"
        "{\"role\":\"season-defaults\",\"source_entry\":\"prg/bank01\",\"source_offset\":%llu,\"cpu_address\":48848,\"size\":79,\"fingerprint_fnv1a32\":\"FF4433C8\"},"
        "{\"role\":\"cursor-record\",\"source_entry\":\"prg/bank01\",\"source_offset\":%llu,\"cpu_address\":32817,\"size\":5,\"fingerprint_fnv1a32\":\"7D5835D4\",\"resolved_chr_offsets\":[49728,49744]},"
        "{\"role\":\"sprite-palette\",\"source_entry\":\"prg/bank00\",\"source_offset\":%llu,\"cpu_address\":48695,\"size\":16,\"fingerprint_fnv1a32\":\"F85BA74A\"},"
        "{\"role\":\"leader-label-records\",\"source_entry\":\"prg/bank00\",\"source_offset\":%llu,\"cpu_address\":45896,\"size\":165,\"fingerprint_fnv1a32\":\"7CFF9EFB\"},"
        "{\"role\":\"division-starts\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"cpu_address\":45699,\"size\":4,\"fingerprint_fnv1a32\":\"3B420652\"},"
        "{\"role\":\"division-team-order\",\"source_entry\":\"prg/bank03\",\"source_offset\":%llu,\"cpu_address\":45703,\"size\":27,\"fingerprint_fnv1a32\":\"B33FB72A\"},"
        "{\"role\":\"leader-navigation\",\"source_entry\":\"prg/bank00\",\"source_offset\":%llu,\"cpu_address\":44349,\"size\":28,\"fingerprint_fnv1a32\":\"EB6AA6B8\"},"
        "{\"role\":\"leader-template-map\",\"source_entry\":\"prg/bank00\",\"source_offset\":%llu,\"cpu_address\":44565,\"size\":7,\"fingerprint_fnv1a32\":\"0FB6BBED\"},"
        "{\"role\":\"leader-category-flow\",\"source_entry\":\"prg/bank00\",\"source_offset\":%llu,\"cpu_address\":44210,\"size\":362,\"fingerprint_fnv1a32\":\"9C715947\"},"
        "{\"role\":\"leader-ranking-flow\",\"source_entry\":\"prg/bank00\",\"source_offset\":%llu,\"cpu_address\":45260,\"size\":180,\"fingerprint_fnv1a32\":\"FFADC10A\"},"
        "{\"role\":\"leader-row-flow\",\"source_entry\":\"prg/bank00\",\"source_offset\":%llu,\"cpu_address\":46128,\"size\":128,\"fingerprint_fnv1a32\":\"AC47E9DB\"},"
        "{\"role\":\"fixed-input-helpers\",\"source_entry\":\"prg/fixed\",\"source_offset\":%llu,\"cpu_address\":55060,\"size\":212,\"fingerprint_fnv1a32\":\"BC71E228\"},"
        "{\"role\":\"fixed-screen-loader\",\"source_entry\":\"prg/fixed\",\"source_offset\":%llu,\"cpu_address\":55595,\"size\":492,\"fingerprint_fnv1a32\":\"E07A8EB7\"},"
        "{\"role\":\"screen-descriptors\",\"source_entry\":\"prg/fixed\",\"source_offsets\":[%llu,%llu,%llu,%llu,%llu],\"cpu_addresses\":[56572,56579,56586,56593,56600],\"size_each\":7,\"fingerprints_fnv1a32\":[\"0D624597\",\"23D90656\",\"4EA1E05B\",\"9373DC72\",\"4EB8B3AB\"]},"
        "{\"role\":\"screen-streams\",\"source_offsets\":[%llu,%llu,%llu,%llu,%llu],\"encoded_sizes\":[%llu,%llu,%llu,%llu,%llu],\"decoded_sizes\":[2048,1024,2048,1024,2048],\"encoded_fingerprints_fnv1a32\":[\"CE7BD109\",\"4C78CE45\",\"1298E5F8\",\"33EB31D7\",\"3C7D9D61\"]},"
        "{\"role\":\"screen-palettes\",\"source_offsets\":[%llu,%llu,%llu,%llu,%llu],\"size_each\":16,\"fingerprints_fnv1a32\":[\"B389D1A4\",\"7F07658B\",\"B389D1A4\",\"B389D1A4\",\"B389D1A4\"]},"
        "{\"role\":\"leader-screen-descriptors\",\"source_entry\":\"prg/fixed\",\"source_offsets\":[%llu,%llu,%llu,%llu,%llu,%llu,%llu],\"cpu_addresses\":[56719,56726,56733,56740,56747,56754,56761],\"size_each\":7,\"fingerprints_fnv1a32\":[\"F981E49D\",\"0E226C60\",\"6372358A\",\"71A6D7FC\",\"24D27A4B\",\"D59A23BB\",\"A1F18139\"]},"
        "{\"role\":\"leader-screen-streams\",\"source_offsets\":[%llu,%llu,%llu,%llu,%llu,%llu,%llu],\"encoded_sizes\":[%llu,%llu,%llu,%llu,%llu,%llu,%llu],\"decoded_size_each\":1024,\"encoded_fingerprints_fnv1a32\":[\"1CE88C44\",\"8A23D17D\",\"79815D52\",\"AC3F7A71\",\"C33278AD\",\"206FC3A7\",\"E3851387\"]},"
        "{\"role\":\"leader-screen-palettes\",\"source_offsets\":[%llu,%llu,%llu,%llu,%llu,%llu,%llu],\"size_each\":16,\"fingerprints_fnv1a32\":[\"14D2E107\",\"72B80255\",\"72B80255\",\"4D920E47\",\"72B80255\",\"4D920E47\",\"72B80255\"]},"
        "{\"role\":\"full-chr\",\"source_entry\":\"chr/all\",\"source_offset\":%llu,\"size\":262144,\"fingerprint_fnv1a32\":\"F6F6E854\",\"fingerprint_fnv1a64\":\"96A64F53B240ABB4\"}],"
        "\"native_contract\":{\"payload_size\":%u,\"payload_fingerprint_fnv1a32\":\"%08X\",\"screens\":[17,18,19,20,21,38,39,40,41,42,43,44],\"teams\":27,\"schedule_records\":1107,\"filtered_schedule_counts\":[1107,567,351,1107],\"leader_categories\":7,\"native_game_simulation\":false,\"game_result_boundary\":\"pending-completed-result-required\",\"controlled_match_terminal\":\"gameplay-launch-blocked\",\"unexecuted_boundary_cpu\":34201,\"unexecuted_target_cpu\":45695}}",
        prefix, TECMO_ASSET_PACK_SEASON_MENU_ID,
        (unsigned long long)p->dispatch_offset,
        (unsigned long long)p->route_table_offset,
        (unsigned long long)p->leaders_offset,
        (unsigned long long)p->game_prelaunch_offset,
        (unsigned long long)p->game_terminal_offset,
        (unsigned long long)p->game_launch_target_offset,
        (unsigned long long)p->team_control_offset,
        (unsigned long long)p->standings_offset,
        (unsigned long long)p->schedule_core_offset,
        (unsigned long long)p->schedule_helpers_offset,
        (unsigned long long)p->popup_records_offset,
        (unsigned long long)p->popup_cursor_tables_offset,
        (unsigned long long)p->standings_half_game_tile_offset,
        (unsigned long long)p->font_offset,
        (unsigned long long)p->schedule_offset,
        (unsigned long long)p->defaults_offset,
        (unsigned long long)p->cursor_offset,
        (unsigned long long)p->sprite_palette_offset,
        (unsigned long long)p->leader_records_offset,
        (unsigned long long)p->division_starts_offset,
        (unsigned long long)p->division_teams_offset,
        (unsigned long long)p->leader_navigation_offset,
        (unsigned long long)p->leader_template_offset,
        (unsigned long long)p->leader_core_offset,
        (unsigned long long)p->leader_ranking_offset,
        (unsigned long long)p->leader_rows_offset,
        (unsigned long long)p->fixed_input_offset,
        (unsigned long long)p->fixed_loader_offset,
        (unsigned long long)p->descriptor_offsets[0],
        (unsigned long long)p->descriptor_offsets[1],
        (unsigned long long)p->descriptor_offsets[2],
        (unsigned long long)p->descriptor_offsets[3],
        (unsigned long long)p->descriptor_offsets[4],
        (unsigned long long)p->stream_offsets[0],
        (unsigned long long)p->stream_offsets[1],
        (unsigned long long)p->stream_offsets[2],
        (unsigned long long)p->stream_offsets[3],
        (unsigned long long)p->stream_offsets[4],
        (unsigned long long)p->stream_sizes[0],
        (unsigned long long)p->stream_sizes[1],
        (unsigned long long)p->stream_sizes[2],
        (unsigned long long)p->stream_sizes[3],
        (unsigned long long)p->stream_sizes[4],
        (unsigned long long)p->palette_offsets[0],
        (unsigned long long)p->palette_offsets[1],
        (unsigned long long)p->palette_offsets[2],
        (unsigned long long)p->palette_offsets[3],
        (unsigned long long)p->palette_offsets[4],
        (unsigned long long)p->leader_descriptor_offsets[0],
        (unsigned long long)p->leader_descriptor_offsets[1],
        (unsigned long long)p->leader_descriptor_offsets[2],
        (unsigned long long)p->leader_descriptor_offsets[3],
        (unsigned long long)p->leader_descriptor_offsets[4],
        (unsigned long long)p->leader_descriptor_offsets[5],
        (unsigned long long)p->leader_descriptor_offsets[6],
        (unsigned long long)p->leader_stream_offsets[0],
        (unsigned long long)p->leader_stream_offsets[1],
        (unsigned long long)p->leader_stream_offsets[2],
        (unsigned long long)p->leader_stream_offsets[3],
        (unsigned long long)p->leader_stream_offsets[4],
        (unsigned long long)p->leader_stream_offsets[5],
        (unsigned long long)p->leader_stream_offsets[6],
        (unsigned long long)p->leader_stream_sizes[0],
        (unsigned long long)p->leader_stream_sizes[1],
        (unsigned long long)p->leader_stream_sizes[2],
        (unsigned long long)p->leader_stream_sizes[3],
        (unsigned long long)p->leader_stream_sizes[4],
        (unsigned long long)p->leader_stream_sizes[5],
        (unsigned long long)p->leader_stream_sizes[6],
        (unsigned long long)p->leader_palette_offsets[0],
        (unsigned long long)p->leader_palette_offsets[1],
        (unsigned long long)p->leader_palette_offsets[2],
        (unsigned long long)p->leader_palette_offsets[3],
        (unsigned long long)p->leader_palette_offsets[4],
        (unsigned long long)p->leader_palette_offsets[5],
        (unsigned long long)p->leader_palette_offsets[6],
        (unsigned long long)p->chr_offset,
        (unsigned)TECMO_ASSET_PACK_SEASON_SIZE,
        (unsigned)TECMO_ASSET_PACK_SEASON_FNV1A32);
}

static int append_gameplay_source_map_entry(
    char *buffer,
    size_t capacity,
    size_t *length,
    int *first,
    const TecmoGameplayProvenance *p)
{
    static const char *const roles[TECMO_GAMEPLAY_ASSET_SOURCE_COUNT] = {
        "actor-metasprite-records", "actor-pointer-table",
        "actor-aeef-oam-data", "bank01-indexed-oam-renderer",
        "actor-palette-setup",
        "actor-palette-pointers", "actor-palette-groups",
        "fixed-actor-renderer", "actor-render-staging",
        "sprite-r2-selector-table", "rule-setup", "rule-lookup",
        "rule-subtype", "rule-animation", "rule-state",
        "rule-shot-result", "rule-shot-launch", "rule-close-shot",
        "rule-trajectory", "rule-finish", "period-banner-dispatch",
        "period-banner-pointers", "period-banner-strings",
        "scoreboard-violation-dispatch-and-text", "foul-overlay-and-text",
        "halftime-final-banner-loop-and-data",
        "live-orientation-select-and-dispatch", "live-orientation-screen-ids",
        "live-irq-arm", "live-irq-band-dispatch", "live-band-initializer"
    };
    const char *prefix = *first != 0 ? "" : ",\n";

    *first = 0;
    if (tecmo_asset_pack_append_text(
            buffer, capacity, length,
            "%s"
            "    {\"id\":\"%s\",\"kind\":\"gameplay-core-native\","
            "\"schema\":\"tecmo.gameplay/TGPL-1\",\"size\":%u,"
            "\"fingerprint_fnv1a32\":\"%08X\","
            "\"dependencies\":[{\"entry\":\"chr/all\",\"size\":%u,"
            "\"fingerprint_fnv1a32\":\"F6F6E854\","
            "\"fingerprint_fnv1a64\":\"96A64F53B240ABB4\"}],"
            "\"screens\":[",
            prefix, TECMO_ASSET_PACK_GAMEPLAY_ID,
            (unsigned)TECMO_ASSET_PACK_GAMEPLAY_SIZE,
            (unsigned)TECMO_ASSET_PACK_GAMEPLAY_FNV1A32,
            (unsigned)TECMO_ASSET_PACK_GAMEPLAY_CHR_SIZE) != 0) {
        return -1;
    }
    for (size_t index = 0U; index < TECMO_GAMEPLAY_ASSET_SCREEN_COUNT; ++index) {
        const TecmoGameplayExpectedScreen *screen =
            &tecmo_gameplay_expected_screens[index];
        if (tecmo_asset_pack_append_text(
                buffer, capacity, length,
                "%s{\"screen_id\":%u,"
                "\"descriptor\":{\"source_entry\":\"prg/fixed\","
                "\"source_offset\":%llu,\"cpu_address\":%u,\"size\":7,"
                "\"fingerprint_fnv1a32\":\"%08X\"},"
                "\"compressed_screen\":{\"source_entry\":\"prg/bank%02u\","
                "\"source_offset\":%llu,\"bank\":%u,\"cpu_address\":%u,"
                "\"encoded_size\":%llu,\"decoded_size\":2048,"
                "\"encoded_fingerprint_fnv1a32\":\"%08X\","
                "\"decoded_fingerprint_fnv1a32\":\"%08X\"},"
                "\"palette\":{\"source_entry\":\"prg/bank%02u\","
                "\"source_offset\":%llu,\"bank\":%u,\"cpu_address\":%u,"
                "\"size\":16,\"fingerprint_fnv1a32\":\"%08X\"},"
                "\"nametable_role\":\"live-orientation-base\","
                "\"descriptor_chr_role\":\"tipoff-close-up-only\","
                "\"live_chr_contract\":\"irq-band-context-required\"}",
                index == 0U ? "" : ",", (unsigned)screen->screen_id,
                (unsigned long long)p->descriptor_offsets[index],
                (unsigned)screen->descriptor_cpu,
                (unsigned)screen->descriptor_fingerprint,
                (unsigned)screen->source_bank,
                (unsigned long long)p->stream_offsets[index],
                (unsigned)screen->source_bank, (unsigned)screen->stream_cpu,
                (unsigned long long)p->stream_sizes[index],
                (unsigned)screen->encoded_fingerprint,
                (unsigned)screen->decoded_fingerprint,
                (unsigned)screen->source_bank,
                (unsigned long long)p->palette_offsets[index],
                (unsigned)screen->source_bank, (unsigned)screen->palette_cpu,
                (unsigned)screen->palette_fingerprint) != 0) {
            return -1;
        }
    }
    if (tecmo_asset_pack_append_text(buffer, capacity, length,
                                     "],\"source_spans\":[") != 0) {
        return -1;
    }
    for (size_t index = 0U; index < TECMO_GAMEPLAY_ASSET_SOURCE_COUNT; ++index) {
        const TecmoGameplayExpectedSource *source =
            &tecmo_gameplay_expected_sources[index];
        char source_entry[24];
        if (source->fixed_bank != 0U) {
            (void)snprintf(source_entry, sizeof(source_entry), "prg/fixed");
        } else {
            (void)snprintf(source_entry, sizeof(source_entry),
                           "prg/bank%02u", (unsigned)source->bank);
        }
        if (tecmo_asset_pack_append_text(
                buffer, capacity, length,
                "%s{\"role\":\"%s\",\"source_entry\":\"%s\","
                "\"source_offset\":%llu,\"bank\":%u,"
                "\"cpu_start\":%u,\"cpu_end\":%u,\"size\":%u,"
                "\"fingerprint_fnv1a32\":\"%08X\"}",
                index == 0U ? "" : ",", roles[index], source_entry,
                (unsigned long long)p->source_offsets[index],
                (unsigned)source->bank, (unsigned)source->cpu_start,
                (unsigned)((uint32_t)source->cpu_start +
                           source->byte_count - 1U),
                (unsigned)source->byte_count,
                (unsigned)source->fingerprint) != 0) {
            return -1;
        }
    }
    return tecmo_asset_pack_append_text(
        buffer, capacity, length,
        "],\"actor_oam_contract\":{"
        "\"data_cpu_range\":[44783,45011],\"data_size\":229,"
        "\"renderer_cpu_range\":[45012,45057],\"renderer_size\":46,"
        "\"renderer_end\":\"includes branch displacement and RTS at $B001\","
        "\"combined_fingerprint_fnv1a32\":\"2397C99B\"},"
        "\"live_background_contract\":{"
        "\"orientation_screens\":[27,46],"
        "\"orientation_dispatch_cpu_range\":[58679,58696],"
        "\"orientation_dispatch\":\"selects $1B/$2E then calls $D92E\","
        "\"irq_arm_cpu_range\":[52652,52688],"
        "\"irq_arm\":\"complete $CDAC-$CDD0 path through RTS\","
        "\"band_start_scanlines\":[0,32,48,80,128,176],"
        "\"pre_asl_pairs\":[[91,92],[93,93],[94,95],[96,97],[98,99],[100,null]],"
        "\"final_r1\":\"explicit team/context selector ($0599)\","
        "\"descriptor_chr\":\"tipoff hand/ball close-up, not live court\","
        "\"top_hud_nametable\":\"dynamic runtime writes, not frozen in orientation base\","
        "\"runtime_chr_dependency\":\"same-pack chr/all\"},"
        "\"pose_contract\":{\"pointer_cpu_start\":42425,"
        "\"pointer_cpu_end\":44782,\"pointer_count\":1179,"
        "\"record_cpu_range\":[32768,42425],\"max_pieces\":15,"
        "\"dimensions\":\"low-nibble columns, high-nibble rows\","
        "\"tile\":\"(cell & 0x3E) + actor_slot_base\","
        "\"actor_slot_base\":\"ROM-generatable $01/$41/$81/$C1\","
        "\"attributes\":\"(cell & 0x41) | actor_attributes\","
        "\"live_actor_attributes\":\"fixed $F1F2 supplies $04B0[actor] & $03: profile-group bit 0 plus team-side bit 1\","
        "\"pose_palette_recipe\":\"Bank01 $B0ED-$B133 selects $B138/$B148 with profile bit 0 and injects the side uniform color at offsets 6/7/9 plus its six-bit +$10 variant at offset 13\","
        "\"live_palette_recipe\":\"fixed $F2E2-$F2F1 supplies four profile/side groups; fixed $DEAB-$DEDF supplies the selected matchup colors at group entries 3/7/11/15\","
        "\"chr\":\"explicit MMC3 R2-R5 context\","
        "\"semantic_clip_names\":\"engine-state mapping pending\"}}");
}

static int append_gameplay_court_source_map_entry(
    char *buffer,
    size_t capacity,
    size_t *length,
    int *first,
    const TecmoGameplayCourtProvenance *p)
{
    static const char *const roles[
        TECMO_ASSET_PACK_GAMEPLAY_COURT_SOURCE_COUNT] = {
        "screen-0f-descriptor", "screen-0f-encoded-stream",
        "screen-0f-descriptor-palette", "court-pointer-tuple",
        "court-layout", "court-macro-tiles", "court-macro-attributes",
        "fixed-macro-builder-and-tables", "fixed-court-layout-loop",
        "fixed-live-background-palette"
    };
    const char *prefix = *first != 0 ? "" : ",\n";

    *first = 0;
    if (tecmo_asset_pack_append_text(
            buffer, capacity, length,
            "%s"
            "    {\"id\":\"%s\",\"kind\":\"gameplay-court-world-foundation\","
            "\"schema\":\"tecmo.gameplay-court/TGCT-1\",\"size\":%u,"
            "\"fingerprint_fnv1a32\":\"%08X\","
            "\"input_contract\":\"ines-only\","
            "\"dependencies\":[{\"entry\":\"chr/all\","
            "\"same_pack_required\":true,"
            "\"source_offset\":%llu,\"size\":%u,"
            "\"fingerprint_fnv1a32\":\"F6F6E854\","
            "\"fingerprint_fnv1a64\":\"96A64F53B240ABB4\"}],"
            "\"source_spans\":[",
            prefix, TECMO_ASSET_PACK_GAMEPLAY_COURT_ID,
            (unsigned)TECMO_ASSET_PACK_GAMEPLAY_COURT_SIZE,
            (unsigned)TECMO_ASSET_PACK_GAMEPLAY_COURT_FNV1A32,
            (unsigned long long)p->chr_offset,
            (unsigned)TECMO_ASSET_PACK_GAMEPLAY_COURT_CHR_SIZE) != 0) {
        return -1;
    }
    for (size_t index = 0U;
         index < TECMO_ASSET_PACK_GAMEPLAY_COURT_SOURCE_COUNT; ++index) {
        const TecmoGameplayCourtExpectedSource *source =
            &tecmo_gameplay_court_expected_sources[index];
        char source_entry[24];
        if (source->fixed_bank != 0U) {
            (void)snprintf(source_entry, sizeof(source_entry), "prg/fixed");
        } else {
            (void)snprintf(source_entry, sizeof(source_entry),
                           "prg/bank%02u", (unsigned)source->bank);
        }
        if (tecmo_asset_pack_append_text(
                buffer, capacity, length,
                "%s{\"role\":\"%s\",\"source_entry\":\"%s\","
                "\"source_offset\":%llu,\"bank\":%u,"
                "\"fixed_bank\":%s,\"cpu_start\":%u,\"cpu_end\":%u,"
                "\"size\":%u,\"fingerprint_fnv1a32\":\"%08X\"}",
                index == 0U ? "" : ",", roles[index], source_entry,
                (unsigned long long)p->source_offsets[index],
                (unsigned)source->bank,
                source->fixed_bank != 0U ? "true" : "false",
                (unsigned)source->cpu_start,
                (unsigned)((uint32_t)source->cpu_start +
                           source->byte_count - 1U),
                (unsigned)source->byte_count,
                (unsigned)source->fingerprint) != 0) {
            return -1;
        }
    }
    return tecmo_asset_pack_append_text(
        buffer, capacity, length,
        "],\"decoded_screen_contract\":{"
        "\"screen_id\":15,\"decoder\":\"fixed $D9F6\","
        "\"encoded_size\":201,\"decoded_size\":1024,"
        "\"decoded_fingerprint_fnv1a32\":\"483171E7\","
        "\"role\":\"descriptor scaffold validation, not the live court\"},"
        "\"native_contract\":{"
        "\"legacy_viewport_width_tiles\":32,"
        "\"legacy_viewport_height_tiles\":30,"
        "\"world_width_tiles\":96,\"world_height_tiles\":30,"
        "\"world_width_pixels\":768,\"world_height_pixels\":240,"
        "\"macro_rows\":15,\"macro_columns\":48,"
        "\"initial_nametable_fill\":\"$FF before macro writes; unused lower final-row attribute quadrants remain $FF\","
        "\"world_layout\":\"little-endian indexes from row*0x60+col*2\","
        "\"legacy_center_layout\":\"little-endian indexes from row*0x60+0x20+col*2\","
        "\"macro_cells\":\"four tiles in top-left, top-right, bottom-left, bottom-right order\","
        "\"palette\":\"(attribute[index] & 0x0C) >> 2 replicated across the 2x2 macro cell\","
        "\"macro_index_min\":0,\"macro_index_max\":360,"
        "\"world_unique_macro_indexes\":346,"
        "\"world_tile_count\":2880,"
        "\"world_tile_fingerprint_fnv1a32\":\"6458B5E5\","
        "\"world_palette_index_fingerprint_fnv1a32\":\"7F650645\","
        "\"legacy_unique_macro_indexes\":130,"
        "\"legacy_nametable_size\":1024,"
        "\"legacy_nametable_fingerprint_fnv1a32\":\"0CF54A0E\","
        "\"legacy_tile_fingerprint_fnv1a32\":\"D2F8364A\","
        "\"legacy_attribute_fingerprint_fnv1a32\":\"B54833D1\","
        "\"viewport_width_pixels\":256,\"viewport_tile_stride\":33,"
        "\"viewport_camera_x_min\":0,\"viewport_camera_x_max\":512,"
        "\"viewport\":\"32 columns when tile-aligned, otherwise 33; unused aligned tail cells are zero\","
        "\"viewport_ordering\":\"canonical camera view; does not emulate staged PPU prefetch/write ordering\","
        "\"scene_slice\":{\"transactional\":true,"
        "\"possession_binding\":\"TGOR tracked possession, direction, and "
        "transition serial are captured with the TGCT viewport\","
        "\"camera_binding\":\"TGCT viewport camera_x must equal the TGCP "
        "scene-projection camera_x for one rendered frame\","
        "\"actor_binding\":\"one combined transactional court frame owns "
        "the TGCT viewport plus all TGCP player and ball projections\","
        "\"native_checkpoints\":{\"left_camera_x\":102,"
        "\"center_camera_x\":256,\"right_camera_x\":408,"
        "\"left_render_fnv1a32\":\"770FAE95\","
        "\"center_render_fnv1a32\":\"6E530421\","
        "\"right_render_fnv1a32\":\"2DBDF155\"},"
        "\"integration_is_additional_rom_claim\":false},"
        "\"live_palette_size\":16,"
        "\"live_palette_fingerprint_fnv1a32\":\"B20C1E11\","
        "\"live_palette_matchup\":\"fixed $DEAB-$DEDF and team-data $DC19-$DC35 substitute entries 3/7/11/15\","
        "\"boundary\":\"strict full-court decode and production camera-positioned live viewport slicing; native framebuffer rendering clips 32/33 columns inside the 256x240 gameplay subview; TGFL-1 remains the separate owner of free-throw positions consumed by native scene composition; excludes staged PPU prefetch/write ordering, vertical camera, and capture-derived behavior\","
        "\"runtime_inputs\":\"TGCT-1 plus same-pack chr/all; no decompilation, trace, capture, screenshot, dump, state, or video\"}}" );
}

static int append_gameplay_court_orientation_source_map_entry(
    char *buffer,
    size_t capacity,
    size_t *length,
    int *first,
    const TecmoGameplayCourtOrientationProvenance *provenance)
{
    const char *prefix = *first != 0 ? "" : ",\n";
    const TecmoGameplayCourtOrientationExpectedSource *gate =
        &tecmo_gameplay_court_orientation_expected_sources[0U];
    const TecmoGameplayCourtOrientationExpectedSource *role =
        &tecmo_gameplay_court_orientation_expected_sources[1U];
    const TecmoGameplayCourtOrientationExpectedSource *delta =
        &tecmo_gameplay_court_orientation_expected_sources[2U];
    const TecmoGameplayCourtOrientationExpectedSource *targets =
        &tecmo_gameplay_court_orientation_expected_sources[3U];

    *first = 0;
    return tecmo_asset_pack_append_text(
        buffer, capacity, length,
        "%s"
        "    {\"id\":\"%s\",\"kind\":\"gameplay-court-orientation-state\","
        "\"schema\":\"tecmo.gameplay-court-orientation/TGOR-1\","
        "\"payload_size\":%u,\"payload_fingerprint_fnv1a32\":\"%08X\","
        "\"dependencies\":["
        "{\"id\":\"%s\",\"schema\":\"tecmo.gameplay/TGPL-1\","
        "\"payload_size\":%u,\"payload_fingerprint_fnv1a32\":\"%08X\","
        "\"evidence\":\"fixed $E537-$E548 derives $0758 from $04FC bit 7; "
        "$E699-$E69A supplies screen IDs $1B/$2E; presentation selector only, "
        "not orientation ownership\"},"
        "{\"id\":\"%s\",\"schema\":\"tecmo.gameplay-shot-resolution/TGSR-3\","
        "\"payload_size\":%u,\"payload_fingerprint_fnv1a32\":\"%08X\","
        "\"evidence\":\"Bank05 $B87C-$B8F5 is a conditional alternate "
        "claimant-settlement path, not a universal post-shot path\"}],"
        "\"sources\":["
        "{\"role\":\"possession-transition-gate-and-swap\","
        "\"source_entry\":\"prg/bank05\",\"source_offset\":%llu,"
        "\"bank\":%u,\"cpu_start\":%u,\"cpu_end\":%u,\"size\":%u,"
        "\"fingerprint_fnv1a32\":\"%08X\"},"
        "{\"role\":\"actor-role-bit-toggle-and-queue\","
        "\"source_entry\":\"prg/bank05\",\"source_offset\":%llu,"
        "\"bank\":%u,\"cpu_start\":%u,\"cpu_end\":%u,\"size\":%u,"
        "\"fingerprint_fnv1a32\":\"%08X\","
        "\"evidence\":\"toggles $04B0 bit $10 for slots 0..9 and queues $17; "
        "this is not labeled a team switch\"},"
        "{\"role\":\"orientation-target-absolute-delta\","
        "\"source_entry\":\"prg/bank05\",\"source_offset\":%llu,"
        "\"bank\":%u,\"cpu_start\":%u,\"cpu_end\":%u,\"size\":%u,"
        "\"fingerprint_fnv1a32\":\"%08X\","
        "\"evidence\":\"uses $035A to select target X and Y target $94\"},"
        "{\"role\":\"orientation-target-x-table\","
        "\"source_entry\":\"prg/bank05\",\"source_offset\":%llu,"
        "\"bank\":%u,\"cpu_start\":%u,\"cpu_end\":%u,\"size\":%u,"
        "\"fingerprint_fnv1a32\":\"%08X\",\"targets\":[160,608]}],"
        "\"state_contract\":{\"directions\":[0,1],"
        "\"fresh_launch\":{\"direction\":0,\"tracked_possession\":\"away\","
        "\"offensive_hoop\":[160,148],"
        "\"policy\":\"cold-start-aligned; repeat-game initialization remains unproven\"},"
        "\"same_possession\":\"successful no-op, including same-possession "
        "period and foul restarts\","
        "\"possession_change\":\"atomically save previous direction, XOR current "
        "direction, update tracked team and full offensive-hoop coordinate, "
        "increment serial\","
        "\"hoop_landmarks\":{\"left\":[160,148],\"right\":[608,148],"
        "\"coordinate_space\":\"TGCT full-court pixels from upper-left; "
        "X right, Y down\"},"
        "\"canonical_court\":\"TGCT-1 remains left-to-right and unchanged\"},"
        "\"evidence_limits\":\"$035B is only save-before-toggle evidence and has "
        "no direct reads; direct $035A stores occur at $8FC4 and $B8E0; broad "
        "STA $0300,X occurs only in fixed-bank cold boot at $CC68\","
        "\"supported_boundary\":\"strict binary offensive-direction ownership "
        "and production full-hoop selection for TGCP follow direction, "
        "world-space shot endpoints, close-distance orientation, and "
        "production TGFL-1 orientation selection; TGFL-1 remains the "
        "coordinate/pose-data owner; ordinary flight Y=$8F remains a separate endpoint; no "
        "generalized post-shot dispatcher semantics\","
        "\"runtime_inputs\":\"TGOR-1 plus same-pack TGPL-1 and TGSR-3; no ROM, "
        "decompilation, ASM, trace, capture, screenshot, video, log, dump, Lua "
        "output, or save state\"}",
        prefix,
        TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_ID,
        TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_SIZE,
        TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_FNV1A32,
        TECMO_ASSET_PACK_GAMEPLAY_ID,
        TECMO_ASSET_PACK_GAMEPLAY_SIZE,
        TECMO_ASSET_PACK_GAMEPLAY_FNV1A32,
        TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_ID,
        TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_SIZE,
        TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_FNV1A32,
        (unsigned long long)provenance->source_offsets[0U],
        (unsigned)gate->bank, (unsigned)gate->cpu_start,
        (unsigned)(gate->cpu_start + gate->byte_count - 1U),
        (unsigned)gate->byte_count, (unsigned)gate->fingerprint,
        (unsigned long long)provenance->source_offsets[1U],
        (unsigned)role->bank, (unsigned)role->cpu_start,
        (unsigned)(role->cpu_start + role->byte_count - 1U),
        (unsigned)role->byte_count, (unsigned)role->fingerprint,
        (unsigned long long)provenance->source_offsets[2U],
        (unsigned)delta->bank, (unsigned)delta->cpu_start,
        (unsigned)(delta->cpu_start + delta->byte_count - 1U),
        (unsigned)delta->byte_count, (unsigned)delta->fingerprint,
        (unsigned long long)provenance->source_offsets[3U],
        (unsigned)targets->bank, (unsigned)targets->cpu_start,
        (unsigned)(targets->cpu_start + targets->byte_count - 1U),
        (unsigned)targets->byte_count, (unsigned)targets->fingerprint);
}

static int append_gameplay_backcourt_source_map_entry(
    char *buffer,
    size_t capacity,
    size_t *length,
    int *first,
    const TecmoGameplayBackcourtProvenance *provenance)
{
    const TecmoGameplayBackcourtExpectedSource *source =
        &tecmo_gameplay_backcourt_expected_sources[0U];
    const char *prefix = *first != 0 ? "" : ",\n";
    *first = 0;
    return tecmo_asset_pack_append_text(
        buffer, capacity, length,
        "%s"
        "    {\"id\":\"%s\",\"kind\":\"gameplay-backcourt-native\","
        "\"schema\":\"tecmo.gameplay-backcourt/TGBC-1\",\"size\":%u,"
        "\"fingerprint_fnv1a32\":\"%08X\","
        "\"revision_sha256\":\"076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4\","
        "\"dependencies\":["
        "{\"entry\":\"%s\",\"size\":%u,\"fingerprint_fnv1a32\":\"%08X\","
        "\"reason\":\"possession-owned offensive orientation\"},"
        "{\"entry\":\"%s\",\"size\":%u,\"fingerprint_fnv1a32\":\"%08X\","
        "\"reason\":\"selector 2 BACKCOURT presentation and restart\"}],"
        "\"source_spans\":[{\"role\":\"ten-second-prefix-and-live-backcourt-detector-$970B-$9786\","
        "\"source_entry\":\"prg/bank05\",\"source_offset\":%llu,"
        "\"bank\":%u,\"fixed_bank\":false,\"cpu_start\":%u,"
        "\"cpu_end\":%u,\"size\":%u,\"fingerprint_fnv1a32\":\"%08X\","
        "\"payload_offset\":%u}],"
        "\"native_contract\":{\"implemented_span\":\"$971F-$9786\","
        "\"global_object_state\":0,\"frontcourt_progress_mask\":16,"
        "\"violation_selector\":2,"
        "\"orientation_0\":{\"establish_max_x\":375,\"return_min_x\":386},"
        "\"orientation_1\":{\"establish_min_x\":392,\"return_max_x\":383},"
        "\"arithmetic\":\"exact unsigned 16-bit 6502 subtract, high-byte sign and low-byte compare\","
        "\"transactional_validation\":true},"
        "\"excluded\":\"the selector-4 ten-second test at $970B-$971E remains source evidence only\","
        "\"live_adapter\":{\"sample\":\"attached held-ball canonical X after all ordinary actor movement\","
        "\"reset\":\"every possession handoff\","
        "\"presentation\":\"same-pack TPNL-1 and TGVR-1 BACKCOURT referee route\"},"
        "\"runtime_inputs\":\"TGBC-1 plus same-pack TGOR-1 and TPNL-1; no ROM, decompilation, trace, capture, or save state\"}",
        prefix,
        TECMO_ASSET_PACK_GAMEPLAY_BACKCOURT_ID,
        (unsigned)TECMO_ASSET_PACK_GAMEPLAY_BACKCOURT_SIZE,
        (unsigned)TECMO_ASSET_PACK_GAMEPLAY_BACKCOURT_FNV1A32,
        TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_ID,
        (unsigned)TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_SIZE,
        (unsigned)TECMO_ASSET_PACK_GAMEPLAY_COURT_ORIENTATION_FNV1A32,
        TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_ID,
        (unsigned)TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_SIZE,
        (unsigned)TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_FNV1A32,
        (unsigned long long)provenance->source_offsets[0U],
        (unsigned)source->bank,
        (unsigned)source->cpu_start,
        (unsigned)((uint32_t)source->cpu_start + source->byte_count - 1U),
        (unsigned)source->byte_count,
        (unsigned)source->fingerprint,
        (unsigned)source->payload_offset);
}

static int append_gameplay_camera_source_map_entry(
    char *buffer,
    size_t capacity,
    size_t *length,
    int *first,
    const TecmoGameplayCameraProvenance *p)
{
    static const char *const roles[
        TECMO_GAMEPLAY_CAMERA_SOURCE_COUNT] = {
        "camera-initialize-$DE13-$DE2C",
        "direction-aware-column-streamer-$DF05-$DFFF",
        "dynamic-attribute-helper-$E0E7-$E13B",
        "threshold-table-follow-and-route-gate-$E168-$E2E6",
        "forced-settle-$EB4F-$EB8C",
        "actor-projector-$F1CB-$F1F1",
        "ordinary-actor-dispatch-and-clamp-$F106-$F1B0"
    };
    const TecmoGameplayCameraExpectedSource *actor_clamp =
        &tecmo_gameplay_camera_expected_sources[6U];
    const char *prefix = *first != 0 ? "" : ",\n";

    *first = 0;
    if (tecmo_asset_pack_append_text(
            buffer, capacity, length,
            "%s"
            "    {\"id\":\"%s\",\"kind\":\"gameplay-camera-projection-native\","
            "\"schema\":\"tecmo.gameplay-camera/TGCP-2\",\"size\":%u,"
            "\"fingerprint_fnv1a32\":\"%08X\","
            "\"revision_sha256_identity\":\"076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4\","
            "\"revision_full_rom_fnv1a32\":\"0650F5B0\","
            "\"revision_full_rom_fnv1a32_verified\":true,"
            "\"revision_source_fingerprints_verified\":true,"
            "\"dependencies\":["
            "{\"entry\":\"%s\",\"same_pack_required\":true,\"size\":%u,"
            "\"fingerprint_fnv1a32\":\"%08X\","
            "\"reason\":\"validate the downstream actor compositor and projection tail\"},"
            "{\"entry\":\"%s\",\"same_pack_required\":true,\"size\":%u,"
            "\"fingerprint_fnv1a32\":\"%08X\","
            "\"reason\":\"bind camera streaming to the same strict court layout and macro sources\"}],"
            "\"source_spans\":[",
            prefix,
            TECMO_ASSET_PACK_GAMEPLAY_CAMERA_ID,
            (unsigned)TECMO_ASSET_PACK_GAMEPLAY_CAMERA_SIZE,
            (unsigned)TECMO_ASSET_PACK_GAMEPLAY_CAMERA_FNV1A32,
            TECMO_ASSET_PACK_GAMEPLAY_ID,
            (unsigned)TECMO_ASSET_PACK_GAMEPLAY_SIZE,
            (unsigned)TECMO_ASSET_PACK_GAMEPLAY_FNV1A32,
            TECMO_ASSET_PACK_GAMEPLAY_COURT_ID,
            (unsigned)TECMO_ASSET_PACK_GAMEPLAY_COURT_SIZE,
            (unsigned)TECMO_ASSET_PACK_GAMEPLAY_COURT_FNV1A32) != 0) {
        return -1;
    }
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_CAMERA_SOURCE_COUNT; ++index) {
        const TecmoGameplayCameraExpectedSource *source =
            &tecmo_gameplay_camera_expected_sources[index];
        if (tecmo_asset_pack_append_text(
                buffer, capacity, length,
                "%s{\"role\":\"%s\",\"source_entry\":\"prg/fixed\","
                "\"source_offset\":%llu,\"bank\":%u,\"fixed_bank\":true,"
                "\"cpu_start\":%u,\"cpu_end\":%u,\"size\":%u,"
                "\"fingerprint_fnv1a32\":\"%08X\"%s,"
                "\"payload_offset\":%u}",
                index == 0U ? "" : ",", roles[index],
                (unsigned long long)p->source_offsets[index],
                (unsigned)source->bank,
                (unsigned)source->cpu_start,
                (unsigned)((uint32_t)source->cpu_start +
                           source->byte_count - 1U),
                (unsigned)source->byte_count,
                (unsigned)source->fingerprint,
                index == 6U
                    ? ",\"fingerprint_sha256\":\"0B97A9AAC4DF35E4EDF7979C6C0355852B9DE7398844B2679CFAB298F0C0CBA6\""
                    : "",
                (unsigned)source->payload_offset) != 0) {
            return -1;
        }
    }
    return tecmo_asset_pack_append_text(
        buffer, capacity, length,
        "],\"state_contract\":{"
        "\"camera_x\":\"$01:$00\","
        "\"scroll_x\":\"$0300\","
        "\"scroll_aux\":\"$0301\","
        "\"nametable_page\":\"$0302 bit 0\","
        "\"aux\":\"$0303\","
        "\"stream_direction\":\"$3B\","
        "\"layout_cursor\":\"$38\","
        "\"thresholds\":\"$06E3/$06E4\","
        "\"endpoint_latch\":\"$05B6 bit 7\","
        "\"focus_world_x\":\"$F2:$7D\","
        "\"pure_initialize\":{\"source\":\"fixed $DE13-$DE2C\","
        "\"camera_x\":\"$01:$00\",\"layout_cursor\":\"$20\"},"
        "\"live_prime\":{\"source\":\"fixed $DDFB call to $DF05\","
        "\"layout_cursor\":\"$21\",\"single_use\":true,"
        "\"camera_tuple_unchanged\":true}},"
        "\"follow_contract\":{"
        "\"camera_disabled\":\"$07DE nonzero preserves state\","
        "\"orientation_zero_thresholds\":[216,232],"
        "\"orientation_one_thresholds\":[32,4],"
        "\"center_thresholds\":[80,160],"
        "\"normal_speed_cap\":7,\"endpoint_speed_cap\":2,"
        "\"left_cursor_bound\":12,\"right_cursor_bound\":52,"
        "\"suppressed_action_routes\":[1,18,19],"
        "\"coarse_boundary\":\"scroll_x eight-pixel crossing updates layout cursor and stream direction; reversal advances three cursor units\","
        "\"live_state_validator\":{\"right_cursor\":\"min((camera_x >> 3)+1,$34)\","
        "\"left_cursor\":\"max((camera_x >> 3)-1,$0B)\","
        "\"unlatched_thresholds\":[80,160],"
        "\"left_endpoint_thresholds\":[216,232],"
        "\"right_endpoint_thresholds\":[32,4],"
        "\"thresholds_invalid_requires_latch_clear\":true},"
        "\"settle\":\"bounded transactional route-zero follow until scroll_x is unchanged; PPU commits excluded\"},"
        "\"projection_contract\":{"
        "\"screen_x\":\"low byte of world_x-camera_x when the high byte is zero\","
        "\"visible\":\"world_x-camera_x is in the unsigned 0..255 viewport\","
        "\"offscreen_sentinel\":\"visible=false, screen_x=0, screen_y=0; ROM branches before Y projection\","
        "\"screen_y\":\"visible actors use max(0, raw_world_y-altitude)\","
        "\"orientation_transform\":false,\"vertical_camera\":false},"
        "\"live_runtime_contract\":{"
        "\"asset_pack\":\"TGCP-2, TGCT-1, TGOR-1, TGFL-1, TGPL-1, and chr/all resolve from one canonical pack\","
        "\"world_seed\":\"legacy screen X plus $0100; ball and every actor/anchor/shot endpoint use world coordinates\","
        "\"coordinate_space\":{\"origin\":\"upper-left of complete TGCT court\","
        "\"x\":\"right\",\"y\":\"down\",\"integer_bounds\":[[0,767],[0,239]],"
        "\"ball_fractional_bits\":8,\"player_layout\":\"native approximation\"},"
        "\"coordinate_adapter\":{\"follow\":\"validate and floor canonical Q8 "
        "ball once into exact TGCP focus_world_x\","
        "\"projection\":\"integer player anchors and Q8 ball route through "
        "transactional TGCP projection adapters\","
        "\"scene_snapshot\":\"one camera_x plus ten player projections and one "
        "ball projection; offscreen entries retain the neutral sentinel\","
        "\"adapter_is_additional_rom_claim\":false},"
        "\"launch\":\"pure $DE13 init, one $DDFB->$DF05 live prime, world seed, then one bounded route-zero settle\","
        "\"update\":\"exactly one route-zero follow after all live object and ball mutations; focus is ball world X and orientation is TGOR current_direction\","
        "\"freeze\":\"free-throw entry performs the documented TGFL-1 typed forced settle, then subsequent free-throw updates and TGDK black/cutaway frames preserve camera state; first live-return update resumes follow\","
        "\"possession\":\"TGOR transition clears only camera threshold validity and endpoint latch; camera position is continuous\","
        "\"viewport\":\"TGCT-1 32/33-column slice at camera_x; x=column*8-fine_scroll_x; framebuffer subview clips partial edge columns to 256x240\","
        "\"court_slice\":\"one transactional scene snapshot binds TGOR "
        "possession/direction/transition serial to the TGCT viewport and "
        "requires its camera_x to match the TGCP projection camera_x\","
        "\"court_frame\":{\"transactional\":true,"
        "\"composition\":\"scene frame and camera-follow serial plus one "
        "TGCT possession slice and one TGCP player/ball projection\","
        "\"stationary_actor_x\":\"screen_x changes by the inverse signed "
        "camera_x delta while visible\","
        "\"stationary_actor_y\":\"unchanged by horizontal camera motion\","
        "\"visibility_transition\":\"offscreen player and ball projections "
        "use visible=false with zero X/Y\","
        "\"covered_transitions\":[\"fine-scroll\",\"coarse-tile\","
        "\"possession-reversal\",\"left-endpoint\",\"right-endpoint\"],"
        "\"integration_is_additional_rom_claim\":false},"
        "\"projection\":\"one canonical scene snapshot uses TGCP for all ten "
        "players and the ball; jump altitude is applied exactly once to the "
        "actor and never added to the ball\","
        "\"shot_target\":{\"orientation_0_x\":160,\"orientation_1_x\":608,"
        "\"hoop_y\":148,\"launch_y\":143,"
        "\"hoop_and_flight_y_are_distinct\":true,"
        "\"endpoint_captured_once\":true}},"
        "\"ordinary_movement_geometry\":{"
        "\"strict_tgcp2_source\":true,"
        "\"source_kind\":\"ordinary-actor-dispatch-and-clamp\","
        "\"source_entry\":\"prg/fixed\",\"source_offset\":%llu,"
        "\"bank\":%u,\"fixed_bank\":true,\"cpu_start\":%u,"
        "\"cpu_end\":%u,\"size\":%u,"
        "\"fingerprint_fnv1a32\":\"%08X\","
        "\"fingerprint_sha256\":\"0B97A9AAC4DF35E4EDF7979C6C0355852B9DE7398844B2679CFAB298F0C0CBA6\","
        "\"payload_offset\":%u,"
        "\"closed_span\":\"$F106-$F1B0\","
        "\"page0_left\":\"$00DF-floor(world_y/2)\","
        "\"page1\":\"interior continuation\","
        "\"page2_right\":\"$0220+floor(world_y/2)\","
        "\"native_policy\":\"TGCP-2 owns the shared trapezoid geometry; production user-controlled movement applies the TGMO-1 dispatcher policy\","
        "\"production_controlled_movement\":\"TGMO-1 exact dispatcher exclusions and violation-latch conditions\","
        "\"boundary_settlement\":\"selector $0742 value 1 is consumed through same-pack TPNL-1 as OUT OF BOUNDS\","
        "\"dispatcher_exceptions_not_implemented\":[\"$0478\",\"$046E\",\"$0463\"],"
        "\"backcourt_progress_bit\":\"$0588 bit 4 is independently owned by strict TGBC-1\"},"
        "\"supported_boundary\":\"strict pure and production live camera state, full-court slicing, actor/ball projection, clipped native composition, and typed free-throw settle/projection of TGFL-1-owned coordinates; TGCP-2 does not own TGFL-1 positions or native scene slot policy; no staged PPU commit/prefetch ordering, vertical camera, generalized movement-dispatch exceptions, HUD provenance, or capture-derived behavior\","
        "\"runtime_inputs\":\"TGCP-2 plus same-pack TGPL-1 and TGCT-1; no ROM, decompilation, ASM, trace, capture, screenshot, video, log, dump, Lua output, or save state\"}",
        (unsigned long long)p->source_offsets[6U],
        (unsigned)actor_clamp->bank,
        (unsigned)actor_clamp->cpu_start,
        (unsigned)((uint32_t)actor_clamp->cpu_start +
                   actor_clamp->byte_count - 1U),
        (unsigned)actor_clamp->byte_count,
        (unsigned)actor_clamp->fingerprint,
        (unsigned)actor_clamp->payload_offset);
}

static int append_gameplay_movement_source_map_entry(
    char *buffer,
    size_t capacity,
    size_t *length,
    int *first,
    const TecmoGameplayMovementProvenance *provenance)
{
    static const char *const roles[
        TECMO_GAMEPLAY_MOVEMENT_SOURCE_COUNT] = {
        "player-profile-and-game-speed-$A89E-$A90D",
        "ordinary-animation-config-$ACE4-$AD25",
        "condition-q4-and-diagonal-delta-$879B-$8866",
        "eight-direction-handlers-and-animation-$88F9-$89BC",
        "controller-state-direction-and-pose-$8E58-$8F96",
        "direction-mapping-tables-$BF6C-$BFA7",
        "actor-dispatch-and-trapezoid-clamp-$F106-$F1B0"
    };
    const char *prefix = *first != 0 ? "" : ",\n";

    *first = 0;
    if (tecmo_asset_pack_append_text(
            buffer, capacity, length,
            "%s"
            "    {\"id\":\"%s\",\"kind\":\"gameplay-ordinary-actor-movement-native\","
            "\"schema\":\"tecmo.gameplay-movement/TGMO-1\",\"size\":%u,"
            "\"fingerprint_fnv1a32\":\"%08X\","
            "\"revision_sha256_identity\":\"076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4\","
            "\"revision_full_rom_fnv1a32\":\"0650F5B0\","
            "\"revision_full_rom_sha256_verified\":true,"
            "\"revision_full_rom_fnv1a32_verified\":true,"
            "\"revision_source_fingerprints_verified\":true,"
            "\"dependencies\":["
            "{\"entry\":\"%s\",\"same_pack_required\":true,\"size\":%u,"
            "\"fingerprint_fnv1a32\":\"%08X\"},"
            "{\"entry\":\"%s\",\"same_pack_required\":true,\"size\":%u,"
            "\"fingerprint_fnv1a32\":\"%08X\","
            "\"reason\":\"cross-check the identical fixed clamp span\"},"
            "{\"entry\":\"%s\",\"same_pack_required\":true,\"size\":%u,"
            "\"fingerprint_fnv1a32\":\"%08X\","
            "\"reason\":\"supply the selected player's ROM profile rating and fresh condition seed\"}],"
            "\"source_spans\":[",
            prefix,
            TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_ID,
            (unsigned)TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_SIZE,
            (unsigned)TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_FNV1A32,
            TECMO_ASSET_PACK_GAMEPLAY_ID,
            (unsigned)TECMO_ASSET_PACK_GAMEPLAY_SIZE,
            (unsigned)TECMO_ASSET_PACK_GAMEPLAY_FNV1A32,
            TECMO_ASSET_PACK_GAMEPLAY_CAMERA_ID,
            (unsigned)TECMO_ASSET_PACK_GAMEPLAY_CAMERA_SIZE,
            (unsigned)TECMO_ASSET_PACK_GAMEPLAY_CAMERA_FNV1A32,
            TECMO_ASSET_PACK_TEAM_DATA_ID,
            (unsigned)TECMO_ASSET_PACK_TEAM_DATA_SIZE,
            (unsigned)TECMO_ASSET_PACK_TEAM_DATA_FNV1A32) != 0) {
        return -1;
    }
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_MOVEMENT_SOURCE_COUNT; ++index) {
        const TecmoGameplayMovementExpectedSource *source =
            &tecmo_gameplay_movement_expected_sources[index];
        if (tecmo_asset_pack_append_text(
                buffer, capacity, length,
                "%s{\"role\":\"%s\",\"source_entry\":\"%s\","
                "\"source_offset\":%llu,\"bank\":%u,"
                "\"fixed_bank\":%s,\"cpu_start\":%u,\"cpu_end\":%u,"
                "\"size\":%u,\"fingerprint_fnv1a32\":\"%08X\","
                "\"payload_offset\":%u}",
                index == 0U ? "" : ",", roles[index],
                source->fixed_bank != 0U ? "prg/fixed" :
                    (source->bank == 2U ? "prg/bank02" :
                     source->bank == 4U ? "prg/bank04" :
                     "prg/bank05"),
                (unsigned long long)provenance->source_offsets[index],
                (unsigned)source->bank,
                source->fixed_bank != 0U ? "true" : "false",
                (unsigned)source->cpu_start,
                (unsigned)((uint32_t)source->cpu_start +
                           source->byte_count - 1U),
                (unsigned)source->byte_count,
                (unsigned)source->fingerprint,
                (unsigned)source->payload_offset) != 0) {
            return -1;
        }
    }
    return tecmo_asset_pack_append_text(
        buffer, capacity, length,
        "],\"native_contract\":{"
        "\"input_bits\":{\"right\":1,\"left\":2,\"down\":4,\"up\":8},"
        "\"direction_change_latency_updates\":1,"
        "\"movement_fractional_bits\":4,"
        "\"condition_formula\":\"max(8, adjusted_rating+(condition>>4)-6)\","
        "\"game_speed_adjustments\":[5,-1,-6],"
        "\"diagonal_formula\":\"amount-floor(amount/4)\","
        "\"vertical_compare_before_move\":{\"up\":74,\"down\":236},"
        "\"animation\":{\"period\":8,\"delay_high_nibble\":3,"
        "\"direction_transition_high_nibble\":5},"
        "\"ordinary_clamp\":{\"left\":\"$00DF-floor(y/2)\","
        "\"right\":\"$0220+floor(y/2)\","
        "\"dispatcher_exceptions\":[\"object-state 4\","
        "\"action states $0F/$10\",\"movement-flags bit 3\"],"
        "\"violation_latch_conditions_explicit\":true},"
        "\"transactional\":true,\"overflow_rejected\":true},"
        "\"live_adapter\":{"
        "\"scope\":\"ordinary user-controlled and TGAI-directed CPU locomotion\","
        "\"rating\":\"TTDT-1 selected roster profile byte 0\","
        "\"condition\":\"TGFT-1 evolves TTDT-1 fresh condition/capacity and supplies the next update\","
        "\"object_state\":0,\"movement_flags\":0,"
        "\"contradictory_axis_input\":\"normalized to neutral on that axis\","
        "\"starting_layout\":\"Bank04 AC76-$ADDF static startup positions/directions/links are exact source evidence; reusing them as the native post-tip stable layout is native-faithful/inferred, not a proven first-running-clock RAM snapshot; unbound direct/test calls retain a scene-owned legacy compatibility layout\","
        "\"roster_binding\":\"production binds selected TTDT starters by value; unbound direct callers normalize identity 0..4 and retain legacy policy\","
        "\"boundary_latch_reset_and_settlement\":\"offensive primary latch consumes TPNL selector 1, clears the latch, and restarts with the other team; other violation detection remains unported\","
        "\"pose_half_selection\":\"exact $8F02 signed comparison against the explicit linked actor\","
        "\"matchup_link\":\"Bank04 fixed-link seed values are exact startup evidence; native LIVE attachment uses scene-owned fixed links while dynamic matchup remains inferred and separate\","
        "\"cpu_target_and_shot_policy\":\"accepted TGAI target/direction lifecycle is live-wired; deferred or unproven targets produce no movement, and shot requests are native approximation with explicit deferred/non-launch playback\"},"
        "\"developer_harness\":{\"deterministic\":true,"
        "\"normal_game_flow_exposed\":false},"
        "\"runtime_inputs\":\"TGMO-1 plus same-pack TGPL-1, TGCP-2, and TTDT-1; no ROM, decompilation, ASM, trace, capture, screenshot, video, log, dump, Lua output, or save state\"}");
}

static int append_gameplay_ball_dribble_source_map_entry(
    char *buffer,
    size_t capacity,
    size_t *length,
    int *first,
    const TecmoGameplayBallDribbleProvenance *provenance)
{
    static const char *const roles[
        TECMO_GAMEPLAY_BALL_DRIBBLE_SOURCE_COUNT] = {
        "held-ball-position-height-and-sound-routine-$B52E-$B5BF",
        "direction-half-bounce-height-and-attachment-tables-$B5C0-$B677"
    };
    const char *prefix = *first != 0 ? "" : ",\n";
    *first = 0;
    if (tecmo_asset_pack_append_text(
            buffer, capacity, length,
            "%s"
            "    {\"id\":\"%s\",\"kind\":\"gameplay-held-ball-dribble-native\","
            "\"schema\":\"tecmo.gameplay-ball-dribble/TGBD-1\",\"size\":%u,"
            "\"fingerprint_fnv1a32\":\"%08X\","
            "\"revision_sha256_identity\":\"076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4\","
            "\"revision_full_rom_fnv1a32\":\"0650F5B0\","
            "\"dependencies\":["
            "{\"entry\":\"%s\",\"same_pack_required\":true,"
            "\"size\":%u,\"fingerprint_fnv1a32\":\"%08X\"},"
            "{\"entry\":\"%s\",\"same_pack_required\":true,"
            "\"size\":%u,\"fingerprint_fnv1a32\":\"%08X\"}],"
            "\"source_spans\":[",
            prefix, TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_ID,
            (unsigned)TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_SIZE,
            (unsigned)TECMO_ASSET_PACK_GAMEPLAY_BALL_DRIBBLE_FNV1A32,
            TECMO_ASSET_PACK_GAMEPLAY_ID,
            (unsigned)TECMO_ASSET_PACK_GAMEPLAY_SIZE,
            (unsigned)TECMO_ASSET_PACK_GAMEPLAY_FNV1A32,
            TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_ID,
            (unsigned)TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_SIZE,
            (unsigned)TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_FNV1A32) != 0) {
        return -1;
    }
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_BALL_DRIBBLE_SOURCE_COUNT; ++index) {
        const TecmoGameplayBallDribbleExpectedSource *source =
            &tecmo_gameplay_ball_dribble_expected_sources[index];
        if (tecmo_asset_pack_append_text(
                buffer, capacity, length,
                "%s{\"role\":\"%s\",\"source_entry\":\"prg/bank05\","
                "\"source_offset\":%llu,\"bank\":%u,"
                "\"fixed_bank\":false,\"cpu_start\":%u,\"cpu_end\":%u,"
                "\"size\":%u,\"fingerprint_fnv1a32\":\"%08X\","
                "\"payload_offset\":%u}",
                index == 0U ? "" : ",", roles[index],
                (unsigned long long)provenance->source_offsets[index],
                (unsigned)source->bank,
                (unsigned)source->cpu_start,
                (unsigned)((uint32_t)source->cpu_start +
                           source->byte_count - 1U),
                (unsigned)source->byte_count,
                (unsigned)source->fingerprint,
                (unsigned)source->payload_offset) != 0) {
            return -1;
        }
    }
    return tecmo_asset_pack_append_text(
        buffer, capacity, length,
        "],\"native_contract\":{"
        "\"direction_count\":8,\"animation_phase_count\":8,"
        "\"table_half_source\":\"exact TGMO $8F02 linked-actor comparison\","
        "\"attachment\":\"signed X/Y offsets from $B648/$B658/$B668\","
        "\"bounce_height\":\"$B5C8[half*64+direction*8+phase]\","
        "\"sound_trigger\":\"animation low nibble 3 and high nibble 0\","
        "\"transactional\":true},"
        "\"live_adapter\":{"
        "\"scope\":\"ordinary held ball for human and CPU possession\","
        "\"altitude_projection\":\"exact height flattened into canonical visible Y before TGCP projection\","
        "\"matchup_link\":\"Bank04 fixed-link seed values are separate exact startup evidence; native LIVE attachment uses scene-owned fixed links and dynamic matchup semantics remain inferred/unproven\","
        "\"free_throw_and_shot_ball_routes\":\"remain separately owned\"},"
        "\"runtime_inputs\":\"TGBD-1 plus same-pack TGPL-1 and TGMO-1; no ROM, decompilation, trace, capture, or save state\"}");
}

static int append_gameplay_fatigue_source_map_entry(
    char *buffer,
    size_t capacity,
    size_t *length,
    int *first,
    const TecmoGameplayFatigueProvenance *provenance)
{
    static const char *const roles[TECMO_GAMEPLAY_FATIGUE_SOURCE_COUNT] = {
        "difficulty-cadence-active-decay-and-bench-recovery-$B4E6-$B5C7",
        "live-state-gate-and-Bank02-caller-$ED2F-$ED3E"
    };
    const char *prefix = *first != 0 ? "" : ",\n";
    *first = 0;
    if (tecmo_asset_pack_append_text(
            buffer, capacity, length,
            "%s"
            "    {\"id\":\"%s\",\"kind\":\"gameplay-fatigue-native\","
            "\"schema\":\"tecmo.gameplay-fatigue/TGFT-1\",\"size\":%u,"
            "\"fingerprint_fnv1a32\":\"%08X\","
            "\"revision_sha256_identity\":\"076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4\","
            "\"revision_full_rom_fnv1a32\":\"0650F5B0\","
            "\"dependency\":{\"entry\":\"%s\",\"same_pack_required\":true,"
            "\"size\":%u,\"fingerprint_fnv1a32\":\"%08X\"},"
            "\"source_spans\":[",
            prefix, TECMO_ASSET_PACK_GAMEPLAY_FATIGUE_ID,
            (unsigned)TECMO_ASSET_PACK_GAMEPLAY_FATIGUE_SIZE,
            (unsigned)TECMO_ASSET_PACK_GAMEPLAY_FATIGUE_FNV1A32,
            TECMO_ASSET_PACK_TEAM_DATA_ID,
            (unsigned)TECMO_ASSET_PACK_TEAM_DATA_SIZE,
            (unsigned)TECMO_ASSET_PACK_TEAM_DATA_FNV1A32) != 0) {
        return -1;
    }
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_FATIGUE_SOURCE_COUNT; ++index) {
        const TecmoGameplayFatigueExpectedSource *source =
            &tecmo_gameplay_fatigue_expected_sources[index];
        if (tecmo_asset_pack_append_text(
                buffer, capacity, length,
                "%s{\"role\":\"%s\",\"source_entry\":\"%s\","
                "\"source_offset\":%llu,\"bank\":%u,"
                "\"fixed_bank\":%s,\"cpu_start\":%u,\"cpu_end\":%u,"
                "\"size\":%u,\"fingerprint_fnv1a32\":\"%08X\","
                "\"payload_offset\":%u}",
                index == 0U ? "" : ",", roles[index],
                source->fixed_bank != 0U ? "prg/fixed" : "prg/bank02",
                (unsigned long long)provenance->source_offsets[index],
                (unsigned)source->bank,
                source->fixed_bank != 0U ? "true" : "false",
                (unsigned)source->cpu_start,
                (unsigned)((uint32_t)source->cpu_start +
                           source->byte_count - 1U),
                (unsigned)source->byte_count,
                (unsigned)source->fingerprint,
                (unsigned)source->payload_offset) != 0) {
            return -1;
        }
    }
    return tecmo_asset_pack_append_text(
        buffer, capacity, length,
        "],\"native_contract\":{"
        "\"difficulty_cadence_reload\":[6,4,1],"
        "\"active_decay\":\"countdown; capacity and condition decrement only at zero with the ROM >=10 guards\","
        "\"bench_recovery\":\"condition +4 capped 100; capacity +4 capped at TTDT profile byte 3\","
        "\"second_team_countdown_asymmetry_preserved\":true,"
        "\"transactional_validation\":true},"
        "\"live_adapter\":{"
        "\"active_roster_binding\":\"selected TTDT roster indices 0..11 bind by value to stable local slots 0..4 per side; session-to-slot staging is native-faithful/inferred, while unbound direct callers normalize identity 0..4 for legacy compatibility\","
        "\"tick_boundary\":\"once per native scene live-action update; exact 6502 intra-frame actor ordering is not claimed\","
        "\"movement_condition\":\"synchronized into TGMO on the following scene update\"},"
        "\"runtime_inputs\":\"TGFT-1 plus same-pack TTDT-1 only; no ROM, decompilation, trace, capture, or save state\"}");
}

static int append_gameplay_cpu_steering_source_map_entry(
    char *buffer,
    size_t capacity,
    size_t *length,
    int *first,
    const TecmoGameplayCpuSteeringProvenance *provenance)
{
    static const char *const roles[
        TECMO_GAMEPLAY_CPU_STEERING_SOURCE_COUNT] = {
        "cpu-actor-loop-and-state-dispatch-$81F7-$82D3",
        "actor-to-reference-octant-direction-$87AE-$88AF",
        "target-delta-octant-direction-and-motion-$88DA-$8A95",
        "bank04-command-fetch-and-24-way-dispatch-$8B90-$8BE0",
        "command-handler-cluster-$8BE1-$9237",
        "target-store-delta-zero-guard-and-direction-tail-$9280-$9329",
        "court-cell-formation-stream-selector-$938B-$9620",
        "fixed-C006-command-reader-trampoline-$C006-$C008",
        "fixed-bank04-five-byte-command-reader-$CBE0-$CBF6",
        "bank04-680-five-byte-command-records-$9F2E-$AC75"
    };
    const char *prefix = *first != 0 ? "" : ",\n";

    *first = 0;
    if (tecmo_asset_pack_append_text(
            buffer, capacity, length,
            "%s"
            "    {\"id\":\"%s\",\"kind\":\"gameplay-cpu-steering-evidence\","
            "\"schema\":\"tecmo.gameplay-cpu-steering/TGAI-1\",\"size\":%u,"
            "\"fingerprint_fnv1a32\":\"%08X\","
            "\"revision_sha256_identity\":\"076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4\","
            "\"revision_full_rom_fnv1a32\":\"0650F5B0\","
            "\"revision_full_rom_sha256_verified\":true,"
            "\"revision_full_rom_fnv1a32_verified\":true,"
            "\"revision_source_fingerprints_verified\":true,"
            "\"dependency\":{\"entry\":\"%s\",\"same_pack_required\":true,"
            "\"size\":%u,\"fingerprint_fnv1a32\":\"%08X\","
            "\"reason\":\"preserve the controlled/CPU actor direction-code and console composition locomotion boundary\"},"
            "\"source_spans\":[",
            prefix,
            TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_ID,
            (unsigned)TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_SIZE,
            (unsigned)TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_FNV1A32,
            TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_ID,
            (unsigned)TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_SIZE,
            (unsigned)TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_FNV1A32) != 0) {
        return -1;
    }
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_CPU_STEERING_SOURCE_COUNT; ++index) {
        const TecmoGameplayCpuSteeringExpectedSource *source =
            &tecmo_gameplay_cpu_steering_expected_sources[index];
        const char *source_entry = source->fixed_bank != 0U
            ? "prg/fixed"
            : (source->bank == 4U ? "prg/bank04" : "prg/bank06");
        if (tecmo_asset_pack_append_text(
                buffer, capacity, length,
                "%s{\"role\":\"%s\",\"source_entry\":\"%s\","
                "\"source_offset\":%llu,\"bank\":%u,"
                "\"fixed_bank\":%s,\"cpu_start\":%u,\"cpu_end\":%u,"
                "\"size\":%u,\"fingerprint_fnv1a32\":\"%08X\","
                "\"payload_offset\":%u}",
                index == 0U ? "" : ",", roles[index], source_entry,
                (unsigned long long)provenance->source_offsets[index],
                (unsigned)source->bank,
                source->fixed_bank != 0U ? "true" : "false",
                (unsigned)source->cpu_start,
                (unsigned)((uint32_t)source->cpu_start +
                           source->byte_count - 1U),
                (unsigned)source->byte_count,
                (unsigned)source->fingerprint,
                (unsigned)source->payload_offset) != 0) {
            return -1;
        }
    }
    return tecmo_asset_pack_append_text(
        buffer, capacity, length,
        "],\"native_contract\":{"
        "\"actor_slots\":10,\"script_state\":4,"
        "\"command_transport\":{\"actor_offset_ram\":\"$0547/$0551\","
        "\"bank04_cpu_base\":\"$9F2E\",\"fixed_trampoline\":\"$C006\","
        "\"fixed_reader\":\"$CBE0-$CBF6\",\"mapped_bank\":4,"
        "\"record_bytes\":5,\"destination_ram\":\"$C7-$CB\","
        "\"record_count\":680,\"last_record_cpu\":\"$AC71\","
        "\"code_resumes_cpu\":\"$AC76\"},"
        "\"opcode_dispatch\":{\"count\":24,"
        "\"handler_cpu\":[37088,37707,37504,36958,36858,36754,"
        "36653,36626,36567,36805,36048,35904,36431,37157,"
        "37190,37234,36997,35866,35866,35866,36914,35830,"
        "35809,36722]},"
        "\"direction_quantizer\":{\"cpu\":\"$92D4-$92DD; $92FE -> $88DA-$899D\","
        "\"dominant_axis_ratio\":\"2:1 inclusive for court-reachable deltas; exact 16-bit doubling wrap retained\","
        "\"octant_map\":[3,6,4,7,0,1,2,5],"
        "\"direction_names\":[\"right\",\"left\",\"down\","
        "\"down-right\",\"down-left\",\"up\",\"up-right\","
        "\"up-left\"],\"zero_vector_keeps_prior_direction\":true},"
        "\"exact_test_api\":[\"aligned command decode\","
        "\"target-minus-actor direction quantization\"],"
        "\"live_wired\":false,\"transactional\":true},"
        "\"evidence_limits\":{"
        "\"not_claimed\":[\"complete CPU play policy\","
        "\"shot/pass/steal choice\",\"actor-link assignment\","
        "\"script initialization outside the isolated formation selector\","
        "\"live speed/fatigue/collision ownership\","
        "\"candidate scan $B081-$B32E as ordinary movement targeting\"],"
        "\"next_integration\":\"replace bounded native targets with proven command inputs, ROM actor-link assignments, and command advancement\"},"
        "\"live_scene_adapter\":{\"enabled\":true,"
        "\"target_policy_rom_exact\":false,"
        "\"holder_target\":\"orientation-aware 48/48/40 hoop approach\","
        "\"other_target\":\"scene-owned explicit native coordinate\","
        "\"offense_formation_orientation_0\":[[256,148],[288,112],"
        "[288,184],[352,96],[352,200]],"
        "\"offense_formation_orientation_1\":\"X mirrored as 767-X\","
        "\"defender_goal_side_x_by_orientation\":[-32,32],"
        "\"defender_depth_split\":[0,-10,10,-14,14],"
        "\"defender_out_of_bounds_fallback\":\"equal 32-pixel offset toward court side before final validation\","
        "\"fixed_opposing_link_use\":\"pose/facing and defender reference metadata; not an implicit target coordinate\","
        "\"immutable_ten_actor_snapshot\":true,"
        "\"transactional_actor_commit\":true,"
        "\"rom_command_offset\":\"explicit no-command sentinel\","
        "\"rom_command_advance_owned\":false,"
        "\"direction_quantizer_rom_exact\":true,"
        "\"movement_kernel_rom_exact\":true,"
        "\"shot_choice_and_cadence_rom_exact\":false,"
        "\"scope\":\"legacy direct/default-initializer harness and scene compatibility; bound production behavior is additive in live_foundation_integration\"},"
        "\"developer_harness\":{\"deterministic\":true,"
        "\"cli_only\":true,\"coordinate_slots\":10,"
        "\"coordinate_space\":\"TGCT-1 canonical X=0..767 Y=0..239\","
        "\"inputs\":[\"selected actor\",\"ten actor X/Y coordinates\","
        "\"possession\",\"orientation\",\"ball holder\","
        "\"opposing linked/matchup actor\",\"difficulty 0..2\","
        "\"optional validated explicit target coordinate\"],"
        "\"possession_holder_coherent\":true,"
        "\"matchup_assignment_caller_owned\":true,"
        "\"target_policy\":{\"rom_exact\":false,"
        "\"holder\":\"native scene-style hoop approach\","
        "\"other_actor_default\":\"explicit linked/matchup actor coordinate\","
        "\"explicit_override\":\"caller-owned validated coordinate; live scene uses this for non-holders\"},"
        "\"direction_quantizer_rom_exact\":true,"
        "\"zero_vector\":\"preserve prior direction\","
        "\"snapshot_fingerprint\":\"domain-separated canonical FNV1a32\","
        "\"movement_adapter\":{"
        "\"cli\":\"--gameplay-cpu-steering-movement-harness\","
        "\"direction_to_input\":\"exact same-pack TGMO direction identity\","
        "\"movement_kernel\":\"exact TGMO-1 transactional step\","
        "\"one_update_latency_rom_exact\":true,"
        "\"primary_holder_path\":true,"
        "\"secondary_nonholder_path\":true,"
        "\"role_coherent\":true,"
        "\"zero_vector_input\":\"native neutral policy; TGAI no-write remains exact\","
        "\"selected_actor_coordinate_reconciled_each_step\":true,"
        "\"live_wired\":true,"
        "\"transactional\":true},"
        "\"normal_game_flow_exposed\":false},"
        "\"live_foundation_integration\":{"
        "\"scope\":\"bound production LIVE scene launch and normal game flow; this additive adapter does not rewrite the accepted TGAI-1 harness contract\","
        "\"normal_game_flow_exposed\":true,\"live_wired\":true,"
        "\"evidence\":{"
        "\"rom\":{\"revision\":\"Rev1\",\"length\":393232,"
        "\"sha256\":\"076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4\"},"
        "\"bank03_starter_commit\":{\"bank\":3,\"address\":\"$8FC2-$9102\","
        "\"byte_count\":321,\"sha256\":\"FA3B396D01581451717CEB44A0F5628560FC664191E8F15F5843B0EAB316A9F5\","
        "\"classification\":\"exact\"},"
        "\"bank04_static_setup\":{\"bank\":4,\"address\":\"$AC76-$ADDF\",\"byte_count\":362,"
        "\"sha256\":\"E123614333986D9D5084678C9AE32DD3A1A28ABF52F6D6265FE749FC0070C6E0\","
        "\"classification\":\"exact\"},"
        "\"bank04_starter_stage\":{\"bank\":4,\"address\":\"$ADE0-$ADF3\",\"byte_count\":20,"
        "\"sha256\":\"B4CC98CF95216620E6DAAB21C71BC1D9A679AFE9BB8BE5DC455A239E07640A3B\","
        "\"classification\":\"exact\"},"
        "\"positions\":[[528,144],[448,144],[362,112],[364,192],[392,144],"
        "[176,144],[320,144],[408,112],[400,192],[372,144]],"
        "\"directions\":[1,1,2,5,1,0,0,2,5,0],"
        "\"fixed_links\":[5,6,7,8,9,0,1,2,3,4],"
        "\"static_seeds\":{\"primary\":4,\"defender\":9,\"matchup\":[2,7]},"
        "\"formation_selector\":{\"source_pinned_starts\":46,\"theoretical_count\":48,"
        "\"rejected_indices\":[46,47],\"classification\":\"exact accepted selector\"},"
        "\"lineup_binding\":{\"staging\":\"exact\","
        "\"session_to_slot\":\"native-faithful/inferred\","
        "\"one_for_one_staged_to_slot\":\"not directly proven\"}},"
        "\"formation_selector\":{\"source_pinned_starts\":46,"
        "\"theoretical_count\":48,\"accepted_indices\":\"0..45\","
        "\"rejected_indices\":[46,47],\"index_formula\":\"depth_row*12+x_bucket\","
        "\"coordinate_bucketing\":\"source coordinate buckets\"},"
        "\"play_state\":{\"contract\":\"accepted play_state_initialize after formation_select\","
        "\"step_budget\":1,\"actor_order\":[0,1,2,3,4,5,6,7,8,9],"
        "\"source_command_advance\":true,\"deferred_effects_explicit\":true},"
        "\"immutable_ten_actor_snapshot\":true,\"transactional\":true,"
        "\"source_target_policy\":\"valid source actor references require a valid recorded coordinate; the native adapter follows the current referenced actor on every immutable post-human snapshot/tick; original Bank05 dynamic retarget/matchup semantics remain incomplete/unproven\","
        "\"source_direction_application\":\"validated source direction is composed into TGMO input; target-to-direction equivalence is tested where direct source direction is unavailable\","
        "\"fixed_opposing_link_use\":\"exact Bank04 fixed links; native matchup is inferred metadata and is not claimed as ROM dynamic assignment\","
        "\"classifications\":{\"formation_source_pinned\":true,\"native_matchup_inferred\":true,"
        "\"workspace_native_approximation\":true,\"shot_request_native_approximation\":true},"
        "\"shot_request_adapter\":\"accepted deterministic predicate with native random/workspace approximation; unsupported shots.c playback is explicit deferred/non-launch\","
        "\"serial_contract\":\"adapter observation counters wrap modulo 2^32; accepted CPU step_serial wraps modulo 2^16\"},"
        "\"runtime_inputs\":\"TGAI-1 plus same-pack TGMO-1; no ROM, decompilation, ASM, trace, capture, screenshot, video, log, dump, Lua output, or save state\"}");
}

static int append_gameplay_close_shot_source_map_entry(
    char *buffer,
    size_t capacity,
    size_t *length,
    int *first,
    const TecmoGameplayCloseShotProvenance *p)
{
    static const char *const roles[
        TECMO_GAMEPLAY_CLOSE_SHOT_SOURCE_COUNT] = {
        "control-and-phase-family-$8542-$8694",
        "state-helper-$919C-$91BB",
        "trajectory-family-$98E1-$9A5F",
        "contact-helper-$A214-$A25E",
        "resolution-family-$A503-$A6ED",
        "actor-state-family-$AB36-$AC09",
        "launch-helper-$B100-$B13E",
        "motion-family-$B32C-$B521",
        "contact-state-$B678-$B6E4",
        "release-helper-$B775-$B7AC",
        "state-tail-$BDEF-$BDF6",
        "state-tail-$BFC2-$BFC8",
        "pose-low-high-table-$8CED-$8D3C"
    };
    const char *prefix = *first != 0 ? "" : ",\n";

    *first = 0;
    if (tecmo_asset_pack_append_text(
            buffer, capacity, length,
            "%s"
            "    {\"id\":\"%s\",\"kind\":\"gameplay-close-shots-native\","
            "\"schema\":\"tecmo.gameplay-close-shots/TGCS-1\",\"size\":%u,"
            "\"fingerprint_fnv1a32\":\"%08X\","
            "\"dependency\":{\"entry\":\"%s\",\"size\":%u,"
            "\"fingerprint_fnv1a32\":\"%08X\","
            "\"reason\":\"actor-pointer-index resolution\"},"
            "\"source_spans\":[",
            prefix, TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_ID,
            (unsigned)TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_SIZE,
            (unsigned)TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_FNV1A32,
            TECMO_ASSET_PACK_GAMEPLAY_ID,
            (unsigned)TECMO_ASSET_PACK_GAMEPLAY_SIZE,
            (unsigned)TECMO_ASSET_PACK_GAMEPLAY_FNV1A32) != 0) {
        return -1;
    }
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_CLOSE_SHOT_SOURCE_COUNT; ++index) {
        const TecmoGameplayCloseShotExpectedSource *source =
            &tecmo_gameplay_close_shot_expected_sources[index];
        if (tecmo_asset_pack_append_text(
                buffer, capacity, length,
                "%s{\"role\":\"%s\",\"source_entry\":\"prg/bank05\","
                "\"source_offset\":%llu,\"bank\":5,\"cpu_start\":%u,"
                "\"cpu_end\":%u,\"size\":%u,"
                "\"fingerprint_fnv1a32\":\"%08X\"}",
                index == 0U ? "" : ",", roles[index],
                (unsigned long long)p->source_offsets[index],
                (unsigned)source->cpu_start,
                (unsigned)((uint32_t)source->cpu_start +
                           source->byte_count - 1U),
                (unsigned)source->byte_count,
                (unsigned)source->fingerprint) != 0) {
            return -1;
        }
    }
    return tecmo_asset_pack_append_text(
        buffer, capacity, length,
        "],\"raw_aggregate\":{\"size\":%u,"
        "\"fingerprint_fnv1a32\":\"%08X\"},"
        "\"supported_variants\":["
        "{\"numeric_id\":0,\"semantic_kind\":\"dunk\","
        "\"family\":[\"direct\",\"held-release\"],"
        "\"step_count\":32,\"pose_phase_count\":7,"
        "\"phase_table\":[1,2,3,3,3,4,4,4,4,4,4,4,4,4,4,4,"
        "6,6,6,6,6,6,6,5,5,5,5,5,5,5,5,5]},"
        "{\"numeric_id\":2,\"semantic_kind\":\"layup\","
        "\"family\":[\"arc\",\"longer-trajectory\",\"contactable\"],"
        "\"step_count\":16,\"pose_phase_count\":6,"
        "\"phase_table\":[0,1,2,3,3,4,4,4,5,5,5,5,5,5,5,5]}],"
        "\"semantic_mapping_contract\":\"derived exact 0:dunk,2:layup map; local verification artifacts are excluded from pack provenance and runtime inputs\","
        "\"phase_tables_fingerprint_fnv1a32\":\"%08X\","
        "\"pose_contract\":{\"source_cpu_range\":[36077,36156],"
        "\"raw_fingerprint_fnv1a32\":\"%08X\","
        "\"encoding\":\"40 low bytes followed by 40 high bytes; even byte offsets divided by two\","
        "\"profile_count\":2,\"direction_count\":8,"
        "\"variant0_bases\":["
        "[637,609,623,630,616,595,644,602],"
        "[693,665,679,686,672,651,700,658]],"
        "\"variant2_bases\":["
        "[807,783,795,801,789,771,813,777],"
        "[855,831,843,849,837,819,861,825]],"
        "\"resolved_pointer_count\":%u,"
        "\"resolved_sequence_fingerprint_fnv1a32\":\"%08X\","
        "\"unsupported_numeric_ids\":[1],"
        "\"unsupported_raw_group_policy\":\"intentionally unexposed\"}}",
        (unsigned)TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_RAW_SIZE,
        (unsigned)TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_RAW_FNV1A32,
        (unsigned)TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_PHASES_FNV1A32,
        (unsigned)TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_POSE_TABLE_FNV1A32,
        (unsigned)TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_RESOLVED_POSE_COUNT,
        (unsigned)TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_RESOLVED_POSE_FNV1A32);
}

static int append_gameplay_dunk_source_map_entry(
    char *buffer,
    size_t capacity,
    size_t *length,
    int *first,
    const TecmoGameplayDunkProvenance *p)
{
    static const char *const roles[TECMO_GAMEPLAY_DUNK_SOURCE_COUNT] = {
        "variant0-cutaway-trigger-$856B-$85A7",
        "clear-lane-helper-$85F3-$8640",
        "fixed-presentation-dispatch-$E770-$E78D",
        "screen-$0B-descriptor-$DCD2-$DCD8",
        "screen-$0B-compressed-stream-$9022-$9346",
        "screen-$0B-base-palette-$9346-$9355",
        "cutaway-controller-$B002-$B08A",
        "seven-stage-setup-$B08B-$B0C6",
        "stage-anchor-and-chr-tables-$B0C7-$B0EC",
        "fixed-selector-dispatch-$C711-$C73B",
        "fixed-selector-bank-table-$CAF5-$CB32",
        "fixed-selector-address-low-$CB33-$CB70",
        "fixed-selector-address-high-$CB71-$CBAE",
        "relative-sprite-emitter-$B37C-$B3C3",
        "side-pointer-control-$B3C4-$B3E9",
        "four-byte-relative-geometry-$B3EA-$BC3B",
        "profile-and-uniform-palette-recipe-$B0ED-$B157",
        "fixed-court-restore-$EB8D-$EC05"
    };
    const char *prefix = *first != 0 ? "" : ",\n";
    size_t index;
    *first = 0;
    if (tecmo_asset_pack_append_text(
            buffer, capacity, length,
            "%s"
            "    {\"id\":\"%s\",\"kind\":\"gameplay-dunk-cutaway-native\","
            "\"schema\":\"tecmo.gameplay-dunk-cutaway/TGDK-1\","
            "\"size\":%u,\"fingerprint_fnv1a32\":\"%08X\","
            "\"dependencies\":[{\"entry\":\"chr/all\",\"size\":%u,"
            "\"fingerprint_fnv1a32\":\"%08X\","
            "\"fingerprint_fnv1a64\":\"%016llX\"}],\"source_spans\":[",
            prefix, TECMO_ASSET_PACK_GAMEPLAY_DUNK_ID,
            (unsigned)TECMO_ASSET_PACK_GAMEPLAY_DUNK_SIZE,
            (unsigned)TECMO_ASSET_PACK_GAMEPLAY_DUNK_FNV1A32,
            (unsigned)TECMO_ASSET_PACK_GAMEPLAY_DUNK_CHR_SIZE,
            (unsigned)TECMO_ASSET_PACK_GAMEPLAY_DUNK_CHR_FNV1A32,
            (unsigned long long)TECMO_ASSET_PACK_GAMEPLAY_DUNK_CHR_FNV1A64) != 0) {
        return -1;
    }
    for (index = 0U; index < TECMO_GAMEPLAY_DUNK_SOURCE_COUNT; ++index) {
        const TecmoGameplayDunkExpectedSource *source =
            &tecmo_gameplay_dunk_expected_sources[index];
        const char *source_entry = source->fixed_bank != 0U
            ? "prg/fixed" : (source->bank == 0U ? "prg/bank00" :
              source->bank == 1U ? "prg/bank01" :
              source->bank == 5U ? "prg/bank05" : "prg/bank06");
        if (tecmo_asset_pack_append_text(
                buffer, capacity, length,
                "%s{\"role\":\"%s\",\"source_entry\":\"%s\","
                "\"source_offset\":%llu,\"bank\":%u,\"cpu_start\":%u,"
                "\"cpu_end\":%u,\"size\":%u,"
                "\"fingerprint_fnv1a32\":\"%08X\"}",
                index == 0U ? "" : ",", roles[index], source_entry,
                (unsigned long long)p->source_offsets[index],
                (unsigned)source->bank, (unsigned)source->cpu_start,
                (unsigned)((uint32_t)source->cpu_start +
                           source->byte_count - 1U),
                (unsigned)source->byte_count,
                (unsigned)source->fingerprint) != 0) {
            return -1;
        }
    }
    return tecmo_asset_pack_append_text(
        buffer, capacity, length,
        "],\"decoded_screen\":{\"screen_id\":11,"
        "\"encoded_size\":805,\"decoded_size\":2048,"
        "\"fingerprint_fnv1a32\":\"5414E3B6\","
        "\"page_fingerprints_fnv1a32\":[\"BA33539B\",\"7DF03FAC\"],"
        "\"terminator_palette_overlap\":\"encoded byte 805 is also the first palette byte and is bounded by both source spans\"},"
        "\"native_pages\":[{\"side\":0,\"decoded_page\":0,"
        "\"background_chr_pages\":[208,209,210,211]},{\"side\":1,"
        "\"decoded_page\":1,\"background_chr_pages\":[4,5,6,7]}],"
        "\"stages\":{\"count\":7,\"cadence_frames\":5,"
        "\"assignment_frames\":[27,32,37,42,47,52,57],"
        "\"visible_frames\":[28,32,37,42,47,52,57],"
        "\"anchor_y\":[111,79,63,47,31,16,31],"
        "\"side0_anchor_x\":[144,136,128,120,112,104,80],"
        "\"side1_anchor_x\":[32,48,64,80,88,96,80],"
        "\"sprite_chr_bases\":[212,212,212,212,216,216,220]},"
        "\"geometry\":{\"side_pointer_offsets\":[0,18],"
        "\"null_slots\":[7,8,16,17],\"piece_limit\":64,"
        "\"record_counts\":[[10,29,39,39,51,49,48],[10,29,39,39,51,49,47]],"
        "\"encoding\":\"count then count native relative-y,tile-minus-one,attributes,relative-x records; 8x16 display adds one to y and tile; record order is NES OAM priority and native composites in reverse\"},"
        "\"reference_palette\":{\"profile\":1,\"uniform_color\":48,"
        "\"background_fingerprint_fnv1a32\":\"DC2F67E5\","
        "\"sprite_fingerprint_fnv1a32\":\"7E958A9E\","
        "\"combined_fingerprint_fnv1a32\":\"939EBCBE\"},"
        "\"timing\":{\"live\":[1,22],\"dispatch\":23,"
        "\"initial_black\":[24,27],\"visible_cutaway\":[28,62],"
        "\"palette_black_with_staged_sprites\":63,\"sprites_cleared\":64,"
        "\"court_rebuild\":[66,70],\"live_return\":71,"
        "\"route_resume\":75,\"a9c5_dmc\":87,"
        "\"action_resolution\":132},"
        "\"approximations\":\"native clear-lane trigger and make/miss policy\","
        "\"production_palette_binding\":\"scene-selected TTDT roster profile byte 2 bit 7 plus fixed $DEAB/$DC19 matchup uniform color; standalone API retains explicit profile/uniform inputs\","
        "\"runtime_inputs\":\"TGDK-1 plus same-pack chr/all; no decompilation, capture, trace, screenshot, video, log, dump, Lua output, or save state\"}");
}

static int append_gameplay_jump_shot_source_map_entry(
    char *buffer,
    size_t capacity,
    size_t *length,
    int *first,
    const TecmoGameplayJumpShotProvenance *p)
{
    static const char *const roles[
        TECMO_GAMEPLAY_JUMP_SHOT_SOURCE_COUNT] = {
        "signed-multiply-division-$8001-$815A",
        "family-bases-$8469-$846A",
        "animation-counter-$8999-$89C0",
        "initial-velocity-derivation-$8D92-$8DD2",
        "phase-decrement-$9C29-$9C3F",
        "made-state08-timer-and-state9-$AC0A-$AC6E",
        "route1-follow-release-$AD41-$AF21",
        "route10-$B6E5-$B774",
        "bounce-motion-collision-$B7C1-$B87B",
        "post-shot-settlement-$BA65-$BAC0",
        "distance-flight-helpers-$BCA1-$BDC6",
        "distance-flight-lookup-$BDF7-$BEF6"
    };
    const char *prefix = *first != 0 ? "" : ",\n";

    *first = 0;
    if (tecmo_asset_pack_append_text(
            buffer, capacity, length,
            "%s"
            "    {\"id\":\"%s\",\"kind\":\"gameplay-jump-shots-native\","
            "\"schema\":\"tecmo.gameplay-jump-shots/TGJS-2\",\"size\":%u,"
            "\"fingerprint_fnv1a32\":\"%08X\","
            "\"dependencies\":["
            "{\"entry\":\"%s\",\"size\":%u,\"fingerprint_fnv1a32\":\"%08X\",\"reason\":\"pose records and selector source\"},"
            "{\"entry\":\"%s\",\"size\":%u,\"fingerprint_fnv1a32\":\"%08X\",\"reason\":\"shared $AB36 made seed and $B100/$B32C/$B678 flight ownership\"}],"
            "\"source_spans\":[",
            prefix, TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_ID,
            (unsigned)TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_SIZE,
            (unsigned)TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_FNV1A32,
            TECMO_ASSET_PACK_GAMEPLAY_ID,
            (unsigned)TECMO_ASSET_PACK_GAMEPLAY_SIZE,
            (unsigned)TECMO_ASSET_PACK_GAMEPLAY_FNV1A32,
            TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_ID,
            (unsigned)TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_SIZE,
            (unsigned)TECMO_ASSET_PACK_GAMEPLAY_CLOSE_SHOTS_FNV1A32) != 0) {
        return -1;
    }
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_JUMP_SHOT_SOURCE_COUNT; ++index) {
        const TecmoGameplayJumpShotExpectedSource *source =
            &tecmo_gameplay_jump_shot_expected_sources[index];
        if (tecmo_asset_pack_append_text(
                buffer, capacity, length,
                "%s{\"role\":\"%s\",\"source_entry\":\"prg/bank05\","
                "\"source_offset\":%llu,\"bank\":5,\"cpu_start\":%u,"
                "\"cpu_end\":%u,\"size\":%u,"
                "\"fingerprint_fnv1a32\":\"%08X\","
                "\"fingerprint_fnv1a64\":\"%016llX\"}",
                index == 0U ? "" : ",", roles[index],
                (unsigned long long)p->source_offsets[index],
                (unsigned)source->cpu_start,
                (unsigned)((uint32_t)source->cpu_start +
                           source->byte_count - 1U),
                (unsigned)source->byte_count,
                (unsigned)source->fingerprint,
                (unsigned long long)source->fingerprint_fnv1a64) != 0) {
            return -1;
        }
    }
    return tecmo_asset_pack_append_text(
        buffer, capacity, length,
        "],\"raw_aggregate\":{\"size\":%u,"
        "\"fingerprint_fnv1a32\":\"%08X\"},"
        "\"constants\":{\"nes_b_mask\":64,"
        "\"actor_states\":[30,11,12,13,14,0],"
        "\"phase_seeds\":[48,49,4,5,86],"
        "\"ball_states\":[18,1,5,23,16,0],"
        "\"gravity_q8\":40,\"floor_wrap_clamp\":246,"
        "\"bounce_decay_q8\":128,\"outcome_flag_mask\":128,"
        "\"crowd_sfx\":11,\"side_result_base\":12,"
        "\"made_state\":8,\"made_timer\":[4,2],"
        "\"made_terminal_stage\":12,\"made_updates\":26,"
        "\"made_complete_state\":9,\"flight_states\":[5,7],"
        "\"flight_count_limit\":60,\"flight_altitude_threshold\":60,"
        "\"fingerprint_fnv1a32\":\"%08X\"},"
        "\"pose_contract\":{\"source_cpu_ranges\":[[36157,36188],[36189,36220]],"
        "\"source_fingerprint_fnv1a32\":\"%08X\","
        "\"encoding\":\"32 low bytes then 32 high bytes; even byte offsets divided by two\","
        "\"layout\":\"family*16 + profile_bit*8 + direction\","
        "\"family_count\":2,\"profile_count\":2,\"direction_count\":8,"
        "\"pointer_count\":32,\"pointer_fingerprint_fnv1a32\":\"%08X\"},"
        "\"behavior_boundary\":\"current-B release, actor state transitions, explicit-input distance-flight initialization/update, Q8.8 gravity/clamp, pose selection, conditional bounce DMC, terminal outcome flag, state-08 26-update made settlement, and post-shot ordering; AD6E launch ownership, two-point admission, live outcome inputs, and contact remain unsupported\","
        "\"runtime_inputs\":\"TGJS-2 plus same-pack TGPL-1/TGCS-1; no decompilation, trace, capture, screenshot, log, dump, state, Lua, or video\"}",
        (unsigned)TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_RAW_SIZE,
        (unsigned)TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_RAW_FNV1A32,
        (unsigned)TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_CONSTANTS_FNV1A32,
        (unsigned)TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_POSE_SOURCE_FNV1A32,
        (unsigned)TECMO_ASSET_PACK_GAMEPLAY_JUMP_SHOTS_POSES_FNV1A32);
}

static int append_gameplay_shot_resolution_source_map_entry(
    char *buffer,
    size_t capacity,
    size_t *length,
    int *first,
    const TecmoGameplayShotResolutionProvenance *p)
{
    static const char *const roles[
        TECMO_GAMEPLAY_SHOT_RESOLUTION_SOURCE_COUNT] = {
        "outcome-calculation-and-bit-helpers-$91BC-$943A",
        "numeric-rim-route-dispatch-$A6EE-$A9D9",
        "claimant-scan-and-proximity-$B73E-$B87B",
        "claimant-driven-settlement-$B87C-$B8F5"
    };
    const char *prefix = *first != 0 ? "" : ",\n";

    *first = 0;
    if (tecmo_asset_pack_append_text(
            buffer, capacity, length,
            "%s"
            "    {\"id\":\"%s\",\"kind\":\"gameplay-shot-resolution-native\","
            "\"schema\":\"tecmo.gameplay-shot-resolution/TGSR-3\",\"size\":%u,"
            "\"fingerprint_fnv1a32\":\"%08X\","
            "\"fingerprint_fnv1a64\":\"%016llX\","
            "\"dependencies\":[{\"entry\":\"%s\",\"size\":%u,"
            "\"fingerprint_fnv1a32\":\"%08X\","
            "\"reason\":\"same-revision gameplay contract\"}],"
            "\"source_spans\":[",
            prefix, TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_ID,
            (unsigned)TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_SIZE,
            (unsigned)TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_FNV1A32,
            (unsigned long long)
                TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_FNV1A64,
            TECMO_ASSET_PACK_GAMEPLAY_ID,
            (unsigned)TECMO_ASSET_PACK_GAMEPLAY_SIZE,
            (unsigned)TECMO_ASSET_PACK_GAMEPLAY_FNV1A32) != 0) {
        return -1;
    }
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_SHOT_RESOLUTION_SOURCE_COUNT; ++index) {
        const TecmoGameplayShotResolutionExpectedSource *source =
            &tecmo_gameplay_shot_resolution_expected_sources[index];
        if (tecmo_asset_pack_append_text(
                buffer, capacity, length,
                "%s{\"role\":\"%s\",\"source_entry\":\"prg/bank05\","
                "\"source_offset\":%llu,\"bank\":5,\"cpu_start\":%u,"
                "\"cpu_end\":%u,\"size\":%u,"
                "\"fingerprint_fnv1a32\":\"%08X\","
                "\"fingerprint_fnv1a64\":\"%016llX\"}",
                index == 0U ? "" : ",", roles[index],
                (unsigned long long)p->source_offsets[index],
                (unsigned)source->cpu_start,
                (unsigned)((uint32_t)source->cpu_start +
                           source->byte_count - 1U),
                (unsigned)source->byte_count,
                (unsigned)source->fingerprint_fnv1a32,
                (unsigned long long)source->fingerprint_fnv1a64) != 0) {
            return -1;
        }
    }
    if (tecmo_asset_pack_append_text(
            buffer, capacity, length,
            ",{\"role\":\"state15-convergence-predicate-$A2DF-$A2F7\","
            "\"source_entry\":\"prg/bank05\",\"source_offset\":%llu,"
            "\"bank\":5,\"cpu_start\":%u,\"cpu_end\":%u,\"size\":%u,"
            "\"fingerprint_fnv1a32\":\"%08X\","
            "\"fingerprint_fnv1a64\":\"%016llX\"},"
            "{\"role\":\"state15-launch-target-$AD4E-$AD64\","
            "\"source_entry\":\"prg/bank05\",\"source_offset\":%llu,"
            "\"bank\":5,\"cpu_start\":%u,\"cpu_end\":%u,\"size\":%u,"
            "\"fingerprint_fnv1a32\":\"%08X\","
            "\"fingerprint_fnv1a64\":\"%016llX\"},"
            "{\"role\":\"state15-orientation-snap-table-$BDF3-$BDF6\","
            "\"source_entry\":\"prg/bank05\",\"source_offset\":%llu,"
            "\"bank\":5,\"cpu_start\":%u,\"cpu_end\":%u,\"size\":%u,"
            "\"fingerprint_fnv1a32\":\"%08X\","
            "\"fingerprint_fnv1a64\":\"%016llX\"},"
            "{\"role\":\"three-point-arc-boundary-table-$BEEF-$BF6A\","
            "\"source_entry\":\"prg/bank05\",\"source_offset\":%llu,"
            "\"bank\":5,\"cpu_start\":%u,\"cpu_end\":%u,\"size\":%u,"
            "\"fingerprint_fnv1a32\":\"%08X\","
            "\"fingerprint_fnv1a64\":\"%016llX\"}",
            (unsigned long long)p->convergence_predicate_offset,
            (unsigned)
                TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_CONVERGENCE_CPU,
            (unsigned)(
                TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_CONVERGENCE_CPU +
                TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_CONVERGENCE_SIZE -
                1U),
            (unsigned)
                TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_CONVERGENCE_SIZE,
            (unsigned)
                TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_CONVERGENCE_FNV1A32,
            (unsigned long long)
                TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_CONVERGENCE_FNV1A64,
            (unsigned long long)p->rim_rattle_launch_target_offset,
            (unsigned)
                TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_RATTLE_TARGET_CPU,
            (unsigned)(
                TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_RATTLE_TARGET_CPU +
                TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_RATTLE_TARGET_SIZE -
                1U),
            (unsigned)
                TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_RATTLE_TARGET_SIZE,
            (unsigned)
                TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_RATTLE_TARGET_FNV1A32,
            (unsigned long long)
                TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_RATTLE_TARGET_FNV1A64,
            (unsigned long long)p->rim_rattle_snap_table_offset,
            (unsigned)
                TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_RATTLE_SNAP_CPU,
            (unsigned)(
                TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_RATTLE_SNAP_CPU +
                TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_RATTLE_SNAP_SIZE -
                1U),
            (unsigned)
                TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_RATTLE_SNAP_SIZE,
            (unsigned)
                TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_RATTLE_SNAP_FNV1A32,
            (unsigned long long)
                TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_RATTLE_SNAP_FNV1A64,
            (unsigned long long)p->point_arc_table_offset,
            (unsigned)
                TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_POINT_ARC_CPU,
            (unsigned)(
                TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_POINT_ARC_CPU +
                TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_POINT_ARC_SIZE - 1U),
            (unsigned)
                TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_POINT_ARC_SIZE,
            (unsigned)
                TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_POINT_ARC_FNV1A32,
            (unsigned long long)
                TECMO_ASSET_PACK_GAMEPLAY_SHOT_RESOLUTION_POINT_ARC_FNV1A64) !=
        0) {
        return -1;
    }
    return tecmo_asset_pack_append_text(
        buffer, capacity, length,
        "],\"outcome\":{\"terminal_context_required\":true,"
        "\"flag_mask\":128,\"clear\":\"make\",\"set\":\"miss\","
        "\"clear_helper_cpu\":37933,\"set_helper_cpu\":37940},"
        "\"point_value\":{\"classifier_cpu\":47509,"
        "\"classifier_end_cpu\":47679,"
        "\"classifier_dependency\":\"same-pack TGPL-1 $B995-$BA3F\","
        "\"shot_flags_low2_nonzero\":1,"
        "\"field_goal\":2,\"three_point\":3,"
        "\"y_min_inclusive\":91,\"y_max_exclusive\":215,"
        "\"orientation_values\":[0,1],"
        "\"arc_table_cpu_start\":48879,\"arc_table_cpu_end\":49002,"
        "\"arithmetic\":\"6502 low-byte subtract with borrow into "
        "orientation-specific high-byte adjustment\"},"
        "\"rim_routes\":{\"selector_mask\":3,"
        "\"targets_cpu\":[42760,42921,43241,42760],"
        "\"semantic_policy\":\"numeric/address-bound only\"},"
        "\"rim_rattle\":{\"object_state\":21,"
        "\"orientation_start_x\":[157,611],\"start_y\":147,"
        "\"source_launch_target_x\":[160,608],"
        "\"source_launch_target_y\":143,"
        "\"screen_mapping_policy\":\"retain raw TGSR state; apply raw minus "
        "ROM launch target relative to the native shot endpoint\","
        "\"target_x_dependency\":\"same-pack TGCS-1 $BDEF-$BDF6\","
        "\"horizontal_velocity_q6\":64,\"coordinate_delta_per_update\":1,"
        "\"altitude\":56,\"pass_timer_updates\":4,"
        "\"pass_derivation\":\"((source & 3) + 1) << 4; preserve low nibble\","
        "\"repeat_dmc_length\":10,"
        "\"render_script_addresses\":[47801,47801,47807,47813,"
        "47819,47819,47825,47831],"
        "\"exit_render_script_addresses\":[47837,47873],"
        "\"render_script_address_policy\":\"source-derived selection "
        "addresses, not literal sprite or CHR IDs\","
        "\"completion_policy\":\"restore saved velocity, then apply the "
        "separate $A2DF predicate; state 10 is not universal\"},"
        "\"claimant_thresholds\":{\"horizontal_delta\":[-11,10],"
        "\"depth_delta\":[-7,6],"
        "\"grounded_ball_altitude_max_inclusive\":39,"
        "\"airborne_ball_above_claimant_max_inclusive\":59},"
        "\"settlement\":{\"same_team\":\"select claimant without possession change\","
        "\"other_team\":\"select claimant and change possession\"},"
        "\"limits\":\"an address hit alone is not terminal; $9434 also occurs in nonterminal close animations; claimant is not labeled rebound, steal, block, or recovery\","
        "\"runtime_inputs\":\"TGSR-3 plus same-pack TGPL-1; the debug screen "
        "mapping additionally requires same-pack TGCS-1; no decompilation, "
        "ASM, trace, capture, screenshot, log, dump, state, Lua, video, or "
        "ROM\"}");
}

static int append_gameplay_penalty_source_map_entry(
    char *buffer,
    size_t capacity,
    size_t *length,
    int *first,
    const TecmoGameplayPenaltyProvenance *p)
{
    static const char *const roles[TECMO_GAMEPLAY_PENALTY_SOURCE_COUNT] = {
        "foul-commit-metadata-$9571-$9649",
        "foul-rules-and-presentation-$B0F8-$B398",
        "foul-presentation-script-$E95E-$EA11",
        "presentation-release-gate-$EA14-$EA2F",
        "violation-presentation-script-$EC5B-$ED14",
        "nes-a-release-helper-$D2B9-$D2CE",
        "violation-selector-presentation-$BE87-$BFA8",
        "shared-screen-$22-delayed-sfx6-request-$BA1F-$BA3E"
    };
    const char *prefix = *first != 0 ? "" : ",\n";

    *first = 0;
    if (tecmo_asset_pack_append_text(
            buffer, capacity, length,
            "%s"
            "    {\"id\":\"%s\",\"kind\":\"gameplay-penalties-native\","
            "\"schema\":\"tecmo.gameplay-penalties/TPNL-1\",\"size\":%u,"
            "\"fingerprint_fnv1a32\":\"%08X\","
            "\"revision_sha256\":\"076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4\","
            "\"revision_sha256_verified\":true,"
            "\"dependencies\":["
            "{\"entry\":\"%s\",\"size\":%u,\"fingerprint_fnv1a32\":\"%08X\",\"reason\":\"shared foul and violation presentation spans\"},"
            "{\"entry\":\"%s\",\"size\":%u,\"fingerprint_fnv1a32\":\"%08X\",\"reason\":\"cue 6 vocabulary and C009/F2F6 queue semantics\"}],"
            "\"source_spans\":[",
            prefix, TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_ID,
            (unsigned)TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_SIZE,
            (unsigned)TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_FNV1A32,
            TECMO_ASSET_PACK_GAMEPLAY_ID,
            (unsigned)TECMO_ASSET_PACK_GAMEPLAY_SIZE,
            (unsigned)TECMO_ASSET_PACK_GAMEPLAY_FNV1A32,
            TECMO_ASSET_PACK_GAMEPLAY_SFX_ID,
            (unsigned)TECMO_GAMEPLAY_SFX_PAYLOAD_SIZE,
            (unsigned)TECMO_GAMEPLAY_SFX_PAYLOAD_FNV1A32) != 0) {
        return -1;
    }
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_PENALTY_SOURCE_COUNT; ++index) {
        const TecmoGameplayPenaltyExpectedSource *source =
            &tecmo_gameplay_penalty_expected_sources[index];
        const char *source_entry = source->fixed_bank != 0U
            ? "prg/fixed"
            : NULL;
        char bank_entry[16];
        if (source_entry == NULL) {
            (void)snprintf(bank_entry, sizeof(bank_entry),
                           "prg/bank%02u", (unsigned)source->bank);
            source_entry = bank_entry;
        }
        if (tecmo_asset_pack_append_text(
                buffer, capacity, length,
                "%s{\"role\":\"%s\",\"source_entry\":\"%s\","
                "\"source_offset\":%llu,\"bank\":%u,"
                "\"fixed_bank\":%s,\"cpu_start\":%u,\"cpu_end\":%u,"
                "\"size\":%u,\"fingerprint_fnv1a32\":\"%08X\"}",
                index == 0U ? "" : ",", roles[index], source_entry,
                (unsigned long long)p->source_offsets[index],
                (unsigned)source->bank,
                source->fixed_bank != 0U ? "true" : "false",
                (unsigned)source->cpu_start,
                (unsigned)((uint32_t)source->cpu_start +
                           source->byte_count - 1U),
                (unsigned)source->byte_count,
                (unsigned)source->fingerprint) != 0) {
            return -1;
        }
    }
    return tecmo_asset_pack_append_text(
        buffer, capacity, length,
        "],\"rules\":{"
        "\"individual_foul_cap\":6,\"team_foul_cap\":5,"
        "\"regulation_bonus_threshold\":5,\"overtime_bonus_threshold\":4,"
        "\"offensive_primary_route_zero\":\"charging\","
        "\"offensive_primary_nonzero_route\":\"pushing\","
        "\"defensive_classes\":[\"blocking\",\"pushing\"],"
        "\"two_attempt_saved_route_selectors\":[1,5,11,12,13,14,15,18],"
        "\"one_attempt_current_route_selectors\":[8,9],"
        "\"offensive_foul_attempts\":0,\"offensive_foul_turnover\":true},"
        "\"presentations\":{"
        "\"foul\":{\"banked_dispatcher_selector\":34,\"lead_in_frames\":4,\"maximum_wait_frames\":160,\"presentation_sfx_id\":6,\"presentation_sfx_delay_frames\":16,\"live_restart_sfx_id\":5,\"live_restart_music_id\":5,\"live_restart_requires_game_music\":true},"
        "\"violation\":{\"selector_min\":1,\"selector_max\":7,"
        "\"five_seconds_selector\":3,\"lead_in_frames\":4,"
        "\"maximum_wait_frames\":120,\"banked_dispatcher_selector\":34,\"screen_id\":5,\"presentation_sfx_id\":6,\"presentation_sfx_delay_frames\":16,\"live_restart_sfx_id\":5,\"live_restart_music_id\":5,\"live_restart_requires_game_music\":true},"
        "\"release\":{\"initial_delay_frames\":4,\"poll_interval_frames\":1,\"nes_a_mask\":128,\"controller_count\":2},"
        "\"restart_route_semantics\":{"
        "\"qualification\":\"caller precondition from exact live boundary; no route selector is encoded in TPNL-1\","
        "\"qualifying_direct_live_restart\":{\"requires_game_music\":true,\"sfx_id\":5,\"music_track_id\":5},"
        "\"foul_to_free_throws\":{\"requires_game_music\":true,\"sfx_id\":null,\"music_track_id\":5},"
        "\"final_free_throw_return\":{\"sfx_id\":null,\"music_track_id\":null},"
        "\"game_music_disabled\":{\"sfx_id\":null,\"music_track_id\":null}}},"
        "\"selector_note\":\"numeric route selectors retain neutral ROM identities; no collision or foul detector is inferred\","
        "\"runtime_inputs\":\"TPNL-1 plus same-pack TGPL-1 and TSFX-1; no decompilation, trace, capture, screenshot, video, log, dump, Lua output, ROM, or save state\"}");
}

static int append_gameplay_violation_referee_source_map_entry(
    char *buffer,
    size_t capacity,
    size_t *length,
    int *first,
    const TecmoGameplayViolationRefereeProvenance *p)
{
    static const char *const roles[
        TECMO_GAMEPLAY_VIOLATION_REFEREE_SOURCE_COUNT] = {
        "violation-screen-route-$BE87-$BEA0",
        "violation-text-renderer-and-tables-$BEC7-$BFA8",
        "bank06-character-map-$9AC8-$9B07",
        "screen05-descriptor-$DCA8-$DCAE",
        "screen05-compressed-stream-$B600-$B71F",
        "screen05-background-palette-$B5F0-$B5FF",
        "referee-controller-$BA1F-$BAF1",
        "referee-sprite-palette-$BA06-$BA15",
        "referee-sequence-pointers-and-lists-$B317-$B33E",
        "referee-metasprite-pointers-and-records-$B33F-$BA05"
    };
    const char *prefix = *first != 0 ? "" : ",\n";
    size_t index;

    *first = 0;
    if (tecmo_asset_pack_append_text(
            buffer, capacity, length,
            "%s"
            "    {\"id\":\"%s\",\"kind\":\"gameplay-violation-referee-native\","
            "\"schema\":\"tecmo.gameplay-violation-referee/TGVR-1\",\"size\":%u,"
            "\"fingerprint_fnv1a32\":\"%08X\","
            "\"revision_sha256\":\"076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4\","
            "\"revision_sha256_verified\":true,"
            "\"dependencies\":["
            "{\"entry\":\"chr/all\",\"size\":%u,\"fingerprint_fnv1a32\":\"%08X\",\"fingerprint_fnv1a64\":\"%016llX\",\"reason\":\"screen and 8x16 referee tiles\"},"
            "{\"entry\":\"%s\",\"size\":%u,\"fingerprint_fnv1a32\":\"%08X\",\"reason\":\"selector-to-violation presentation contract\"}],"
            "\"source_spans\":[",
            prefix,
            TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_ID,
            (unsigned)TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_SIZE,
            (unsigned)TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_FNV1A32,
            (unsigned)TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_CHR_SIZE,
            (unsigned)TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_CHR_FNV1A32,
            (unsigned long long)
                TECMO_ASSET_PACK_GAMEPLAY_VIOLATION_REFEREE_CHR_FNV1A64,
            TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_ID,
            (unsigned)TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_SIZE,
            (unsigned)TECMO_ASSET_PACK_GAMEPLAY_PENALTIES_FNV1A32) != 0) {
        return -1;
    }
    for (index = 0U;
         index < TECMO_GAMEPLAY_VIOLATION_REFEREE_SOURCE_COUNT; ++index) {
        const TecmoGameplayViolationRefereeExpectedSource *source =
            &tecmo_gameplay_violation_referee_expected_sources[index];
        const char *source_entry = source->fixed_bank != 0U
            ? "prg/fixed" : NULL;
        char bank_entry[16];
        if (source_entry == NULL) {
            (void)snprintf(bank_entry, sizeof(bank_entry),
                           "prg/bank%02u", (unsigned)source->bank);
            source_entry = bank_entry;
        }
        if (tecmo_asset_pack_append_text(
                buffer, capacity, length,
                "%s{\"role\":\"%s\",\"source_entry\":\"%s\","
                "\"source_offset\":%llu,\"bank\":%u,"
                "\"fixed_bank\":%s,\"cpu_start\":%u,\"cpu_end\":%u,"
                "\"size\":%u,\"fingerprint_fnv1a32\":\"%08X\","
                "\"payload_offset\":%u}",
                index == 0U ? "" : ",", roles[index], source_entry,
                (unsigned long long)p->source_offsets[index],
                (unsigned)source->bank,
                source->fixed_bank != 0U ? "true" : "false",
                (unsigned)source->cpu_start,
                (unsigned)((uint32_t)source->cpu_start +
                           source->byte_count - 1U),
                (unsigned)source->byte_count,
                (unsigned)source->fingerprint,
                (unsigned)source->payload_offset) != 0) {
            return -1;
        }
    }
    return tecmo_asset_pack_append_text(
        buffer, capacity, length,
        "],\"native_contract\":{"
        "\"screen_id\":5,\"screen_loader\":\"fixed $D92E/$D9F6\","
        "\"background_chr_selectors\":[202,250],"
        "\"sprite_chr_selectors\":[202,113,114,115],"
        "\"dispatcher_selector\":34,"
        "\"initial_group\":0,\"initial_hold_frames\":16,"
        "\"group_cadence_frames\":4,"
        "\"shot_clock_selector\":5,"
        "\"shot_clock_sequence_id\":3,"
        "\"shot_clock_groups\":[9,10,10,10],"
        "\"message\":\"SHOT CLOCK VIOLATION\","
        "\"message_ppu_address\":8934,"
        "\"out_of_bounds_selector\":1,"
        "\"out_of_bounds_sequence_id\":1,"
        "\"out_of_bounds_groups\":[3,4,5,5,5],"
        "\"out_of_bounds_message\":\"OUT OF BOUNDS\","
        "\"out_of_bounds_message_ppu_address\":8937,"
        "\"live_out_of_bounds_trigger\":\"TGMO-1 fixed $F10C-$F1B0 primary-holder boundary latch consumed through TPNL-1 selector 1\","
        "\"backcourt_selector\":2,"
        "\"backcourt_sequence_id\":1,"
        "\"backcourt_groups\":[3,4,5,5,5],"
        "\"backcourt_message\":\"BACKCOURT\","
        "\"backcourt_message_ppu_address\":8939,"
        "\"live_backcourt_trigger\":\"TGBC-1 Bank05 $971F-$9786 frontcourt latch and return detector consumed through TPNL-1 selector 2\","
        "\"presentation_sfx_id\":6,\"sfx_delay_frames\":16},"
        "\"capture_bounded_alignment\":{"
        "\"black_frames\":9,\"fade_step_frames\":4,"
        "\"sequence_visible_start_frame\":23,"
        "\"evidence\":\"local exact-Rev1 FCEUX frame probe; generic PPU-loader execution is not cycle-ported\"},"
        "\"runtime_inputs\":\"TGVR-1 plus exact same-pack chr/all and TPNL-1; no ROM, ASM, decompilation, capture, screenshot, video, trace, log, dump, Lua output, or save state\"}");
}

static int append_gameplay_free_throw_lineup_source_map_entry(
    char *buffer,
    size_t capacity,
    size_t *length,
    int *first,
    const TecmoGameplayFreeThrowLineupProvenance *p)
{
    static const char *const roles[
        TECMO_GAMEPLAY_FREE_THROW_LINEUP_SOURCE_COUNT] = {
        "pose-offset-lookup-$88B0-$88D9",
        "round-lineup-seed-and-slot-order-$9621-$976E",
        "free-throw-followup-lineup-seed-$976F-$985C",
        "free-throw-lineup-pointers-coordinates-and-directions-$985D-$9918"
    };
    const char *prefix = *first != 0 ? "" : ",\n";

    *first = 0;
    if (tecmo_asset_pack_append_text(
            buffer, capacity, length,
            "%s"
            "    {\"id\":\"%s\",\"kind\":\"gameplay-free-throw-lineup-native\","
            "\"schema\":\"tecmo.gameplay-free-throw-lineup/TGFL-1\","
            "\"size\":%u,\"fingerprint_fnv1a32\":\"%08X\","
            "\"revision_sha256_identity\":\"076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4\","
            "\"revision_full_rom_fnv1a32\":\"0650F5B0\","
            "\"revision_full_rom_fnv1a32_verified\":true,"
            "\"revision_source_fingerprints_verified\":true,"
            "\"dependencies\":[{\"entry\":\"%s\",\"size\":%u,"
            "\"fingerprint_fnv1a32\":\"%08X\","
            "\"reason\":\"validate raw even pose offsets as TGPL pointer indexes offset/2\"}],"
            "\"source_spans\":[",
            prefix,
            TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_ID,
            (unsigned)TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_SIZE,
            (unsigned)TECMO_ASSET_PACK_GAMEPLAY_FREE_THROW_LINEUP_FNV1A32,
            TECMO_ASSET_PACK_GAMEPLAY_ID,
            (unsigned)TECMO_ASSET_PACK_GAMEPLAY_SIZE,
            (unsigned)TECMO_ASSET_PACK_GAMEPLAY_FNV1A32) != 0) {
        return -1;
    }
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_FREE_THROW_LINEUP_SOURCE_COUNT; ++index) {
        const TecmoGameplayFreeThrowLineupExpectedSource *source =
            &tecmo_gameplay_free_throw_lineup_expected_sources[index];
        if (tecmo_asset_pack_append_text(
                buffer, capacity, length,
                "%s{\"role\":\"%s\",\"source_entry\":\"prg/bank06\","
                "\"source_offset\":%llu,\"bank\":6,\"fixed_bank\":false,"
                "\"cpu_start\":%u,\"cpu_end\":%u,\"size\":%u,"
                "\"fingerprint_fnv1a32\":\"%08X\","
                "\"payload_offset\":%u}",
                index == 0U ? "" : ",", roles[index],
                (unsigned long long)p->source_offsets[index],
                (unsigned)source->cpu_start,
                (unsigned)((uint32_t)source->cpu_start +
                           source->byte_count - 1U),
                (unsigned)source->byte_count,
                (unsigned)source->fingerprint,
                (unsigned)source->payload_offset) != 0) {
            return -1;
        }
    }
    return tecmo_asset_pack_append_text(
        buffer, capacity, length,
        "],\"lineup_contract\":{"
        "\"orientations\":[0,1],\"actor_slots\":10,"
        "\"coordinates\":\"raw 16-bit world X and raw 8-bit world Y; unclamped and unprojected\","
        "\"pose_offset_units\":\"even byte offset\","
        "\"pose_index_rule\":\"raw_pose_offset/2\","
        "\"validated_pose_indexes\":[517,518,519,520],"
        "\"shooter_pose\":\"preserved/undefined because $976F-$985C does not call $88B0 for the shooter\","
        "\"stream_policy\":\"descending actor slots; shooter skip consumes neither coordinate pair nor pose-direction item\","
        "\"base_nonshooter_state\":{\"raw_046E\":0,\"raw_057C\":1,\"raw_0458\":48,\"raw_0479\":193,\"raw_048F\":0},"
        "\"base_shooter_state\":{\"raw_046E\":32,\"raw_057C\":1,\"raw_0458\":48,\"raw_0479\":65,\"raw_048F\":0},"
        "\"control_overrides\":\"not encoded; secondary raw phase $15 and shooter script override require explicit side-control inputs\"},"
        "\"live_scene_integration\":{"
        "\"orientation_source\":\"TGOR-1 current_direction synchronized from free-throw scoring-team possession\","
        "\"position_binding\":\"copy exact TGFL-1 raw world X/Y for all ten actors into canonical TGCT-1 full-court positions and anchors\","
        "\"shooter_selection_policy\":\"native adapter selects the current scoring-team ball holder, otherwise the first scoring-team actor\","
        "\"secondary_selection_policy\":\"native adapter selects the assigned opposing controlled actor, otherwise the first opposing actor\","
        "\"ball_binding\":\"native held-ball attachment from the shooter coordinate and hoop-facing orientation; not owned by TGFL-1\","
        "\"camera_binding\":\"typed TGCP-2 forced settle on the shooter coordinate at entry; orientation 0 camera_x=102 and orientation 1 camera_x=408; frozen through the free-throw sequence\","
        "\"render_binding\":\"one combined TGCT-1 slice and TGCP-2 actor projection frame at the settled camera\","
        "\"pose_binding\":\"live scene preserves existing actor poses; TGFL-1 nonshooter raw pose indexes remain available but are not applied\","
        "\"integration_is_additional_rom_claim\":false},"
        "\"supported_boundary\":\"exact pure TGFL-1 derivation plus native live positioning and TGCT-1/TGCP-2 projection composition; shooter/secondary selection, held-ball attachment, and camera composition are native adapters, not additional ROM claims; no live pose-state override, aim, attempt decrement, outcome, rebound, CPU positioning/script, or capture-derived behavior\","
        "\"runtime_inputs\":\"TGFL-1 plus same-pack TGPL-1; no ROM, decompilation, ASM, trace, capture, screenshot, video, log, dump, Lua output, or save state\"}");
}

static int append_gameplay_hud_source_map_entry(
    char *buffer,
    size_t capacity,
    size_t *length,
    int *first,
    const TecmoGameplayHudProvenance *provenance)
{
    static const char *const roles[TECMO_GAMEPLAY_HUD_SOURCE_COUNT] = {
        "team-mark-writer-and-ppu-destinations-$BDF0-$BE1E",
        "team-mark-offsets-and-five-tile-rows-$BE1F-$BECC",
        "score-digit-and-player-name-font-formatters-$AF64-$B07B"
    };
    const char *prefix = *first != 0 ? "" : ",\n";
    size_t index;
    *first = 0;
    if (tecmo_asset_pack_append_text(
            buffer, capacity, length,
            "%s    {\"id\":\"%s\",\"kind\":\"gameplay-live-hud-native\","
            "\"schema\":\"tecmo.gameplay-hud/THUD-1\","
            "\"size\":%u,\"fingerprint_fnv1a32\":\"%08X\","
            "\"revision_sha256_identity\":\"076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4\","
            "\"revision_full_rom_fnv1a32\":\"0650F5B0\","
            "\"revision_full_rom_fnv1a32_verified\":true,"
            "\"revision_source_fingerprints_verified\":true,"
            "\"dependencies\":["
            "{\"entry\":\"%s\",\"size\":%u,\"fingerprint_fnv1a32\":\"%08X\"},"
            "{\"entry\":\"menu/team-data\",\"size\":%u,\"fingerprint_fnv1a32\":\"%08X\"},"
            "{\"entry\":\"chr/all\",\"size\":%u,\"fingerprint_fnv1a32\":\"%08X\",\"fingerprint_fnv1a64\":\"%016llX\"}],"
            "\"source_spans\":[",
            prefix, TECMO_ASSET_PACK_GAMEPLAY_HUD_ID,
            (unsigned)TECMO_ASSET_PACK_GAMEPLAY_HUD_SIZE,
            (unsigned)TECMO_ASSET_PACK_GAMEPLAY_HUD_FNV1A32,
            TECMO_ASSET_PACK_GAMEPLAY_ID,
            (unsigned)TECMO_ASSET_PACK_GAMEPLAY_SIZE,
            (unsigned)TECMO_ASSET_PACK_GAMEPLAY_FNV1A32,
            (unsigned)TECMO_ASSET_PACK_TEAM_DATA_SIZE,
            (unsigned)TECMO_ASSET_PACK_TEAM_DATA_FNV1A32,
            (unsigned)TECMO_ASSET_PACK_GAMEPLAY_CHR_SIZE,
            (unsigned)TECMO_ASSET_PACK_GAMEPLAY_CHR_FNV1A32,
            (unsigned long long)TECMO_ASSET_PACK_GAMEPLAY_CHR_FNV1A64) != 0) {
        return -1;
    }
    for (index = 0U; index < TECMO_GAMEPLAY_HUD_SOURCE_COUNT; ++index) {
        const TecmoGameplayHudExpectedSource *source =
            &tecmo_gameplay_hud_expected_sources[index];
        char source_entry[16];
        (void)snprintf(source_entry, sizeof(source_entry),
                       "prg/bank%02u", (unsigned)source->bank);
        if (tecmo_asset_pack_append_text(
                buffer, capacity, length,
                "%s{\"role\":\"%s\",\"source_entry\":\"%s\","
                "\"source_offset\":%llu,\"bank\":%u,\"fixed_bank\":false,"
                "\"cpu_start\":%u,\"cpu_end\":%u,\"size\":%u,"
                "\"fingerprint_fnv1a32\":\"%08X\",\"payload_offset\":%u}",
                index == 0U ? "" : ",", roles[index], source_entry,
                (unsigned long long)provenance->source_offsets[index],
                (unsigned)source->bank, (unsigned)source->cpu_start,
                (unsigned)((uint32_t)source->cpu_start +
                           source->byte_count - 1U),
                (unsigned)source->byte_count,
                (unsigned)source->fingerprint,
                (unsigned)source->payload_offset) != 0) {
            return -1;
        }
    }
    return tecmo_asset_pack_append_text(
        buffer, capacity, length,
        "],\"exact_contract\":{"
        "\"team_mark_rows\":29,\"team_mark_width_tiles\":5,"
        "\"team_mark_ppu_destinations\":[8257,8279],"
        "\"font_ascii_first\":32,\"font_ascii_last\":90,"
        "\"player_name_format\":\"first initial, dot, surname clipped/padded to nine tiles\","
        "\"font_chr_selector\":250,"
        "\"font_chr_dependency\":\"all 59 Bank02 character-map tiles must equal the same-pack TTDT-1 `$FA` font records and resolved chr/all offsets\"},"
        "\"live_scene_integration\":{"
        "\"fixed_rows\":[2,3],"
        "\"row_2\":\"away team mark and score, clock, home team mark and score\","
        "\"row_3\":\"BCD jersey number and one selected player per team; no shot clock or period label\","
        "\"placement_status\":\"team mark destinations and Bank02 tile mappings are ROM-exact; complete two-row ownership, remaining blank columns, live `$FA` top-row binding, colon tile, and black cell backing are reference-verified presentation bounds\","
        "\"selected_cpu_actor_policy\":\"native adapter uses holder or opposing same-position matchup when no controller owns that team\","
        "\"score_display_policy\":\"native renderer caps the state model's wider score at three visible digits\"},"
        "\"runtime_inputs\":\"THUD-1 plus same-pack TGPL-1, TTDT-1, and chr/all; no ROM, decompilation, ASM, trace, capture, screenshot, video, log, dump, Lua output, or save state\"}");
}

static int append_gameplay_pretip_source_map_entry(
    char *buffer,
    size_t capacity,
    size_t *length,
    int *first,
    const TecmoGameplayPreTipProvenance *provenance)
{
    static const char *roles[TECMO_GAMEPLAY_PRETIP_SOURCE_COUNT] = {
        "blank-screen-descriptor","blank-screen-stream",
        "blank-screen-palette","presentation-screen-wait-helpers",
        "matchup-sequence-and-team-text","mode-and-versus-strings",
        "mode-string-pointer-table","character-to-tile-map",
        "character-16px-metatile-table",
        "card-text-chr-selector-setup",
        "tipoff-closeup-entry","tipoff-closeup-palettes",
        "tipoff-closeup-control",
        "tipoff-closeup-timing-and-lineup-tables",
        "fixed-d861-sprite-staging",
        "center-tip-object-setup","center-tip-object-update",
        "pregame-launch-bridge","live-handoff",
        "tipoff-orientation-select"
    };
    const char *prefix = *first != 0 ? "" : ",\n";
    size_t index;
    *first = 0;
    if (tecmo_asset_pack_append_text(
            buffer, capacity, length,
            "%s    {\"id\":\"%s\",\"kind\":\"gameplay-pre-tip-presentation\","
            "\"schema\":\"tecmo.gameplay-pre-tip/TPTI-1\","
            "\"payload_size\":%u,\"payload_fingerprint_fnv1a32\":\"%08X\","
            "\"dependencies\":["
            "{\"entry\":\"gameplay/core\",\"schema\":\"tecmo.gameplay/TGPL-1\",\"size\":23416,\"fingerprint_fnv1a32\":\"2047CCE0\"},"
            "{\"entry\":\"menu/team-data\",\"schema\":\"tecmo.team-data/TTDT-1\",\"size\":96372,\"fingerprint_fnv1a32\":\"812628F0\"},"
            "{\"entry\":\"audio/music\",\"schema\":\"tecmo.music/TMUS-1\",\"size\":36784,\"fingerprint_fnv1a32\":\"05C00ECB\"},"
            "{\"entry\":\"arena/intro/warriors-transition\",\"schema\":\"tecmo.intro.warriors/TWAR-1\",\"size\":%u,\"fingerprint_fnv1a32\":\"%08X\"},"
            "{\"entry\":\"chr/all\",\"size\":262144,\"fingerprint_fnv1a32\":\"F6F6E854\",\"fingerprint_fnv1a64\":\"96A64F53B240ABB4\"}],"
            "\"sources\":[",
            prefix, TECMO_ASSET_PACK_GAMEPLAY_PRETIP_ID,
            TECMO_ASSET_PACK_GAMEPLAY_PRETIP_SIZE,
            TECMO_ASSET_PACK_GAMEPLAY_PRETIP_FNV1A32,
            TECMO_ASSET_PACK_GAMEPLAY_PRETIP_TWAR_SIZE,
            TECMO_ASSET_PACK_GAMEPLAY_PRETIP_TWAR_FNV1A32) != 0) {
        return -1;
    }
    for (index = 0U; index < TECMO_GAMEPLAY_PRETIP_SOURCE_COUNT; ++index) {
        const TecmoGameplayPreTipExpectedSource *source =
            &tecmo_gameplay_pretip_expected_sources[index];
        const char *source_entry =
            source->fixed_bank != 0U ? "prg/fixed" : NULL;
        char bank_entry[16];
        if (source_entry == NULL) {
            (void)snprintf(bank_entry, sizeof(bank_entry),
                           "prg/bank%02u", (unsigned)source->bank);
            source_entry = bank_entry;
        }
        if (tecmo_asset_pack_append_text(
                buffer, capacity, length,
                "%s{\"role\":\"%s\",\"source_entry\":\"%s\","
                "\"source_offset\":%llu,\"bank\":%u,\"fixed_bank\":%s,"
                "\"cpu_address\":%u,\"size\":%u,"
                "\"fingerprint_fnv1a32\":\"%08X\","
                "\"fingerprint_fnv1a64\":\"%016llX\"}",
                index == 0U ? "" : ",", roles[index], source_entry,
                (unsigned long long)provenance->source_offsets[index],
                (unsigned)source->bank,
                source->fixed_bank != 0U ? "true" : "false",
                (unsigned)source->cpu_start, (unsigned)source->byte_count,
                (unsigned)source->fingerprint_fnv1a32,
                (unsigned long long)source->fingerprint_fnv1a64) != 0) {
            return -1;
        }
    }
    return tecmo_asset_pack_append_text(
        buffer, capacity, length,
        "],\"native_contract\":{"
        "\"screen_id\":21,\"closeup_screen_id\":26,"
        "\"wait_frames\":[61,121,61],"
        "\"card_text\":\"Bank06 character mapping plus AF05 2x2 metatiles at ROM 16-pixel cell positions\","
        "\"cancel\":\"Bank06 $A10A-$A124 reads either controller NES B only when route byte $69 bit 0 is set; native preseason clears the gate and regular season sets it\","
        "\"tip_input_source\":{\"source_entry\":\"prg/bank05\","
        "\"source_offset\":%llu,\"bank\":5,\"cpu_start\":39006,"
        "\"cpu_end\":39018,\"size\":13,"
        "\"fingerprint_fnv1a32\":\"423816F1\","
        "\"fingerprint_fnv1a64\":\"032F8A7A4F4439D1\","
        "\"rom_exact\":true,"
        "\"proves\":\"current-level NES B mask $40 is read and latched before the later height/countdown gate; the exact Bank05 $98E1 lda $030C,Y / beq L9920 branch sends nonzero through $04A5,X/L9360/L9354/L9936 and zero to the held-B test at $05,Y & $40, supporting but not proving an automatic/CPU-selector interpretation\","
        "\"does_not_prove\":\"winner settlement, caller/state identity, exact CPU/human or automatic/CPU-selector meaning of $030C,Y, or original CPU timing/decision cadence\"},"
        "\"tip_input\":\"the first 30 native jump-contest updates accept the first held NES B level routed by team; target frame 0, capped error 0..11, no-sample error 12, and tie-away settlement are explicit native approximations; an unassigned team receives one deterministic CPU decision at contest frame 8 inside that visible window as an evidence-informed approximation, not a claim of exact automatic-path timing\","
        "\"winner_query_gate\":\"rejects before jump-contest without mutating caller output; accepted during jump-contest and live\","
        "\"tip_lineup\":\"Bank04 $AC8C-$ACD9 initializes object slots 0..10 from $ADA3/$ADAE/$ADB9; native play consumes the exact ten player coordinates and ball anchor\","
        "\"tip_jumper_selectors\":[4,9],"
        "\"tip_animation\":\"both TPTI-selected actors use the native 30-frame contest-input window and a native 60-update crouch/takeoff/rise/apex/contact/fall/land presentation; generic action poses use actor-facing mirroring, pose and projected altitude are native approximations, while anchors and live handoff remain transactional\","
        "\"phase_order\":[\"preseason\",\"matchup\",\"first-period\",\"closeup\",\"center-setup\",\"ball-descent\",\"toss-closeup\",\"jump-contest\",\"live\"],"
        "\"closeup_motion\":\"Bank04 $88 and fixed $D861 move the sprite player left while nametable scroll moves the other figures right; the 33-frame phase anchor is capture-bounded\","
        "\"toss_cut_in\":\"TGPL-1 screen $1B nametable page 1; page 1 matches ball X 176..239 and hand X 67..159 geometry\","
        "\"ball_descent\":\"Y 71..145 over the first 60 of 120 frames, then held through the phase boundary\","
        "\"clock_rules_controls\":\"frozen until live handoff\","
        "\"music\":\"track 8 at card start; game-music track 5 only at live handoff\","
        "\"winner_policy\":\"native lower-error contest timing drives jump interaction and initial possession; an unassigned team samples at fixed contest frame 8 as an explicit CPU approximation, equal errors choose away deterministically, while exact original winner, claim settlement, the inferred CPU/human selector meaning of the $030C,Y branch, and original timing remain unported\","
        "\"runtime_inputs\":\"TPTI-1 and exact same-pack dependencies only; no ROM, ASM, decompilation, Lua, trace, capture, screenshot, video, log, dump, or save state\"}}",
        (unsigned long long)(
            provenance->source_offsets[
                TECMO_GAMEPLAY_PRETIP_SOURCE_TIP_SETUP - 1U] + 3U));
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
                                   const TecmoPreseasonMenuProvenance *preseason_provenance,
                                   const TecmoAllStarMenuProvenance *all_star_provenance,
                                   const TecmoMusicProvenance *music_provenance,
                                   const TecmoFrontendAudioProvenance *frontend_audio_provenance,
                                   const TecmoGameplayAudioProvenance *gameplay_audio_provenance,
                                   const TecmoTeamDataProvenance *team_data_provenance,
                                   const TecmoTeamManagementProvenance *team_management_provenance,
                                   const TecmoSeasonMenuProvenance *season_provenance,
                                   const TecmoGameplayProvenance *gameplay_provenance,
                                   const TecmoGameplayCourtProvenance *gameplay_court_provenance,
                                   const TecmoGameplayCourtOrientationProvenance *court_orientation_provenance,
                                   const TecmoGameplayBackcourtProvenance *backcourt_provenance,
                                   const TecmoGameplayCameraProvenance *gameplay_camera_provenance,
                                   const TecmoGameplayMovementProvenance *gameplay_movement_provenance,
                                   const TecmoGameplayBallDribbleProvenance *ball_dribble_provenance,
                                   const TecmoGameplayFatigueProvenance *gameplay_fatigue_provenance,
                                   const TecmoGameplayCpuSteeringProvenance *cpu_steering_provenance,
                                   const TecmoGameplayHudProvenance *gameplay_hud_provenance,
                                   const TecmoGameplayCloseShotProvenance *close_shot_provenance,
                                   const TecmoGameplayDunkProvenance *dunk_provenance,
                                   const TecmoGameplayJumpShotProvenance *jump_shot_provenance,
                                   const TecmoGameplayShotResolutionProvenance *shot_resolution_provenance,
    const TecmoGameplayPenaltyProvenance *penalty_provenance,
    const TecmoGameplayViolationRefereeProvenance *violation_referee_provenance,
    const TecmoGameplayFreeThrowLineupProvenance *free_throw_lineup_provenance,
    const TecmoGameplayPreTipProvenance *pretip_provenance,
    size_t *source_map_size_out)
{
    size_t entry_count = (size_t)prg_banks + (size_t)chr_banks + 33U;
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

    if (entry_count >
        (SIZE_MAX - TECMO_ASSET_PACK_SOURCE_MAP_BASE_CAPACITY) / 512U) {
        return NULL;
    }
    capacity = TECMO_ASSET_PACK_SOURCE_MAP_BASE_CAPACITY +
               entry_count * 512U;
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
                                            &first_logical, start_menu_provenance) != 0) ||
        (preseason_provenance->flow_offset != 0U &&
         append_preseason_source_map_entry(source_map, capacity, &length,
                                           &first_logical, preseason_provenance) != 0) ||
        (all_star_provenance->flow_offset != 0U &&
         append_all_star_source_map_entry(source_map, capacity, &length,
                                          &first_logical, all_star_provenance) != 0) ||
        (music_provenance->payload_size != 0U &&
         append_music_source_map_entry(source_map, capacity, &length,
                                       &first_logical, music_provenance) != 0) ||
        (frontend_audio_provenance->payload_size != 0U &&
         append_frontend_audio_source_map_entry(
             source_map, capacity, &length, &first_logical,
             frontend_audio_provenance) != 0) ||
        (gameplay_audio_provenance->sfx_payload_size != 0U &&
         append_gameplay_audio_source_map_entries(
             source_map, capacity, &length, &first_logical,
             gameplay_audio_provenance) != 0) ||
        (team_data_provenance->entry_return_offset != 0U &&
         append_team_data_source_map_entry(source_map, capacity, &length,
                                           &first_logical, team_data_provenance) != 0) ||
        (team_management_provenance->starters_flow_offset != 0U &&
         append_team_management_source_map_entry(
             source_map, capacity, &length, &first_logical,
             team_management_provenance) != 0) ||
        (season_provenance->dispatch_offset != 0U &&
         append_season_source_map_entry(source_map, capacity, &length,
                                        &first_logical, season_provenance) != 0) ||
        (gameplay_provenance->source_offsets[0] != 0U &&
         append_gameplay_source_map_entry(source_map, capacity, &length,
                                          &first_logical,
                                          gameplay_provenance) != 0) ||
        (gameplay_court_provenance->source_offsets[0] != 0U &&
         append_gameplay_court_source_map_entry(
             source_map, capacity, &length, &first_logical,
             gameplay_court_provenance) != 0) ||
        (court_orientation_provenance->source_offsets[0] != 0U &&
         append_gameplay_court_orientation_source_map_entry(
             source_map, capacity, &length, &first_logical,
             court_orientation_provenance) != 0) ||
        (backcourt_provenance->source_offsets[0] != 0U &&
         append_gameplay_backcourt_source_map_entry(
             source_map, capacity, &length, &first_logical,
             backcourt_provenance) != 0) ||
        (gameplay_camera_provenance->source_offsets[0] != 0U &&
         append_gameplay_camera_source_map_entry(
             source_map, capacity, &length, &first_logical,
             gameplay_camera_provenance) != 0) ||
        (gameplay_movement_provenance->source_offsets[0] != 0U &&
         append_gameplay_movement_source_map_entry(
             source_map, capacity, &length, &first_logical,
             gameplay_movement_provenance) != 0) ||
        (ball_dribble_provenance->source_offsets[0] != 0U &&
         append_gameplay_ball_dribble_source_map_entry(
             source_map, capacity, &length, &first_logical,
             ball_dribble_provenance) != 0) ||
        (gameplay_fatigue_provenance->source_offsets[0] != 0U &&
         append_gameplay_fatigue_source_map_entry(
             source_map, capacity, &length, &first_logical,
             gameplay_fatigue_provenance) != 0) ||
        (cpu_steering_provenance->source_offsets[0] != 0U &&
         append_gameplay_cpu_steering_source_map_entry(
             source_map, capacity, &length, &first_logical,
             cpu_steering_provenance) != 0) ||
        (gameplay_hud_provenance->source_offsets[0] != 0U &&
         append_gameplay_hud_source_map_entry(
             source_map, capacity, &length, &first_logical,
             gameplay_hud_provenance) != 0) ||
        (close_shot_provenance->source_offsets[0] != 0U &&
         append_gameplay_close_shot_source_map_entry(
             source_map, capacity, &length, &first_logical,
             close_shot_provenance) != 0) ||
        (dunk_provenance->source_offsets[0] != 0U &&
         append_gameplay_dunk_source_map_entry(
             source_map, capacity, &length, &first_logical,
             dunk_provenance) != 0) ||
        (jump_shot_provenance->source_offsets[0] != 0U &&
         append_gameplay_jump_shot_source_map_entry(
             source_map, capacity, &length, &first_logical,
             jump_shot_provenance) != 0) ||
        (shot_resolution_provenance->source_offsets[0] != 0U &&
         append_gameplay_shot_resolution_source_map_entry(
             source_map, capacity, &length, &first_logical,
             shot_resolution_provenance) != 0) ||
        (penalty_provenance->source_offsets[0] != 0U &&
         append_gameplay_penalty_source_map_entry(
             source_map, capacity, &length, &first_logical,
             penalty_provenance) != 0) ||
        (violation_referee_provenance->source_offsets[0] != 0U &&
         append_gameplay_violation_referee_source_map_entry(
             source_map, capacity, &length, &first_logical,
             violation_referee_provenance) != 0) ||
        (free_throw_lineup_provenance->source_offsets[0] != 0U &&
         append_gameplay_free_throw_lineup_source_map_entry(
             source_map, capacity, &length, &first_logical,
             free_throw_lineup_provenance) != 0) ||
        (pretip_provenance->source_offsets[0] != 0U &&
         append_gameplay_pretip_source_map_entry(
             source_map, capacity, &length, &first_logical,
             pretip_provenance) != 0)) {
        free(source_map);
        return NULL;
    }

    if (tecmo_asset_pack_append_text(source_map,
                    capacity,
                    &length,
                    "\n"
                    "  ],\n"
                    "  \"input_contract\":\"ines-only\",\n"
                    "  \"logical_entry_note\":\"ROM-only pack with sanitized native scene, menu, music, SFX, and DMC entries; no decomp, capture, or loose-file entries are imported\"\n"
                    "}\n") != 0) {
        free(source_map);
        return NULL;
    }

    *source_map_size_out = length;
    return source_map;
}
