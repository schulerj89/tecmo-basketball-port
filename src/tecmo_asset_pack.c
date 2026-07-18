#define _CRT_SECURE_NO_WARNINGS

#include "tecmo_asset_pack.h"

#include "asset_pack/tecmo_asset_pack_arena.h"
#include "asset_pack/tecmo_asset_pack_d9f6.h"
#include "asset_pack/tecmo_asset_pack_finale.h"
#include "asset_pack/tecmo_asset_pack_import_layout.h"
#include "asset_pack/tecmo_asset_pack_opening.h"
#include "asset_pack/tecmo_asset_pack_post_arena.h"
#include "asset_pack/tecmo_asset_pack_source_map.h"
#include "asset_pack/tecmo_asset_pack_start_menu.h"
#include "asset_pack/tecmo_asset_pack_title.h"
#include "asset_pack/tecmo_asset_pack_util.h"
#include "asset_pack/tecmo_asset_pack_writer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#endif


static uint64_t prg_bank_cpu_source_offset(uint64_t prg_offset,
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


static int add_native_arena_intro_entries(TecmoAssetPackBuilder *builder,
                                          const uint8_t *rom,
                                          uint64_t rom_size,
                                          uint64_t prg_offset,
                                          uint32_t prg_banks,
                                          uint64_t chr_offset,
                                          uint64_t chr_size,
                                          int enforce_finale_revision_fingerprints,
                                          TecmoOpeningScreenProvenance opening_provenance[2],
                                          TecmoArenaBackgroundProvenance *background_provenance,
                                          TecmoArenaSpriteGroupsProvenance *sprite_groups_provenance,
                                          TecmoPostArenaProvenance *post_arena_provenance,
                                          TecmoFinaleProvenance *finale_provenance,
                                          TecmoTitleProvenance title_provenance[2],
                                          TecmoStartGameMenuProvenance *start_menu_provenance,
                                          char *message,
                                          size_t message_size)
{
    uint8_t opening_payload[TECMO_ASSET_PACK_PRESENTS_SIZE];
    char script_payload[2048];
    uint8_t background_payload[TECMO_ASSET_PACK_ARENA_LAYER_SIZE];
    char palette_payload[2048];
    uint8_t sprite_groups_payload[TECMO_ASSET_PACK_ARENA_SPRITE_GROUPS_SIZE];
    uint8_t ready_payload[TECMO_ASSET_PACK_READY_SIZE];
    uint8_t warriors_payload[TECMO_ASSET_PACK_WARRIORS_SIZE];
    uint8_t clippers_payload[TECMO_ASSET_PACK_CLIPPERS_SIZE];
    uint8_t bucks_payload[TECMO_ASSET_PACK_BUCKS_SIZE];
    uint8_t pass_payload[TECMO_ASSET_PACK_PASS_SIZE];
    uint8_t finale_payload[TECMO_ASSET_PACK_FINALE_SIZE];
    uint8_t title_attract_payload[TECMO_ASSET_PACK_TITLE_ATTRACT_SIZE];
    uint8_t title_screen_payload[TECMO_ASSET_PACK_TITLE_SCREEN_SIZE];
    uint8_t start_menu_payload[TECMO_ASSET_PACK_START_MENU_SIZE];
    uint64_t script_source_offset =
        prg_bank_cpu_source_offset(prg_offset,
                                   prg_banks,
                                   TECMO_ASSET_PACK_ARENA_BANK04,
                                   TECMO_ASSET_PACK_ARENA_ROUTE_CPU);
    uint64_t palette_source_offset =
        prg_bank_cpu_source_offset(prg_offset,
                                   prg_banks,
                                   TECMO_ASSET_PACK_ARENA_BANK04,
                                   TECMO_ASSET_PACK_ARENA_PALETTE_CPU);
    uint32_t source_bank =
        prg_banks > TECMO_ASSET_PACK_ARENA_BANK04
            ? TECMO_ASSET_PACK_ARENA_BANK04
            : prg_banks - 1U;
    int bank04_available = prg_banks > TECMO_ASSET_PACK_ARENA_BANK04;
    int payload_length;
    TecmoAssetPackEntryInfo entry_info;

    if (tecmo_asset_pack_build_opening_screen(rom,
                             rom_size,
                             prg_offset,
                             prg_banks,
                             chr_size,
                             0U,
                             enforce_finale_revision_fingerprints,
                             opening_payload,
                             TECMO_ASSET_PACK_PRESENTS_SIZE,
                             &opening_provenance[0],
                             message,
                             message_size) != 0) {
        return -1;
    }
    entry_info = tecmo_asset_pack_make_entry_info(TECMO_ASSET_PACK_PRESENTS_ID,
                                 TECMO_ASSET_PACK_TYPE_DATA,
                                 0U,
                                 TECMO_ASSET_PACK_PRESENTS_STREAM_CPU,
                                 opening_provenance[0].stream_offset,
                                 TECMO_ASSET_PACK_FLAG_DERIVED | TECMO_ASSET_PACK_FLAG_LOCAL);
    if (tecmo_asset_pack_builder_add_memory(builder,
                                            &entry_info,
                                            opening_payload,
                                            TECMO_ASSET_PACK_PRESENTS_SIZE,
                                            message,
                                            message_size) != 0) {
        tecmo_asset_pack_set_message(message, message_size, "Could not write native TECMO presents entry.");
        return -1;
    }

    if (tecmo_asset_pack_build_opening_screen(rom,
                             rom_size,
                             prg_offset,
                             prg_banks,
                             chr_size,
                             1U,
                             enforce_finale_revision_fingerprints,
                             opening_payload,
                             TECMO_ASSET_PACK_LICENSE_SIZE,
                             &opening_provenance[1],
                             message,
                             message_size) != 0) {
        return -1;
    }
    entry_info = tecmo_asset_pack_make_entry_info(TECMO_ASSET_PACK_LICENSE_ID,
                                 TECMO_ASSET_PACK_TYPE_DATA,
                                 0U,
                                 TECMO_ASSET_PACK_LICENSE_STREAM_CPU,
                                 opening_provenance[1].stream_offset,
                                 TECMO_ASSET_PACK_FLAG_DERIVED | TECMO_ASSET_PACK_FLAG_LOCAL);
    if (tecmo_asset_pack_builder_add_memory(builder,
                                            &entry_info,
                                            opening_payload,
                                            TECMO_ASSET_PACK_LICENSE_SIZE,
                                            message,
                                            message_size) != 0) {
        tecmo_asset_pack_set_message(message, message_size, "Could not write native NBA license entry.");
        return -1;
    }

    payload_length = snprintf(script_payload,
                              sizeof(script_payload),
                              "{\n"
                              "  \"format\":\"tecmo.arena-intro.script/1\",\n"
                              "  \"input_contract\":\"ines-only\",\n"
                              "  \"source_route\":\"bank04:C-0127\",\n"
                              "  \"source_bank_available\":%s,\n"
                              "  \"runtime_shape\":\"native-scene-script\",\n"
                              "  \"phases\":[\"enter\",\"pan_to_goal\",\"hold_goal\",\"handoff\"],\n"
                              "  \"camera\":{\"viewport\":[256,240],\"start\":[0,0],\"end\":[40,72],\"pan_frames\":96},\n"
                              "  \"timeline\":[\n"
                              "    {\"op\":\"set_phase\",\"phase\":\"enter\",\"frame\":0},\n"
                              "    {\"op\":\"move_camera\",\"phase\":\"pan_to_goal\",\"duration_frames\":96},\n"
                              "    {\"op\":\"set_phase\",\"phase\":\"hold_goal\",\"frame\":96},\n"
                              "    {\"op\":\"handoff\",\"phase\":\"handoff\",\"frame\":192,\"target\":\"arena/intro/ready-screen\"}\n"
                              "  ]\n"
                              "}\n",
                              bank04_available ? "true" : "false");
    if (payload_length < 0 || (size_t)payload_length >= sizeof(script_payload)) {
        tecmo_asset_pack_set_message(message, message_size, "Could not build arena intro script entry.");
        return -1;
    }

    entry_info = tecmo_asset_pack_make_entry_info(TECMO_ASSET_PACK_ARENA_INTRO_SCRIPT_ID,
                                 TECMO_ASSET_PACK_TYPE_DATA,
                                 source_bank,
                                 TECMO_ASSET_PACK_ARENA_ROUTE_CPU,
                                 script_source_offset,
                                 TECMO_ASSET_PACK_FLAG_DERIVED | TECMO_ASSET_PACK_FLAG_LOCAL);
    if (tecmo_asset_pack_builder_add_memory(builder,
                                            &entry_info,
                                            script_payload,
                                            (uint64_t)payload_length,
                                            message,
                                            message_size) != 0) {
        tecmo_asset_pack_set_message(message, message_size, "Could not write arena intro script entry.");
        return -1;
    }

    if (tecmo_asset_pack_build_arena_background_layer(rom,
                                     rom_size,
                                     prg_offset,
                                     prg_banks,
                                     chr_size,
                                     background_payload,
                                     sizeof(background_payload),
                                     background_provenance,
                                     message,
                                     message_size) != 0) {
        return -1;
    }

    entry_info = tecmo_asset_pack_make_entry_info(TECMO_ASSET_PACK_ARENA_INTRO_BACKGROUND_ID,
                                 TECMO_ASSET_PACK_TYPE_DATA,
                                 background_provenance->stream_bank,
                                 background_provenance->stream_cpu,
                                 background_provenance->stream_source_offset,
                                 TECMO_ASSET_PACK_FLAG_DERIVED | TECMO_ASSET_PACK_FLAG_LOCAL);
    if (tecmo_asset_pack_builder_add_memory(builder,
                                            &entry_info,
                                            background_payload,
                                            sizeof(background_payload),
                                            message,
                                            message_size) != 0) {
        tecmo_asset_pack_set_message(message, message_size, "Could not write arena background layer entry.");
        return -1;
    }

    payload_length = snprintf(palette_payload,
                              sizeof(palette_payload),
                              "{\n"
                              "  \"format\":\"tecmo.arena-intro.palette-cycle/1\",\n"
                              "  \"input_contract\":\"ines-only\",\n"
                              "  \"source_route\":\"bank04:C-0132\",\n"
                              "  \"source_bank_available\":%s,\n"
                              "  \"runtime_shape\":\"native-palette-cycle\",\n"
                              "  \"source_snapshot_cpu\":%u,\n"
                              "  \"fixed_helper\":\"C05A/D700 setup snapshot copy\",\n"
                              "  \"work_ranges\":{\"full\":\"033E-034D\",\"low_nibbles\":\"031E-032D\"},\n"
                              "  \"stages\":[\n"
                              "    {\"name\":\"setup\",\"frame\":0,\"mode\":\"copy_rom_snapshot\"},\n"
                              "    {\"name\":\"fade_step\",\"source\":\"bank04:L88A9\",\"mode\":\"subtract_clamped\"}\n"
                              "  ],\n"
                              "  \"palette_state\":\"extractor-populated-runtime-state-pending\"\n"
                              "}\n",
                              bank04_available ? "true" : "false",
                              TECMO_ASSET_PACK_ARENA_PALETTE_CPU);
    if (payload_length < 0 || (size_t)payload_length >= sizeof(palette_payload)) {
        tecmo_asset_pack_set_message(message, message_size, "Could not build arena palette cycle entry.");
        return -1;
    }

    entry_info = tecmo_asset_pack_make_entry_info(TECMO_ASSET_PACK_ARENA_INTRO_PALETTE_ID,
                                 TECMO_ASSET_PACK_TYPE_DATA,
                                 source_bank,
                                 TECMO_ASSET_PACK_ARENA_PALETTE_CPU,
                                 palette_source_offset,
                                 TECMO_ASSET_PACK_FLAG_DERIVED | TECMO_ASSET_PACK_FLAG_LOCAL);
    if (tecmo_asset_pack_builder_add_memory(builder,
                                            &entry_info,
                                            palette_payload,
                                            (uint64_t)payload_length,
                                            message,
                                            message_size) != 0) {
        tecmo_asset_pack_set_message(message, message_size, "Could not write arena palette cycle entry.");
        return -1;
    }

    if (tecmo_asset_pack_build_arena_sprite_groups(rom,
                                  rom_size,
                                  prg_offset,
                                  prg_banks,
                                  chr_offset,
                                  chr_size,
                                  sprite_groups_payload,
                                  sizeof(sprite_groups_payload),
                                  sprite_groups_provenance,
                                  message,
                                  message_size) != 0) {
        return -1;
    }

    entry_info = tecmo_asset_pack_make_entry_info(TECMO_ASSET_PACK_ARENA_INTRO_SPRITE_GROUPS_ID,
                                 TECMO_ASSET_PACK_TYPE_DATA,
                                 0U,
                                 TECMO_ASSET_PACK_ARENA_POINTER_TABLE_CPU,
                                 sprite_groups_provenance->pointer_table_source_offset,
                                 TECMO_ASSET_PACK_FLAG_DERIVED | TECMO_ASSET_PACK_FLAG_LOCAL);
    if (tecmo_asset_pack_builder_add_memory(builder,
                                            &entry_info,
                                            sprite_groups_payload,
                                            sizeof(sprite_groups_payload),
                                            message,
                                            message_size) != 0) {
        tecmo_asset_pack_set_message(message, message_size, "Could not write arena sprite groups entry.");
        return -1;
    }

    if (tecmo_asset_pack_build_ready_screen(rom,
                           rom_size,
                           prg_offset,
                           prg_banks,
                           chr_size,
                           ready_payload,
                           post_arena_provenance,
                           message,
                           message_size) != 0) {
        return -1;
    }
    entry_info = tecmo_asset_pack_make_entry_info(TECMO_ASSET_PACK_READY_ID,
                                 TECMO_ASSET_PACK_TYPE_DATA,
                                 4U,
                                 TECMO_ASSET_PACK_READY_ROUTE_CPU,
                                 post_arena_provenance->ready_route_offset,
                                 TECMO_ASSET_PACK_FLAG_DERIVED | TECMO_ASSET_PACK_FLAG_LOCAL);
    if (tecmo_asset_pack_builder_add_memory(builder,
                                            &entry_info,
                                            ready_payload,
                                            sizeof(ready_payload),
                                            message,
                                            message_size) != 0) {
        tecmo_asset_pack_set_message(message, message_size, "Could not write native READY entry.");
        return -1;
    }

    if (tecmo_asset_pack_build_warriors_transition(rom,
                                  rom_size,
                                  prg_offset,
                                  prg_banks,
                                  chr_size,
                                  warriors_payload,
                                  post_arena_provenance,
                                  message,
                                  message_size) != 0) {
        return -1;
    }
    entry_info = tecmo_asset_pack_make_entry_info(TECMO_ASSET_PACK_WARRIORS_ID,
                                 TECMO_ASSET_PACK_TYPE_DATA,
                                 4U,
                                 TECMO_ASSET_PACK_WARRIORS_ROUTE_CPU,
                                 post_arena_provenance->warriors_route_offset,
                                 TECMO_ASSET_PACK_FLAG_DERIVED | TECMO_ASSET_PACK_FLAG_LOCAL);
    if (tecmo_asset_pack_builder_add_memory(builder,
                                            &entry_info,
                                            warriors_payload,
                                            sizeof(warriors_payload),
                                            message,
                                            message_size) != 0) {
        tecmo_asset_pack_set_message(message, message_size, "Could not write native WARRIORS entry.");
        return -1;
    }

    if (tecmo_asset_pack_build_clippers_transition(rom,
                                  rom_size,
                                  prg_offset,
                                  prg_banks,
                                  chr_size,
                                  clippers_payload,
                                  post_arena_provenance,
                                  message,
                                  message_size) != 0) {
        return -1;
    }
    entry_info = tecmo_asset_pack_make_entry_info(TECMO_ASSET_PACK_CLIPPERS_ID,
                                 TECMO_ASSET_PACK_TYPE_DATA,
                                 4U,
                                 TECMO_ASSET_PACK_CLIPPERS_ROUTE_CPU,
                                 post_arena_provenance->clippers_route_offset,
                                 TECMO_ASSET_PACK_FLAG_DERIVED | TECMO_ASSET_PACK_FLAG_LOCAL);
    if (tecmo_asset_pack_builder_add_memory(builder,
                                            &entry_info,
                                            clippers_payload,
                                            sizeof(clippers_payload),
                                            message,
                                            message_size) != 0) {
        tecmo_asset_pack_set_message(message, message_size, "Could not write native CLIPPERS entry.");
        return -1;
    }

    if (tecmo_asset_pack_build_bucks_transition(rom,
                               rom_size,
                               prg_offset,
                               prg_banks,
                               chr_size,
                               bucks_payload,
                               post_arena_provenance,
                               message,
                               message_size) != 0) {
        return -1;
    }
    entry_info = tecmo_asset_pack_make_entry_info(TECMO_ASSET_PACK_BUCKS_ID,
                                 TECMO_ASSET_PACK_TYPE_DATA,
                                 4U,
                                 TECMO_ASSET_PACK_BUCKS_ROUTE_CPU,
                                 post_arena_provenance->bucks_route_offset,
                                 TECMO_ASSET_PACK_FLAG_DERIVED | TECMO_ASSET_PACK_FLAG_LOCAL);
    if (tecmo_asset_pack_builder_add_memory(builder,
                                            &entry_info,
                                            bucks_payload,
                                            sizeof(bucks_payload),
                                            message,
                                            message_size) != 0) {
        tecmo_asset_pack_set_message(message, message_size, "Could not write native BUCKS entry.");
        return -1;
    }

    if (tecmo_asset_pack_build_pass_transition(rom,
                              rom_size,
                              prg_offset,
                              prg_banks,
                              chr_size,
                              pass_payload,
                              post_arena_provenance,
                              message,
                              message_size) != 0) {
        return -1;
    }
    entry_info = tecmo_asset_pack_make_entry_info(TECMO_ASSET_PACK_PASS_ID,
                                 TECMO_ASSET_PACK_TYPE_DATA,
                                 4U,
                                 TECMO_ASSET_PACK_PASS_ROUTE_CPU,
                                 post_arena_provenance->pass_route_offset,
                                 TECMO_ASSET_PACK_FLAG_DERIVED | TECMO_ASSET_PACK_FLAG_LOCAL);
    if (tecmo_asset_pack_builder_add_memory(builder,
                                            &entry_info,
                                            pass_payload,
                                            sizeof(pass_payload),
                                            message,
                                            message_size) != 0) {
        tecmo_asset_pack_set_message(message, message_size, "Could not write native PASS entry.");
        return -1;
    }

    if (tecmo_asset_pack_build_finale_sequence(rom,
                              rom_size,
                              prg_offset,
                              prg_banks,
                              chr_size,
                              enforce_finale_revision_fingerprints,
                              finale_payload,
                              finale_provenance,
                              message,
                              message_size) != 0) {
        return -1;
    }
    entry_info = tecmo_asset_pack_make_entry_info(TECMO_ASSET_PACK_FINALE_ID,
                                 TECMO_ASSET_PACK_TYPE_DATA,
                                 4U,
                                 0x82CFU,
                                 finale_provenance->dispatch_offset,
                                 TECMO_ASSET_PACK_FLAG_DERIVED | TECMO_ASSET_PACK_FLAG_LOCAL);
    if (tecmo_asset_pack_builder_add_memory(builder,
                                            &entry_info,
                                            finale_payload,
                                            sizeof(finale_payload),
                                            message,
                                            message_size) != 0) {
        tecmo_asset_pack_set_message(message, message_size, "Could not write native intro finale entry.");
        return -1;
    }

    if (enforce_finale_revision_fingerprints != 0) {
    if (tecmo_asset_pack_build_title_asset(rom, rom_size, prg_offset, prg_banks,
                                           chr_size, 0U,
                                           enforce_finale_revision_fingerprints,
                                           title_attract_payload,
                                           sizeof(title_attract_payload),
                                           &title_provenance[0], message,
                                           message_size) != 0) return -1;
    entry_info = tecmo_asset_pack_make_entry_info(TECMO_ASSET_PACK_TITLE_ATTRACT_ID,
                                 TECMO_ASSET_PACK_TYPE_DATA, 0U,
                                 TECMO_ASSET_PACK_TITLE_ATTRACT_STREAM_CPU,
                                 title_provenance[0].stream_offset,
                                 TECMO_ASSET_PACK_FLAG_DERIVED | TECMO_ASSET_PACK_FLAG_LOCAL);
    if (tecmo_asset_pack_builder_add_memory(builder, &entry_info,
                                            title_attract_payload,
                                            sizeof(title_attract_payload),
                                            message, message_size) != 0) {
        tecmo_asset_pack_set_message(message, message_size, "Could not write native title attract entry.");
        return -1;
    }
    if (tecmo_asset_pack_build_title_asset(rom, rom_size, prg_offset, prg_banks,
                                           chr_size, 1U,
                                           enforce_finale_revision_fingerprints,
                                           title_screen_payload,
                                           sizeof(title_screen_payload),
                                           &title_provenance[1], message,
                                           message_size) != 0) return -1;
    entry_info = tecmo_asset_pack_make_entry_info(TECMO_ASSET_PACK_TITLE_SCREEN_ID,
                                 TECMO_ASSET_PACK_TYPE_DATA, 0U,
                                 TECMO_ASSET_PACK_TITLE_START_STREAM_CPU,
                                 title_provenance[1].stream_offset,
                                 TECMO_ASSET_PACK_FLAG_DERIVED | TECMO_ASSET_PACK_FLAG_LOCAL);
    if (tecmo_asset_pack_builder_add_memory(builder, &entry_info,
                                            title_screen_payload,
                                            sizeof(title_screen_payload),
                                            message, message_size) != 0) {
        tecmo_asset_pack_set_message(message, message_size, "Could not write native START GAME title entry.");
        return -1;
    }
    if (tecmo_asset_pack_build_start_game_menu(rom, rom_size, prg_offset,
                                               prg_banks, chr_size,
                                               enforce_finale_revision_fingerprints,
                                               start_menu_payload,
                                               sizeof(start_menu_payload),
                                               start_menu_provenance,
                                               message, message_size) != 0) return -1;
    entry_info = tecmo_asset_pack_make_entry_info(TECMO_ASSET_PACK_START_GAME_MENU_ID,
                                 TECMO_ASSET_PACK_TYPE_DATA, 0U,
                                 TECMO_ASSET_PACK_START_MENU_STREAM_CPU,
                                 start_menu_provenance->stream_offset,
                                 TECMO_ASSET_PACK_FLAG_DERIVED | TECMO_ASSET_PACK_FLAG_LOCAL);
    if (tecmo_asset_pack_builder_add_memory(builder, &entry_info,
                                            start_menu_payload,
                                            sizeof(start_menu_payload),
                                            message, message_size) != 0) {
        tecmo_asset_pack_set_message(message, message_size, "Could not write native start-game menu entry.");
        return -1;
    }
    }

    return 0;
}

static int tecmo_asset_pack_build_from_ines_internal(
    const char *rom_path,
    const char *out_path,
    int enforce_finale_revision_fingerprints,
    char *message,
    size_t message_size)
{
    uint8_t *rom = NULL;
    uint64_t rom_size = 0;
    uint64_t prg_offset;
    uint64_t prg_size;
    uint64_t chr_offset;
    uint64_t chr_size;
    uint32_t prg_banks;
    uint32_t chr_banks;
    uint32_t mapper;
    uint32_t trainer_bytes;
    TecmoAssetPackBuilder *builder = NULL;
    TecmoAssetPackEntryInfo entry_info;
    char manifest[512];
    char *source_map = NULL;
    size_t source_map_size = 0U;
    TecmoOpeningScreenProvenance opening_provenance[2];
    TecmoArenaBackgroundProvenance background_provenance;
    TecmoArenaSpriteGroupsProvenance sprite_groups_provenance;
    TecmoPostArenaProvenance post_arena_provenance;
    TecmoFinaleProvenance finale_provenance;
    TecmoTitleProvenance title_provenance[2];
    TecmoStartGameMenuProvenance start_menu_provenance;
    int manifest_length;
    int result = -1;

    if (rom_path == NULL || out_path == NULL) {
        tecmo_asset_pack_set_message(message, message_size, "ROM path and output path are required.");
        return -1;
    }
    if (tecmo_asset_pack_read_file(rom_path, &rom, &rom_size) != 0) {
        tecmo_asset_pack_set_message(message, message_size, "Could not read iNES ROM.");
        return -1;
    }
    if (rom_size < 16U ||
        rom[0] != 'N' ||
        rom[1] != 'E' ||
        rom[2] != 'S' ||
        rom[3] != 0x1AU) {
        tecmo_asset_pack_set_message(message, message_size, "Input is not an iNES ROM.");
        goto cleanup;
    }

    prg_banks = rom[4];
    chr_banks = rom[5];
    mapper = (uint32_t)(rom[6] >> 4U) | ((uint32_t)rom[7] & 0xF0U);
    trainer_bytes = (rom[6] & 0x04U) != 0U ? 512U : 0U;
    prg_offset = 16ULL + (uint64_t)trainer_bytes;
    prg_size = (uint64_t)prg_banks * TECMO_ASSET_PACK_PRG_BANK_BYTES;
    chr_offset = prg_offset + prg_size;
    chr_size = (uint64_t)chr_banks * TECMO_ASSET_PACK_CHR_BANK_BYTES;

    if (prg_banks == 0U || rom_size < chr_offset + chr_size) {
        tecmo_asset_pack_set_message(message, message_size, "ROM is shorter than its iNES PRG/CHR sizes.");
        goto cleanup;
    }

    if (tecmo_asset_pack_builder_begin(&builder, out_path, message, message_size) != 0) {
        goto cleanup;
    }

    manifest_length = snprintf(manifest,
                               sizeof(manifest),
                               "format=tecmo.assetpack/1\n"
                               "source=ines\n"
                               "source_map=system/source-map\n"
                               "mapper=%u\n"
                               "trainer_bytes=%u\n"
                               "prg_banks_16k=%u\n"
                               "chr_banks_8k=%u\n"
                               "raw_entry_prefixes=prg/,chr/\n"
                               "logical_entry_prefixes=arena/intro/,intro/,title/,menu/\n"
                               "input_contract=ines-only\n",
                               mapper,
                               trainer_bytes,
                               prg_banks,
                               chr_banks);
    if (manifest_length < 0 || (size_t)manifest_length >= sizeof(manifest)) {
        tecmo_asset_pack_set_message(message, message_size, "Could not build asset pack manifest.");
        goto cleanup;
    }

    entry_info = tecmo_asset_pack_make_entry_info("system/manifest",
                                 TECMO_ASSET_PACK_TYPE_META,
                                 0U,
                                 0U,
                                 0U,
                                 TECMO_ASSET_PACK_FLAG_DERIVED | TECMO_ASSET_PACK_FLAG_LOCAL);
    if (tecmo_asset_pack_builder_add_memory(builder,
                                            &entry_info,
                                            manifest,
                                            (uint64_t)manifest_length,
                                            message,
                                            message_size) != 0) {
        tecmo_asset_pack_set_message(message, message_size, "Could not write asset pack manifest.");
        goto cleanup;
    }

    for (uint32_t bank = 0; bank < prg_banks; ++bank) {
        char id[TECMO_ASSET_PACK_ID_SIZE];
        uint64_t offset = prg_offset + (uint64_t)bank * TECMO_ASSET_PACK_PRG_BANK_BYTES;

        (void)snprintf(id, sizeof(id), "prg/bank%02u", bank);
        entry_info = tecmo_asset_pack_make_entry_info(id,
                                     TECMO_ASSET_PACK_TYPE_PRG,
                                     bank,
                                     0x8000U,
                                     offset,
                                     TECMO_ASSET_PACK_FLAG_RAW_ROM | TECMO_ASSET_PACK_FLAG_LOCAL);
        if (tecmo_asset_pack_builder_add_memory(builder,
                                                &entry_info,
                                                rom + (size_t)offset,
                                                TECMO_ASSET_PACK_PRG_BANK_BYTES,
                                                message,
                                                message_size) != 0) {
            tecmo_asset_pack_set_message(message, message_size, "Could not write PRG bank asset.");
            goto cleanup;
        }
    }

    {
        uint32_t fixed_bank = prg_banks - 1U;
        uint64_t offset = prg_offset + (uint64_t)fixed_bank * TECMO_ASSET_PACK_PRG_BANK_BYTES;

        entry_info = tecmo_asset_pack_make_entry_info("prg/fixed",
                                     TECMO_ASSET_PACK_TYPE_PRG,
                                     fixed_bank,
                                     0xC000U,
                                     offset,
                                     TECMO_ASSET_PACK_FLAG_RAW_ROM | TECMO_ASSET_PACK_FLAG_LOCAL);
        if (tecmo_asset_pack_builder_add_memory(builder,
                                                &entry_info,
                                                rom + (size_t)offset,
                                                TECMO_ASSET_PACK_PRG_BANK_BYTES,
                                                message,
                                                message_size) != 0) {
            tecmo_asset_pack_set_message(message, message_size, "Could not write fixed PRG bank asset.");
            goto cleanup;
        }
    }

    if (chr_size > 0U) {
        entry_info = tecmo_asset_pack_make_entry_info("chr/all",
                                     TECMO_ASSET_PACK_TYPE_CHR,
                                     0U,
                                     0U,
                                     chr_offset,
                                     TECMO_ASSET_PACK_FLAG_RAW_ROM | TECMO_ASSET_PACK_FLAG_LOCAL);
        if (tecmo_asset_pack_builder_add_memory(builder,
                                                &entry_info,
                                                rom + (size_t)chr_offset,
                                                chr_size,
                                                message,
                                                message_size) != 0) {
            tecmo_asset_pack_set_message(message, message_size, "Could not write CHR asset.");
            goto cleanup;
        }

        for (uint32_t bank = 0; bank < chr_banks; ++bank) {
            char id[TECMO_ASSET_PACK_ID_SIZE];
            uint64_t offset = chr_offset + (uint64_t)bank * TECMO_ASSET_PACK_CHR_BANK_BYTES;

            (void)snprintf(id, sizeof(id), "chr/bank%02u", bank);
            entry_info = tecmo_asset_pack_make_entry_info(id,
                                         TECMO_ASSET_PACK_TYPE_CHR,
                                         bank,
                                         0U,
                                         offset,
                                         TECMO_ASSET_PACK_FLAG_RAW_ROM | TECMO_ASSET_PACK_FLAG_LOCAL);
            if (tecmo_asset_pack_builder_add_memory(builder,
                                                    &entry_info,
                                                    rom + (size_t)offset,
                                                    TECMO_ASSET_PACK_CHR_BANK_BYTES,
                                                    message,
                                                    message_size) != 0) {
                tecmo_asset_pack_set_message(message, message_size, "Could not write CHR bank asset.");
                goto cleanup;
            }
        }
    }

    memset(opening_provenance, 0, sizeof(opening_provenance));
    memset(&background_provenance, 0, sizeof(background_provenance));
    memset(&sprite_groups_provenance, 0, sizeof(sprite_groups_provenance));
    memset(&post_arena_provenance, 0, sizeof(post_arena_provenance));
    memset(title_provenance, 0, sizeof(title_provenance));
    memset(&start_menu_provenance, 0, sizeof(start_menu_provenance));
    if (add_native_arena_intro_entries(builder,
                                       rom,
                                       rom_size,
                                       prg_offset,
                                       prg_banks,
                                       chr_offset,
                                       chr_size,
                                       enforce_finale_revision_fingerprints,
                                       opening_provenance,
                                       &background_provenance,
                                       &sprite_groups_provenance,
                                       &post_arena_provenance,
                                       &finale_provenance,
                                       title_provenance,
                                       &start_menu_provenance,
                                       message,
                                       message_size) != 0) {
        goto cleanup;
    }

    source_map = tecmo_asset_pack_build_ines_source_map(mapper,
                                       trainer_bytes,
                                       prg_banks,
                                       chr_banks,
                                       prg_offset,
                                       chr_offset,
                                       chr_size,
                                       opening_provenance,
                                       &background_provenance,
                                       &sprite_groups_provenance,
                                       &post_arena_provenance,
                                       &finale_provenance,
                                       title_provenance,
                                       &start_menu_provenance,
                                       &source_map_size);
    if (source_map == NULL) {
        tecmo_asset_pack_set_message(message, message_size, "Could not build asset pack source map.");
        goto cleanup;
    }

    entry_info = tecmo_asset_pack_make_entry_info("system/source-map",
                                 TECMO_ASSET_PACK_TYPE_META,
                                 0U,
                                 0U,
                                 0U,
                                 TECMO_ASSET_PACK_FLAG_DERIVED | TECMO_ASSET_PACK_FLAG_LOCAL);
    if (tecmo_asset_pack_builder_add_memory(builder,
                                            &entry_info,
                                            source_map,
                                            (uint64_t)source_map_size,
                                            message,
                                            message_size) != 0) {
        tecmo_asset_pack_set_message(message, message_size, "Could not write asset pack source map.");
        goto cleanup;
    }
    free(source_map);
    source_map = NULL;

    {
        size_t entry_count = tecmo_asset_pack_builder_entry_count(builder);
        int finish_result = tecmo_asset_pack_builder_finish(builder, message, message_size);

        builder = NULL;
        if (finish_result != 0) {
            goto cleanup;
        }
        if (message != NULL && message_size > 0U) {
            (void)snprintf(message,
                           message_size,
                           "Wrote %u PRG banks, %u CHR banks, %llu entries to %s from iNES ROM",
                           prg_banks,
                           chr_banks,
                           (unsigned long long)entry_count,
                           out_path);
        }
    }
    result = 0;

cleanup:
    if (builder != NULL) {
        tecmo_asset_pack_builder_cancel(builder);
    }
    free(source_map);
    free(rom);
    return result;
}

int tecmo_asset_pack_build_from_ines(const char *rom_path,
                                     const char *out_path,
                                     char *message,
                                     size_t message_size)
{
    return tecmo_asset_pack_build_from_ines_internal(rom_path,
                                                      out_path,
                                                      1,
                                                      message,
                                                      message_size);
}

static int make_self_test_temp_dir(char *path, size_t path_size)
{
    if (path == NULL || path_size == 0U) {
        return -1;
    }
    path[0] = '\0';

#ifdef _WIN32
    {
        char temp_root[TECMO_ASSET_PACK_PATH_SIZE];
        char temp_name[TECMO_ASSET_PACK_PATH_SIZE];
        DWORD root_length = GetTempPathA((DWORD)sizeof(temp_root), temp_root);

        if (root_length == 0U || root_length >= sizeof(temp_root)) {
            return -1;
        }
        if (GetTempFileNameA(temp_root, "tap", 0U, temp_name) == 0U) {
            return -1;
        }
        if (DeleteFileA(temp_name) == 0) {
            return -1;
        }
        if (CreateDirectoryA(temp_name, NULL) == 0) {
            return -1;
        }
        return tecmo_asset_pack_copy_path(path, path_size, temp_name);
    }
#else
    {
        const char *directory = getenv("TMPDIR");
        char template_path[TECMO_ASSET_PACK_PATH_SIZE];
        char *created;
        int written;

        if (directory == NULL || directory[0] == '\0') {
            directory = "/tmp";
        }
        written = snprintf(template_path,
                           sizeof(template_path),
                           "%s%stecmo_asset_pack_self_test_XXXXXX",
                           directory,
                           directory[strlen(directory) - 1U] == '/' ? "" : "/");
        if (written < 0 || (size_t)written >= sizeof(template_path)) {
            return -1;
        }
        created = mkdtemp(template_path);
        if (created == NULL) {
            return -1;
        }
        return tecmo_asset_pack_copy_path(path, path_size, created);
    }
#endif
}

static int make_self_test_path(char *path,
                               size_t path_size,
                               const char *directory,
                               const char *file_name)
{
    const char *separator = "";
    size_t directory_length;
    int written;

    if (path == NULL || path_size == 0U ||
        directory == NULL || directory[0] == '\0' ||
        file_name == NULL || file_name[0] == '\0') {
        return -1;
    }

    directory_length = strlen(directory);
    if (directory_length > 0U &&
        directory[directory_length - 1U] != '/' &&
        directory[directory_length - 1U] != '\\') {
        separator = "/";
    }

    written = snprintf(path, path_size, "%s%s%s", directory, separator, file_name);
    if (written < 0 || (size_t)written >= path_size) {
        return -1;
    }
    return 0;
}

static void remove_self_test_file(const char *path)
{
    if (path != NULL && path[0] != '\0') {
        (void)remove(path);
    }
}

static void remove_self_test_dir(const char *path)
{
    if (path == NULL || path[0] == '\0') {
        return;
    }
#ifdef _WIN32
    (void)_rmdir(path);
#else
    (void)rmdir(path);
#endif
}

static int write_self_test_file(const char *path, const uint8_t *bytes, size_t byte_count)
{
    FILE *file;
    int result = -1;

    if (path == NULL || (bytes == NULL && byte_count > 0U)) {
        return -1;
    }

    file = fopen(path, "wb");
    if (file == NULL) {
        return -1;
    }
    if (byte_count == 0U || fwrite(bytes, 1U, byte_count, file) == byte_count) {
        result = 0;
    }
    if (fclose(file) != 0) {
        result = -1;
    }
    return result;
}

static int bytes_contain_text(const uint8_t *bytes, uint64_t byte_count, const char *needle)
{
    size_t haystack_size;
    size_t needle_size;

    if (bytes == NULL || needle == NULL || byte_count > (uint64_t)SIZE_MAX) {
        return 0;
    }

    haystack_size = (size_t)byte_count;
    needle_size = strlen(needle);
    if (needle_size == 0U) {
        return 1;
    }
    if (needle_size > haystack_size) {
        return 0;
    }

    for (size_t i = 0; i <= haystack_size - needle_size; ++i) {
        if (memcmp(bytes + i, needle, needle_size) == 0) {
            return 1;
        }
    }
    return 0;
}

static int self_test_read_and_compare(const char *pack_path,
                                      const char *entry_id,
                                      const uint8_t *expected,
                                      uint64_t expected_size,
                                      char *message,
                                      size_t message_size)
{
    uint8_t *bytes = NULL;
    uint64_t byte_count = 0U;
    int result = -1;

    if (tecmo_asset_pack_read_entry(pack_path, entry_id, &bytes, &byte_count) != 0) {
        tecmo_asset_pack_set_messagef(message, message_size, "Self-test could not read entry '%s'.", entry_id);
        return -1;
    }
    if (byte_count != expected_size ||
        (expected_size > 0U && memcmp(bytes, expected, (size_t)expected_size) != 0)) {
        tecmo_asset_pack_set_messagef(message, message_size, "Self-test readback mismatch for entry '%s'.", entry_id);
        goto cleanup;
    }

    result = 0;

cleanup:
    tecmo_asset_pack_free(bytes);
    return result;
}

static int self_test_arena_background_layer(const char *pack_path,
                                            const uint8_t expected_palette[16],
                                            char *message,
                                            size_t message_size)
{
    uint8_t *bytes = NULL;
    uint64_t byte_count = 0U;
    const uint8_t *cell;
    int result = -1;

    if (tecmo_asset_pack_read_entry(pack_path,
                                    TECMO_ASSET_PACK_ARENA_INTRO_BACKGROUND_ID,
                                    &bytes,
                                    &byte_count) != 0) {
        tecmo_asset_pack_set_message(message, message_size, "Self-test could not read arena background layer.");
        return -1;
    }
    if (byte_count != TECMO_ASSET_PACK_ARENA_LAYER_SIZE ||
        memcmp(bytes, "TATL", 4U) != 0 ||
        tecmo_asset_pack_read_u16(bytes + 4U) != 1U ||
        tecmo_asset_pack_read_u16(bytes + 6U) != 48U ||
        tecmo_asset_pack_read_u16(bytes + 8U) != 32U ||
        tecmo_asset_pack_read_u16(bytes + 10U) != 51U ||
        tecmo_asset_pack_read_u16(bytes + 12U) != 32U ||
        tecmo_asset_pack_read_u16(bytes + 14U) != 30U ||
        tecmo_asset_pack_read_u16(bytes + 16U) != 6U ||
        tecmo_asset_pack_read_u16(bytes + 18U) != 0U ||
        tecmo_asset_pack_read_u32(bytes + 20U) != 1632U ||
        tecmo_asset_pack_read_u32(bytes + 24U) != 48U ||
        tecmo_asset_pack_read_u32(bytes + 28U) != 32U ||
        memcmp(bytes + 32U, expected_palette, 16U) != 0) {
        tecmo_asset_pack_set_message(message, message_size, "Self-test arena background header mismatch.");
        goto cleanup;
    }

#define SELF_TEST_CELL(row, column) \
    (bytes + TECMO_ASSET_PACK_ARENA_LAYER_HEADER_SIZE + \
     (((size_t)(row) * TECMO_ASSET_PACK_ARENA_LAYER_WIDTH + (column)) * \
      TECMO_ASSET_PACK_ARENA_LAYER_CELL_STRIDE))
    cell = SELF_TEST_CELL(0U, 0U);
    if (cell[0] != 1U || cell[1] != 0U || tecmo_asset_pack_read_u32(cell + 2U) != 16U) {
        tecmo_asset_pack_set_message(message, message_size, "Self-test literal tile cell mismatch.");
        goto cleanup;
    }
    cell = SELF_TEST_CELL(0U, 2U);
    if (cell[0] != 3U || cell[1] != 2U || tecmo_asset_pack_read_u32(cell + 2U) != 48U) {
        tecmo_asset_pack_set_message(message, message_size, "Self-test attribute quadrant mismatch.");
        goto cleanup;
    }
    cell = SELF_TEST_CELL(16U, 0U);
    if (cell[0] != 9U || cell[1] != 0U || tecmo_asset_pack_read_u32(cell + 2U) != 96400U) {
        tecmo_asset_pack_set_message(message, message_size, "Self-test page-1 lower CHR cell mismatch.");
        goto cleanup;
    }
    cell = SELF_TEST_CELL(38U, 0U);
    if (cell[0] != 6U || cell[1] != 0U || tecmo_asset_pack_read_u32(cell + 2U) != 96U) {
        tecmo_asset_pack_set_messagef(message,
                     message_size,
                     "Self-test page-0 upper CHR cell mismatch: %u/%u/%u.",
                     (unsigned int)cell[0],
                     (unsigned int)cell[1],
                     tecmo_asset_pack_read_u32(cell + 2U));
        goto cleanup;
    }
    cell = SELF_TEST_CELL(48U, 0U);
    if (cell[0] != 8U || cell[1] != 0U || tecmo_asset_pack_read_u32(cell + 2U) != 96384U) {
        tecmo_asset_pack_set_message(message, message_size, "Self-test page-0 lower CHR split mismatch.");
        goto cleanup;
    }
#undef SELF_TEST_CELL

    result = 0;

cleanup:
    tecmo_asset_pack_free(bytes);
    return result;
}

static int self_test_arena_sprite_groups(const char *pack_path,
                                         const uint8_t expected_palette[16],
                                         char *message,
                                         size_t message_size)
{
    uint8_t *bytes = NULL;
    uint64_t byte_count = 0U;
    const uint8_t *jumbotron;
    const uint8_t *goal;
    const uint8_t *piece;
    size_t connector_overlay_piece_count = 0U;
    size_t zero_connector_overlay_count = 0U;
    int result = -1;

    if (tecmo_asset_pack_read_entry(pack_path,
                                    TECMO_ASSET_PACK_ARENA_INTRO_SPRITE_GROUPS_ID,
                                    &bytes,
                                    &byte_count) != 0) {
        tecmo_asset_pack_set_message(message, message_size, "Self-test could not read arena sprite groups.");
        return -1;
    }
    if (byte_count != TECMO_ASSET_PACK_ARENA_SPRITE_GROUPS_SIZE ||
        memcmp(bytes, "TASG", 4U) != 0 ||
        tecmo_asset_pack_read_u16(bytes + 4U) != 2U ||
        tecmo_asset_pack_read_u16(bytes + 6U) != 48U ||
        tecmo_asset_pack_read_u16(bytes + 8U) != 2U ||
        tecmo_asset_pack_read_u16(bytes + 10U) != 20U ||
        tecmo_asset_pack_read_u32(bytes + 12U) != 71U ||
        tecmo_asset_pack_read_u16(bytes + 16U) != 12U ||
        tecmo_asset_pack_read_u16(bytes + 18U) != 1U ||
        tecmo_asset_pack_read_u32(bytes + 20U) != 48U ||
        tecmo_asset_pack_read_u32(bytes + 24U) != 64U ||
        tecmo_asset_pack_read_u32(bytes + 28U) != 104U ||
        memcmp(bytes + 48U, expected_palette, 16U) != 0) {
        tecmo_asset_pack_set_message(message, message_size, "Self-test TASG header or palette mismatch.");
        goto cleanup;
    }
    for (size_t index = 32U; index < 48U; ++index) {
        if (bytes[index] != 0U) {
            tecmo_asset_pack_set_message(message, message_size, "Self-test TASG reserved header bytes are nonzero.");
            goto cleanup;
        }
    }
    for (size_t index = TECMO_ASSET_PACK_ARENA_SPRITE_PALETTE_OFFSET;
         index < TECMO_ASSET_PACK_ARENA_SPRITE_PALETTE_OFFSET + 16U;
         ++index) {
        if (bytes[index] > 0x3FU) {
            tecmo_asset_pack_set_message(message, message_size, "Self-test TASG palette color is outside the NES palette.");
            goto cleanup;
        }
    }

    jumbotron = bytes + TECMO_ASSET_PACK_ARENA_SPRITE_GROUPS_OFFSET;
    goal = jumbotron + TECMO_ASSET_PACK_ARENA_SPRITE_GROUP_STRIDE;
    if (tecmo_asset_pack_read_u16(jumbotron + 0U) != 1U ||
        tecmo_asset_pack_read_u16(jumbotron + 2U) != 1U ||
        tecmo_asset_pack_read_u32(jumbotron + 4U) != 0U ||
        tecmo_asset_pack_read_u32(jumbotron + 8U) != 55U ||
        tecmo_asset_pack_read_u16(jumbotron + 12U) != 0U ||
        tecmo_asset_pack_read_u16(jumbotron + 14U) != 0U ||
        tecmo_asset_pack_read_u16(jumbotron + 16U) != 0U ||
        tecmo_asset_pack_read_u16(jumbotron + 18U) != 2U ||
        tecmo_asset_pack_read_u16(goal + 0U) != 2U ||
        tecmo_asset_pack_read_u16(goal + 2U) != 0U ||
        tecmo_asset_pack_read_u32(goal + 4U) != 55U ||
        tecmo_asset_pack_read_u32(goal + 8U) != 16U ||
        tecmo_asset_pack_read_u16(goal + 12U) != 165U ||
        tecmo_asset_pack_read_u16(goal + 14U) != 350U ||
        tecmo_asset_pack_read_u16(goal + 16U) != 0U ||
        tecmo_asset_pack_read_u16(goal + 18U) != 2U) {
        tecmo_asset_pack_set_message(message, message_size, "Self-test TASG group descriptors mismatch.");
        goto cleanup;
    }

    piece = bytes + TECMO_ASSET_PACK_ARENA_SPRITE_PIECES_OFFSET;
    if ((int16_t)tecmo_asset_pack_read_u16(piece + 0U) != 32 ||
        (int16_t)tecmo_asset_pack_read_u16(piece + 2U) != 16 ||
        tecmo_asset_pack_read_u32(piece + 4U) != 8192U + 0x22U * 16U ||
        piece[8] != 3U || piece[9] != 7U || tecmo_asset_pack_read_u16(piece + 10U) != 0U) {
        tecmo_asset_pack_set_message(message, message_size, "Self-test TASG first jumbotron piece mismatch.");
        goto cleanup;
    }
    piece = bytes + TECMO_ASSET_PACK_ARENA_SPRITE_PIECES_OFFSET +
            TECMO_ASSET_PACK_ARENA_JUMBOTRON_COUNT *
                TECMO_ASSET_PACK_ARENA_SPRITE_PIECE_STRIDE;
    if ((int16_t)tecmo_asset_pack_read_u16(piece + 0U) != 3 ||
        (int16_t)tecmo_asset_pack_read_u16(piece + 2U) != 0 ||
        tecmo_asset_pack_read_u32(piece + 4U) != 8192U + 0x40U * 16U ||
        piece[8] != 2U || piece[9] != 1U || tecmo_asset_pack_read_u16(piece + 10U) != 0U) {
        tecmo_asset_pack_set_message(message, message_size, "Self-test TASG first goal piece mismatch.");
        goto cleanup;
    }

    for (size_t index = 0U; index < TECMO_ASSET_PACK_ARENA_SPRITE_PIECE_COUNT; ++index) {
        uint32_t chr_byte_offset;
        int16_t connector_overlay_y_adjust;
        piece = bytes + TECMO_ASSET_PACK_ARENA_SPRITE_PIECES_OFFSET +
                index * TECMO_ASSET_PACK_ARENA_SPRITE_PIECE_STRIDE;
        chr_byte_offset = tecmo_asset_pack_read_u32(piece + 4U);
        connector_overlay_y_adjust = (int16_t)tecmo_asset_pack_read_u16(piece + 10U);
        if (connector_overlay_y_adjust ==
            TECMO_ASSET_PACK_ARENA_GOAL_CONNECTOR_OVERLAY_Y_ADJUST) {
            ++connector_overlay_piece_count;
            if (index < TECMO_ASSET_PACK_ARENA_JUMBOTRON_COUNT ||
                (int16_t)tecmo_asset_pack_read_u16(piece + 0U) != 16 ||
                (int16_t)tecmo_asset_pack_read_u16(piece + 2U) != 32 ||
                chr_byte_offset != 9056U) {
                tecmo_asset_pack_set_message(message, message_size, "Self-test TASG connector-overlay goal piece mismatch.");
                goto cleanup;
            }
        } else if (connector_overlay_y_adjust == 0) {
            ++zero_connector_overlay_count;
        } else {
            tecmo_asset_pack_set_message(message, message_size, "Self-test TASG piece connector overlay mismatch.");
            goto cleanup;
        }
        if (piece[8] > 3U || piece[9] > 7U ||
            (chr_byte_offset & 0x0FU) != 0U ||
            chr_byte_offset < TECMO_ASSET_PACK_ARENA_SPRITE_CHR_BASE ||
            (uint64_t)chr_byte_offset + 32U >
                TECMO_ASSET_PACK_ARENA_SPRITE_CHR_LIMIT) {
            tecmo_asset_pack_set_message(message, message_size, "Self-test TASG piece contract mismatch.");
            goto cleanup;
        }
    }
    if (connector_overlay_piece_count != 1U || zero_connector_overlay_count != 70U) {
        tecmo_asset_pack_set_message(message, message_size, "Self-test TASG connector-overlay counts mismatch.");
        goto cleanup;
    }

    result = 0;

cleanup:
    tecmo_asset_pack_free(bytes);
    return result;
}

static int self_test_arena_sprite_group_validation(uint8_t *rom,
                                                   uint64_t rom_size,
                                                   uint64_t prg_offset,
                                                   uint64_t chr_offset,
                                                   uint64_t chr_size,
                                                   char *message,
                                                   size_t message_size)
{
    uint8_t payload[TECMO_ASSET_PACK_ARENA_SPRITE_GROUPS_SIZE];
    TecmoArenaSpriteGroupsProvenance provenance;
    uint64_t bank04_offset = prg_offset +
                             (uint64_t)TECMO_ASSET_PACK_ARENA_BANK04 *
                                 TECMO_ASSET_PACK_PRG_BANK_BYTES;
    uint64_t palette_offset = bank04_offset +
                              (TECMO_ASSET_PACK_ARENA_PALETTE_CPU -
                               TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE);
    uint64_t seeds_offset = bank04_offset +
                            (TECMO_ASSET_PACK_ARENA_SEEDS_CPU -
                             TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE);
    uint64_t params_offset = bank04_offset +
                             (TECMO_ASSET_PACK_ARENA_PARAMS_CPU -
                              TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE);
    uint64_t first_record_tile_offset = prg_offset + (0xA7E3U - 0x8000U) + 2U;
    uint64_t goal_connector_record_offset =
        prg_offset + (0xA8C0U - 0x8000U) + 1U + 7U * 4U;
    uint8_t saved;
    char validation_message[192];

    saved = rom[(size_t)palette_offset + 1U];
    rom[(size_t)palette_offset + 1U] = 0x40U;
    if (tecmo_asset_pack_build_arena_sprite_groups(rom,
                                  rom_size,
                                  prg_offset,
                                  8U,
                                  chr_offset,
                                  chr_size,
                                  payload,
                                  sizeof(payload),
                                  &provenance,
                                  validation_message,
                                  sizeof(validation_message)) == 0) {
        rom[(size_t)palette_offset + 1U] = saved;
        tecmo_asset_pack_set_message(message, message_size, "Self-test accepted an invalid sprite palette color.");
        return -1;
    }
    rom[(size_t)palette_offset + 1U] = saved;

    saved = rom[(size_t)first_record_tile_offset];
    rom[(size_t)first_record_tile_offset] = 0x7DU;
    if (tecmo_asset_pack_build_arena_sprite_groups(rom,
                                  rom_size,
                                  prg_offset,
                                  8U,
                                  chr_offset,
                                  chr_size,
                                  payload,
                                  sizeof(payload),
                                  &provenance,
                                  validation_message,
                                  sizeof(validation_message)) != 0 ||
        tecmo_asset_pack_read_u32(payload + TECMO_ASSET_PACK_ARENA_SPRITE_PIECES_OFFSET + 4U) !=
            TECMO_ASSET_PACK_ARENA_SPRITE_CHR_LIMIT - 32U) {
        rom[(size_t)first_record_tile_offset] = saved;
        tecmo_asset_pack_set_message(message, message_size, "Self-test rejected the final valid sprite CHR pair.");
        return -1;
    }
    rom[(size_t)first_record_tile_offset] = 0x7FU;
    if (tecmo_asset_pack_build_arena_sprite_groups(rom,
                                  rom_size,
                                  prg_offset,
                                  8U,
                                  chr_offset,
                                  chr_size,
                                  payload,
                                  sizeof(payload),
                                  &provenance,
                                  validation_message,
                                  sizeof(validation_message)) == 0) {
        rom[(size_t)first_record_tile_offset] = saved;
        tecmo_asset_pack_set_message(message, message_size, "Self-test accepted a sprite CHR pair outside pages 8/9.");
        return -1;
    }
    rom[(size_t)first_record_tile_offset] = saved;

    saved = rom[(size_t)goal_connector_record_offset + 1U];
    /* $35 still normalizes to top tile $36, so this checks raw identity too. */
    rom[(size_t)goal_connector_record_offset + 1U] = 0x35U;
    validation_message[0] = '\0';
    if (tecmo_asset_pack_build_arena_sprite_groups(rom,
                                  rom_size,
                                  prg_offset,
                                  8U,
                                  chr_offset,
                                  chr_size,
                                  payload,
                                  sizeof(payload),
                                  &provenance,
                                  validation_message,
                                  sizeof(validation_message)) == 0 ||
        strstr(validation_message, "connector source-contract mismatch") == NULL) {
        rom[(size_t)goal_connector_record_offset + 1U] = saved;
        tecmo_asset_pack_set_message(message,
                    message_size,
                    "Self-test accepted a connector record with the wrong raw tile identity.");
        return -1;
    }
    rom[(size_t)goal_connector_record_offset + 1U] = saved;

    saved = rom[(size_t)goal_connector_record_offset + 2U];
    rom[(size_t)goal_connector_record_offset + 2U] = 0x03U;
    validation_message[0] = '\0';
    if (tecmo_asset_pack_build_arena_sprite_groups(rom,
                                  rom_size,
                                  prg_offset,
                                  8U,
                                  chr_offset,
                                  chr_size,
                                  payload,
                                  sizeof(payload),
                                  &provenance,
                                  validation_message,
                                  sizeof(validation_message)) == 0 ||
        strstr(validation_message, "connector source-contract mismatch") == NULL) {
        rom[(size_t)goal_connector_record_offset + 2U] = saved;
        tecmo_asset_pack_set_message(message,
                    message_size,
                    "Self-test accepted a connector record with the wrong source attributes.");
        return -1;
    }
    rom[(size_t)goal_connector_record_offset + 2U] = saved;

    saved = rom[(size_t)seeds_offset + 1U];
    rom[(size_t)seeds_offset + 1U] ^= 1U;
    if (tecmo_asset_pack_build_arena_sprite_groups(rom,
                                  rom_size,
                                  prg_offset,
                                  8U,
                                  chr_offset,
                                  chr_size,
                                  payload,
                                  sizeof(payload),
                                  &provenance,
                                  validation_message,
                                  sizeof(validation_message)) == 0) {
        rom[(size_t)seeds_offset + 1U] = saved;
        tecmo_asset_pack_set_message(message, message_size, "Self-test accepted invalid sprite anchor seeds.");
        return -1;
    }
    rom[(size_t)seeds_offset + 1U] = saved;

    saved = rom[(size_t)params_offset + 1U];
    rom[(size_t)params_offset + 1U] ^= 1U;
    if (tecmo_asset_pack_build_arena_sprite_groups(rom,
                                  rom_size,
                                  prg_offset,
                                  8U,
                                  chr_offset,
                                  chr_size,
                                  payload,
                                  sizeof(payload),
                                  &provenance,
                                  validation_message,
                                  sizeof(validation_message)) == 0) {
        rom[(size_t)params_offset + 1U] = saved;
        tecmo_asset_pack_set_message(message, message_size, "Self-test accepted invalid sprite anchor parameters.");
        return -1;
    }
    rom[(size_t)params_offset + 1U] = saved;
    return 0;
}

static int self_test_entry_contains_text(const char *pack_path,
                                         const char *entry_id,
                                         const char *needle,
                                         char *message,
                                         size_t message_size)
{
    uint8_t *bytes = NULL;
    uint64_t byte_count = 0U;
    int result = -1;

    if (tecmo_asset_pack_read_entry(pack_path, entry_id, &bytes, &byte_count) != 0) {
        tecmo_asset_pack_set_messagef(message, message_size, "Self-test could not read entry '%s'.", entry_id);
        return -1;
    }
    if (!bytes_contain_text(bytes, byte_count, needle)) {
        tecmo_asset_pack_set_messagef(message,
                     message_size,
                     "Self-test entry '%s' did not contain '%s'.",
                     entry_id,
                     needle);
        goto cleanup;
    }

    result = 0;

cleanup:
    tecmo_asset_pack_free(bytes);
    return result;
}

typedef struct TecmoAssetPackSelfTestBuilderListState {
    unsigned int count;
    int saw_memory;
    int saw_file;
    uint64_t memory_size;
    uint64_t file_size;
    char *message;
    size_t message_size;
} TecmoAssetPackSelfTestBuilderListState;

static int self_test_builder_list_entry(const TecmoAssetPackDirectoryEntryInfo *entry_info,
                                        void *user_data)
{
    TecmoAssetPackSelfTestBuilderListState *state =
        (TecmoAssetPackSelfTestBuilderListState *)user_data;

    if (entry_info == NULL || state == NULL) {
        return -1;
    }

    ++state->count;
    if (strcmp(entry_info->id, "test/memory") == 0) {
        if (entry_info->type != TECMO_ASSET_PACK_TYPE_DATA ||
            entry_info->bank != 7U ||
            entry_info->cpu_address != 0x8123U ||
            entry_info->source_offset != 0x1234ULL ||
            entry_info->byte_count != state->memory_size ||
            entry_info->flags != TECMO_ASSET_PACK_FLAG_DERIVED) {
            tecmo_asset_pack_set_message(state->message,
                        state->message_size,
                        "Self-test memory directory metadata mismatch.");
            return -1;
        }
        state->saw_memory = 1;
    } else if (strcmp(entry_info->id, "test/file") == 0) {
        if (entry_info->type != TECMO_ASSET_PACK_TYPE_DATA ||
            entry_info->bank != 3U ||
            entry_info->cpu_address != 0x9000U ||
            entry_info->source_offset != 0x5678ULL ||
            entry_info->byte_count != state->file_size ||
            entry_info->flags != TECMO_ASSET_PACK_FLAG_LOCAL) {
            tecmo_asset_pack_set_message(state->message,
                        state->message_size,
                        "Self-test file directory metadata mismatch.");
            return -1;
        }
        state->saw_file = 1;
    }

    return 0;
}

typedef struct TecmoAssetPackSelfTestInesListState {
    unsigned int count;
    int saw_manifest;
    int saw_source_map;
    int saw_presents;
    int presents_metadata_valid;
    int saw_license;
    int license_metadata_valid;
    int saw_arena_intro_script;
    int saw_arena_intro_background;
    int arena_intro_background_metadata_valid;
    int saw_arena_intro_palette;
    int saw_arena_intro_sprite_groups;
    int arena_intro_sprite_groups_metadata_valid;
    int saw_ready;
    int ready_metadata_valid;
    int saw_warriors;
    int warriors_metadata_valid;
    int saw_clippers;
    int clippers_metadata_valid;
    int saw_bucks;
    int bucks_metadata_valid;
    int saw_pass;
    int pass_metadata_valid;
    int saw_finale;
    int finale_metadata_valid;
    int saw_prg_bank0;
    int saw_prg_bank1;
    int saw_prg_fixed;
    int saw_chr_all;
    int saw_chr_bank0;
    int saw_chr_bank1;
} TecmoAssetPackSelfTestInesListState;

static int self_test_ines_list_entry(const TecmoAssetPackDirectoryEntryInfo *entry_info,
                                     void *user_data)
{
    TecmoAssetPackSelfTestInesListState *state =
        (TecmoAssetPackSelfTestInesListState *)user_data;

    if (entry_info == NULL || state == NULL) {
        return -1;
    }

    ++state->count;
    if (strcmp(entry_info->id, "system/manifest") == 0) {
        state->saw_manifest = 1;
    } else if (strcmp(entry_info->id, "system/source-map") == 0) {
        state->saw_source_map = 1;
    } else if (strcmp(entry_info->id, TECMO_ASSET_PACK_PRESENTS_ID) == 0) {
        state->saw_presents = 1;
        state->presents_metadata_valid =
            entry_info->type == TECMO_ASSET_PACK_TYPE_DATA &&
            entry_info->bank == 0U &&
            entry_info->cpu_address == TECMO_ASSET_PACK_PRESENTS_STREAM_CPU &&
            entry_info->source_offset == 16ULL +
                                             (TECMO_ASSET_PACK_PRESENTS_STREAM_CPU -
                                              TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE) &&
            entry_info->byte_count == TECMO_ASSET_PACK_PRESENTS_SIZE;
    } else if (strcmp(entry_info->id, TECMO_ASSET_PACK_LICENSE_ID) == 0) {
        state->saw_license = 1;
        state->license_metadata_valid =
            entry_info->type == TECMO_ASSET_PACK_TYPE_DATA &&
            entry_info->bank == 0U &&
            entry_info->cpu_address == TECMO_ASSET_PACK_LICENSE_STREAM_CPU &&
            entry_info->source_offset == 16ULL +
                                             (TECMO_ASSET_PACK_LICENSE_STREAM_CPU -
                                              TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE) &&
            entry_info->byte_count == TECMO_ASSET_PACK_LICENSE_SIZE;
    } else if (strcmp(entry_info->id, TECMO_ASSET_PACK_ARENA_INTRO_SCRIPT_ID) == 0) {
        state->saw_arena_intro_script = 1;
    } else if (strcmp(entry_info->id, TECMO_ASSET_PACK_ARENA_INTRO_BACKGROUND_ID) == 0) {
        state->saw_arena_intro_background = 1;
        state->arena_intro_background_metadata_valid =
            entry_info->type == TECMO_ASSET_PACK_TYPE_DATA &&
            entry_info->bank == 0U &&
            entry_info->cpu_address == 0xA000U &&
            entry_info->source_offset == 16ULL + 0x2000ULL &&
            entry_info->byte_count == TECMO_ASSET_PACK_ARENA_LAYER_SIZE;
    } else if (strcmp(entry_info->id, TECMO_ASSET_PACK_ARENA_INTRO_PALETTE_ID) == 0) {
        state->saw_arena_intro_palette = 1;
    } else if (strcmp(entry_info->id, TECMO_ASSET_PACK_ARENA_INTRO_SPRITE_GROUPS_ID) == 0) {
        state->saw_arena_intro_sprite_groups = 1;
        state->arena_intro_sprite_groups_metadata_valid =
            entry_info->type == TECMO_ASSET_PACK_TYPE_DATA &&
            entry_info->bank == 0U &&
            entry_info->cpu_address == TECMO_ASSET_PACK_ARENA_POINTER_TABLE_CPU &&
            entry_info->source_offset == 16ULL +
                                             (TECMO_ASSET_PACK_ARENA_POINTER_TABLE_CPU -
                                              TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE) &&
            entry_info->byte_count == TECMO_ASSET_PACK_ARENA_SPRITE_GROUPS_SIZE;
    } else if (strcmp(entry_info->id, TECMO_ASSET_PACK_READY_ID) == 0) {
        state->saw_ready = 1;
        state->ready_metadata_valid =
            entry_info->type == TECMO_ASSET_PACK_TYPE_DATA &&
            entry_info->bank == TECMO_ASSET_PACK_ARENA_BANK04 &&
            entry_info->cpu_address == TECMO_ASSET_PACK_READY_ROUTE_CPU &&
            entry_info->byte_count == TECMO_ASSET_PACK_READY_SIZE;
    } else if (strcmp(entry_info->id, TECMO_ASSET_PACK_WARRIORS_ID) == 0) {
        state->saw_warriors = 1;
        state->warriors_metadata_valid =
            entry_info->type == TECMO_ASSET_PACK_TYPE_DATA &&
            entry_info->bank == TECMO_ASSET_PACK_ARENA_BANK04 &&
            entry_info->cpu_address == TECMO_ASSET_PACK_WARRIORS_ROUTE_CPU &&
            entry_info->byte_count == TECMO_ASSET_PACK_WARRIORS_SIZE;
    } else if (strcmp(entry_info->id, TECMO_ASSET_PACK_CLIPPERS_ID) == 0) {
        state->saw_clippers = 1;
        state->clippers_metadata_valid =
            entry_info->type == TECMO_ASSET_PACK_TYPE_DATA &&
            entry_info->bank == TECMO_ASSET_PACK_ARENA_BANK04 &&
            entry_info->cpu_address == TECMO_ASSET_PACK_CLIPPERS_ROUTE_CPU &&
            entry_info->byte_count == TECMO_ASSET_PACK_CLIPPERS_SIZE;
    } else if (strcmp(entry_info->id, TECMO_ASSET_PACK_BUCKS_ID) == 0) {
        state->saw_bucks = 1;
        state->bucks_metadata_valid =
            entry_info->type == TECMO_ASSET_PACK_TYPE_DATA &&
            entry_info->bank == TECMO_ASSET_PACK_ARENA_BANK04 &&
            entry_info->cpu_address == TECMO_ASSET_PACK_BUCKS_ROUTE_CPU &&
            entry_info->byte_count == TECMO_ASSET_PACK_BUCKS_SIZE;
    } else if (strcmp(entry_info->id, TECMO_ASSET_PACK_PASS_ID) == 0) {
        state->saw_pass = 1;
        state->pass_metadata_valid =
            entry_info->type == TECMO_ASSET_PACK_TYPE_DATA &&
            entry_info->bank == TECMO_ASSET_PACK_ARENA_BANK04 &&
            entry_info->cpu_address == TECMO_ASSET_PACK_PASS_ROUTE_CPU &&
            entry_info->byte_count == TECMO_ASSET_PACK_PASS_SIZE;
    } else if (strcmp(entry_info->id, TECMO_ASSET_PACK_FINALE_ID) == 0) {
        state->saw_finale = 1;
        state->finale_metadata_valid =
            entry_info->type == TECMO_ASSET_PACK_TYPE_DATA &&
            entry_info->bank == 4U &&
            entry_info->cpu_address == 0x82CFU &&
            entry_info->byte_count == TECMO_ASSET_PACK_FINALE_SIZE;
    } else if (strcmp(entry_info->id, "prg/bank00") == 0) {
        state->saw_prg_bank0 = 1;
    } else if (strcmp(entry_info->id, "prg/bank01") == 0) {
        state->saw_prg_bank1 = 1;
    } else if (strcmp(entry_info->id, "prg/fixed") == 0) {
        state->saw_prg_fixed = 1;
    } else if (strcmp(entry_info->id, "chr/all") == 0) {
        state->saw_chr_all = 1;
    } else if (strcmp(entry_info->id, "chr/bank00") == 0) {
        state->saw_chr_bank0 = 1;
    } else if (strcmp(entry_info->id, "chr/bank01") == 0) {
        state->saw_chr_bank1 = 1;
    }

    return 0;
}

static size_t write_self_test_opening_stream(uint8_t *bank00,
                                             size_t offset,
                                             unsigned int group_count,
                                             uint8_t literal_count,
                                             uint8_t fourth_repeat_count,
                                             uint8_t terminator_count)
{
    size_t source = offset;

    for (unsigned int group = 0U; group < group_count; ++group) {
        bank00[source++] = group == 0U ? 0xA9U : 0xAAU;
        for (unsigned int slot = 0U; slot < 4U; ++slot) {
            unsigned int operation_index = group * 4U + slot;
            if (operation_index == 0U) {
                bank00[source++] = literal_count;
                memset(bank00 + source, 0, literal_count);
                source += literal_count;
            } else {
                unsigned int repeat_index = operation_index - 1U;
                bank00[source++] = repeat_index < 3U
                                       ? 0U
                                       : (repeat_index == 3U ? fourth_repeat_count : 1U);
                bank00[source++] = 0U;
            }
        }
    }
    bank00[source++] = 0U;
    bank00[source++] = terminator_count;
    return source - offset;
}

static int populate_self_test_opening_fixture(uint8_t *bank00, uint8_t *fixed)
{
    static const uint8_t selector_contract[7] = {
        0xA2U,0xF4U,0x86U,0x57U,0xE8U,0x86U,0x58U
    };
    uint8_t *presents_descriptor = fixed +
        (TECMO_ASSET_PACK_SCREEN_DESCRIPTOR_CPU - 0xC000U) +
        TECMO_ASSET_PACK_PRESENTS_SCREEN_ID * TECMO_ASSET_PACK_SCREEN_DESCRIPTOR_SIZE;
    uint8_t *license_descriptor = fixed +
        (TECMO_ASSET_PACK_SCREEN_DESCRIPTOR_CPU - 0xC000U) +
        TECMO_ASSET_PACK_LICENSE_SCREEN_ID * TECMO_ASSET_PACK_SCREEN_DESCRIPTOR_SIZE;
    size_t presents_size;
    size_t license_size;

    if (bank00 == NULL || fixed == NULL) return -1;
    presents_descriptor[0] = TECMO_ASSET_PACK_PRESENTS_BG_R0 / 2U;
    presents_descriptor[1] = TECMO_ASSET_PACK_PRESENTS_BG_R1 / 2U;
    tecmo_asset_pack_store_u16(presents_descriptor + 2U, TECMO_ASSET_PACK_OPENING_PALETTE_CPU);
    tecmo_asset_pack_store_u16(presents_descriptor + 4U, TECMO_ASSET_PACK_PRESENTS_STREAM_CPU);
    presents_descriptor[6] = 0U;
    memcpy(license_descriptor, presents_descriptor, TECMO_ASSET_PACK_SCREEN_DESCRIPTOR_SIZE);
    tecmo_asset_pack_store_u16(license_descriptor + 4U, TECMO_ASSET_PACK_LICENSE_STREAM_CPU);

    presents_size = write_self_test_opening_stream(
        bank00,
        TECMO_ASSET_PACK_PRESENTS_STREAM_CPU - TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE,
        12U,
        7U,
        206U,
        0xA9U);
    license_size = write_self_test_opening_stream(
        bank00,
        TECMO_ASSET_PACK_LICENSE_STREAM_CPU - TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE,
        18U,
        14U,
        175U,
        0U);
    if (presents_size != TECMO_ASSET_PACK_PRESENTS_STREAM_SIZE ||
        license_size != TECMO_ASSET_PACK_LICENSE_STREAM_SIZE) return -1;

    for (size_t color = 0U; color < 16U; ++color) {
        bank00[TECMO_ASSET_PACK_OPENING_PALETTE_CPU -
               TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE + color] =
            color % 4U == 0U ? 0x0FU : (uint8_t)(0x10U + color);
    }
    memcpy(bank00 +
               (TECMO_ASSET_PACK_PRESENTS_SELECTOR_CODE_CPU -
                TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE),
           selector_contract,
           sizeof(selector_contract));
    bank00[TECMO_ASSET_PACK_PRESENTS_Y_BASE_OPERAND_CPU -
           TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE] = 0x40U;
    bank00[TECMO_ASSET_PACK_PRESENTS_TILE_BASE_OPERAND_CPU -
           TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE] = 0x55U;
    bank00[TECMO_ASSET_PACK_PRESENTS_X_BASE_OPERAND_CPU -
           TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE] = 0x50U;
    bank00[TECMO_ASSET_PACK_PRESENTS_SPRITE_TABLE_CPU -
           TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE] =
        TECMO_ASSET_PACK_PRESENTS_SPRITE_COUNT;
    for (size_t record = 0U;
         record < TECMO_ASSET_PACK_PRESENTS_SPRITE_COUNT;
         ++record) {
        size_t offset = TECMO_ASSET_PACK_PRESENTS_SPRITE_TABLE_CPU -
                        TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE + 1U + record * 4U;
        bank00[offset + 0U] = (uint8_t)((record / 4U) * 8U);
        bank00[offset + 1U] = (uint8_t)record;
        bank00[offset + 2U] = (uint8_t)(record >= 10U ? 1U : 0U);
        bank00[offset + 3U] = (uint8_t)((record % 4U) * 8U);
    }
    for (size_t color = 0U; color < 16U; ++color) {
        bank00[TECMO_ASSET_PACK_PRESENTS_SPRITE_PALETTE_CPU -
               TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE + color] =
            color % 4U == 0U ? 0x0FU : (uint8_t)(0x20U + color);
    }
    return 0;
}

static void write_self_test_chr_selector(uint8_t *bank04,
                                         uint16_t instruction_cpu,
                                         uint8_t selector,
                                         uint8_t destination)
{
    size_t offset = instruction_cpu - 0x8000U;
    bank04[offset + 0U] = 0xA9U;
    bank04[offset + 1U] = selector;
    bank04[offset + 2U] = 0x85U;
    bank04[offset + 3U] = destination;
}

int tecmo_asset_pack_self_test(char *message, size_t message_size)
{
    static const uint8_t memory_entry_bytes[] = {0x00U, 0x01U, 0x7FU, 0x80U, 0xFFU};
    static const uint8_t file_entry_bytes[] = {
        'l', 'o', 'c', 'a', 'l', '-', 'f', 'i', 'l', 'e', '-', 'e', 'n', 't', 'r', 'y'
    };
    static const uint8_t arena_palette[16] = {
        0x0FU, 0x01U, 0x02U, 0x03U, 0x0FU, 0x04U, 0x05U, 0x06U,
        0x0FU, 0x07U, 0x08U, 0x09U, 0x0FU, 0x0AU, 0x0BU, 0x0CU
    };
    static const uint8_t arena_sprite_palette[16] = {
        0x02U, 0x11U, 0x12U, 0x13U, 0x02U, 0x14U, 0x15U, 0x16U,
        0x02U, 0x17U, 0x18U, 0x19U, 0x02U, 0x1AU, 0x1BU, 0x1CU
    };
    static const uint8_t expected_sprite_palette[16] = {
        0x0FU, 0x11U, 0x12U, 0x13U, 0x0FU, 0x14U, 0x15U, 0x16U,
        0x0FU, 0x17U, 0x18U, 0x19U, 0x0FU, 0x1AU, 0x1BU, 0x1CU
    };
    static const uint8_t arena_sprite_seeds[4] = {0x00U, 0x1EU, 0x00U, 0x01U};
    static const uint8_t arena_sprite_params[4] = {0x00U, 0xB8U, 0x00U, 0x01U};
    static const uint8_t arena_stream[] = {
        0xB9U,
        0x04U, 0x01U, 0x02U, 0x03U, 0x04U,
        0x00U, 0x05U,
        0x04U, 0x07U, 0x00U,
        0x00U, 0x06U,
        0xAAU,
        0x00U, 0x07U,
        0x00U, 0x08U,
        0x00U, 0x09U,
        0x00U, 0x0AU,
        0x0AU,
        0x00U, 0x0BU,
        0xF8U, 0x0CU,
        0x00U
    };
    char builder_pack_path[1024] = {0};
    char local_file_path[1024] = {0};
    char rom_path[1024] = {0};
    char ines_pack_path[1024] = {0};
    char temp_dir[1024] = {0};
    TecmoAssetPackBuilder *builder = NULL;
    TecmoAssetPackEntryInfo entry_info;
    TecmoAssetPackSelfTestBuilderListState builder_list_state;
    TecmoAssetPackSelfTestInesListState ines_list_state;
    uint8_t *rom = NULL;
    char *dump = NULL;
    uint64_t prg_offset = 16ULL;
    uint64_t chr_offset = 16ULL + 8ULL * TECMO_ASSET_PACK_PRG_BANK_BYTES;
    uint64_t chr_size = 32ULL * TECMO_ASSET_PACK_CHR_BANK_BYTES;
    uint64_t rom_size = 16ULL + 8ULL * TECMO_ASSET_PACK_PRG_BANK_BYTES +
                        32ULL * TECMO_ASSET_PACK_CHR_BANK_BYTES;
    size_t dump_size = 0U;
    int result = -1;

    if (tecmo_asset_pack_finale_self_test(message, message_size) != 0) {
        return -1;
    }

    if (message != NULL && message_size > 0U) {
        message[0] = '\0';
    }

    if (tecmo_asset_pack_d9f6_self_test(message, message_size) != 0) {
        goto cleanup;
    }

    if (make_self_test_temp_dir(temp_dir, sizeof(temp_dir)) != 0 ||
        make_self_test_path(builder_pack_path, sizeof(builder_pack_path), temp_dir, "builder.assetpack") != 0 ||
        make_self_test_path(local_file_path, sizeof(local_file_path), temp_dir, "local.bin") != 0 ||
        make_self_test_path(rom_path, sizeof(rom_path), temp_dir, "input.nes") != 0 ||
        make_self_test_path(ines_pack_path, sizeof(ines_pack_path), temp_dir, "ines.assetpack") != 0) {
        tecmo_asset_pack_set_message(message, message_size, "Self-test could not create temporary paths.");
        goto cleanup;
    }

    if (tecmo_asset_pack_builder_begin(&builder, builder_pack_path, message, message_size) != 0) {
        goto cleanup;
    }

    entry_info = tecmo_asset_pack_make_entry_info("test/memory",
                                 TECMO_ASSET_PACK_TYPE_DATA,
                                 7U,
                                 0x8123U,
                                 0x1234ULL,
                                 TECMO_ASSET_PACK_FLAG_DERIVED);
    if (tecmo_asset_pack_builder_add_memory(builder,
                                            &entry_info,
                                            memory_entry_bytes,
                                            sizeof(memory_entry_bytes),
                                            message,
                                            message_size) != 0) {
        goto cleanup;
    }

    if (write_self_test_file(local_file_path, file_entry_bytes, sizeof(file_entry_bytes)) != 0) {
        tecmo_asset_pack_set_message(message, message_size, "Self-test could not write temporary file entry source.");
        goto cleanup;
    }

    entry_info = tecmo_asset_pack_make_entry_info("test/file",
                                 TECMO_ASSET_PACK_TYPE_DATA,
                                 3U,
                                 0x9000U,
                                 0x5678ULL,
                                 TECMO_ASSET_PACK_FLAG_LOCAL);
    if (tecmo_asset_pack_builder_add_file(builder,
                                          &entry_info,
                                          local_file_path,
                                          message,
                                          message_size) != 0) {
        goto cleanup;
    }
    if (tecmo_asset_pack_builder_finish(builder, message, message_size) != 0) {
        builder = NULL;
        goto cleanup;
    }
    builder = NULL;

    if (self_test_read_and_compare(builder_pack_path,
                                   "test/memory",
                                   memory_entry_bytes,
                                   sizeof(memory_entry_bytes),
                                   message,
                                   message_size) != 0 ||
        self_test_read_and_compare(builder_pack_path,
                                   "test/file",
                                   file_entry_bytes,
                                   sizeof(file_entry_bytes),
                                   message,
                                   message_size) != 0) {
        goto cleanup;
    }

    memset(&builder_list_state, 0, sizeof(builder_list_state));
    builder_list_state.memory_size = sizeof(memory_entry_bytes);
    builder_list_state.file_size = sizeof(file_entry_bytes);
    builder_list_state.message = message;
    builder_list_state.message_size = message_size;
    if (tecmo_asset_pack_list_entries(builder_pack_path,
                                      self_test_builder_list_entry,
                                      &builder_list_state) != 0 ||
        builder_list_state.count != 2U ||
        !builder_list_state.saw_memory ||
        !builder_list_state.saw_file) {
        if (message != NULL && message_size > 0U && message[0] == '\0') {
            tecmo_asset_pack_set_message(message, message_size, "Self-test directory enumeration mismatch.");
        }
        goto cleanup;
    }

    if (tecmo_asset_pack_dump_directory(builder_pack_path, NULL, 0U, &dump_size) != 0 ||
        dump_size == 0U) {
        tecmo_asset_pack_set_message(message, message_size, "Self-test could not size directory dump.");
        goto cleanup;
    }
    dump = (char *)malloc(dump_size);
    if (dump == NULL) {
        tecmo_asset_pack_set_message(message, message_size, "Self-test could not allocate directory dump.");
        goto cleanup;
    }
    if (tecmo_asset_pack_dump_directory(builder_pack_path, dump, dump_size, &dump_size) != 0 ||
        strstr(dump, "test/memory") == NULL ||
        strstr(dump, "test/file") == NULL) {
        tecmo_asset_pack_set_message(message, message_size, "Self-test directory dump did not include expected entries.");
        goto cleanup;
    }
    free(dump);
    dump = NULL;

    if (rom_size > (uint64_t)SIZE_MAX) {
        tecmo_asset_pack_set_message(message, message_size, "Self-test ROM fixture is too large.");
        goto cleanup;
    }
    rom = (uint8_t *)calloc(1U, (size_t)rom_size);
    if (rom == NULL) {
        tecmo_asset_pack_set_message(message, message_size, "Self-test could not allocate ROM fixture.");
        goto cleanup;
    }
    rom[0] = 'N';
    rom[1] = 'E';
    rom[2] = 'S';
    rom[3] = 0x1AU;
    rom[4] = 8U;
    rom[5] = 32U;

    for (uint32_t bank = 0U; bank < 8U; ++bank) {
        for (size_t i = 0; i < (size_t)TECMO_ASSET_PACK_PRG_BANK_BYTES; ++i) {
            rom[(size_t)(prg_offset + (uint64_t)bank * TECMO_ASSET_PACK_PRG_BANK_BYTES) + i] =
                (uint8_t)((bank * 29U) ^ (i & 0xFFU));
        }
    }
    for (size_t i = 0; i < (size_t)chr_size; ++i) {
        rom[(size_t)chr_offset + i] = (uint8_t)(0x40U ^ (i & 0xFFU));
    }

    if (populate_self_test_opening_fixture(
            rom + (size_t)prg_offset,
            rom + (size_t)(prg_offset + 7ULL * TECMO_ASSET_PACK_PRG_BANK_BYTES)) != 0) {
        tecmo_asset_pack_set_message(message, message_size, "Self-test could not build opening screen fixture.");
        goto cleanup;
    }

    {
        uint64_t fixed_offset = prg_offset + 7ULL * TECMO_ASSET_PACK_PRG_BANK_BYTES;
        uint64_t descriptor_offset = fixed_offset +
                                     (TECMO_ASSET_PACK_SCREEN_DESCRIPTOR_CPU - 0xC000U) +
                                     TECMO_ASSET_PACK_ARENA_SCREEN_ID *
                                         TECMO_ASSET_PACK_SCREEN_DESCRIPTOR_SIZE;
        uint64_t stream_offset = prg_offset + (0xA000U - TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE);
        uint64_t palette_offset = prg_offset + (0xA100U - TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE);

        rom[(size_t)descriptor_offset + 0U] = 0U;
        rom[(size_t)descriptor_offset + 1U] = 1U;
        rom[(size_t)descriptor_offset + 2U] = 0x00U;
        rom[(size_t)descriptor_offset + 3U] = 0xA1U;
        rom[(size_t)descriptor_offset + 4U] = 0x00U;
        rom[(size_t)descriptor_offset + 5U] = 0xA0U;
        rom[(size_t)descriptor_offset + 6U] = 0U;
        memcpy(rom + (size_t)stream_offset, arena_stream, sizeof(arena_stream));
        memcpy(rom + (size_t)palette_offset, arena_palette, sizeof(arena_palette));
        rom[(size_t)(fixed_offset +
                     (TECMO_ASSET_PACK_ARENA_LOWER_R0_TABLE_CPU - 0xC000U) +
                     TECMO_ASSET_PACK_ARENA_LOWER_SELECTOR_INDEX)] = 2U;
        rom[(size_t)(fixed_offset +
                     (TECMO_ASSET_PACK_ARENA_LOWER_R1_TABLE_CPU - 0xC000U) +
                     TECMO_ASSET_PACK_ARENA_LOWER_SELECTOR_INDEX)] = 4U;
    }
    {
        uint64_t bank00_offset = prg_offset;
        uint64_t bank04_offset = prg_offset + 4ULL * TECMO_ASSET_PACK_PRG_BANK_BYTES;
        uint8_t *pointer_table = rom + (size_t)(bank00_offset +
                                                (TECMO_ASSET_PACK_ARENA_POINTER_TABLE_CPU -
                                                 TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE));
        uint8_t *jumbotron = rom + (size_t)(bank00_offset + (0xA7E3U - 0x8000U));
        uint8_t *goal = rom + (size_t)(bank00_offset + (0xA8C0U - 0x8000U));

        tecmo_asset_pack_store_u16(pointer_table + 0U, 0xA7E3U);
        tecmo_asset_pack_store_u16(pointer_table + 2U, 0xA8C0U);
        jumbotron[0] = TECMO_ASSET_PACK_ARENA_JUMBOTRON_COUNT;
        for (size_t index = 0U; index < TECMO_ASSET_PACK_ARENA_JUMBOTRON_COUNT; ++index) {
            uint8_t *record = jumbotron + 1U + index * 4U;
            record[0] = (uint8_t)(0x10U + index);
            record[1] = (uint8_t)(0x20U + index % 0x40U);
            record[2] = (uint8_t)(index & 0x03U);
            if ((index & 1U) != 0U) record[2] |= 0x20U;
            if ((index & 2U) != 0U) record[2] |= 0x40U;
            if ((index & 4U) != 0U) record[2] |= 0x80U;
            record[3] = (uint8_t)(0x20U + index * 3U);
        }
        jumbotron[1U + 1U] = 0x21U;
        jumbotron[1U + 2U] = 0xE3U;

        goal[0] = TECMO_ASSET_PACK_ARENA_GOAL_COUNT;
        for (size_t index = 0U; index < TECMO_ASSET_PACK_ARENA_GOAL_COUNT; ++index) {
            uint8_t *record = goal + 1U + index * 4U;
            record[0] = (uint8_t)(0xC0U + (index % 4U) * 0x10U);
            record[1] = (uint8_t)(0x40U + index * 2U);
            record[2] = (uint8_t)(index & 0x03U);
            if ((index & 1U) != 0U) record[2] |= 0x20U;
            if ((index & 2U) != 0U) record[2] |= 0x80U;
            record[3] = (uint8_t)(0xF0U + index);
        }
        goal[1U + 2U] = 0x42U;
        goal[1U + 7U * 4U + 0U] = TECMO_ASSET_PACK_ARENA_GOAL_CONNECTOR_RAW_Y;
        goal[1U + 7U * 4U + 1U] = 0x36U;
        goal[1U + 7U * 4U + 2U] = 0x02U;
        goal[1U + 7U * 4U + 3U] = TECMO_ASSET_PACK_ARENA_GOAL_CONNECTOR_RAW_X;
        memcpy(rom + (size_t)(bank04_offset +
                              (TECMO_ASSET_PACK_ARENA_PALETTE_CPU -
                               TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE)),
               arena_sprite_palette,
               sizeof(arena_sprite_palette));
        memcpy(rom + (size_t)(bank04_offset +
                              (TECMO_ASSET_PACK_ARENA_SEEDS_CPU -
                               TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE)),
               arena_sprite_seeds,
               sizeof(arena_sprite_seeds));
        memcpy(rom + (size_t)(bank04_offset +
                              (TECMO_ASSET_PACK_ARENA_PARAMS_CPU -
                               TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE)),
               arena_sprite_params,
               sizeof(arena_sprite_params));
    }
    {
        static const uint8_t ready_script[TECMO_ASSET_PACK_READY_SCRIPT_SIZE] = {
            0x00U,0x00U,0x00U,0x00U, 0x08U,0x00U,0x00U,0x00U,
            0x0CU,0x02U,0x00U,0x00U, 0x08U,0x0BU,0x00U,0x00U,
            0x04U,0x0EU,0x02U,0x00U, 0x00U,0x09U,0x0BU,0x00U,
            0x00U,0x04U,0x0EU,0x02U, 0x00U,0x00U,0x09U,0x0BU,
            0x00U,0x00U,0x04U,0x0EU, 0x00U,0x00U,0x00U,0x09U,
            0x00U,0x00U,0x00U,0x04U, 0x00U,0x00U,0x00U,0x00U
        };
        static const uint16_t wordmark_glyph_cpu[TECMO_ASSET_PACK_WARRIORS_WORDMARK_GLYPH_COUNT] = {
            0xAF55U, 0xAF05U, 0xAF41U, 0xAF41U,
            0xAF21U, 0xAF39U, 0xAF41U, 0xAF45U
        };
        uint64_t fixed_offset = prg_offset + 7ULL * TECMO_ASSET_PACK_PRG_BANK_BYTES;
        uint64_t bank00_offset = prg_offset;
        uint64_t bank01_offset = prg_offset + TECMO_ASSET_PACK_PRG_BANK_BYTES;
        uint64_t bank02_offset = prg_offset + 2ULL * TECMO_ASSET_PACK_PRG_BANK_BYTES;
        uint64_t bank04_offset = prg_offset + 4ULL * TECMO_ASSET_PACK_PRG_BANK_BYTES;
        uint64_t bank06_offset = prg_offset + 6ULL * TECMO_ASSET_PACK_PRG_BANK_BYTES;
        uint8_t *ready_descriptor = rom + (size_t)(fixed_offset +
            (TECMO_ASSET_PACK_SCREEN_DESCRIPTOR_CPU - 0xC000U) +
            TECMO_ASSET_PACK_READY_SCREEN_ID * TECMO_ASSET_PACK_SCREEN_DESCRIPTOR_SIZE);
        uint8_t *warriors_descriptor = rom + (size_t)(fixed_offset +
            (TECMO_ASSET_PACK_SCREEN_DESCRIPTOR_CPU - 0xC000U) +
            TECMO_ASSET_PACK_WARRIORS_SCREEN_ID * TECMO_ASSET_PACK_SCREEN_DESCRIPTOR_SIZE);
        uint8_t *clippers_descriptor = rom + (size_t)(fixed_offset +
            (TECMO_ASSET_PACK_SCREEN_DESCRIPTOR_CPU - 0xC000U) +
            TECMO_ASSET_PACK_CLIPPERS_SCREEN_ID * TECMO_ASSET_PACK_SCREEN_DESCRIPTOR_SIZE);
        uint8_t *bucks_descriptor = rom + (size_t)(fixed_offset + (0xDD34U - 0xC000U));
        uint8_t *pass_descriptor = rom + (size_t)(fixed_offset + (0xDD50U - 0xC000U));
        uint8_t *ready_stream = rom + (size_t)(bank04_offset + (0xBAF2U - 0x8000U));
        uint8_t *warriors_stream = rom + (size_t)(bank01_offset + (0xB4C5U - 0x8000U));
        uint8_t *clippers_stream = rom + (size_t)(bank01_offset + (0xB5EBU - 0x8000U));
        uint8_t *bucks_stream = rom + (size_t)(bank01_offset + (0xB401U - 0x8000U));
        uint8_t *pass_stream = rom + (size_t)(bank01_offset + (0xB933U - 0x8000U));
        size_t out = 0U;

        tecmo_asset_pack_post_arena_seed_contract_fixture(rom,
                                                          bank04_offset,
                                                          fixed_offset);
        rom[(size_t)(fixed_offset + (0xCAF5U - 0xC000U) + 0x37U)] = 6U;
        rom[(size_t)(fixed_offset + (0xCB33U - 0xC000U) + 0x37U)] = 0xAEU;
        rom[(size_t)(fixed_offset + (0xCB71U - 0xC000U) + 0x37U)] = 0x9EU;
        {
            static const uint8_t text_routine[10] = {
                0xBDU,0x60U,0xADU,0x85U,0x16U,0xBDU,0x61U,0xADU,0x85U,0x17U
            };
            static const uint8_t text_record[9] = {
                8U,'C','L','I','P','P','E','R','S'
            };
            static const uint8_t chars[8] = {'C','L','I','P','P','E','R','S'};
            static const uint8_t maps[8] = {2U,10U,7U,14U,14U,4U,15U,16U};
            static const uint8_t glyphs[8][4] = {
                {0xBAU,0xBBU,0xBCU,0xBDU},{0xD0U,0xFFU,0xC0U,0xD1U},
                {0xC7U,0xC8U,0xC9U,0xCAU},{0xB6U,0xB7U,0xB4U,0xD8U},
                {0xB6U,0xB7U,0xB4U,0xD8U},{0xB6U,0xC2U,0xB8U,0xC3U},
                {0xB6U,0xB7U,0xB4U,0xCFU},{0xD9U,0xDAU,0xDBU,0xB9U}
            };
            memcpy(rom + (size_t)(bank06_offset + (0x9EAEU - 0x8000U)),
                   text_routine,
                   sizeof(text_routine));
            tecmo_asset_pack_store_u16(rom + (size_t)(bank06_offset + (0xAD60U - 0x8000U) + 0x16U),
                      0xACA3U);
            memcpy(rom + (size_t)(bank06_offset + (0xACA3U - 0x8000U)),
                   text_record,
                   sizeof(text_record));
            for (size_t i = 0U; i < 8U; ++i) {
                rom[(size_t)(bank06_offset + (0xA273U - 0x8000U) + chars[i])] = maps[i];
                memcpy(rom + (size_t)(bank06_offset + (0xAF05U - 0x8000U) + maps[i] * 4U),
                       glyphs[i],
                       4U);
            }
        }

        ready_descriptor[0] = 0U;
        ready_descriptor[1] = 1U;
        tecmo_asset_pack_store_u16(ready_descriptor + 2U, 0xBD61U);
        tecmo_asset_pack_store_u16(ready_descriptor + 4U, 0xBAF2U);
        ready_descriptor[6] = 4U;
        warriors_descriptor[0] = 0U;
        warriors_descriptor[1] = 1U;
        tecmo_asset_pack_store_u16(warriors_descriptor + 2U, 0xB5DBU);
        tecmo_asset_pack_store_u16(warriors_descriptor + 4U, 0xB4C5U);
        warriors_descriptor[6] = 1U;
        clippers_descriptor[0] = 0x16U;
        clippers_descriptor[1] = 0x17U;
        tecmo_asset_pack_store_u16(clippers_descriptor + 2U, 0xB7C4U);
        tecmo_asset_pack_store_u16(clippers_descriptor + 4U, 0xB5EBU);
        clippers_descriptor[6] = 1U;
        memcpy(bucks_descriptor, "\x2F\x30\xA5\xB4\x01\xB4\x01", 7U);
        memcpy(pass_descriptor, "\x78\x79\xA9\xBA\x33\xB9\x01", 7U);
        memcpy(rom + (size_t)(bank04_offset +
                              (TECMO_ASSET_PACK_READY_SCRIPT_CPU - 0x8000U)),
               ready_script,
               sizeof(ready_script));
        memcpy(rom + (size_t)(bank04_offset + (0xBD61U - 0x8000U)),
               arena_palette,
               sizeof(arena_palette));
        memcpy(rom + (size_t)(bank01_offset + (0xB5DBU - 0x8000U)),
               arena_palette,
               sizeof(arena_palette));
        memcpy(rom + (size_t)(bank04_offset +
                              (TECMO_ASSET_PACK_WARRIORS_SPRITE_PALETTE_CPU - 0x8000U)),
               arena_sprite_palette,
               sizeof(arena_sprite_palette));

        ready_stream[out++] = 0xA9U;
        ready_stream[out++] = 34U;
        for (size_t i = 0U; i < 34U; ++i) ready_stream[out++] = 0x7FU;
        for (size_t i = 0U; i < 3U; ++i) {
            ready_stream[out++] = 128U;
            ready_stream[out++] = 0x7FU;
        }
        ready_stream[out++] = 0xAAU;
        for (size_t i = 0U; i < 3U; ++i) {
            ready_stream[out++] = 128U;
            ready_stream[out++] = 0x7FU;
        }
        ready_stream[out++] = 222U;
        ready_stream[out++] = 0x7FU;
        ready_stream[out++] = 0U;
        ready_stream[out++] = 0U;
        if (out != 53U) {
            tecmo_asset_pack_set_message(message, message_size, "Self-test READY stream size mismatch.");
            goto cleanup;
        }

        out = 0U;
        warriors_stream[out++] = 0xA5U;
        warriors_stream[out++] = 129U;
        for (size_t i = 0U; i < 129U; ++i) warriors_stream[out++] = 0x7FU;
        warriors_stream[out++] = 130U;
        for (size_t i = 0U; i < 130U; ++i) warriors_stream[out++] = 0x7FU;
        for (size_t i = 0U; i < 2U; ++i) {
            warriors_stream[out++] = 246U;
            warriors_stream[out++] = 0x7FU;
        }
        warriors_stream[out++] = 0xAAU;
        for (size_t i = 0U; i < 4U; ++i) {
            warriors_stream[out++] = 246U;
            warriors_stream[out++] = 0x7FU;
        }
        warriors_stream[out++] = 0x02U;
        warriors_stream[out++] = 249U;
        warriors_stream[out++] = 0x7FU;
        warriors_stream[out++] = 0U;
        if (out != 279U) {
            tecmo_asset_pack_set_message(message, message_size, "Self-test WARRIORS stream size mismatch.");
            goto cleanup;
        }

        out = 0U;
        clippers_stream[out++] = 0xA5U;
        clippers_stream[out++] = 226U;
        for (size_t i = 0U; i < 226U; ++i) clippers_stream[out++] = (uint8_t)i;
        clippers_stream[out++] = 228U;
        for (size_t i = 0U; i < 228U; ++i) clippers_stream[out++] = (uint8_t)(i + 1U);
        for (size_t i = 0U; i < 2U; ++i) {
            clippers_stream[out++] = 227U;
            clippers_stream[out++] = 0x7FU;
        }
        clippers_stream[out++] = 0xAAU;
        for (size_t i = 0U; i < 4U; ++i) {
            clippers_stream[out++] = 227U;
            clippers_stream[out++] = 0x7FU;
        }
        clippers_stream[out++] = 0x02U;
        clippers_stream[out++] = 232U;
        clippers_stream[out++] = 0x7FU;
        clippers_stream[out++] = 0U;
        if (out != 474U) {
            tecmo_asset_pack_set_message(message, message_size, "Self-test CLIPPERS stream size mismatch.");
            goto cleanup;
        }
        memcpy(rom + (size_t)(bank01_offset + (0xB7C4U - 0x8000U)),
               arena_palette,
               sizeof(arena_palette));

        {
            static const uint8_t compact_two_page_stream[20] = {
                0xAAU,0x00U,0x7FU,0x00U,0x7FU,0x00U,0x7FU,0x00U,0x7FU,
                0xAAU,0x00U,0x7FU,0x00U,0x7FU,0x00U,0x7FU,0x00U,0x7FU,
                0x00U,0x00U
            };
            static const uint8_t bucks_palette[16] = {
                0x0FU,0x17U,0x27U,0x01U,0x0FU,0x17U,0x16U,0x26U,
                0x0FU,0x17U,0x27U,0x01U,0x0FU,0x17U,0x27U,0x11U
            };
            static const uint8_t pass_palette[16] = {
                0x0FU,0x17U,0x27U,0x37U,0x0FU,0x0FU,0x16U,0x27U,
                0x0FU,0x0CU,0x01U,0x30U,0x0FU,0x0CU,0x17U,0x27U
            };
            static const uint8_t helper_palette[16] = {
                0x02U,0x01U,0x11U,0x21U,0x02U,0x17U,0x16U,0x26U,
                0x02U,0x0FU,0x09U,0x30U,0x02U,0x0FU,0x00U,0x30U
            };
            static const uint8_t special_palette[16] = {
                0x0FU,0x01U,0x11U,0x21U,0x0FU,0x0FU,0x16U,0x27U,
                0x0FU,0x0CU,0x01U,0x30U,0x0FU,0x0CU,0x17U,0x27U
            };
            static const uint8_t bucks_route[] = {
                0xA9U,0x19U,0x20U,0xF2U,0x82U,0xA9U,0x0BU,0x20U,0x6EU,0x8AU
            };
            static const uint8_t pass_route[] = {
                0xAEU,0xE9U,0x07U,0xBDU,0x3CU,0x86U,0x20U,0xF2U,0x82U,
                0xA9U,0x0DU,0x20U,0x6EU,0x8AU,0xA2U,0xFDU,0xA0U,0x89U
            };
            static const uint8_t pass_emit_helper[] = {
                0x85U,0x09U,0x84U,0x0BU,0xA9U,0x00U,0x85U,0x2DU,0x85U,0x2CU,
                0xA9U,0x01U,0x85U,0x0DU,0x85U,0x2EU,0xA9U,0x00U,0xA2U,0x0FU,
                0xA0U,0xA9U,0x4CU,0x51U,0xC0U
            };
            static const uint8_t thresholds[6] = {0xEFU,0xC0U,0x90U,0x60U,0x30U,0x00U};
            static const uint8_t bucks_record[6] = {5U,'B','U','C','K','S'};
            static const uint8_t bucks_chars[5] = {'B','U','C','K','S'};
            memcpy(bucks_stream, compact_two_page_stream, sizeof(compact_two_page_stream));
            memcpy(pass_stream, compact_two_page_stream, sizeof(compact_two_page_stream));
            memcpy(rom + (size_t)(bank01_offset + (0xB4A5U - 0x8000U)), bucks_palette, 16U);
            memcpy(rom + (size_t)(bank01_offset + (0xBAA9U - 0x8000U)), pass_palette, 16U);
            memcpy(rom + (size_t)(bank04_offset + (0x89FDU - 0x8000U)), helper_palette, 16U);
            memcpy(rom + (size_t)(bank04_offset + (0x8A1DU - 0x8000U)), special_palette, 16U);
            memcpy(rom + (size_t)(bank04_offset + (0x883DU - 0x8000U)), bucks_route, sizeof(bucks_route));
            memcpy(rom + (size_t)(bank04_offset + (0x854FU - 0x8000U)), pass_route, sizeof(pass_route));
            memcpy(rom + (size_t)(bank04_offset + (0x8645U - 0x8000U)),
                   pass_emit_helper,
                   sizeof(pass_emit_helper));
            memcpy(rom + (size_t)(bank04_offset + (0x88A3U - 0x8000U)), thresholds, sizeof(thresholds));
            rom[(size_t)(fixed_offset + (0xDC19U - 0xC000U) + 0x0EU)] = 0x2AU;
            tecmo_asset_pack_store_u16(rom + (size_t)(bank06_offset + (0xAD60U - 0x8000U) + 0x0EU * 2U), 0xACB8U);
            memcpy(rom + (size_t)(bank06_offset + (0xACB8U - 0x8000U)), bucks_record, sizeof(bucks_record));
            for (size_t i = 0U; i < sizeof(bucks_chars); ++i) {
                uint8_t map = (uint8_t)(20U + i);
                rom[(size_t)(bank06_offset + (0xA273U - 0x8000U) + bucks_chars[i])] = map;
                memset(rom + (size_t)(bank06_offset + (0xAF05U - 0x8000U) + map * 4U),
                       (int)(0x80U + i * 4U),
                       4U);
            }
        }

        tecmo_asset_pack_store_u16(rom + (size_t)(bank00_offset +
                                 (TECMO_ASSET_PACK_WARRIORS_POINTER_CPU - 0x8000U)),
                   TECMO_ASSET_PACK_WARRIORS_STREAM_CPU);
        tecmo_asset_pack_store_u16(rom + (size_t)(bank00_offset +
                                 (TECMO_ASSET_PACK_PASS_POINTER_CPU - 0x8000U) + 2U),
                   TECMO_ASSET_PACK_PASS_STREAM_CPU);
        {
            uint8_t *pieces = rom + (size_t)(bank00_offset +
                (TECMO_ASSET_PACK_WARRIORS_STREAM_CPU - 0x8000U));
            pieces[0] = TECMO_ASSET_PACK_WARRIORS_PIECE_COUNT;
            for (size_t i = 0U; i < TECMO_ASSET_PACK_WARRIORS_PIECE_COUNT; ++i) {
                pieces[1U + i * 4U + 0U] = (uint8_t)(i * 3U);
                pieces[1U + i * 4U + 1U] = (uint8_t)((i * 2U) & 0xFCU);
                pieces[1U + i * 4U + 2U] = (uint8_t)(i & 3U);
                pieces[1U + i * 4U + 3U] = (uint8_t)(i * 5U);
            }
        }
        {
            uint8_t *pieces = rom + (size_t)(bank00_offset +
                (TECMO_ASSET_PACK_PASS_STREAM_CPU - 0x8000U));
            pieces[0] = TECMO_ASSET_PACK_PASS_PIECE_COUNT;
            for (size_t i = 0U; i < TECMO_ASSET_PACK_PASS_PIECE_COUNT; ++i) {
                pieces[1U + i * 4U + 0U] = (uint8_t)((i / 3U) * 8U);
                pieces[1U + i * 4U + 1U] = (uint8_t)((i * 2U) & 0x3EU);
                pieces[1U + i * 4U + 2U] = 1U;
                pieces[1U + i * 4U + 3U] = (uint8_t)((i % 3U) * 8U);
            }
        }
        {
            static const uint8_t finale_screen_ids[TECMO_ASSET_PACK_FINALE_SCREEN_COUNT] = {
                0x1CU,0x20U,0x1FU,0x22U,0x2DU
            };
            static const uint16_t finale_stream_cpu[TECMO_ASSET_PACK_FINALE_SCREEN_COUNT] = {
                0x9000U,0x9020U,0x9040U,0x9060U,0x9080U
            };
            static const uint16_t finale_palette_cpu[TECMO_ASSET_PACK_FINALE_SCREEN_COUNT] = {
                0x9100U,0x9120U,0x9140U,0x9160U,0x9180U
            };
            static const uint8_t one_page_stream[11] = {
                0xAAU,0x00U,0x7FU,0x00U,0x7FU,0x00U,0x7FU,0x00U,0x7FU,
                0x00U,0x00U
            };
            static const uint8_t two_page_stream[20] = {
                0xAAU,0x00U,0x7FU,0x00U,0x7FU,0x00U,0x7FU,0x00U,0x7FU,
                0xAAU,0x00U,0x7FU,0x00U,0x7FU,0x00U,0x7FU,0x00U,0x7FU,
                0x00U,0x00U
            };
            static const uint8_t finale_sprite_palette[16] = {
                0x02U,0x01U,0x11U,0x21U,0x02U,0x02U,0x12U,0x22U,
                0x02U,0x03U,0x13U,0x23U,0x02U,0x04U,0x14U,0x24U
            };
            uint8_t *finale_bank04 = rom + (size_t)bank04_offset;
            tecmo_asset_pack_store_u16(finale_bank04 + (0x82DBU - 0x8000U), 0x851CU);
            tecmo_asset_pack_store_u16(finale_bank04 + (0x82DDU - 0x8000U), 0x83EAU);
            tecmo_asset_pack_store_u16(finale_bank04 + (0x82DFU - 0x8000U), 0x852EU);
            tecmo_asset_pack_store_u16(finale_bank04 + (0x82E1U - 0x8000U), 0x83AEU);
            tecmo_asset_pack_store_u16(finale_bank04 + (0x82E3U - 0x8000U), 0x8310U);
            tecmo_asset_pack_store_u16(finale_bank04 + (0x82E5U - 0x8000U), 0xFFFFU);
            finale_bank04[0x82EDU - 0x8000U] = 50U;
            finale_bank04[0x82EEU - 0x8000U] = 30U;
            finale_bank04[0x82EFU - 0x8000U] = 0U;
            finale_bank04[0x82F0U - 0x8000U] = 75U;
            finale_bank04[0x82F1U - 0x8000U] = 1U;
            finale_bank04[0x852FU - 0x8000U] = 2U;
            for (size_t selector = 0U; selector < 3U; ++selector) {
                uint8_t value = (uint8_t)(0x91U + selector * 2U);
                uint8_t destination = (uint8_t)(0x57U + selector);
                write_self_test_chr_selector(finale_bank04,
                                             (uint16_t)(0x83C2U + selector * 4U),
                                             value,
                                             destination);
                write_self_test_chr_selector(finale_bank04,
                                             (uint16_t)(0x8402U + selector * 4U),
                                             value,
                                             destination);
                write_self_test_chr_selector(finale_bank04,
                                             (uint16_t)(0x8569U + selector * 4U),
                                             value,
                                             destination);
            }
            for (size_t anchor = 0U; anchor < 4U; ++anchor) {
                finale_bank04[0x8463U - 0x8000U + anchor] =
                    (uint8_t)(0x80U + anchor * 8U);
                finale_bank04[0x8473U - 0x8000U + anchor] =
                    (uint8_t)(0x40U + anchor * 16U);
            }
            finale_bank04[0x83D8U - 0x8000U] = 0x2CU;
            finale_bank04[0x83DAU - 0x8000U] = 0x30U;
            for (size_t screen = 0U; screen < TECMO_ASSET_PACK_FINALE_SCREEN_COUNT; ++screen) {
                uint8_t *descriptor = rom + (size_t)(fixed_offset +
                    (TECMO_ASSET_PACK_SCREEN_DESCRIPTOR_CPU - 0xC000U) +
                    (uint64_t)finale_screen_ids[screen] * TECMO_ASSET_PACK_SCREEN_DESCRIPTOR_SIZE);
                uint8_t selector = (uint8_t)(screen * 2U);
                descriptor[0] = selector;
                descriptor[1] = selector;
                tecmo_asset_pack_store_u16(descriptor + 2U, finale_palette_cpu[screen]);
                tecmo_asset_pack_store_u16(descriptor + 4U, finale_stream_cpu[screen]);
                descriptor[6U] = 2U;
                memcpy(rom + (size_t)(bank02_offset + finale_stream_cpu[screen] - 0x8000U),
                       screen == 0U || screen == 3U ? one_page_stream : two_page_stream,
                       screen == 0U || screen == 3U ? sizeof(one_page_stream) : sizeof(two_page_stream));
                for (size_t color = 0U; color < 16U; ++color) {
                    rom[(size_t)(bank02_offset + finale_palette_cpu[screen] - 0x8000U + color)] =
                        (uint8_t)((color + screen) & 0x3FU);
                }
            }
            memcpy(rom + (size_t)(bank04_offset +
                                  (TECMO_ASSET_PACK_FINALE_SHORT_PALETTE_CPU - 0x8000U)),
                   finale_sprite_palette,
                   sizeof(finale_sprite_palette));
            rom[(size_t)(bank04_offset + (0x863EU - 0x8000U))] = 0x1FU;
            rom[(size_t)(bank04_offset + (0x8640U - 0x8000U))] = 0x78U;
            rom[(size_t)(bank04_offset + (0x8642U - 0x8000U))] = 0xD8U;
            rom[(size_t)(bank04_offset + (0x8644U - 0x8000U))] = 0xF8U;
            for (size_t i = 0U; i < TECMO_ASSET_PACK_FINALE_TITLE_SOURCE_SIZE; ++i) {
                uint8_t code = (uint8_t)(0x41U + i);
                uint8_t map = (uint8_t)(32U + i);
                rom[(size_t)(bank04_offset +
                             (TECMO_ASSET_PACK_FINALE_TITLE_SOURCE_CPU - 0x8000U) + i)] = code;
                rom[(size_t)(bank06_offset +
                             (TECMO_ASSET_PACK_FINALE_GLYPH_MAP_CPU - 0x8000U) + code)] = map;
                for (size_t tile = 0U; tile < 4U; ++tile) {
                    rom[(size_t)(bank06_offset +
                                 (TECMO_ASSET_PACK_FINALE_GLYPH_TABLE_CPU - 0x8000U) +
                                 (uint64_t)map * 4U + tile)] =
                        (uint8_t)(0x80U + ((i * 4U + tile) & 0x3FU));
                }
            }
            for (size_t tile = 0U; tile < 4U; ++tile) {
                rom[(size_t)(bank06_offset +
                             (TECMO_ASSET_PACK_FINALE_GLYPH_TABLE_CPU - 0x8000U) +
                             0x18U * 4U + tile)] = (uint8_t)(0x80U + tile);
            }
        }
        memset(rom + (size_t)(bank01_offset +
                              (TECMO_ASSET_PACK_WARRIORS_PATCH0_CPU - 0x8000U)),
               0x80,
               64U);
        memset(rom + (size_t)(bank01_offset +
                              (TECMO_ASSET_PACK_WARRIORS_PATCH1_CPU - 0x8000U)),
               0x81,
               64U);
        for (size_t glyph = 0U;
             glyph < TECMO_ASSET_PACK_WARRIORS_WORDMARK_GLYPH_COUNT;
             ++glyph) {
            uint8_t *source = rom + (size_t)(bank06_offset +
                                             (wordmark_glyph_cpu[glyph] - 0x8000U));
            source[0] = (uint8_t)(0x80U + glyph * 4U);
            source[1] = (uint8_t)(0x81U + glyph * 4U);
            source[2] = (uint8_t)(0x82U + glyph * 4U);
            source[3] = (uint8_t)(0x83U + glyph * 4U);
        }
    }

    if (self_test_arena_sprite_group_validation(rom,
                                                rom_size,
                                                prg_offset,
                                                chr_offset,
                                                chr_size,
                                                message,
                                                message_size) != 0) {
        goto cleanup;
    }

    if (write_self_test_file(rom_path, rom, (size_t)rom_size) != 0) {
        tecmo_asset_pack_set_message(message, message_size, "Self-test could not write temporary iNES ROM.");
        goto cleanup;
    }
    if (tecmo_asset_pack_build_from_ines_internal(rom_path,
                                                  ines_pack_path,
                                                  0,
                                                  message,
                                                  message_size) != 0) {
        goto cleanup;
    }

    if (self_test_entry_contains_text(ines_pack_path,
                                      "system/manifest",
                                      "format=tecmo.assetpack/1",
                                      message,
                                      message_size) != 0 ||
        self_test_entry_contains_text(ines_pack_path,
                                      "system/manifest",
                                      "prg_banks_16k=8",
                                      message,
                                      message_size) != 0 ||
        self_test_entry_contains_text(ines_pack_path,
                                      "system/manifest",
                                      "chr_banks_8k=32",
                                      message,
                                      message_size) != 0 ||
        self_test_entry_contains_text(ines_pack_path,
                                      "system/manifest",
                                      "logical_entry_prefixes=arena/intro/",
                                      message,
                                      message_size) != 0 ||
        self_test_entry_contains_text(ines_pack_path,
                                      "system/manifest",
                                      "input_contract=ines-only",
                                      message,
                                      message_size) != 0 ||
        self_test_entry_contains_text(ines_pack_path,
                                      "system/source-map",
                                      "\"format\":\"tecmo.assetpack.source-map/1\"",
                                      message,
                                      message_size) != 0 ||
        self_test_entry_contains_text(ines_pack_path,
                                      "system/source-map",
                                      "\"input_contract\":\"ines-only\"",
                                      message,
                                      message_size) != 0 ||
        self_test_entry_contains_text(ines_pack_path,
                                      "system/source-map",
                                      "\"id\":\"" TECMO_ASSET_PACK_PRESENTS_ID "\"",
                                      message,
                                      message_size) != 0 ||
        self_test_entry_contains_text(ines_pack_path,
                                      "system/source-map",
                                      "\"id\":\"" TECMO_ASSET_PACK_LICENSE_ID "\"",
                                      message,
                                      message_size) != 0 ||
        self_test_entry_contains_text(ines_pack_path,
                                      "system/source-map",
                                      "\"palette_frames\":[0,4,8,12,16,123,125,127,129]",
                                      message,
                                      message_size) != 0 ||
        self_test_entry_contains_text(ines_pack_path,
                                      "system/source-map",
                                      "\"encoded_size\":116,\"decoded_size\":1024",
                                      message,
                                      message_size) != 0 ||
        self_test_entry_contains_text(ines_pack_path,
                                      "system/source-map",
                                      "\"id\":\"" TECMO_ASSET_PACK_ARENA_INTRO_SCRIPT_ID "\"",
                                      message,
                                      message_size) != 0 ||
        self_test_entry_contains_text(ines_pack_path,
                                      "system/source-map",
                                      "\"id\":\"" TECMO_ASSET_PACK_ARENA_INTRO_BACKGROUND_ID "\"",
                                      message,
                                      message_size) != 0 ||
        self_test_entry_contains_text(ines_pack_path,
                                      "system/source-map",
                                      "\"screen_id\":24,\"decoder_cpu_address\":55798",
                                      message,
                                      message_size) != 0 ||
        self_test_entry_contains_text(ines_pack_path,
                                      "system/source-map",
                                      "\"cpu_address\":56621",
                                      message,
                                      message_size) != 0 ||
        self_test_entry_contains_text(ines_pack_path,
                                      "system/source-map",
                                      "\"encoded_size\":28,\"decoded_size\":2048",
                                      message,
                                      message_size) != 0 ||
        self_test_entry_contains_text(ines_pack_path,
                                      "system/source-map",
                                      "\"cpu_address\":64892,\"selector_cpu_address\":64893",
                                      message,
                                      message_size) != 0 ||
        self_test_entry_contains_text(ines_pack_path,
                                      "system/source-map",
                                      "\"cpu_address\":64896,\"selector_cpu_address\":64897",
                                      message,
                                      message_size) != 0 ||
        self_test_entry_contains_text(ines_pack_path,
                                      "system/source-map",
                                      "\"id\":\"" TECMO_ASSET_PACK_ARENA_INTRO_PALETTE_ID "\"",
                                      message,
                                      message_size) != 0 ||
        self_test_entry_contains_text(ines_pack_path,
                                      "system/source-map",
                                      "\"id\":\"" TECMO_ASSET_PACK_ARENA_INTRO_SPRITE_GROUPS_ID "\"",
                                      message,
                                      message_size) != 0 ||
        self_test_entry_contains_text(ines_pack_path,
                                      "system/source-map",
                                      "\"schema\":\"tecmo.arena-intro.sprite-groups/TASG-2\"",
                                      message,
                                      message_size) != 0 ||
        self_test_entry_contains_text(ines_pack_path,
                                      "system/source-map",
                                      "\"pointer_table\":{\"source_entry\":\"prg/bank00\"",
                                      message,
                                      message_size) != 0 ||
        self_test_entry_contains_text(ines_pack_path,
                                      "system/source-map",
                                      "\"record_count\":55",
                                      message,
                                      message_size) != 0 ||
        self_test_entry_contains_text(ines_pack_path,
                                      "system/source-map",
                                      "\"record_count\":16",
                                      message,
                                      message_size) != 0 ||
        self_test_entry_contains_text(ines_pack_path,
                                      "system/source-map",
                                      "\"emitter\":{\"source_entry\":\"prg/bank04\"",
                                      message,
                                      message_size) != 0 ||
        self_test_entry_contains_text(ines_pack_path,
                                      "system/source-map",
                                      "\"chr_pages\":[{\"source_entry\":\"chr/bank01\"",
                                      message,
                                      message_size) != 0 ||
        self_test_entry_contains_text(ines_pack_path,
                                      "system/source-map",
                                      "\"id\":\"prg/bank00\"",
                                      message,
                                      message_size) != 0 ||
        self_test_entry_contains_text(ines_pack_path,
                                      "system/source-map",
                                      "\"id\":\"prg/bank01\"",
                                      message,
                                      message_size) != 0 ||
        self_test_entry_contains_text(ines_pack_path,
                                      "system/source-map",
                                      "\"id\":\"chr/bank00\"",
                                      message,
                                      message_size) != 0 ||
        self_test_entry_contains_text(ines_pack_path,
                                      "system/source-map",
                                      "\"id\":\"" TECMO_ASSET_PACK_FINALE_ID "\"",
                                      message,
                                      message_size) != 0 ||
        self_test_entry_contains_text(ines_pack_path,
                                      "system/source-map",
                                      "\"schema\":\"tecmo.intro.finale/TFIN-1\"",
                                      message,
                                      message_size) != 0 ||
        self_test_entry_contains_text(ines_pack_path,
                                      "system/source-map",
                                      "\"role\":\"selector-two-indexed-operands\"",
                                      message,
                                      message_size) != 0 ||
        self_test_entry_contains_text(ines_pack_path,
                                      "system/source-map",
                                      "\"title_slots\":44,\"title_source_slots\":26,\"blank_slots\":18",
                                      message,
                                      message_size) != 0) {
        goto cleanup;
    }

    if (self_test_entry_contains_text(ines_pack_path,
                                      TECMO_ASSET_PACK_ARENA_INTRO_SCRIPT_ID,
                                      "\"format\":\"tecmo.arena-intro.script/1\"",
                                      message,
                                      message_size) != 0 ||
        self_test_entry_contains_text(ines_pack_path,
                                      TECMO_ASSET_PACK_ARENA_INTRO_PALETTE_ID,
                                      "\"format\":\"tecmo.arena-intro.palette-cycle/1\"",
                                      message,
                                      message_size) != 0 ||
        self_test_entry_contains_text(ines_pack_path,
                                      TECMO_ASSET_PACK_ARENA_INTRO_SCRIPT_ID,
                                      "\"source_route\":\"bank04:C-0127\"",
                                      message,
                                      message_size) != 0) {
        goto cleanup;
    }
    if (self_test_arena_background_layer(ines_pack_path,
                                         arena_palette,
                                         message,
                                         message_size) != 0) {
        goto cleanup;
    }
    if (self_test_arena_sprite_groups(ines_pack_path,
                                      expected_sprite_palette,
                                      message,
                                      message_size) != 0) {
        goto cleanup;
    }

    if (self_test_read_and_compare(ines_pack_path,
                                   "prg/bank00",
                                   rom + (size_t)prg_offset,
                                   TECMO_ASSET_PACK_PRG_BANK_BYTES,
                                   message,
                                   message_size) != 0 ||
        self_test_read_and_compare(ines_pack_path,
                                   "prg/bank01",
                                   rom + (size_t)(prg_offset + TECMO_ASSET_PACK_PRG_BANK_BYTES),
                                   TECMO_ASSET_PACK_PRG_BANK_BYTES,
                                   message,
                                   message_size) != 0 ||
        self_test_read_and_compare(ines_pack_path,
                                   "prg/fixed",
                                   rom + (size_t)(prg_offset + 7ULL * TECMO_ASSET_PACK_PRG_BANK_BYTES),
                                   TECMO_ASSET_PACK_PRG_BANK_BYTES,
                                   message,
                                   message_size) != 0 ||
        self_test_read_and_compare(ines_pack_path,
                                   "chr/all",
                                   rom + (size_t)chr_offset,
                                   chr_size,
                                   message,
                                   message_size) != 0 ||
        self_test_read_and_compare(ines_pack_path,
                                   "chr/bank00",
                                   rom + (size_t)chr_offset,
                                   TECMO_ASSET_PACK_CHR_BANK_BYTES,
                                   message,
                                   message_size) != 0) {
        goto cleanup;
    }

    memset(&ines_list_state, 0, sizeof(ines_list_state));
    if (tecmo_asset_pack_list_entries(ines_pack_path,
                                      self_test_ines_list_entry,
                                      &ines_list_state) != 0 ||
        ines_list_state.count != 56U ||
        !ines_list_state.saw_manifest ||
        !ines_list_state.saw_source_map ||
        !ines_list_state.saw_presents ||
        !ines_list_state.presents_metadata_valid ||
        !ines_list_state.saw_license ||
        !ines_list_state.license_metadata_valid ||
        !ines_list_state.saw_arena_intro_script ||
        !ines_list_state.saw_arena_intro_background ||
        !ines_list_state.arena_intro_background_metadata_valid ||
        !ines_list_state.saw_arena_intro_palette ||
        !ines_list_state.saw_arena_intro_sprite_groups ||
        !ines_list_state.arena_intro_sprite_groups_metadata_valid ||
        !ines_list_state.saw_ready ||
        !ines_list_state.ready_metadata_valid ||
        !ines_list_state.saw_warriors ||
        !ines_list_state.warriors_metadata_valid ||
        !ines_list_state.saw_clippers ||
        !ines_list_state.clippers_metadata_valid ||
        !ines_list_state.saw_bucks ||
        !ines_list_state.bucks_metadata_valid ||
        !ines_list_state.saw_pass ||
        !ines_list_state.pass_metadata_valid ||
        !ines_list_state.saw_finale ||
        !ines_list_state.finale_metadata_valid ||
        !ines_list_state.saw_prg_bank0 ||
        !ines_list_state.saw_prg_bank1 ||
        !ines_list_state.saw_prg_fixed ||
        !ines_list_state.saw_chr_all ||
        !ines_list_state.saw_chr_bank0 ||
        !ines_list_state.saw_chr_bank1) {
        tecmo_asset_pack_set_message(message, message_size, "Self-test iNES pack directory enumeration mismatch.");
        goto cleanup;
    }

    tecmo_asset_pack_set_message(message, message_size, "Asset pack self-test passed.");
    result = 0;

cleanup:
    if (builder != NULL) {
        tecmo_asset_pack_builder_cancel(builder);
    }
    free(dump);
    free(rom);
    remove_self_test_file(builder_pack_path);
    remove_self_test_file(local_file_path);
    remove_self_test_file(rom_path);
    remove_self_test_file(ines_pack_path);
    remove_self_test_dir(temp_dir);
    return result;
}
