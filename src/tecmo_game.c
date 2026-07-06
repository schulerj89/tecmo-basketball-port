#include "tecmo_game.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COURT_LEFT 48
#define COURT_TOP 54
#define COURT_RIGHT 592
#define COURT_BOTTOM 424

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

bool tecmo_runtime_init(TecmoRuntime *runtime, TecmoGameMemory *memory, const char *project_root)
{
    memset(runtime, 0, sizeof(*runtime));
    runtime->memory = memory;

    if (tecmo_collect_rosters(project_root, &runtime->roster) != 0) {
        return false;
    }

    for (size_t i = 0; i < runtime->roster.count; ++i) {
        add_team_if_missing(runtime, runtime->roster.records[i].team);
    }

    runtime->mode = TECMO_MODE_MAIN_MENU;
    runtime->frame_seconds = 1.0f / 60.0f;
    runtime->player_x = 320.0f;
    runtime->player_y = 260.0f;
    runtime->ball_x = runtime->player_x + 14.0f;
    runtime->ball_y = runtime->player_y - 8.0f;
    return runtime->team_count > 0;
}

void tecmo_runtime_shutdown(TecmoRuntime *runtime)
{
    roster_table_free(&runtime->roster);
}

static void update_main_menu(TecmoRuntime *runtime, const TecmoInput *input)
{
    const size_t menu_count = 3;

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
            runtime->mode = TECMO_MODE_PLAY_SETUP;
        } else if (runtime->selected_menu_item == 1) {
            runtime->mode = TECMO_MODE_ROSTERS;
        } else {
            runtime->quit_requested = true;
        }
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
}

void tecmo_runtime_update(TecmoRuntime *runtime, const TecmoInput *input)
{
    ++runtime->frame_counter;

    if (pressed(input->debug_toggle, runtime->previous_input.debug_toggle)) {
        runtime->debug_overlay = !runtime->debug_overlay;
    }

    if (runtime->mode == TECMO_MODE_MAIN_MENU) {
        update_main_menu(runtime, input);
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
    draw_text(fb, 74, 102, "LOCAL HOBBY PORT PROTOTYPE", rgb(144, 176, 192), 1);

    draw_button(fb, 160, 150, 320, 58, "PLAY GAME", runtime->selected_menu_item == 0);
    draw_button(fb, 160, 222, 320, 58, "ROSTERS", runtime->selected_menu_item == 1);
    draw_button(fb, 160, 294, 320, 58, "QUIT", runtime->selected_menu_item == 2);

    draw_text(fb, 150, 396, "UP DOWN SELECT   ENTER CONFIRM   ESC QUIT", rgb(226, 228, 208), 1);
    draw_text(fb, 82, 426, "NO ROM ASM OR EXTRACTED ASSETS ARE LOADED FROM THIS REPO", rgb(124, 148, 160), 1);
}

static void render_roster_browser(const TecmoRuntime *runtime, TecmoFramebuffer *fb, bool play_setup)
{
    char line[256];
    const char *team = runtime->team_count > 0 ? runtime->teams[runtime->selected_team] : "NO DATA";
    size_t player_row = 0;

    clear(fb, rgb(16, 20, 24));
    rect(fb, 0, 0, fb->width, 48, rgb(120, 16, 24));
    draw_text(fb, 24, 16, play_setup ? "PLAY GAME SETUP" : "ROSTERS", rgb(248, 248, 232), 2);

    (void)snprintf(line, sizeof(line), "TEAM %u OF %u: %s",
                   (unsigned)(runtime->selected_team + 1U),
                   (unsigned)runtime->team_count,
                   team);
    draw_text(fb, 32, 72, line, rgb(238, 238, 214), 2);
    draw_text(fb, 32, 102,
              play_setup ? "ARROWS SELECT   ENTER START GAME   ESC MENU" : "ARROWS BROWSE ROSTERS   ESC MENU",
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

    (void)snprintf(line, sizeof(line), "FPS %.1f MENU %u TEAM %u PLAYER %u",
                   (double)fps,
                   (unsigned)runtime->selected_menu_item,
                   (unsigned)(runtime->selected_team + 1U),
                   (unsigned)(runtime->selected_player + 1U));
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

void tecmo_runtime_render(const TecmoRuntime *runtime, TecmoFramebuffer *framebuffer)
{
    if (runtime->mode == TECMO_MODE_MAIN_MENU) {
        render_main_menu(runtime, framebuffer);
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
