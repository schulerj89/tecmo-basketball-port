#include "tecmo_game.h"
#include "tecmo_intro_stage.h"

#include <stdio.h>
#include <string.h>

#define INTRO_LAYOUT_CHR_BANK_BYTES 8192U

static uint32_t layout_chr_bank_count(const TecmoRuntime *runtime)
{
    uint64_t count = runtime->title_chr_byte_count / INTRO_LAYOUT_CHR_BANK_BYTES;
    if (count == 0) {
        return 1U;
    }
    if (count > 32U) {
        count = 32U;
    }
    return (uint32_t)count;
}

static uint32_t layout_selected_chr_bank(const TecmoRuntime *runtime)
{
    uint32_t count = layout_chr_bank_count(runtime);
    if (runtime->selected_chr_bank >= count) {
        return count - 1U;
    }
    return runtime->selected_chr_bank;
}

static uint32_t layout_selected_chr_table(const TecmoRuntime *runtime)
{
    return runtime->selected_chr_table & 1U;
}

static uint16_t layout_selected_tile_id(const TecmoRuntime *runtime)
{
    return (uint16_t)(layout_selected_chr_table(runtime) * 0x100U + (runtime->intro_source_tile & 0xFFU));
}

static void set_intro_layout_status(char *dest, size_t dest_size, const char *text)
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
    fprintf(file, "  \"selected_chr_bank\": %u,\n", (unsigned)layout_selected_chr_bank(runtime));
    fprintf(file, "  \"selected_chr_table\": %u,\n", (unsigned)layout_selected_chr_table(runtime));
    fprintf(file, "  \"selected_tile_id_hex\": \"%03X\",\n", (unsigned)layout_selected_tile_id(runtime));
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

void tecmo_intro_layout_init(TecmoRuntime *runtime)
{
    runtime->intro_source_tile = 0x80U;
    runtime->intro_canvas_cell_x = 4;
    runtime->intro_canvas_cell_y = 4;
    set_intro_layout_status(runtime->intro_layout_status,
                            sizeof(runtime->intro_layout_status),
                            "SPACE RECORD  S SAVE  BACKSPACE REMOVE");
}

int tecmo_intro_layout_save(TecmoRuntime *runtime)
{
    if (write_intro_layout_file_to_path(runtime, "build\\intro_layout_picks.json") == 0) {
        runtime->intro_layout_saved = true;
        runtime->intro_layout_dirty = false;
        set_intro_layout_status(runtime->intro_layout_status,
                                sizeof(runtime->intro_layout_status),
                                "SAVED BUILD INTRO_LAYOUT_PICKS.JSON");
        return 0;
    }
    if (write_intro_layout_file_to_path(runtime, "intro_layout_picks.json") == 0) {
        runtime->intro_layout_saved = true;
        runtime->intro_layout_dirty = false;
        set_intro_layout_status(runtime->intro_layout_status,
                                sizeof(runtime->intro_layout_status),
                                "SAVED INTRO_LAYOUT_PICKS.JSON");
        return 0;
    }

    runtime->intro_layout_saved = false;
    set_intro_layout_status(runtime->intro_layout_status,
                            sizeof(runtime->intro_layout_status),
                            "SAVE FAILED");
    return -1;
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

void tecmo_intro_layout_add_current(TecmoRuntime *runtime)
{
    char label[32];
    uint16_t tile = layout_selected_tile_id(runtime);
    int cell_x = runtime->intro_canvas_cell_x;
    int cell_y = runtime->intro_canvas_cell_y;

    (void)snprintf(label,
                   sizeof(label),
                   "B%02u T%u %03X",
                   (unsigned)layout_selected_chr_bank(runtime),
                   (unsigned)layout_selected_chr_table(runtime),
                   (unsigned)tile);
    if (append_intro_tile_placement(runtime,
                                    layout_selected_chr_bank(runtime),
                                    layout_selected_chr_table(runtime),
                                    tile,
                                    cell_x,
                                    cell_y,
                                    label) != 0) {
        set_intro_layout_status(runtime->intro_layout_status,
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

void tecmo_intro_layout_add_rabbit_preset(TecmoRuntime *runtime)
{
    typedef struct RabbitSprite {
        uint8_t raw_tile_low;
        int dx;
        const char *label;
    } RabbitSprite;

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

    if (chr_bank >= layout_chr_bank_count(runtime)) {
        chr_bank = layout_selected_chr_bank(runtime);
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
        set_intro_layout_status(runtime->intro_layout_status,
                                sizeof(runtime->intro_layout_status),
                                "PLACEMENT LIST FULL");
    } else if (runtime->intro_placement_count - before < sizeof(rabbit_sprites) / sizeof(rabbit_sprites[0]) * 2U) {
        set_intro_layout_status(runtime->intro_layout_status,
                                sizeof(runtime->intro_layout_status),
                                "RABBIT PRESET PARTIAL  LIST FULL");
    } else {
        set_intro_layout_status(runtime->intro_layout_status,
                                sizeof(runtime->intro_layout_status),
                                "RABBIT LOOKUP B31 T1 124-12B RECORDED");
    }
}

void tecmo_intro_layout_add_tecmo_logo_preset(TecmoRuntime *runtime)
{
    uint32_t chr_bank = 31U;
    const uint32_t chr_table = 1U;
    int base_x = runtime->intro_canvas_cell_x;
    int base_y = runtime->intro_canvas_cell_y;
    size_t before = runtime->intro_placement_count;

    if (chr_bank >= layout_chr_bank_count(runtime)) {
        chr_bank = layout_selected_chr_bank(runtime);
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
        set_intro_layout_status(runtime->intro_layout_status,
                                sizeof(runtime->intro_layout_status),
                                "PLACEMENT LIST FULL");
    } else if (runtime->intro_placement_count - before < 20U) {
        set_intro_layout_status(runtime->intro_layout_status,
                                sizeof(runtime->intro_layout_status),
                                "TECMO PRESET PARTIAL  LIST FULL");
    } else {
        set_intro_layout_status(runtime->intro_layout_status,
                                sizeof(runtime->intro_layout_status),
                                "TECMO VISUAL CANDIDATE B31 T1 180-193 RECORDED");
    }
}

void tecmo_intro_layout_add_composite_preset(TecmoRuntime *runtime)
{
    size_t before = runtime->intro_placement_count;

    runtime->intro_canvas_cell_x = 7;
    runtime->intro_canvas_cell_y = 5;
    tecmo_intro_layout_add_tecmo_logo_preset(runtime);

    runtime->intro_canvas_cell_x = 4;
    runtime->intro_canvas_cell_y = 4;
    tecmo_intro_layout_add_rabbit_preset(runtime);

    if (runtime->intro_placement_count == before) {
        set_intro_layout_status(runtime->intro_layout_status,
                                sizeof(runtime->intro_layout_status),
                                "PLACEMENT LIST FULL");
    } else {
        runtime->intro_canvas_cell_x = 4;
        runtime->intro_canvas_cell_y = 4;
        set_intro_layout_status(runtime->intro_layout_status,
                                sizeof(runtime->intro_layout_status),
                                "INTRO COMPOSITE CANDIDATE RABBIT PLUS TECMO RECORDED");
    }
}

void tecmo_intro_layout_remove_last(TecmoRuntime *runtime)
{
    if (runtime->intro_placement_count == 0) {
        set_intro_layout_status(runtime->intro_layout_status,
                                sizeof(runtime->intro_layout_status),
                                "NO PLACEMENT TO REMOVE");
        return;
    }

    --runtime->intro_placement_count;
    memset(&runtime->intro_placements[runtime->intro_placement_count], 0, sizeof(runtime->intro_placements[0]));
    runtime->intro_layout_dirty = true;
    runtime->intro_layout_saved = false;
    set_intro_layout_status(runtime->intro_layout_status,
                            sizeof(runtime->intro_layout_status),
                            "REMOVED LAST PLACEMENT");
}

void tecmo_intro_layout_move_source_tile(TecmoRuntime *runtime, int dx, int dy)
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

void tecmo_intro_layout_move_canvas_cursor(TecmoRuntime *runtime, int dx, int dy)
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
