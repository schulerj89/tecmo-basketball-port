#include "tecmo_game.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COURT_LEFT 48
#define COURT_TOP 54
#define COURT_RIGHT 592
#define COURT_BOTTOM 424
#define TITLE_CHR_BANK_BYTES 8192U
#define INTRO_CANVAS_CELL_SIZE 16
#define INTRO_CANVAS_CELLS_X 17
#define INTRO_CANVAS_CELLS_Y 14

static bool pressed(bool now, bool before)
{
    return now && !before;
}

static int text_equals(const char *a, const char *b)
{
    return strcmp(a, b) == 0;
}

static uint32_t rgb(uint8_t r, uint8_t g, uint8_t b)
{
    return 0xFF000000U | ((uint32_t)r << 16U) | ((uint32_t)g << 8U) | (uint32_t)b;
}

static const char *mode_name(TecmoPlayMode mode)
{
    if (mode == TECMO_MODE_MAIN_MENU) {
        return "MAIN MENU";
    }
    if (mode == TECMO_MODE_TITLE_SCREEN) {
        return "TITLE SCREEN";
    }
    if (mode == TECMO_MODE_INTRO_PROBE) {
        return "INTRO LAB";
    }
    if (mode == TECMO_MODE_CHR_PLAYGROUND) {
        return "CHR PLAYGROUND";
    }
    if (mode == TECMO_MODE_PLAY_SETUP) {
        return "PLAY SETUP";
    }
    if (mode == TECMO_MODE_ROSTERS) {
        return "ROSTERS";
    }
    if (mode == TECMO_MODE_COURT) {
        return "COURT";
    }
    return "UNKNOWN";
}

static void add_team_if_missing(TecmoRuntime *runtime, const char *team)
{
    for (size_t i = 0; i < runtime->team_count; ++i) {
        if (text_equals(runtime->teams[i], team)) {
            return;
        }
    }
    if (runtime->team_count < sizeof(runtime->teams) / sizeof(runtime->teams[0])) {
        size_t len = strlen(team);
        if (len >= sizeof(runtime->teams[0])) {
            len = sizeof(runtime->teams[0]) - 1U;
        }
        memcpy(runtime->teams[runtime->team_count], team, len);
        runtime->teams[runtime->team_count][len] = '\0';
        ++runtime->team_count;
    }
}

static const RosterRecord *selected_player_record(const TecmoRuntime *runtime)
{
    size_t seen = 0;
    if (runtime->team_count == 0 || runtime->selected_team >= runtime->team_count) {
        return 0;
    }

    for (size_t i = 0; i < runtime->roster.count; ++i) {
        const RosterRecord *record = &runtime->roster.records[i];
        if (text_equals(record->team, runtime->teams[runtime->selected_team])) {
            if (seen == runtime->selected_player) {
                return record;
            }
            ++seen;
        }
    }
    return 0;
}

static size_t selected_team_player_count(const TecmoRuntime *runtime)
{
    size_t count = 0;
    if (runtime->team_count == 0 || runtime->selected_team >= runtime->team_count) {
        return 0;
    }
    for (size_t i = 0; i < runtime->roster.count; ++i) {
        if (text_equals(runtime->roster.records[i].team, runtime->teams[runtime->selected_team])) {
            ++count;
        }
    }
    return count;
}

static uint32_t chr_bank_count(const TecmoRuntime *runtime)
{
    uint64_t count = runtime->title_chr_byte_count / TITLE_CHR_BANK_BYTES;
    if (count == 0) {
        return 1U;
    }
    if (count > 32U) {
        count = 32U;
    }
    return (uint32_t)count;
}

static uint32_t selected_chr_bank(const TecmoRuntime *runtime)
{
    uint32_t count = chr_bank_count(runtime);
    if (runtime->selected_chr_bank >= count) {
        return count - 1U;
    }
    return runtime->selected_chr_bank;
}

static uint32_t selected_chr_table(const TecmoRuntime *runtime)
{
    return runtime->selected_chr_table & 1U;
}

static uint16_t selected_intro_tile_id(const TecmoRuntime *runtime)
{
    return (uint16_t)(selected_chr_table(runtime) * 0x100U + (runtime->intro_source_tile & 0xFFU));
}

size_t tecmo_intro_stage_sprite_records(const TecmoIntroSpriteRecord *records,
                                        size_t record_count,
                                        const TecmoIntroSpriteStageConfig *config,
                                        TecmoIntroStagedSprite *entries,
                                        size_t entry_capacity)
{
    size_t staged_count = 0;
    if (records == NULL || config == NULL || entries == NULL || entry_capacity == 0) {
        return 0;
    }

    for (size_t i = 0; i < record_count && staged_count < entry_capacity; ++i) {
        const TecmoIntroSpriteRecord *record = &records[i];
        TecmoIntroStagedSprite *entry = &entries[staged_count++];
        entry->y = (uint8_t)(config->base_y + record->relative_y);
        entry->tile = (uint8_t)(record->tile + config->tile_offset);
        entry->attributes = record->attributes;
        entry->x = (uint8_t)(config->base_x + record->relative_x);
    }

    return staged_count;
}

static void set_intro_stage_test_message(char *dest, size_t dest_size, const char *text)
{
    if (dest == NULL || dest_size == 0) {
        return;
    }
    if (text == NULL) {
        text = "";
    }
    (void)snprintf(dest, dest_size, "%s", text);
}

static bool check_intro_stage_entry(const TecmoIntroStagedSprite *entry,
                                    uint8_t y,
                                    uint8_t tile,
                                    uint8_t attributes,
                                    uint8_t x)
{
    return entry->y == y &&
           entry->tile == tile &&
           entry->attributes == attributes &&
           entry->x == x;
}

bool tecmo_intro_stage_self_test(char *message, size_t message_size)
{
    const TecmoIntroSpriteRecord records[] = {
        {2, 0x10U, 0x01U, 3},
        {-30, 0x20U, 0x02U, 4},
        {5, 0xF8U, 0x03U, 300},
    };
    const TecmoIntroSpriteStageConfig config = {10, 20, 0x0DU};
    TecmoIntroStagedSprite entries[3];
    uint16_t pair[2] = {0, 0};
    size_t count;

    if (message != NULL && message_size > 0) {
        message[0] = '\0';
    }

    if (tecmo_intro_stage_sprite_records(NULL, 1, &config, entries, 3) != 0 ||
        tecmo_intro_stage_sprite_records(records, 1, NULL, entries, 3) != 0 ||
        tecmo_intro_stage_sprite_records(records, 1, &config, NULL, 3) != 0 ||
        tecmo_intro_stage_sprite_records(records, 1, &config, entries, 0) != 0) {
        set_intro_stage_test_message(message, message_size, "NULL OR ZERO INPUT CONTRACT FAILED");
        return false;
    }

    memset(entries, 0, sizeof(entries));
    count = tecmo_intro_stage_sprite_records(records, 3, &config, entries, 2);
    if (count != 2) {
        set_intro_stage_test_message(message, message_size, "CAPACITY TRUNCATION CONTRACT FAILED");
        return false;
    }
    if (!check_intro_stage_entry(&entries[0], 0x16U, 0x1DU, 0x01U, 0x0DU)) {
        set_intro_stage_test_message(message, message_size, "FIRST STAGED ENTRY CONTRACT FAILED");
        return false;
    }
    if (!check_intro_stage_entry(&entries[1], 0xF6U, 0x2DU, 0x02U, 0x0EU)) {
        set_intro_stage_test_message(message, message_size, "BYTE WRAP STAGED ENTRY CONTRACT FAILED");
        return false;
    }

    memset(entries, 0, sizeof(entries));
    count = tecmo_intro_stage_sprite_records(records, 3, &config, entries, 3);
    if (count != 3) {
        set_intro_stage_test_message(message, message_size, "FULL STAGED COUNT CONTRACT FAILED");
        return false;
    }
    if (!check_intro_stage_entry(&entries[2], 0x19U, 0x05U, 0x03U, 0x36U)) {
        set_intro_stage_test_message(message, message_size, "TILE AND X WRAP CONTRACT FAILED");
        return false;
    }

    tecmo_intro_sprite_8x16_pair_for_table(0x25U, 1U, pair);
    if (pair[0] != 0x124U || pair[1] != 0x125U) {
        set_intro_stage_test_message(message, message_size, "RABBIT 8X16 TILE PAIR CONTRACT FAILED");
        return false;
    }
    tecmo_intro_sprite_8x16_pair_for_table(0x80U, 1U, pair);
    if (pair[0] != 0x180U || pair[1] != 0x181U) {
        set_intro_stage_test_message(message, message_size, "TECMO 8X16 TILE PAIR CONTRACT FAILED");
        return false;
    }
    tecmo_intro_sprite_8x16_pair_for_table(0x25U, 0U, pair);
    if (pair[0] != 0x024U || pair[1] != 0x025U) {
        set_intro_stage_test_message(message, message_size, "TABLE ZERO 8X16 TILE PAIR CONTRACT FAILED");
        return false;
    }

    set_intro_stage_test_message(message, message_size, "INTRO SPRITE STAGING SELF TEST PASS");
    return true;
}

void tecmo_intro_sprite_8x16_pair_for_table(uint8_t oam_tile_low, uint32_t chr_table, uint16_t out_tiles[2])
{
    if (out_tiles == NULL) {
        return;
    }

    out_tiles[0] = (uint16_t)((chr_table & 1U) * 0x100U + (uint16_t)(oam_tile_low & 0xFEU));
    out_tiles[1] = (uint16_t)(out_tiles[0] + 1U);
}

uint16_t tecmo_intro_oam_tile_pair_top(uint8_t oam_tile_low, uint32_t chr_table)
{
    uint16_t pair[2] = {0, 0};
    tecmo_intro_sprite_8x16_pair_for_table(oam_tile_low, chr_table, pair);
    return pair[0];
}

uint16_t tecmo_intro_oam_tile_pair_bottom(uint8_t oam_tile_low, uint32_t chr_table)
{
    uint16_t pair[2] = {0, 0};
    tecmo_intro_sprite_8x16_pair_for_table(oam_tile_low, chr_table, pair);
    return pair[1];
}

static void set_runtime_status(char *dest, size_t dest_size, const char *text)
{
    if (dest_size == 0) {
        return;
    }
    if (text == NULL) {
        text = "";
    }
    (void)snprintf(dest, dest_size, "%s", text);
}

static int write_intro_layout_file_to_path(const TecmoRuntime *runtime, const char *path)
{
    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        return -1;
    }

    fprintf(file, "{\n");
    fprintf(file, "  \"schema_version\": 1,\n");
    fprintf(file, "  \"capture_mode\": \"intro_lab\",\n");
    fprintf(file, "  \"data_policy\": \"Local-only user tile placement records; no ROM, CHR, ASM, palette, or screenshot data.\",\n");
    fprintf(file, "  \"selected_chr_bank\": %u,\n", (unsigned)selected_chr_bank(runtime));
    fprintf(file, "  \"selected_chr_table\": %u,\n", (unsigned)selected_chr_table(runtime));
    fprintf(file, "  \"selected_tile_id_hex\": \"%03X\",\n", (unsigned)selected_intro_tile_id(runtime));
    fprintf(file, "  \"coordinate_space\": {\n");
    fprintf(file, "    \"origin\": \"intro_lab_canvas_top_left\",\n");
    fprintf(file, "    \"canvas_cell_px\": %u,\n", (unsigned)INTRO_CANVAS_CELL_SIZE);
    fprintf(file, "    \"source_tile_px\": 8,\n");
    fprintf(file, "    \"preview_scale\": 2\n");
    fprintf(file, "  },\n");
    fprintf(file, "  \"placements\": [\n");
    for (size_t i = 0; i < runtime->intro_placement_count; ++i) {
        const TecmoIntroPlacement *placement = &runtime->intro_placements[i];
        uint16_t tile = placement->tile_count > 0 ? placement->tile_ids[0] : 0U;
        fprintf(file, "    {\n");
        fprintf(file, "      \"id\": \"placement_%03u\",\n", (unsigned)(i + 1U));
        fprintf(file, "      \"chr_bank\": %u,\n", (unsigned)placement->chr_bank);
        fprintf(file, "      \"chr_table\": %u,\n", (unsigned)placement->chr_table);
        fprintf(file, "      \"tile_ids_hex\": [\"%03X\"],\n", (unsigned)tile);
        fprintf(file, "      \"canvas_cell_x\": %d,\n", placement->canvas_cell_x);
        fprintf(file, "      \"canvas_cell_y\": %d,\n", placement->canvas_cell_y);
        fprintf(file, "      \"pixel_x\": %d,\n", placement->pixel_x);
        fprintf(file, "      \"pixel_y\": %d,\n", placement->pixel_y);
        fprintf(file, "      \"scale\": %d,\n", placement->scale);
        fprintf(file, "      \"label\": \"%s\"\n", placement->label);
        fprintf(file, "    }%s\n", i + 1U < runtime->intro_placement_count ? "," : "");
    }
    fprintf(file, "  ]\n");
    fprintf(file, "}\n");

    return fclose(file);
}

static int save_intro_layout_file(TecmoRuntime *runtime)
{
    if (write_intro_layout_file_to_path(runtime, "build\\intro_layout_picks.json") == 0) {
        runtime->intro_layout_saved = true;
        runtime->intro_layout_dirty = false;
        set_runtime_status(runtime->intro_layout_status,
                           sizeof(runtime->intro_layout_status),
                           "SAVED BUILD INTRO_LAYOUT_PICKS.JSON");
        return 0;
    }
    if (write_intro_layout_file_to_path(runtime, "intro_layout_picks.json") == 0) {
        runtime->intro_layout_saved = true;
        runtime->intro_layout_dirty = false;
        set_runtime_status(runtime->intro_layout_status,
                           sizeof(runtime->intro_layout_status),
                           "SAVED INTRO_LAYOUT_PICKS.JSON");
        return 0;
    }

    runtime->intro_layout_saved = false;
    set_runtime_status(runtime->intro_layout_status,
                       sizeof(runtime->intro_layout_status),
                       "SAVE FAILED");
    return -1;
}

bool tecmo_runtime_init(TecmoRuntime *runtime, TecmoGameMemory *memory, const char *project_root)
{
    memset(runtime, 0, sizeof(*runtime));
    runtime->memory = memory;
    runtime->selected_chr_bank = 31U;
    runtime->intro_source_tile = 0x80U;
    runtime->intro_canvas_cell_x = 4;
    runtime->intro_canvas_cell_y = 4;
    set_runtime_status(runtime->intro_layout_status,
                       sizeof(runtime->intro_layout_status),
                       "SPACE RECORD  S SAVE  BACKSPACE REMOVE");

    if (tecmo_collect_rosters(project_root, &runtime->roster) != 0) {
        return false;
    }

    for (size_t i = 0; i < runtime->roster.count; ++i) {
        add_team_if_missing(runtime, runtime->roster.records[i].team);
    }

    if (tecmo_load_original_title_glyphs(project_root, &runtime->title_glyphs) == 0 &&
        tecmo_load_chr_data(project_root, &runtime->title_chr_bytes, &runtime->title_chr_byte_count) == 0) {
        if (runtime->selected_chr_bank >= chr_bank_count(runtime)) {
            runtime->selected_chr_bank = chr_bank_count(runtime) - 1U;
        }
        runtime->title_probe_available = true;
        (void)tecmo_load_title_glyphs_for_text(project_root, "TECMO PRESENTS", &runtime->intro_glyphs);
    } else {
        tecmo_free_buffer(runtime->title_chr_bytes);
        runtime->title_chr_bytes = NULL;
        runtime->title_chr_byte_count = 0;
    }

    runtime->mode = TECMO_MODE_TITLE_SCREEN;
    runtime->frame_seconds = 1.0f / 60.0f;
    runtime->player_x = 320.0f;
    runtime->player_y = 260.0f;
    runtime->ball_x = runtime->player_x + 14.0f;
    runtime->ball_y = runtime->player_y - 8.0f;
    return runtime->team_count > 0;
}

void tecmo_runtime_shutdown(TecmoRuntime *runtime)
{
    tecmo_free_buffer(runtime->title_chr_bytes);
    runtime->title_chr_bytes = NULL;
    runtime->title_chr_byte_count = 0;
    roster_table_free(&runtime->roster);
}

void tecmo_runtime_set_mode(TecmoRuntime *runtime, TecmoPlayMode mode)
{
    runtime->mode = mode;
    runtime->previous_input = (TecmoInput){0};
}

static void update_main_menu(TecmoRuntime *runtime, const TecmoInput *input)
{
    const size_t menu_count = 6;

    if (pressed(input->up, runtime->previous_input.up) && runtime->selected_menu_item > 0) {
        --runtime->selected_menu_item;
    }
    if (pressed(input->down, runtime->previous_input.down) &&
        runtime->selected_menu_item + 1U < menu_count) {
        ++runtime->selected_menu_item;
    }
    if (pressed(input->cancel, runtime->previous_input.cancel)) {
        runtime->quit_requested = true;
    }
    if (pressed(input->confirm, runtime->previous_input.confirm)) {
        if (runtime->selected_menu_item == 0) {
            runtime->mode = TECMO_MODE_TITLE_SCREEN;
        } else if (runtime->selected_menu_item == 1) {
            runtime->mode = TECMO_MODE_INTRO_PROBE;
        } else if (runtime->selected_menu_item == 2) {
            runtime->mode = TECMO_MODE_CHR_PLAYGROUND;
        } else if (runtime->selected_menu_item == 3) {
            runtime->mode = TECMO_MODE_PLAY_SETUP;
        } else if (runtime->selected_menu_item == 4) {
            runtime->mode = TECMO_MODE_ROSTERS;
        } else {
            runtime->quit_requested = true;
        }
    }
}

static void update_title_screen(TecmoRuntime *runtime, const TecmoInput *input)
{
    if (pressed(input->confirm, runtime->previous_input.confirm)) {
        runtime->mode = TECMO_MODE_MAIN_MENU;
    } else if (pressed(input->cancel, runtime->previous_input.cancel)) {
        runtime->quit_requested = true;
    }
}

static int append_intro_tile_placement(TecmoRuntime *runtime,
                                       uint32_t chr_bank,
                                       uint32_t chr_table,
                                       uint16_t tile_id,
                                       int canvas_cell_x,
                                       int canvas_cell_y,
                                       const char *label)
{
    TecmoIntroPlacement *placement;
    if (runtime->intro_placement_count >= TECMO_MAX_INTRO_PLACEMENTS) {
        return -1;
    }

    if (canvas_cell_x < 0) {
        canvas_cell_x = 0;
    }
    if (canvas_cell_x >= INTRO_CANVAS_CELLS_X) {
        canvas_cell_x = INTRO_CANVAS_CELLS_X - 1;
    }
    if (canvas_cell_y < 0) {
        canvas_cell_y = 0;
    }
    if (canvas_cell_y >= INTRO_CANVAS_CELLS_Y) {
        canvas_cell_y = INTRO_CANVAS_CELLS_Y - 1;
    }

    placement = &runtime->intro_placements[runtime->intro_placement_count++];
    memset(placement, 0, sizeof(*placement));
    placement->active = true;
    placement->chr_bank = chr_bank;
    placement->chr_table = chr_table & 1U;
    placement->tile_ids[0] = tile_id;
    placement->tile_count = 1;
    placement->canvas_cell_x = canvas_cell_x;
    placement->canvas_cell_y = canvas_cell_y;
    placement->pixel_x = canvas_cell_x * INTRO_CANVAS_CELL_SIZE;
    placement->pixel_y = canvas_cell_y * INTRO_CANVAS_CELL_SIZE;
    placement->scale = 2;
    if (label != NULL && label[0] != '\0') {
        (void)snprintf(placement->label, sizeof(placement->label), "%s", label);
    } else {
        (void)snprintf(placement->label,
                       sizeof(placement->label),
                       "B%02u T%u %03X",
                       (unsigned)placement->chr_bank,
                       (unsigned)placement->chr_table,
                       (unsigned)placement->tile_ids[0]);
    }

    runtime->intro_layout_dirty = true;
    runtime->intro_layout_saved = false;
    return 0;
}

static void add_intro_placement(TecmoRuntime *runtime)
{
    char label[32];
    uint16_t tile = selected_intro_tile_id(runtime);
    int cell_x = runtime->intro_canvas_cell_x;
    int cell_y = runtime->intro_canvas_cell_y;

    (void)snprintf(label,
                   sizeof(label),
                   "B%02u T%u %03X",
                   (unsigned)selected_chr_bank(runtime),
                   (unsigned)selected_chr_table(runtime),
                   (unsigned)tile);
    if (append_intro_tile_placement(runtime,
                                    selected_chr_bank(runtime),
                                    selected_chr_table(runtime),
                                    tile,
                                    cell_x,
                                    cell_y,
                                    label) != 0) {
        set_runtime_status(runtime->intro_layout_status,
                           sizeof(runtime->intro_layout_status),
                           "PLACEMENT LIST FULL");
        return;
    }

    (void)snprintf(runtime->intro_layout_status,
                   sizeof(runtime->intro_layout_status),
                   "REC %s CELL %02d %02d",
                   label,
                   cell_x,
                   cell_y);
}

static void add_intro_rabbit_head_candidate(TecmoRuntime *runtime)
{
    typedef struct RabbitSprite {
        uint8_t raw_tile_low;
        int dx;
        const char *label;
    } RabbitSprite;

    /*
     * Bank 04 L88E7 seeds the stream that fixed helper C051/D861 stages as
     * 8x16 sprite tile IDs 25, 27, 29, and 2B. In 8x16 mode those imply the
     * table-1 8x8 pairs below for Intro Lab inspection.
     */
    static const RabbitSprite rabbit_sprites[] = {
        {0x24U, 0, "RAB25"},
        {0x26U, 1, "RAB27"},
        {0x28U, 2, "RAB29"},
        {0x2AU, 3, "RAB2B"},
    };
    TecmoIntroSpriteStageConfig stage_config = {0, 0, 1U};
    TecmoIntroSpriteRecord records[sizeof(rabbit_sprites) / sizeof(rabbit_sprites[0])];
    TecmoIntroStagedSprite entries[sizeof(rabbit_sprites) / sizeof(rabbit_sprites[0])];
    uint32_t chr_bank = 31U;
    const uint32_t chr_table = 1U;
    int base_x = runtime->intro_canvas_cell_x;
    int base_y = runtime->intro_canvas_cell_y;
    size_t before = runtime->intro_placement_count;

    if (chr_bank >= chr_bank_count(runtime)) {
        chr_bank = selected_chr_bank(runtime);
    }
    if (base_x > INTRO_CANVAS_CELLS_X - 4) {
        base_x = INTRO_CANVAS_CELLS_X - 4;
    }
    if (base_y > INTRO_CANVAS_CELLS_Y - 2) {
        base_y = INTRO_CANVAS_CELLS_Y - 2;
    }
    if (base_x < 0) {
        base_x = 0;
    }
    if (base_y < 0) {
        base_y = 0;
    }

    runtime->selected_chr_bank = chr_bank;
    runtime->selected_chr_table = chr_table;
    runtime->intro_source_tile = 0x25U;
    runtime->intro_canvas_focus = true;
    runtime->intro_canvas_cell_x = base_x;
    runtime->intro_canvas_cell_y = base_y;

    for (size_t i = 0; i < sizeof(rabbit_sprites) / sizeof(rabbit_sprites[0]); ++i) {
        const RabbitSprite *sprite = &rabbit_sprites[i];
        records[i].relative_y = 0;
        records[i].tile = sprite->raw_tile_low;
        records[i].attributes = 0;
        records[i].relative_x = sprite->dx * INTRO_CANVAS_CELL_SIZE;
    }

    (void)tecmo_intro_stage_sprite_records(records,
                                           sizeof(records) / sizeof(records[0]),
                                           &stage_config,
                                           entries,
                                           sizeof(entries) / sizeof(entries[0]));

    for (size_t i = 0; i < sizeof(rabbit_sprites) / sizeof(rabbit_sprites[0]); ++i) {
        const RabbitSprite *sprite = &rabbit_sprites[i];
        uint16_t top_tile = tecmo_intro_oam_tile_pair_top(entries[i].tile, chr_table);
        uint16_t bottom_tile = tecmo_intro_oam_tile_pair_bottom(entries[i].tile, chr_table);
        char label[32];
        (void)snprintf(label, sizeof(label), "%s TOP", sprite->label);
        if (append_intro_tile_placement(runtime, chr_bank, chr_table, top_tile, base_x + sprite->dx, base_y, label) !=
            0) {
            break;
        }
        (void)snprintf(label, sizeof(label), "%s BOT", sprite->label);
        if (append_intro_tile_placement(runtime,
                                        chr_bank,
                                        chr_table,
                                        bottom_tile,
                                        base_x + sprite->dx,
                                        base_y + 1,
                                        label) != 0) {
            break;
        }
    }

    if (runtime->intro_placement_count == before) {
        set_runtime_status(runtime->intro_layout_status,
                           sizeof(runtime->intro_layout_status),
                           "PLACEMENT LIST FULL");
    } else if (runtime->intro_placement_count - before < sizeof(rabbit_sprites) / sizeof(rabbit_sprites[0]) * 2U) {
        set_runtime_status(runtime->intro_layout_status,
                           sizeof(runtime->intro_layout_status),
                           "RABBIT PRESET PARTIAL  LIST FULL");
    } else {
        set_runtime_status(runtime->intro_layout_status,
                           sizeof(runtime->intro_layout_status),
                           "RABBIT LOOKUP B31 T1 124-12B RECORDED");
    }
}

static void add_intro_tecmo_logo_candidate(TecmoRuntime *runtime)
{
    uint32_t chr_bank = 31U;
    const uint32_t chr_table = 1U;
    int base_x = runtime->intro_canvas_cell_x;
    int base_y = runtime->intro_canvas_cell_y;
    size_t before = runtime->intro_placement_count;

    if (chr_bank >= chr_bank_count(runtime)) {
        chr_bank = selected_chr_bank(runtime);
    }
    if (base_x > INTRO_CANVAS_CELLS_X - 10) {
        base_x = INTRO_CANVAS_CELLS_X - 10;
    }
    if (base_y > INTRO_CANVAS_CELLS_Y - 2) {
        base_y = INTRO_CANVAS_CELLS_Y - 2;
    }
    if (base_x < 0) {
        base_x = 0;
    }
    if (base_y < 0) {
        base_y = 0;
    }

    runtime->selected_chr_bank = chr_bank;
    runtime->selected_chr_table = chr_table;
    runtime->intro_source_tile = 0x80U;
    runtime->intro_canvas_focus = true;
    runtime->intro_canvas_cell_x = base_x;
    runtime->intro_canvas_cell_y = base_y;

    for (uint16_t letter = 0; letter < 5U; ++letter) {
        uint16_t tile_base = (uint16_t)(0x180U + letter * 4U);
        int letter_x = base_x + (int)letter * 2;
        for (uint16_t tile = 0; tile < 4U; ++tile) {
            char label[32];
            int dx = (int)(tile & 1U);
            int dy = (int)(tile >> 1U);
            uint16_t tile_id = (uint16_t)(tile_base + tile);
            (void)snprintf(label, sizeof(label), "TECMO %03X", (unsigned)tile_id);
            if (append_intro_tile_placement(runtime,
                                            chr_bank,
                                            chr_table,
                                            tile_id,
                                            letter_x + dx,
                                            base_y + dy,
                                            label) != 0) {
                break;
            }
        }
        if (runtime->intro_placement_count >= TECMO_MAX_INTRO_PLACEMENTS) {
            break;
        }
    }

    if (runtime->intro_placement_count == before) {
        set_runtime_status(runtime->intro_layout_status,
                           sizeof(runtime->intro_layout_status),
                           "PLACEMENT LIST FULL");
    } else if (runtime->intro_placement_count - before < 20U) {
        set_runtime_status(runtime->intro_layout_status,
                           sizeof(runtime->intro_layout_status),
                           "TECMO PRESET PARTIAL  LIST FULL");
    } else {
        set_runtime_status(runtime->intro_layout_status,
                           sizeof(runtime->intro_layout_status),
                           "TECMO VISUAL CANDIDATE B31 T1 180-193 RECORDED");
    }
}

static void add_intro_composite_candidate(TecmoRuntime *runtime)
{
    size_t before = runtime->intro_placement_count;

    runtime->intro_canvas_cell_x = 7;
    runtime->intro_canvas_cell_y = 5;
    add_intro_tecmo_logo_candidate(runtime);

    runtime->intro_canvas_cell_x = 4;
    runtime->intro_canvas_cell_y = 4;
    add_intro_rabbit_head_candidate(runtime);

    if (runtime->intro_placement_count == before) {
        set_runtime_status(runtime->intro_layout_status,
                           sizeof(runtime->intro_layout_status),
                           "PLACEMENT LIST FULL");
    } else {
        runtime->intro_canvas_cell_x = 4;
        runtime->intro_canvas_cell_y = 4;
        set_runtime_status(runtime->intro_layout_status,
                           sizeof(runtime->intro_layout_status),
                           "INTRO COMPOSITE CANDIDATE RABBIT PLUS TECMO RECORDED");
    }
}

static void remove_intro_placement(TecmoRuntime *runtime)
{
    if (runtime->intro_placement_count == 0) {
        set_runtime_status(runtime->intro_layout_status,
                           sizeof(runtime->intro_layout_status),
                           "NO PLACEMENT TO REMOVE");
        return;
    }

    --runtime->intro_placement_count;
    memset(&runtime->intro_placements[runtime->intro_placement_count], 0, sizeof(runtime->intro_placements[0]));
    runtime->intro_layout_dirty = true;
    runtime->intro_layout_saved = false;
    set_runtime_status(runtime->intro_layout_status,
                       sizeof(runtime->intro_layout_status),
                       "REMOVED LAST PLACEMENT");
}

static void move_intro_source_tile(TecmoRuntime *runtime, int dx, int dy)
{
    int tile = (int)(runtime->intro_source_tile & 0xFFU);
    int col = tile & 0x0F;
    int row = tile >> 4;
    col += dx;
    row += dy;
    if (col < 0) {
        col = 0;
    }
    if (col > 15) {
        col = 15;
    }
    if (row < 0) {
        row = 0;
    }
    if (row > 15) {
        row = 15;
    }
    runtime->intro_source_tile = (uint16_t)((row << 4) | col);
}

static void move_intro_canvas_cursor(TecmoRuntime *runtime, int dx, int dy)
{
    runtime->intro_canvas_cell_x += dx;
    runtime->intro_canvas_cell_y += dy;
    if (runtime->intro_canvas_cell_x < 0) {
        runtime->intro_canvas_cell_x = 0;
    }
    if (runtime->intro_canvas_cell_x >= INTRO_CANVAS_CELLS_X) {
        runtime->intro_canvas_cell_x = INTRO_CANVAS_CELLS_X - 1;
    }
    if (runtime->intro_canvas_cell_y < 0) {
        runtime->intro_canvas_cell_y = 0;
    }
    if (runtime->intro_canvas_cell_y >= INTRO_CANVAS_CELLS_Y) {
        runtime->intro_canvas_cell_y = INTRO_CANVAS_CELLS_Y - 1;
    }
}

static void update_probe_screen(TecmoRuntime *runtime, const TecmoInput *input)
{
    if (runtime->mode == TECMO_MODE_INTRO_PROBE) {
        uint32_t count = chr_bank_count(runtime);
        if (pressed(input->bank_prev, runtime->previous_input.bank_prev) && runtime->selected_chr_bank > 0U) {
            --runtime->selected_chr_bank;
        }
        if (pressed(input->bank_next, runtime->previous_input.bank_next) &&
            runtime->selected_chr_bank + 1U < count) {
            ++runtime->selected_chr_bank;
        }
        if (pressed(input->table_toggle, runtime->previous_input.table_toggle)) {
            runtime->selected_chr_table ^= 1U;
        }
        if (pressed(input->tab, runtime->previous_input.tab)) {
            runtime->intro_canvas_focus = !runtime->intro_canvas_focus;
        }
        if (pressed(input->left, runtime->previous_input.left)) {
            if (runtime->intro_canvas_focus) {
                move_intro_canvas_cursor(runtime, -1, 0);
            } else {
                move_intro_source_tile(runtime, -1, 0);
            }
        }
        if (pressed(input->right, runtime->previous_input.right)) {
            if (runtime->intro_canvas_focus) {
                move_intro_canvas_cursor(runtime, 1, 0);
            } else {
                move_intro_source_tile(runtime, 1, 0);
            }
        }
        if (pressed(input->up, runtime->previous_input.up)) {
            if (runtime->intro_canvas_focus) {
                move_intro_canvas_cursor(runtime, 0, -1);
            } else {
                move_intro_source_tile(runtime, 0, -1);
            }
        }
        if (pressed(input->down, runtime->previous_input.down)) {
            if (runtime->intro_canvas_focus) {
                move_intro_canvas_cursor(runtime, 0, 1);
            } else {
                move_intro_source_tile(runtime, 0, 1);
            }
        }
        if (pressed(input->shoot, runtime->previous_input.shoot)) {
            add_intro_placement(runtime);
        }
        if (pressed(input->preset_rabbit, runtime->previous_input.preset_rabbit)) {
            add_intro_rabbit_head_candidate(runtime);
        }
        if (pressed(input->preset_tecmo, runtime->previous_input.preset_tecmo)) {
            add_intro_tecmo_logo_candidate(runtime);
        }
        if (pressed(input->preset_composite, runtime->previous_input.preset_composite)) {
            add_intro_composite_candidate(runtime);
        }
        if (pressed(input->remove, runtime->previous_input.remove)) {
            remove_intro_placement(runtime);
        }
        if (pressed(input->save, runtime->previous_input.save)) {
            (void)save_intro_layout_file(runtime);
        }
    } else if (runtime->mode == TECMO_MODE_CHR_PLAYGROUND) {
        uint32_t count = chr_bank_count(runtime);
        if (pressed(input->left, runtime->previous_input.left) && runtime->selected_chr_bank > 0U) {
            --runtime->selected_chr_bank;
        }
        if (pressed(input->right, runtime->previous_input.right) &&
            runtime->selected_chr_bank + 1U < count) {
            ++runtime->selected_chr_bank;
        }
        if (pressed(input->tab, runtime->previous_input.tab)) {
            runtime->selected_chr_bank = (runtime->selected_chr_bank + 1U) % count;
        }
        if (pressed(input->up, runtime->previous_input.up) ||
            pressed(input->down, runtime->previous_input.down) ||
            pressed(input->table_toggle, runtime->previous_input.table_toggle)) {
            runtime->selected_chr_table ^= 1U;
        }
    }

    if (pressed(input->confirm, runtime->previous_input.confirm) ||
        pressed(input->cancel, runtime->previous_input.cancel)) {
        runtime->mode = TECMO_MODE_MAIN_MENU;
    }
}

static void update_roster_selection(TecmoRuntime *runtime, const TecmoInput *input, bool allow_start_game)
{
    size_t player_count = selected_team_player_count(runtime);

    if (pressed(input->left, runtime->previous_input.left) && runtime->selected_team > 0) {
        --runtime->selected_team;
        runtime->selected_player = 0;
    }
    if (pressed(input->right, runtime->previous_input.right) &&
        runtime->selected_team + 1U < runtime->team_count) {
        ++runtime->selected_team;
        runtime->selected_player = 0;
    }
    if (pressed(input->up, runtime->previous_input.up) && runtime->selected_player > 0) {
        --runtime->selected_player;
    }
    if (pressed(input->down, runtime->previous_input.down) &&
        runtime->selected_player + 1U < player_count) {
        ++runtime->selected_player;
    }
    if (pressed(input->cancel, runtime->previous_input.cancel)) {
        runtime->mode = TECMO_MODE_MAIN_MENU;
    }
    if (allow_start_game && pressed(input->confirm, runtime->previous_input.confirm)) {
        runtime->mode = TECMO_MODE_COURT;
    }
}

static void clamp_player_to_court(TecmoRuntime *runtime)
{
    if (runtime->player_x < (float)COURT_LEFT + 12.0f) {
        runtime->player_x = (float)COURT_LEFT + 12.0f;
    }
    if (runtime->player_x > (float)COURT_RIGHT - 12.0f) {
        runtime->player_x = (float)COURT_RIGHT - 12.0f;
    }
    if (runtime->player_y < (float)COURT_TOP + 18.0f) {
        runtime->player_y = (float)COURT_TOP + 18.0f;
    }
    if (runtime->player_y > (float)COURT_BOTTOM - 18.0f) {
        runtime->player_y = (float)COURT_BOTTOM - 18.0f;
    }
}

static void update_court(TecmoRuntime *runtime, const TecmoInput *input)
{
    const float speed = 3.0f;
    const float hoop_x = 542.0f;
    const float hoop_y = 126.0f;

    if (input->left) {
        runtime->player_x -= speed;
    }
    if (input->right) {
        runtime->player_x += speed;
    }
    if (input->up) {
        runtime->player_y -= speed;
    }
    if (input->down) {
        runtime->player_y += speed;
    }
    clamp_player_to_court(runtime);

    if (pressed(input->cancel, runtime->previous_input.cancel)) {
        runtime->mode = TECMO_MODE_PLAY_SETUP;
        runtime->ball_in_air = false;
    }

    if (!runtime->ball_in_air) {
        runtime->ball_x = runtime->player_x + 14.0f;
        runtime->ball_y = runtime->player_y - 8.0f;
        if (pressed(input->shoot, runtime->previous_input.shoot)) {
            float dx = hoop_x - runtime->ball_x;
            float dy = hoop_y - runtime->ball_y;
            runtime->ball_vx = dx / 44.0f;
            runtime->ball_vy = dy / 44.0f - 4.0f;
            runtime->ball_in_air = true;
        }
    } else {
        float dx;
        float dy;
        runtime->ball_x += runtime->ball_vx;
        runtime->ball_y += runtime->ball_vy;
        runtime->ball_vy += 0.18f;
        dx = runtime->ball_x - hoop_x;
        dy = runtime->ball_y - hoop_y;
        if ((dx * dx + dy * dy) < 20.0f * 20.0f && runtime->ball_vy > 0.0f) {
            ++runtime->score;
            runtime->ball_in_air = false;
        }
        if (runtime->ball_y > (float)COURT_BOTTOM + 40.0f ||
            runtime->ball_x < 0.0f ||
            runtime->ball_x > 640.0f) {
            runtime->ball_in_air = false;
        }
    }
}

static void write_runtime_watch_memory(TecmoRuntime *runtime)
{
    tecmo_cpu_ram_write(runtime->memory, 0x0000, (uint8_t)runtime->mode);
    tecmo_cpu_ram_write(runtime->memory, 0x0001, (uint8_t)runtime->selected_team);
    tecmo_cpu_ram_write(runtime->memory, 0x0002, (uint8_t)runtime->selected_player);
    tecmo_cpu_ram_write(runtime->memory, 0x0003, (uint8_t)runtime->selected_menu_item);
    tecmo_cpu_ram_write(runtime->memory, 0x0004, (uint8_t)(runtime->frame_counter & 0xFFU));
    tecmo_cpu_ram_write(runtime->memory, 0x0005, (uint8_t)((runtime->frame_counter >> 8U) & 0xFFU));
    tecmo_cpu_ram_write(runtime->memory, 0x0006, (uint8_t)selected_chr_bank(runtime));
    tecmo_cpu_ram_write(runtime->memory, 0x0007, (uint8_t)chr_bank_count(runtime));
    tecmo_cpu_ram_write(runtime->memory, 0x0008, (uint8_t)selected_chr_table(runtime));
}

void tecmo_runtime_update(TecmoRuntime *runtime, const TecmoInput *input)
{
    ++runtime->frame_counter;

    if (pressed(input->debug_toggle, runtime->previous_input.debug_toggle)) {
        runtime->debug_overlay = !runtime->debug_overlay;
    }

    if (runtime->mode == TECMO_MODE_MAIN_MENU) {
        update_main_menu(runtime, input);
    } else if (runtime->mode == TECMO_MODE_TITLE_SCREEN) {
        update_title_screen(runtime, input);
    } else if (runtime->mode == TECMO_MODE_INTRO_PROBE ||
               runtime->mode == TECMO_MODE_CHR_PLAYGROUND) {
        update_probe_screen(runtime, input);
    } else if (runtime->mode == TECMO_MODE_PLAY_SETUP) {
        update_roster_selection(runtime, input, true);
    } else if (runtime->mode == TECMO_MODE_ROSTERS) {
        update_roster_selection(runtime, input, false);
    } else if (runtime->mode == TECMO_MODE_COURT) {
        update_court(runtime, input);
    }

    write_runtime_watch_memory(runtime);
    runtime->previous_input = *input;
}

static void set_flow_test_message(char *dest, size_t dest_size, const char *text)
{
    if (dest == NULL || dest_size == 0) {
        return;
    }
    if (text == NULL) {
        text = "";
    }
    (void)snprintf(dest, dest_size, "%s", text);
}

static void flow_step(TecmoRuntime *runtime, TecmoInput input)
{
    TecmoInput released;
    tecmo_runtime_update(runtime, &input);
    memset(&released, 0, sizeof(released));
    tecmo_runtime_update(runtime, &released);
}

static bool flow_expect_mode(TecmoRuntime *runtime,
                             TecmoPlayMode expected,
                             const char *label,
                             char *message,
                             size_t message_size)
{
    if (runtime->mode != expected) {
        char detail[128];
        (void)snprintf(detail,
                       sizeof(detail),
                       "%s expected mode %s got %s",
                       label,
                       mode_name(expected),
                       mode_name(runtime->mode));
        set_flow_test_message(message, message_size, detail);
        return false;
    }
    if (tecmo_cpu_ram_read(runtime->memory, 0x0000) != (uint8_t)expected) {
        char detail[128];
        (void)snprintf(detail,
                       sizeof(detail),
                       "%s watched RAM mode expected %u got %u",
                       label,
                       (unsigned)expected,
                       (unsigned)tecmo_cpu_ram_read(runtime->memory, 0x0000));
        set_flow_test_message(message, message_size, detail);
        return false;
    }
    return true;
}

static bool flow_expect_menu_item(TecmoRuntime *runtime,
                                  size_t expected,
                                  const char *label,
                                  char *message,
                                  size_t message_size)
{
    if (runtime->selected_menu_item != expected) {
        char detail[128];
        (void)snprintf(detail,
                       sizeof(detail),
                       "%s expected menu item %u got %u",
                       label,
                       (unsigned)expected,
                       (unsigned)runtime->selected_menu_item);
        set_flow_test_message(message, message_size, detail);
        return false;
    }
    if (tecmo_cpu_ram_read(runtime->memory, 0x0003) != (uint8_t)expected) {
        char detail[128];
        (void)snprintf(detail,
                       sizeof(detail),
                       "%s watched RAM menu expected %u got %u",
                       label,
                       (unsigned)expected,
                       (unsigned)tecmo_cpu_ram_read(runtime->memory, 0x0003));
        set_flow_test_message(message, message_size, detail);
        return false;
    }
    return true;
}

static bool flow_press_menu_down(TecmoRuntime *runtime,
                                 size_t count,
                                 size_t expected,
                                 const char *label,
                                 char *message,
                                 size_t message_size)
{
    TecmoInput input;
    for (size_t i = 0; i < count; ++i) {
        memset(&input, 0, sizeof(input));
        input.down = true;
        flow_step(runtime, input);
    }
    return flow_expect_menu_item(runtime, expected, label, message, message_size);
}

static bool flow_press_menu_up(TecmoRuntime *runtime,
                               size_t count,
                               size_t expected,
                               const char *label,
                               char *message,
                               size_t message_size)
{
    TecmoInput input;
    for (size_t i = 0; i < count; ++i) {
        memset(&input, 0, sizeof(input));
        input.up = true;
        flow_step(runtime, input);
    }
    return flow_expect_menu_item(runtime, expected, label, message, message_size);
}

bool tecmo_runtime_flow_self_test(TecmoRuntime *runtime, char *message, size_t message_size)
{
    TecmoInput input;
    size_t original_team;

    if (message != NULL && message_size > 0) {
        message[0] = '\0';
    }
    if (runtime == NULL || runtime->memory == NULL) {
        set_flow_test_message(message, message_size, "runtime or memory is null");
        return false;
    }
    if (runtime->team_count == 0) {
        set_flow_test_message(message, message_size, "no roster teams loaded");
        return false;
    }
    if (runtime->team_count < 2U || runtime->roster.count < 2U) {
        set_flow_test_message(message, message_size, "not enough roster data to browse teams and players");
        return false;
    }

    memset(&input, 0, sizeof(input));
    flow_step(runtime, input);
    if (!flow_expect_mode(runtime, TECMO_MODE_TITLE_SCREEN, "boot title", message, message_size)) {
        return false;
    }

    input.confirm = true;
    flow_step(runtime, input);
    if (!flow_expect_mode(runtime, TECMO_MODE_MAIN_MENU, "title confirm", message, message_size)) {
        return false;
    }
    if (!flow_expect_menu_item(runtime, 0U, "initial menu", message, message_size)) {
        return false;
    }

    if (!flow_press_menu_down(runtime, 4U, 4U, "navigate to rosters", message, message_size)) {
        return false;
    }
    memset(&input, 0, sizeof(input));
    input.confirm = true;
    flow_step(runtime, input);
    if (!flow_expect_mode(runtime, TECMO_MODE_ROSTERS, "enter rosters", message, message_size)) {
        return false;
    }
    if (selected_team_player_count(runtime) < 2U) {
        set_flow_test_message(message, message_size, "selected roster has fewer than two players");
        return false;
    }
    memset(&input, 0, sizeof(input));
    input.down = true;
    flow_step(runtime, input);
    if (runtime->selected_player != 1U || tecmo_cpu_ram_read(runtime->memory, 0x0002) != 1U) {
        set_flow_test_message(message, message_size, "roster player navigation failed");
        return false;
    }
    memset(&input, 0, sizeof(input));
    input.cancel = true;
    flow_step(runtime, input);
    if (!flow_expect_mode(runtime, TECMO_MODE_MAIN_MENU, "exit rosters", message, message_size)) {
        return false;
    }

    if (!flow_press_menu_up(runtime, 1U, 3U, "navigate to play setup", message, message_size)) {
        return false;
    }
    memset(&input, 0, sizeof(input));
    input.confirm = true;
    flow_step(runtime, input);
    if (!flow_expect_mode(runtime, TECMO_MODE_PLAY_SETUP, "enter play setup", message, message_size)) {
        return false;
    }
    original_team = runtime->selected_team;
    memset(&input, 0, sizeof(input));
    input.right = true;
    flow_step(runtime, input);
    if (runtime->selected_team != original_team + 1U ||
        tecmo_cpu_ram_read(runtime->memory, 0x0001) != (uint8_t)runtime->selected_team) {
        set_flow_test_message(message, message_size, "play setup team navigation failed");
        return false;
    }
    memset(&input, 0, sizeof(input));
    input.confirm = true;
    flow_step(runtime, input);
    if (!flow_expect_mode(runtime, TECMO_MODE_COURT, "start court", message, message_size)) {
        return false;
    }
    if (runtime->ball_x <= 0.0f || runtime->ball_y <= 0.0f) {
        set_flow_test_message(message, message_size, "court ball state was not initialized");
        return false;
    }

    memset(&input, 0, sizeof(input));
    input.cancel = true;
    flow_step(runtime, input);
    if (!flow_expect_mode(runtime, TECMO_MODE_PLAY_SETUP, "court cancel", message, message_size)) {
        return false;
    }
    memset(&input, 0, sizeof(input));
    input.cancel = true;
    flow_step(runtime, input);
    if (!flow_expect_mode(runtime, TECMO_MODE_MAIN_MENU, "play setup cancel", message, message_size)) {
        return false;
    }

    if (!flow_press_menu_down(runtime, 2U, 5U, "navigate to quit", message, message_size)) {
        return false;
    }
    memset(&input, 0, sizeof(input));
    input.confirm = true;
    flow_step(runtime, input);
    if (!runtime->quit_requested) {
        set_flow_test_message(message, message_size, "quit menu item did not request quit");
        return false;
    }

    set_flow_test_message(message, message_size, "FLOW TEST PASS: title menu rosters play court quit");
    return true;
}

static void clear(TecmoFramebuffer *fb, uint32_t color)
{
    for (int y = 0; y < fb->height; ++y) {
        uint32_t *row = fb->pixels + (size_t)y * (size_t)fb->pitch_pixels;
        for (int x = 0; x < fb->width; ++x) {
            row[x] = color;
        }
    }
}

static void rect(TecmoFramebuffer *fb, int x, int y, int w, int h, uint32_t color)
{
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = x + w > fb->width ? fb->width : x + w;
    int y1 = y + h > fb->height ? fb->height : y + h;
    for (int py = y0; py < y1; ++py) {
        uint32_t *row = fb->pixels + (size_t)py * (size_t)fb->pitch_pixels;
        for (int px = x0; px < x1; ++px) {
            row[px] = color;
        }
    }
}

static void outline_rect(TecmoFramebuffer *fb, int x, int y, int w, int h, uint32_t color)
{
    rect(fb, x, y, w, 1, color);
    rect(fb, x, y + h - 1, w, 1, color);
    rect(fb, x, y, 1, h, color);
    rect(fb, x + w - 1, y, 1, h, color);
}

static uint8_t glyph_bits(char c, int row)
{
    static const uint8_t font[43][7] = {
        {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E}, {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E},
        {0x0E,0x11,0x01,0x02,0x04,0x08,0x1F}, {0x1E,0x01,0x01,0x0E,0x01,0x01,0x1E},
        {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02}, {0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E},
        {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E}, {0x1F,0x01,0x02,0x04,0x08,0x08,0x08},
        {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E}, {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C},
        {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11}, {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E},
        {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E}, {0x1E,0x11,0x11,0x11,0x11,0x11,0x1E},
        {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F}, {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10},
        {0x0F,0x10,0x10,0x13,0x11,0x11,0x0F}, {0x11,0x11,0x11,0x1F,0x11,0x11,0x11},
        {0x0E,0x04,0x04,0x04,0x04,0x04,0x0E}, {0x07,0x02,0x02,0x02,0x12,0x12,0x0C},
        {0x11,0x12,0x14,0x18,0x14,0x12,0x11}, {0x10,0x10,0x10,0x10,0x10,0x10,0x1F},
        {0x11,0x1B,0x15,0x15,0x11,0x11,0x11}, {0x11,0x19,0x15,0x13,0x11,0x11,0x11},
        {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E}, {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10},
        {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D}, {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11},
        {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E}, {0x1F,0x04,0x04,0x04,0x04,0x04,0x04},
        {0x11,0x11,0x11,0x11,0x11,0x11,0x0E}, {0x11,0x11,0x11,0x11,0x0A,0x0A,0x04},
        {0x11,0x11,0x11,0x15,0x15,0x1B,0x11}, {0x11,0x11,0x0A,0x04,0x0A,0x11,0x11},
        {0x11,0x11,0x0A,0x04,0x04,0x04,0x04}, {0x1F,0x01,0x02,0x04,0x08,0x10,0x1F},
        {0x00,0x00,0x00,0x00,0x00,0x00,0x00}, {0x00,0x00,0x00,0x1F,0x00,0x00,0x00},
        {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C}, {0x04,0x04,0x04,0x04,0x00,0x00,0x04},
        {0x00,0x00,0x0E,0x00,0x00,0x0E,0x00}, {0x00,0x00,0x04,0x00,0x04,0x00,0x00},
        {0x02,0x02,0x04,0x00,0x00,0x00,0x00},
    };
    int index = -1;
    if (c >= '0' && c <= '9') {
        index = c - '0';
    } else if (c >= 'A' && c <= 'Z') {
        index = 10 + c - 'A';
    } else if (c >= 'a' && c <= 'z') {
        index = 10 + c - 'a';
    } else if (c == ' ') {
        index = 36;
    } else if (c == '-') {
        index = 37;
    } else if (c == '.') {
        index = 38;
    } else if (c == '!') {
        index = 39;
    } else if (c == '=') {
        index = 40;
    } else if (c == ':') {
        index = 41;
    } else if (c == '\'') {
        index = 42;
    }
    if (index < 0) {
        return 0;
    }
    return font[index][row];
}

static void draw_text(TecmoFramebuffer *fb, int x, int y, const char *text, uint32_t color, int scale)
{
    int cursor_x = x;
    if (scale < 1) {
        scale = 1;
    }
    for (const char *p = text; *p != '\0'; ++p) {
        for (int row = 0; row < 7; ++row) {
            uint8_t bits = glyph_bits(*p, row);
            for (int col = 0; col < 5; ++col) {
                if ((bits & (uint8_t)(1U << (4 - col))) != 0) {
                    rect(fb, cursor_x + col * scale, y + row * scale, scale, scale, color);
                }
            }
        }
        cursor_x += 6 * scale;
    }
}

static int text_width_pixels(const char *text, int scale)
{
    if (scale < 1) {
        scale = 1;
    }
    return (int)strlen(text) * 6 * scale;
}

static void draw_centered_text(TecmoFramebuffer *fb, int y, const char *text, uint32_t color, int scale)
{
    int width = text_width_pixels(text, scale);
    int x = (fb->width - width) / 2;
    draw_text(fb, x, y, text, color, scale);
}

static void draw_button(TecmoFramebuffer *fb, int x, int y, int w, int h, const char *label, bool selected)
{
    uint32_t fill = selected ? rgb(220, 198, 80) : rgb(34, 42, 50);
    uint32_t border = selected ? rgb(248, 244, 198) : rgb(100, 118, 132);
    uint32_t text = selected ? rgb(24, 24, 22) : rgb(230, 235, 226);

    rect(fb, x, y, w, h, border);
    rect(fb, x + 2, y + 2, w - 4, h - 4, fill);
    draw_text(fb, x + 18, y + 16, label, text, 2);
}

static void render_main_menu(const TecmoRuntime *runtime, TecmoFramebuffer *fb)
{
    clear(fb, rgb(14, 18, 22));
    rect(fb, 0, 0, fb->width, 70, rgb(120, 16, 24));
    draw_text(fb, 34, 24, "TECMO BASKETBALL NATIVE PORT", rgb(248, 248, 232), 2);
    draw_text(fb, 74, 96, "LOCAL HOBBY PORT PROTOTYPE", rgb(144, 176, 192), 1);

    draw_button(fb, 150, 116, 340, 42, "TITLE SCREEN", runtime->selected_menu_item == 0);
    draw_button(fb, 150, 166, 340, 42, "INTRO LAB", runtime->selected_menu_item == 1);
    draw_button(fb, 150, 216, 340, 42, "CHR PLAYGROUND", runtime->selected_menu_item == 2);
    draw_button(fb, 150, 266, 340, 42, "PLAY PROTOTYPE", runtime->selected_menu_item == 3);
    draw_button(fb, 150, 316, 340, 42, "ROSTERS", runtime->selected_menu_item == 4);
    draw_button(fb, 150, 366, 340, 42, "QUIT", runtime->selected_menu_item == 5);

    draw_text(fb, 150, 424, "UP DOWN SELECT   ENTER CONFIRM   ESC QUIT", rgb(226, 228, 208), 1);
    draw_text(fb, 82, 452, "NO ROM ASM OR EXTRACTED ASSETS ARE LOADED FROM THIS REPO", rgb(124, 148, 160), 1);
}

static void render_title_screen_mode(const TecmoRuntime *runtime, TecmoFramebuffer *fb)
{
    if (runtime->title_probe_available) {
        tecmo_render_original_title_chr_probe(fb,
                                              &runtime->title_glyphs,
                                              runtime->title_chr_bytes,
                                              runtime->title_chr_byte_count,
                                              31U);
        draw_text(fb, 22, 22, "ENTER MENU   ESC QUIT", rgb(230, 232, 214), 1);
    } else {
        tecmo_render_original_title_probe(fb, "TITLE DATA MISSING");
        draw_centered_text(fb, 404, "LOCAL TITLE DATA UNAVAILABLE", rgb(230, 232, 214), 1);
    }
}

static void render_roster_browser(const TecmoRuntime *runtime, TecmoFramebuffer *fb, bool play_setup)
{
    char line[256];
    const char *team = runtime->team_count > 0 ? runtime->teams[runtime->selected_team] : "NO DATA";
    size_t player_row = 0;

    clear(fb, rgb(16, 20, 24));
    rect(fb, 0, 0, fb->width, 48, rgb(120, 16, 24));
    draw_text(fb, 24, 16, play_setup ? "PLAY PROTOTYPE SETUP" : "ROSTERS", rgb(248, 248, 232), 2);

    (void)snprintf(line, sizeof(line), "TEAM %u OF %u: %s",
                   (unsigned)(runtime->selected_team + 1U),
                   (unsigned)runtime->team_count,
                   team);
    draw_text(fb, 32, 72, line, rgb(238, 238, 214), 2);
    draw_text(fb, 32, 102,
              play_setup ? "ARROWS SELECT   ENTER START PROTOTYPE   ESC MENU" : "ARROWS BROWSE ROSTERS   ESC MENU",
              rgb(144, 176, 192),
              1);

    for (size_t i = 0; i < runtime->roster.count; ++i) {
        const RosterRecord *record = &runtime->roster.records[i];
        if (!text_equals(record->team, team)) {
            continue;
        }
        if (player_row == runtime->selected_player) {
            rect(fb, 28, 132 + (int)player_row * 22, 430, 18, rgb(220, 198, 80));
        }
        (void)snprintf(line, sizeof(line), "%2u  %-20s  #%02u  %u'%u\"",
                       (unsigned)(player_row + 1U),
                       record->player,
                       (unsigned)(((record->attrs[1] >> 4U) * 10U) + (record->attrs[1] & 0x0FU)),
                       (unsigned)(record->attrs[2] >> 4U),
                       (unsigned)(record->attrs[2] & 0x0FU));
        draw_text(fb, 36, 136 + (int)player_row * 22, line,
                  player_row == runtime->selected_player ? rgb(28, 28, 24) : rgb(220, 224, 218), 1);
        ++player_row;
    }
}

static void render_court(const TecmoRuntime *runtime, TecmoFramebuffer *fb)
{
    char line[256];
    const RosterRecord *player = selected_player_record(runtime);

    clear(fb, rgb(12, 58, 46));
    rect(fb, COURT_LEFT, COURT_TOP, COURT_RIGHT - COURT_LEFT, COURT_BOTTOM - COURT_TOP, rgb(197, 140, 78));
    rect(fb, COURT_LEFT + 6, COURT_TOP + 6, COURT_RIGHT - COURT_LEFT - 12, COURT_BOTTOM - COURT_TOP - 12, rgb(226, 170, 98));
    rect(fb, 314, COURT_TOP + 6, 4, COURT_BOTTOM - COURT_TOP - 12, rgb(245, 236, 210));
    rect(fb, 500, 92, 84, 72, rgb(178, 92, 68));
    rect(fb, 538, 116, 8, 28, rgb(245, 236, 210));
    rect(fb, 528, 120, 28, 5, rgb(245, 236, 210));

    rect(fb, (int)runtime->player_x - 8, (int)runtime->player_y - 18, 16, 36, rgb(42, 70, 164));
    rect(fb, (int)runtime->player_x - 6, (int)runtime->player_y - 26, 12, 10, rgb(228, 182, 134));
    rect(fb, (int)runtime->ball_x - 5, (int)runtime->ball_y - 5, 10, 10, rgb(214, 98, 36));

    (void)snprintf(line, sizeof(line), "SCORE %u", runtime->score * 2U);
    draw_text(fb, 24, 18, line, rgb(248, 248, 232), 2);
    if (player != 0) {
        (void)snprintf(line, sizeof(line), "%s - %s", runtime->teams[runtime->selected_team], player->player);
        draw_text(fb, 220, 20, line, rgb(248, 248, 232), 1);
    }
    draw_text(fb, 24, 452, "ARROWS MOVE   SPACE SHOOT   ESC TEAM SELECT", rgb(236, 232, 208), 1);
}

static void draw_debug_text(TecmoFramebuffer *fb, int x, int y, const char *text)
{
    draw_text(fb, x + 1, y + 1, text, rgb(0, 0, 0), 1);
    draw_text(fb, x, y, text, rgb(214, 250, 210), 1);
}

static void render_debug_overlay(const TecmoRuntime *runtime, TecmoFramebuffer *fb)
{
    char line[256];
    const TecmoGameMemory *memory = runtime->memory;
    const float fps = runtime->frame_seconds > 0.00001f ? 1.0f / runtime->frame_seconds : 0.0f;
    const int x = 10;
    const int y = 314;

    rect(fb, x - 6, y - 8, 412, 152, rgb(10, 14, 18));
    rect(fb, x - 4, y - 6, 408, 148, rgb(28, 38, 42));

    (void)snprintf(line, sizeof(line), "DBG MODE %s FRAME %u", mode_name(runtime->mode), runtime->frame_counter);
    draw_debug_text(fb, x, y, line);

    (void)snprintf(line, sizeof(line), "FPS %.1f MENU %u TEAM %u PLAYER %u CHR %02u/%02u T%u",
                   (double)fps,
                   (unsigned)runtime->selected_menu_item,
                   (unsigned)(runtime->selected_team + 1U),
                   (unsigned)(runtime->selected_player + 1U),
                   (unsigned)selected_chr_bank(runtime),
                   (unsigned)(chr_bank_count(runtime) - 1U),
                   (unsigned)selected_chr_table(runtime));
    draw_debug_text(fb, x, y + 20, line);

    (void)snprintf(line, sizeof(line), "PERM %llu OF %llu HI %llu",
                   (unsigned long long)memory->permanent.used,
                   (unsigned long long)memory->permanent.capacity,
                   (unsigned long long)memory->permanent.high_water);
    draw_debug_text(fb, x, y + 40, line);

    (void)snprintf(line, sizeof(line), "TRAN %llu OF %llu HI %llu",
                   (unsigned long long)memory->transient.used,
                   (unsigned long long)memory->transient.capacity,
                   (unsigned long long)memory->transient.high_water);
    draw_debug_text(fb, x, y + 60, line);

    (void)snprintf(line, sizeof(line), "RAM 0000: %02X %02X %02X %02X %02X %02X %02X %02X",
                   (unsigned)tecmo_cpu_ram_read(memory, 0x0000),
                   (unsigned)tecmo_cpu_ram_read(memory, 0x0001),
                   (unsigned)tecmo_cpu_ram_read(memory, 0x0002),
                   (unsigned)tecmo_cpu_ram_read(memory, 0x0003),
                   (unsigned)tecmo_cpu_ram_read(memory, 0x0004),
                   (unsigned)tecmo_cpu_ram_read(memory, 0x0005),
                   (unsigned)tecmo_cpu_ram_read(memory, 0x0006),
                   (unsigned)tecmo_cpu_ram_read(memory, 0x0007));
    draw_debug_text(fb, x, y + 80, line);

    (void)snprintf(line, sizeof(line), "RAM 0008: %02X %02X %02X %02X %02X %02X %02X %02X",
                   (unsigned)tecmo_cpu_ram_read(memory, 0x0008),
                   (unsigned)tecmo_cpu_ram_read(memory, 0x0009),
                   (unsigned)tecmo_cpu_ram_read(memory, 0x000A),
                   (unsigned)tecmo_cpu_ram_read(memory, 0x000B),
                   (unsigned)tecmo_cpu_ram_read(memory, 0x000C),
                   (unsigned)tecmo_cpu_ram_read(memory, 0x000D),
                   (unsigned)tecmo_cpu_ram_read(memory, 0x000E),
                   (unsigned)tecmo_cpu_ram_read(memory, 0x000F));
    draw_debug_text(fb, x, y + 100, line);

    draw_debug_text(fb, x, y + 124, "F3 TOGGLE DEBUG OVERLAY");
}

void tecmo_render_original_title_probe(TecmoFramebuffer *framebuffer, const char *title_text)
{
    const char *title = title_text != NULL && title_text[0] != '\0' ? title_text : "TITLE DATA MISSING";

    clear(framebuffer, rgb(8, 10, 24));
    rect(framebuffer, 0, 0, framebuffer->width, 46, rgb(18, 18, 34));
    rect(framebuffer, 0, framebuffer->height - 52, framebuffer->width, 52, rgb(18, 18, 34));
    rect(framebuffer, 52, 112, framebuffer->width - 104, 156, rgb(106, 18, 32));
    rect(framebuffer, 60, 120, framebuffer->width - 120, 140, rgb(20, 26, 54));
    rect(framebuffer, 72, 132, framebuffer->width - 144, 116, rgb(170, 42, 44));
    rect(framebuffer, 84, 144, framebuffer->width - 168, 92, rgb(28, 34, 66));

    draw_centered_text(framebuffer, 168, title, rgb(252, 236, 170), 3);
    draw_centered_text(framebuffer, 284, "SOURCE BACKED TITLE PROBE", rgb(142, 174, 190), 1);
    draw_centered_text(framebuffer, 306, "BANK 04 TITLE TABLE  RENDERER STEP 1", rgb(142, 174, 190), 1);

    rect(framebuffer, 132, 356, 376, 2, rgb(236, 214, 112));
    draw_centered_text(framebuffer, 376, "CHR PALETTE AND LAYOUT MAPPING NEXT", rgb(230, 232, 214), 1);
}

void tecmo_render_intro_c051_d861_model(TecmoFramebuffer *framebuffer)
{
    static const TecmoIntroSpriteRecord synthetic_records[] = {
        {0, 0x24U, 0x02U, 0},
        {8, 0x26U, 0x02U, 16},
        {-24, 0xFDU, 0x00U, 32},
    };
    const TecmoIntroSpriteStageConfig synthetic_config = {48, 64, 1U};
    TecmoIntroStagedSprite staged[sizeof(synthetic_records) / sizeof(synthetic_records[0])];
    const uint32_t panel = rgb(20, 24, 32);
    const uint32_t panel_dark = rgb(9, 11, 16);
    const uint32_t line = rgb(78, 98, 112);
    const uint32_t text = rgb(230, 232, 214);
    const uint32_t muted = rgb(142, 174, 190);
    const uint32_t accent = rgb(252, 236, 118);
    size_t staged_count;
    char row[128];
    char self_test_message[96];
    bool self_test_ok;

    self_test_ok = tecmo_intro_stage_self_test(self_test_message, sizeof(self_test_message));
    staged_count = tecmo_intro_stage_sprite_records(synthetic_records,
                                                    sizeof(synthetic_records) / sizeof(synthetic_records[0]),
                                                    &synthetic_config,
                                                    staged,
                                                    sizeof(staged) / sizeof(staged[0]));
    clear(framebuffer, rgb(6, 7, 10));
    rect(framebuffer, 0, 0, framebuffer->width, 54, rgb(18, 18, 34));
    draw_text(framebuffer, 24, 18, "C051 D861 INTRO SPRITE STAGING MODEL", text, 2);
    draw_text(framebuffer, 30, 64, "READ ONLY DIAGNOSTIC  SYNTHETIC RECORDS ONLY", muted, 1);
    draw_text(framebuffer, 30, 76, self_test_message, self_test_ok ? accent : rgb(232, 92, 76), 1);

    rect(framebuffer, 30, 92, 260, 134, panel);
    outline_rect(framebuffer, 30, 92, 260, 134, line);
    draw_text(framebuffer, 48, 110, "INPUT STREAM", accent, 1);
    draw_text(framebuffer, 48, 132, "COUNT BYTE SELECTS RECORD TOTAL", text, 1);
    draw_text(framebuffer, 48, 154, "EACH RECORD IS FOUR FIELDS", text, 1);
    draw_text(framebuffer, 70, 184, "Y    TILE    ATTR    X", muted, 1);
    rect(framebuffer, 66, 202, 34, 14, rgb(54, 65, 72));
    rect(framebuffer, 116, 202, 46, 14, rgb(54, 65, 72));
    rect(framebuffer, 178, 202, 42, 14, rgb(54, 65, 72));
    rect(framebuffer, 236, 202, 28, 14, rgb(54, 65, 72));

    rect(framebuffer, 350, 92, 260, 134, panel);
    outline_rect(framebuffer, 350, 92, 260, 134, line);
    draw_text(framebuffer, 368, 110, "OAM SHAPED OUTPUT", accent, 1);
    draw_text(framebuffer, 368, 132, "BASE IS 0200 PLUS N TIMES 4", text, 1);
    draw_text(framebuffer, 368, 154, "COUNTER IS 058D", text, 1);
    draw_text(framebuffer, 368, 184, "0200 YBASE PLUS Y", muted, 1);
    draw_text(framebuffer, 368, 202, "0201 TILE PLUS 0D", muted, 1);
    draw_text(framebuffer, 492, 184, "0202 ATTR", muted, 1);
    draw_text(framebuffer, 492, 202, "0203 XBASE PLUS X", muted, 1);

    rect(framebuffer, 292, 151, 54, 6, accent);
    rect(framebuffer, 336, 146, 10, 16, accent);

    rect(framebuffer, 42, 262, 556, 92, panel_dark);
    outline_rect(framebuffer, 42, 262, 556, 92, line);
    draw_text(framebuffer, 60, 280, "NATIVE MODEL STEPS", accent, 1);
    draw_text(framebuffer, 60, 304, "1  BANK04 CHOOSES POINTER INDEX AND BASE OFFSETS", text, 1);
    draw_text(framebuffer, 60, 324, "2  C051 ENTERS D861 AND STAGES FOUR BYTE RECORDS", text, 1);
    draw_text(framebuffer, 60, 344, "3  INTRO LAB MAPS OAM TILE LOWS TO TABLE ONE 8X16 PAIRS", text, 1);

    rect(framebuffer, 42, 382, 556, 56, panel_dark);
    outline_rect(framebuffer, 42, 382, 556, 56, line);
    draw_text(framebuffer, 60, 400, "CURRENT SAFE FACTS", accent, 1);
    draw_text(framebuffer, 60, 420, "RABBIT LOWS 25 27 29 2B  TECMO VISUAL CANDIDATE 180 TO 193", text, 1);
    draw_text(framebuffer, 60, 456, "NO ROM BYTES  NO ASM PAYLOADS  NO CHR DATA  NO LOCAL PATHS", muted, 1);

    for (size_t i = 0; i < staged_count; ++i) {
        (void)snprintf(row,
                       sizeof(row),
                       "S%u Y%02X T%02X A%02X X%02X",
                       (unsigned)i,
                       (unsigned)staged[i].y,
                       (unsigned)staged[i].tile,
                       (unsigned)staged[i].attributes,
                       (unsigned)staged[i].x);
        draw_text(framebuffer, 368, 232 + (int)i * 10, row, muted, 1);
    }
}

static void draw_chr_tile(TecmoFramebuffer *fb,
                          const uint8_t *chr_bytes,
                          uint64_t chr_byte_count,
                          uint32_t chr_bank,
                          uint16_t tile,
                          int x,
                          int y,
                          int scale)
{
    static const uint32_t palette[4] = {
        0x00000000U,
        0xFF614C5CU,
        0xFFC9B45CU,
        0xFFF8F0C8U,
    };
    uint64_t tile_offset = (uint64_t)chr_bank * TITLE_CHR_BANK_BYTES + (uint64_t)tile * 16ULL;

    if (tile > 0x1FFU || tile_offset + 15ULL >= chr_byte_count) {
        return;
    }

    for (int row = 0; row < 8; ++row) {
        uint8_t plane0 = chr_bytes[tile_offset + (uint64_t)row];
        uint8_t plane1 = chr_bytes[tile_offset + (uint64_t)row + 8ULL];
        for (int col = 0; col < 8; ++col) {
            uint8_t bit = (uint8_t)(7 - col);
            uint8_t value = (uint8_t)(((plane0 >> bit) & 1U) | (((plane1 >> bit) & 1U) << 1U));
            uint32_t color = palette[value];
            if (value == 0 || color == 0) {
                continue;
            }
            rect(fb, x + col * scale, y + row * scale, scale, scale, color);
        }
    }
}

static void draw_title_glyph(TecmoFramebuffer *fb,
                             const uint8_t *chr_bytes,
                             uint64_t chr_byte_count,
                             uint32_t chr_bank,
                             uint32_t chr_table,
                             const TecmoTitleGlyph *glyph,
                             int x,
                             int y,
                             int scale)
{
    uint16_t base = (uint16_t)((chr_table & 1U) * 0x100U);
    if (glyph->glyph_tiles[0] != 0xFFU) {
        draw_chr_tile(fb, chr_bytes, chr_byte_count, chr_bank, (uint16_t)(base + glyph->glyph_tiles[0]), x, y, scale);
    }
    if (glyph->glyph_tiles[1] != 0xFFU) {
        draw_chr_tile(fb, chr_bytes, chr_byte_count, chr_bank, (uint16_t)(base + glyph->glyph_tiles[1]), x + 8 * scale, y, scale);
    }
    if (glyph->glyph_tiles[2] != 0xFFU) {
        draw_chr_tile(fb, chr_bytes, chr_byte_count, chr_bank, (uint16_t)(base + glyph->glyph_tiles[2]), x, y + 8 * scale, scale);
    }
    if (glyph->glyph_tiles[3] != 0xFFU) {
        draw_chr_tile(fb, chr_bytes, chr_byte_count, chr_bank, (uint16_t)(base + glyph->glyph_tiles[3]), x + 8 * scale, y + 8 * scale, scale);
    }
}

static void draw_title_glyph_range(TecmoFramebuffer *fb,
                                   const uint8_t *chr_bytes,
                                   uint64_t chr_byte_count,
                                   uint32_t chr_bank,
                                   uint32_t chr_table,
                                   const TecmoOriginalTitleGlyphs *glyphs,
                                   size_t first,
                                   size_t count,
                                   int x,
                                   int y,
                                   int scale)
{
    int glyph_width = 16 * scale;
    for (size_t i = 0; i < count && first + i < glyphs->glyph_count; ++i) {
        const TecmoTitleGlyph *glyph = &glyphs->glyphs[first + i];
        if (glyph->character != ' ') {
            draw_title_glyph(fb,
                             chr_bytes,
                             chr_byte_count,
                             chr_bank,
                             chr_table,
                             glyph,
                             x + (int)i * glyph_width,
                             y,
                             scale);
        }
    }
}

static void draw_chr_bank_sheet(TecmoFramebuffer *fb,
                                const uint8_t *chr_bytes,
                                uint64_t chr_byte_count,
                                uint32_t chr_bank,
                                uint32_t chr_table,
                                int x,
                                int y,
                                int scale)
{
    uint16_t tile_base = (uint16_t)((chr_table & 1U) * 0x100U);
    for (uint16_t tile = 0; tile < 256U; ++tile) {
        int tile_x = x + (int)(tile & 0x0FU) * 8 * scale;
        int tile_y = y + (int)(tile >> 4U) * 8 * scale;
        draw_chr_tile(fb, chr_bytes, chr_byte_count, chr_bank, (uint16_t)(tile_base + tile), tile_x, tile_y, scale);
    }
}

static void render_chr_playground(const TecmoRuntime *runtime, TecmoFramebuffer *fb)
{
    const uint32_t chr_bank = selected_chr_bank(runtime);
    const uint32_t bank_count = chr_bank_count(runtime);
    const uint32_t chr_table = selected_chr_table(runtime);
    const uint16_t tile_base = (uint16_t)(chr_table * 0x100U);
    const int tile_scale = 2;
    const int cell_w = 34;
    const int cell_h = 28;
    const int grid_x = 28;
    const int grid_y = 74;
    char line[160];

    clear(fb, rgb(8, 10, 16));
    rect(fb, 0, 0, fb->width, 52, rgb(18, 18, 34));
    (void)snprintf(line,
                   sizeof(line),
                   "CHR PLAYGROUND - BANK %02u OF %02u TABLE %u",
                   (unsigned)chr_bank,
                   (unsigned)(bank_count - 1U),
                   (unsigned)chr_table);
    draw_text(fb, 24, 18, line, rgb(248, 248, 232), 2);
    draw_text(fb, 356, 40, "LR BANK UP DN TABLE TAB NEXT ENTER ESC", rgb(142, 174, 190), 1);

    if (!runtime->title_probe_available) {
        draw_centered_text(fb, 212, "LOCAL CHR DATA UNAVAILABLE", rgb(252, 236, 170), 2);
        return;
    }

    (void)snprintf(line,
                   sizeof(line),
                   "TILE ID GRID %03X-%03X  NUMBERS LETTERS AND TITLE PARTS",
                   (unsigned)(tile_base + 0x80U),
                   (unsigned)(tile_base + 0xAFU));
    draw_text(fb, 28, 56, line, rgb(142, 174, 190), 1);
    for (uint8_t row = 0; row < 3; ++row) {
        for (uint8_t col = 0; col < 16; ++col) {
            uint16_t tile = (uint16_t)(tile_base + 0x80U + row * 16U + col);
            int x = grid_x + (int)col * cell_w;
            int y = grid_y + (int)row * cell_h;
            (void)snprintf(line, sizeof(line), "%03X", (unsigned)tile);
            draw_text(fb, x, y - 10, line, rgb(92, 116, 128), 1);
            draw_chr_tile(fb,
                          runtime->title_chr_bytes,
                          runtime->title_chr_byte_count,
                          chr_bank,
                          tile,
                          x + 5,
                          y,
                          tile_scale);
        }
    }

    rect(fb, 26, 174, 588, 2, rgb(52, 60, 72));
    (void)snprintf(line,
                   sizeof(line),
                   "ASSEMBLED 2X2 TITLE GLYPHS FROM BANK 06 MAP AND BANK %02u TABLE %u",
                   (unsigned)chr_bank,
                   (unsigned)chr_table);
    draw_text(fb, 28, 190, line, rgb(142, 174, 190), 1);
    {
        int scale = 2;
        int glyph_width = 16 * scale;
        int title_width = (int)runtime->title_glyphs.glyph_count * glyph_width;
        int start_x;
        if (title_width > fb->width - 40) {
            scale = 1;
            glyph_width = 16;
            title_width = (int)runtime->title_glyphs.glyph_count * glyph_width;
        }
        start_x = (fb->width - title_width) / 2;
        for (size_t i = 0; i < runtime->title_glyphs.glyph_count; ++i) {
            draw_title_glyph(fb,
                             runtime->title_chr_bytes,
                             runtime->title_chr_byte_count,
                             chr_bank,
                             chr_table,
                             &runtime->title_glyphs.glyphs[i],
                             start_x + (int)i * glyph_width,
                             218,
                             scale);
        }
    }

    draw_text(fb, 28, 286, "TITLE MAP SAMPLE", rgb(230, 232, 214), 1);
    for (size_t row = 0; row < 4; ++row) {
        char *out = line;
        size_t remaining = sizeof(line);
        size_t start = row * 3U;
        int written;
        line[0] = '\0';
        for (size_t i = start; i < start + 3U && i < runtime->title_glyphs.glyph_count; ++i) {
            const TecmoTitleGlyph *glyph = &runtime->title_glyphs.glyphs[i];
            written = snprintf(out,
                               remaining,
                               "%c=%02X[%02X %02X %02X %02X]  ",
                               glyph->character,
                               (unsigned)glyph->tile_index,
                               (unsigned)glyph->glyph_tiles[0],
                               (unsigned)glyph->glyph_tiles[1],
                               (unsigned)glyph->glyph_tiles[2],
                               (unsigned)glyph->glyph_tiles[3]);
            if (written < 0 || (size_t)written >= remaining) {
                break;
            }
            out += written;
            remaining -= (size_t)written;
        }
        draw_text(fb, 28, 308 + (int)row * 18, line, rgb(142, 174, 190), 1);
    }

    draw_text(fb, 28, 398, "SOURCE CANDIDATES: BANK04 C-0116..C-0140 DRIVER, BANK00 C-0191 C-0192 TEXT LAYOUT", rgb(142, 174, 190), 1);
    draw_text(fb, 28, 420, "PLAYGROUND OUTPUT STAYS LOCAL UNDER BUILD WHEN RENDERED BY TESTS", rgb(230, 232, 214), 1);
}

static void render_intro_layout_lab(const TecmoRuntime *runtime, TecmoFramebuffer *fb)
{
    const uint32_t chr_bank = selected_chr_bank(runtime);
    const uint32_t bank_count = chr_bank_count(runtime);
    const uint32_t chr_table = selected_chr_table(runtime);
    const uint16_t selected_tile = selected_intro_tile_id(runtime);
    const int sheet_x = 30;
    const int sheet_y = 76;
    const int sheet_scale = 2;
    const int canvas_x = 330;
    const int canvas_y = 82;
    const int canvas_w = 284;
    const int canvas_h = 230;
    char line[128];

    clear(fb, rgb(6, 7, 10));
    rect(fb, 0, 0, fb->width, 52, rgb(18, 18, 34));
    (void)snprintf(line,
                   sizeof(line),
                   "INTRO LAB - BANK %02u OF %02u TABLE %u",
                   (unsigned)chr_bank,
                   (unsigned)(bank_count - 1U),
                   (unsigned)chr_table);
    draw_text(fb, 22, 18, line, rgb(248, 248, 232), 2);
    draw_text(fb, 282, 40, "QE BANK T TABLE TAB FOCUS R RAB M TECMO C COMP", rgb(142, 174, 190), 1);

    if (!runtime->title_probe_available) {
        draw_centered_text(fb, 212, "LOCAL CHR DATA UNAVAILABLE", rgb(252, 236, 170), 2);
        return;
    }

    (void)snprintf(line, sizeof(line), "REAL CHR BANK %02u TABLE %u SHEET", (unsigned)chr_bank, (unsigned)chr_table);
    draw_text(fb, sheet_x, 58, line, rgb(142, 174, 190), 1);
    for (uint8_t col = 0; col < 16U; ++col) {
        (void)snprintf(line, sizeof(line), "%X", (unsigned)col);
        draw_text(fb, sheet_x + (int)col * 16 + 5, sheet_y - 12, line, rgb(92, 116, 128), 1);
    }
    for (uint8_t row = 0; row < 16U; ++row) {
        (void)snprintf(line, sizeof(line), "%X", (unsigned)row);
        draw_text(fb, sheet_x - 12, sheet_y + (int)row * 16 + 5, line, rgb(92, 116, 128), 1);
    }
    draw_chr_bank_sheet(fb,
                        runtime->title_chr_bytes,
                        runtime->title_chr_byte_count,
                        chr_bank,
                        chr_table,
                        sheet_x,
                        sheet_y,
                        sheet_scale);
    {
        int source_col = (int)(runtime->intro_source_tile & 0x0FU);
        int source_row = (int)((runtime->intro_source_tile >> 4U) & 0x0FU);
        uint32_t color = runtime->intro_canvas_focus ? rgb(92, 116, 128) : rgb(252, 236, 118);
        outline_rect(fb,
                     sheet_x + source_col * 16 - 2,
                     sheet_y + source_row * 16 - 2,
                     20,
                     20,
                     color);
    }

    rect(fb, canvas_x - 2, canvas_y - 2, canvas_w + 4, canvas_h + 4, rgb(72, 86, 96));
    rect(fb, canvas_x, canvas_y, canvas_w, canvas_h, rgb(0, 0, 0));
    for (int x = 0; x <= canvas_w; x += 16) {
        rect(fb, canvas_x + x, canvas_y, 1, canvas_h, rgb(22, 26, 32));
    }
    for (int y = 0; y <= canvas_h; y += 16) {
        rect(fb, canvas_x, canvas_y + y, canvas_w, 1, rgb(22, 26, 32));
    }
    draw_text(fb, canvas_x, canvas_y - 20, "ASSET-BACKED TARGET CANVAS", rgb(142, 174, 190), 1);

    if (runtime->intro_placement_count == 0 && runtime->intro_glyphs.glyph_count >= 14U) {
        draw_title_glyph_range(fb,
                               runtime->title_chr_bytes,
                               runtime->title_chr_byte_count,
                               chr_bank,
                               chr_table,
                               &runtime->intro_glyphs,
                               0,
                               5,
                               canvas_x + 62,
                               canvas_y + 58,
                               2);
        draw_title_glyph_range(fb,
                               runtime->title_chr_bytes,
                               runtime->title_chr_byte_count,
                               chr_bank,
                               chr_table,
                               &runtime->intro_glyphs,
                               6,
                               8,
                               canvas_x + 14,
                               canvas_y + 138,
                               2);
    } else if (runtime->intro_placement_count == 0) {
        draw_text(fb, canvas_x + 24, canvas_y + 104, "INTRO GLYPH MAP MISSING", rgb(252, 236, 170), 1);
    }

    for (size_t i = 0; i < runtime->intro_placement_count; ++i) {
        const TecmoIntroPlacement *placement = &runtime->intro_placements[i];
        if (!placement->active) {
            continue;
        }
        for (size_t tile_index = 0; tile_index < placement->tile_count; ++tile_index) {
            draw_chr_tile(fb,
                          runtime->title_chr_bytes,
                          runtime->title_chr_byte_count,
                          placement->chr_bank,
                          placement->tile_ids[tile_index],
                          canvas_x + placement->pixel_x + (int)tile_index * 8 * placement->scale,
                          canvas_y + placement->pixel_y,
                          placement->scale);
        }
    }

    {
        int cursor_x = canvas_x + runtime->intro_canvas_cell_x * INTRO_CANVAS_CELL_SIZE;
        int cursor_y = canvas_y + runtime->intro_canvas_cell_y * INTRO_CANVAS_CELL_SIZE;
        uint32_t color = runtime->intro_canvas_focus ? rgb(252, 236, 118) : rgb(92, 116, 128);
        outline_rect(fb, cursor_x, cursor_y, INTRO_CANVAS_CELL_SIZE, INTRO_CANVAS_CELL_SIZE, color);
    }

    (void)snprintf(line,
                   sizeof(line),
                   "FOCUS %s  SRC B%02u T%u %03X  CELL %02d %02d  RECORDS %02u",
                   runtime->intro_canvas_focus ? "CANVAS" : "SOURCE",
                   (unsigned)chr_bank,
                   (unsigned)chr_table,
                   (unsigned)selected_tile,
                   runtime->intro_canvas_cell_x,
                   runtime->intro_canvas_cell_y,
                   (unsigned)runtime->intro_placement_count);
    draw_text(fb, 30, 342, line, rgb(230, 232, 214), 1);
    draw_text(fb, 30, 360, "ARROWS MOVE FOCUS  SPACE RECORD  S SAVE  BACKSPACE REMOVE  ENTER ESC MENU", rgb(142, 174, 190), 1);
    draw_text(fb, 30, 378, runtime->intro_layout_status, runtime->intro_layout_dirty ? rgb(252, 236, 118) : rgb(142, 174, 190), 1);

    if (runtime->intro_placement_count == 0) {
        draw_text(fb, 30, 406, "NO RECORDS YET  SPACE ADDS THE SELECTED TILE TO THE CANVAS", rgb(92, 116, 128), 1);
    } else {
        size_t first = runtime->intro_placement_count > 8U ? runtime->intro_placement_count - 8U : 0U;
        draw_text(fb, 30, 400, "LOCAL PLACEMENT RECORDS", rgb(230, 232, 214), 1);
        for (size_t i = first; i < runtime->intro_placement_count; ++i) {
            const TecmoIntroPlacement *placement = &runtime->intro_placements[i];
            (void)snprintf(line,
                           sizeof(line),
                           "%02u  %s  TILE %03X  CELL %02d %02d",
                           (unsigned)(i + 1U),
                           placement->label,
                           (unsigned)(placement->tile_count > 0 ? placement->tile_ids[0] : 0U),
                           placement->canvas_cell_x,
                           placement->canvas_cell_y);
            draw_text(fb, 30, 408 + (int)(i - first) * 8, line, rgb(142, 174, 190), 1);
        }
    }
    draw_text(fb, 30, 470, "S WRITES IGNORED BUILD INTRO_LAYOUT_PICKS.JSON", rgb(92, 116, 128), 1);
}

void tecmo_render_original_title_chr_probe(TecmoFramebuffer *framebuffer,
                                           const TecmoOriginalTitleGlyphs *glyphs,
                                           const uint8_t *chr_bytes,
                                           uint64_t chr_byte_count,
                                           uint32_t chr_bank)
{
    char line[160];
    int scale = 3;
    int glyph_width = 16 * scale;
    int title_width;
    int start_x;
    int y = 150;

    clear(framebuffer, rgb(8, 10, 24));
    rect(framebuffer, 0, 0, framebuffer->width, 48, rgb(18, 18, 34));
    rect(framebuffer, 0, framebuffer->height - 58, framebuffer->width, 58, rgb(18, 18, 34));
    rect(framebuffer, 36, 94, framebuffer->width - 72, 192, rgb(52, 28, 58));
    rect(framebuffer, 46, 104, framebuffer->width - 92, 172, rgb(16, 22, 48));
    rect(framebuffer, 58, 116, framebuffer->width - 116, 148, rgb(96, 34, 52));
    rect(framebuffer, 68, 126, framebuffer->width - 136, 128, rgb(20, 26, 54));

    if (glyphs == NULL || glyphs->glyph_count == 0 || chr_bytes == NULL || chr_byte_count == 0) {
        draw_centered_text(framebuffer, 188, "CHR TITLE DATA MISSING", rgb(252, 236, 170), 2);
        return;
    }

    title_width = (int)glyphs->glyph_count * glyph_width;
    if (title_width > framebuffer->width - 32) {
        scale = 2;
        glyph_width = 16 * scale;
        title_width = (int)glyphs->glyph_count * glyph_width;
    }
    if (title_width > framebuffer->width - 32) {
        scale = 1;
        glyph_width = 16 * scale;
        title_width = (int)glyphs->glyph_count * glyph_width;
    }
    start_x = (framebuffer->width - title_width) / 2;

    for (size_t i = 0; i < glyphs->glyph_count; ++i) {
        draw_title_glyph(framebuffer, chr_bytes, chr_byte_count, chr_bank, 0U, &glyphs->glyphs[i], start_x + (int)i * glyph_width, y, scale);
    }

    draw_centered_text(framebuffer, 294, "NATIVE CHR TITLE GLYPH PROBE", rgb(142, 174, 190), 1);
    (void)snprintf(line,
                   sizeof(line),
                   "C711 %02X -> BANK %02X:%04X  CHR BANK %02u",
                   (unsigned)glyphs->dispatcher_call_index,
                   (unsigned)glyphs->dispatcher_bank,
                   (unsigned)glyphs->dispatcher_target,
                   (unsigned)chr_bank);
    draw_centered_text(framebuffer, 316, line, rgb(142, 174, 190), 1);
    (void)snprintf(line,
                   sizeof(line),
                   "SETUP 0100=%02X 0352=%02X  BA16 05B6|=%02X",
                   (unsigned)glyphs->chr_config_0100,
                   (unsigned)glyphs->setup_selector_0352,
                   (unsigned)glyphs->ba16_update_flags_or_05b6);
    draw_centered_text(framebuffer, 338, line, rgb(230, 232, 214), 1);

    if (glyphs->setup_summary.loaded) {
        const TecmoTitleSetupSummary *setup = &glyphs->setup_summary;
        (void)snprintf(line,
                       sizeof(line),
                       "STAGE BA25 CALLS %02u/%02u WRITES %02u TABLES %02u/%02u",
                       (unsigned)setup->driver_call_count,
                       (unsigned)setup->driver_call_invocations,
                       (unsigned)setup->driver_write_count,
                       (unsigned)setup->verified_table_reference_count,
                       (unsigned)setup->table_reference_count);
        draw_centered_text(framebuffer, 356, line, rgb(142, 174, 190), 1);

        if (setup->fixed_helper_summary_loaded) {
            if (setup->fixed_vector_summary_loaded) {
                (void)snprintf(line,
                               sizeof(line),
                               "FIXED %02u-%02u VEC %02u-%02u WAIT %03u SEED %02u FIN %02u-%02u",
                               (unsigned)setup->fixed_helper_unique_count,
                               (unsigned)setup->fixed_helper_call_invocations,
                               (unsigned)setup->fixed_vector_jmp_entry_count,
                               (unsigned)setup->fixed_vector_entry_count,
                               (unsigned)setup->fixed_wait_request_total,
                               (unsigned)setup->fixed_staging_seed_call_count,
                               (unsigned)setup->fixed_setup_finalize_call_count,
                               (unsigned)setup->fixed_stream_finalize_call_count);
            } else {
                (void)snprintf(line,
                               sizeof(line),
                               "FIXED %02u-%02u WAIT %02u=%03u SEED %02u FIN %02u-%02u",
                               (unsigned)setup->fixed_helper_unique_count,
                               (unsigned)setup->fixed_helper_call_invocations,
                               (unsigned)setup->fixed_wait_call_count,
                               (unsigned)setup->fixed_wait_request_total,
                               (unsigned)setup->fixed_staging_seed_call_count,
                               (unsigned)setup->fixed_setup_finalize_call_count,
                               (unsigned)setup->fixed_stream_finalize_call_count);
            }
            draw_centered_text(framebuffer, 374, line, rgb(142, 174, 190), 1);
        }

        if (setup->first_unclassified_call != 0U) {
            (void)snprintf(line,
                           sizeof(line),
                           "STREAM BAA4 WRITES %02u  UNCLASSIFIED CALL %04X",
                           (unsigned)setup->stream_write_count,
                           (unsigned)setup->first_unclassified_call);
        } else {
            (void)snprintf(line,
                           sizeof(line),
                           "STREAM BAA4 WRITES %02u  FIXED HELPERS PENDING",
                           (unsigned)setup->stream_write_count);
        }
        draw_centered_text(framebuffer, 392, line, rgb(142, 174, 190), 1);

        if (setup->stream_format_summary_loaded) {
            (void)snprintf(line,
                           sizeof(line),
                           "STREAM TABLE %02u/%02u SELECTED %02u ROWS %02u/%02u MAX %02u REC",
                           (unsigned)setup->verified_stream_table_entry_count,
                           (unsigned)setup->stream_table_entry_count,
                           (unsigned)setup->selected_stream_count,
                           (unsigned)setup->terminated_selector_row_count,
                           (unsigned)setup->dynamic_selector_row_count,
                           (unsigned)setup->max_stream_record_count);
            draw_centered_text(framebuffer, 410, line, rgb(142, 174, 190), 1);
        }

        if (setup->stream_effect_summary_loaded) {
            (void)snprintf(line,
                           sizeof(line),
                           "STREAM EFFECT BASE %02u SRC %02u OUT %02u MAX %03u TO %03u",
                           (unsigned)setup->stream_base_parameter_bytes,
                           (unsigned)setup->stream_source_fields_per_record,
                           (unsigned)setup->stream_staged_fields_per_record,
                           (unsigned)setup->max_stream_bytes_consumed,
                           (unsigned)setup->max_stream_emitted_bytes);
            draw_centered_text(framebuffer, 428, line, rgb(142, 174, 190), 1);
        }

        if (setup->stream_staging_summary_loaded) {
            (void)snprintf(line,
                           sizeof(line),
                           "STAGING %02u STREAMS %03u REC %04u BYTES %04X-%04X",
                           (unsigned)setup->stream_staging_stream_count,
                           (unsigned)setup->stream_staging_record_count,
                           (unsigned)setup->stream_staging_bytes_written,
                           (unsigned)setup->stream_staging_first_write,
                           (unsigned)setup->stream_staging_last_write);
            draw_centered_text(framebuffer, 442, line, rgb(142, 174, 190), 1);
        }
    }

    rect(framebuffer, 116, 452, 408, 2, rgb(236, 214, 112));
    if (glyphs->setup_summary.loaded && glyphs->setup_summary.palette_probe_summary_loaded) {
        const TecmoTitleSetupSummary *setup = &glyphs->setup_summary;
        (void)snprintf(line,
                       sizeof(line),
                       "PAL PPU %02u-%02u HIGH %02u VEC %02u-%02u QUEUE %s",
                       (unsigned)setup->palette_direct_ppu_addr_write_count,
                       (unsigned)setup->palette_direct_ppu_data_write_count,
                       (unsigned)setup->palette_direct_high_literal_count,
                       (unsigned)setup->fixed_vector_jmp_entry_count,
                       (unsigned)setup->fixed_vector_entry_count,
                       setup->palette_queue_decode_pending ? "PENDING" : "CHECK");
        draw_centered_text(framebuffer, 464, line, rgb(230, 232, 214), 1);
    } else {
        draw_centered_text(framebuffer, 464, "HELPER DETAILS AND PALETTE DECODE NEXT", rgb(230, 232, 214), 1);
    }
}

void tecmo_runtime_render(const TecmoRuntime *runtime, TecmoFramebuffer *framebuffer)
{
    if (runtime->mode == TECMO_MODE_MAIN_MENU) {
        render_main_menu(runtime, framebuffer);
    } else if (runtime->mode == TECMO_MODE_TITLE_SCREEN) {
        render_title_screen_mode(runtime, framebuffer);
    } else if (runtime->mode == TECMO_MODE_INTRO_PROBE) {
        render_intro_layout_lab(runtime, framebuffer);
    } else if (runtime->mode == TECMO_MODE_CHR_PLAYGROUND) {
        render_chr_playground(runtime, framebuffer);
    } else if (runtime->mode == TECMO_MODE_PLAY_SETUP) {
        render_roster_browser(runtime, framebuffer, true);
    } else if (runtime->mode == TECMO_MODE_ROSTERS) {
        render_roster_browser(runtime, framebuffer, false);
    } else if (runtime->mode == TECMO_MODE_COURT) {
        render_court(runtime, framebuffer);
    }

    if (runtime->debug_overlay) {
        render_debug_overlay(runtime, framebuffer);
    }
}
