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

bool tecmo_runtime_init(TecmoRuntime *runtime, TecmoGameMemory *memory, const char *project_root)
{
    memset(runtime, 0, sizeof(*runtime));
    runtime->memory = memory;
    runtime->selected_chr_bank = 31U;

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

static void update_probe_screen(TecmoRuntime *runtime, const TecmoInput *input)
{
    if (runtime->mode == TECMO_MODE_INTRO_PROBE || runtime->mode == TECMO_MODE_CHR_PLAYGROUND) {
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

    (void)snprintf(line, sizeof(line), "FPS %.1f MENU %u TEAM %u PLAYER %u CHR %02u/%02u",
                   (double)fps,
                   (unsigned)runtime->selected_menu_item,
                   (unsigned)(runtime->selected_team + 1U),
                   (unsigned)(runtime->selected_player + 1U),
                   (unsigned)selected_chr_bank(runtime),
                   (unsigned)(chr_bank_count(runtime) - 1U));
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

static void draw_chr_tile(TecmoFramebuffer *fb,
                          const uint8_t *chr_bytes,
                          uint64_t chr_byte_count,
                          uint32_t chr_bank,
                          uint8_t tile,
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

    if (tile == 0xFFU || tile_offset + 15ULL >= chr_byte_count) {
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
                             const TecmoTitleGlyph *glyph,
                             int x,
                             int y,
                             int scale)
{
    draw_chr_tile(fb, chr_bytes, chr_byte_count, chr_bank, glyph->glyph_tiles[0], x, y, scale);
    draw_chr_tile(fb, chr_bytes, chr_byte_count, chr_bank, glyph->glyph_tiles[1], x + 8 * scale, y, scale);
    draw_chr_tile(fb, chr_bytes, chr_byte_count, chr_bank, glyph->glyph_tiles[2], x, y + 8 * scale, scale);
    draw_chr_tile(fb, chr_bytes, chr_byte_count, chr_bank, glyph->glyph_tiles[3], x + 8 * scale, y + 8 * scale, scale);
}

static void draw_title_glyph_range(TecmoFramebuffer *fb,
                                   const uint8_t *chr_bytes,
                                   uint64_t chr_byte_count,
                                   uint32_t chr_bank,
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
                                int x,
                                int y,
                                int scale)
{
    for (uint16_t tile = 0; tile < 256U; ++tile) {
        int tile_x = x + (int)(tile & 0x0FU) * 8 * scale;
        int tile_y = y + (int)(tile >> 4U) * 8 * scale;
        draw_chr_tile(fb, chr_bytes, chr_byte_count, chr_bank, (uint8_t)tile, tile_x, tile_y, scale);
    }
}

static void render_chr_playground(const TecmoRuntime *runtime, TecmoFramebuffer *fb)
{
    const uint32_t chr_bank = selected_chr_bank(runtime);
    const uint32_t bank_count = chr_bank_count(runtime);
    const int tile_scale = 2;
    const int cell_w = 34;
    const int cell_h = 28;
    const int grid_x = 28;
    const int grid_y = 74;
    char line[160];

    clear(fb, rgb(8, 10, 16));
    rect(fb, 0, 0, fb->width, 52, rgb(18, 18, 34));
    (void)snprintf(line, sizeof(line), "CHR PLAYGROUND - BANK %02u OF %02u", (unsigned)chr_bank, (unsigned)(bank_count - 1U));
    draw_text(fb, 24, 18, line, rgb(248, 248, 232), 2);
    draw_text(fb, 394, 40, "LEFT RIGHT BANK TAB NEXT ENTER ESC MENU", rgb(142, 174, 190), 1);

    if (!runtime->title_probe_available) {
        draw_centered_text(fb, 212, "LOCAL CHR DATA UNAVAILABLE", rgb(252, 236, 170), 2);
        return;
    }

    draw_text(fb, 28, 56, "TILE ID GRID 80-AF  NUMBERS LETTERS AND TITLE PARTS", rgb(142, 174, 190), 1);
    for (uint8_t row = 0; row < 3; ++row) {
        for (uint8_t col = 0; col < 16; ++col) {
            uint8_t tile = (uint8_t)(0x80U + row * 16U + col);
            int x = grid_x + (int)col * cell_w;
            int y = grid_y + (int)row * cell_h;
            (void)snprintf(line, sizeof(line), "%02X", (unsigned)tile);
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
                   "ASSEMBLED 2X2 TITLE GLYPHS FROM BANK 06 MAP AND BANK %02u CHR",
                   (unsigned)chr_bank);
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
    (void)snprintf(line, sizeof(line), "INTRO LAB - BANK %02u OF %02u", (unsigned)chr_bank, (unsigned)(bank_count - 1U));
    draw_text(fb, 22, 18, line, rgb(248, 248, 232), 2);
    draw_text(fb, 394, 40, "LEFT RIGHT BANK TAB NEXT ENTER ESC MENU", rgb(142, 174, 190), 1);

    if (!runtime->title_probe_available) {
        draw_centered_text(fb, 212, "LOCAL CHR DATA UNAVAILABLE", rgb(252, 236, 170), 2);
        return;
    }

    (void)snprintf(line, sizeof(line), "REAL CHR BANK %02u SHEET", (unsigned)chr_bank);
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
                        sheet_x,
                        sheet_y,
                        sheet_scale);

    rect(fb, canvas_x - 2, canvas_y - 2, canvas_w + 4, canvas_h + 4, rgb(72, 86, 96));
    rect(fb, canvas_x, canvas_y, canvas_w, canvas_h, rgb(0, 0, 0));
    for (int x = 0; x <= canvas_w; x += 16) {
        rect(fb, canvas_x + x, canvas_y, 1, canvas_h, rgb(22, 26, 32));
    }
    for (int y = 0; y <= canvas_h; y += 16) {
        rect(fb, canvas_x, canvas_y + y, canvas_w, 1, rgb(22, 26, 32));
    }
    draw_text(fb, canvas_x, canvas_y - 20, "ASSET-BACKED TARGET CANVAS", rgb(142, 174, 190), 1);

    if (runtime->intro_glyphs.glyph_count >= 14U) {
        draw_title_glyph_range(fb,
                               runtime->title_chr_bytes,
                               runtime->title_chr_byte_count,
                               chr_bank,
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
                               &runtime->intro_glyphs,
                               6,
                               8,
                               canvas_x + 14,
                               canvas_y + 138,
                               2);
    } else {
        draw_text(fb, canvas_x + 24, canvas_y + 104, "INTRO GLYPH MAP MISSING", rgb(252, 236, 170), 1);
    }

    draw_text(fb, 30, 352, "POINT ME AT TILES BY HEX ID: ROW AND COL, FOR EXAMPLE B6 OR D2", rgb(230, 232, 214), 1);
    draw_text(fb, 30, 374, "POINT ME AT CANVAS CELLS BY 16PX GRID OFFSET FROM THE CANVAS TOP LEFT", rgb(142, 174, 190), 1);
    (void)snprintf(line,
                   sizeof(line),
                   "CURRENT RIGHT SIDE USES BANK06 CHARACTER MAP AND BANK %02u 2X2 GLYPH TILES",
                   (unsigned)chr_bank);
    draw_text(fb, 30, 396, line, rgb(142, 174, 190), 1);
    draw_text(fb, 30, 418, "NEXT: TURN YOUR TILE PICKS INTO LOCAL-ONLY LAYOUT JSON AND THEN DECODE BANK04 SCRIPT PATH", rgb(142, 174, 190), 1);
    draw_text(fb, 30, 446, "NO CHR BYTES OR GENERATED ORIGINAL-ASSET SCREENSHOTS ARE COMMITTED", rgb(92, 116, 128), 1);
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
        draw_title_glyph(framebuffer, chr_bytes, chr_byte_count, chr_bank, &glyphs->glyphs[i], start_x + (int)i * glyph_width, y, scale);
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
