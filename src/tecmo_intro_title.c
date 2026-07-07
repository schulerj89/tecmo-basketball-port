#include "tecmo_intro_title.h"
#include "tecmo_nametable_screen.h"

#define TECMO_INTRO_PRESENTS_TILE_ROW 14
#define TECMO_INTRO_PRESENTS_TILE_COL 15
#define TECMO_INTRO_PRESENTS_LEN 8U

static const TecmoNametableTile INTRO_CAPTURED_TITLE_TILES[] = {
    {0x210BU, 0x21U},
    {0x212BU, 0x22U}, {0x212CU, 0x23U},
    {0x214BU, 0x24U}, {0x214CU, 0x26U},
    {0x216BU, 0x25U}, {0x216CU, 0x27U}, {0x216DU, 0x28U}, {0x216EU, 0x29U},
    {0x216FU, 0x2AU}, {0x2170U, 0x2BU}, {0x2171U, 0x2CU}, {0x2172U, 0x2DU},
    {0x2173U, 0x2EU}, {0x2174U, 0x2FU}, {0x2175U, 0x30U}, {0x2176U, 0x31U},
    {0x218AU, 0x32U}, {0x218BU, 0x33U}, {0x218CU, 0x36U}, {0x218DU, 0x37U},
    {0x218EU, 0x39U}, {0x218FU, 0x3AU}, {0x2190U, 0x3DU}, {0x2191U, 0x3EU},
    {0x2192U, 0x41U}, {0x2193U, 0x42U}, {0x2194U, 0x45U}, {0x2195U, 0x46U},
    {0x2196U, 0x49U}, {0x2197U, 0x4AU}, {0x2198U, 0x4DU},
    {0x21AAU, 0x34U}, {0x21ABU, 0x35U}, {0x21ACU, 0x38U},
    {0x21AEU, 0x3BU}, {0x21AFU, 0x3CU}, {0x21B0U, 0x3FU}, {0x21B1U, 0x40U},
    {0x21B2U, 0x43U}, {0x21B3U, 0x44U}, {0x21B4U, 0x47U}, {0x21B5U, 0x48U},
    {0x21B6U, 0x4BU}, {0x21B7U, 0x4CU},
    {0x21CAU, 0x4EU}, {0x21CBU, 0x4FU}, {0x21CCU, 0x52U},
    {0x21CFU, 0x9BU}, {0x21D0U, 0x9DU}, {0x21D1U, 0x90U}, {0x21D2U, 0x9EU},
    {0x21D3U, 0x90U}, {0x21D4U, 0x99U}, {0x21D5U, 0x9FU}, {0x21D6U, 0x9EU},
    {0x21EAU, 0x50U}, {0x21EBU, 0x51U},
};

static void intro_rect(TecmoFramebuffer *fb, int x, int y, int w, int h, uint32_t color)
{
    int x0 = x;
    int y0 = y;
    int x1 = x + w;
    int y1 = y + h;

    if (fb == 0 || fb->pixels == 0 || w <= 0 || h <= 0) {
        return;
    }
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > fb->width) x1 = fb->width;
    if (y1 > fb->height) y1 = fb->height;
    if (x0 >= x1 || y0 >= y1) {
        return;
    }

    for (int row = y0; row < y1; ++row) {
        uint32_t *dst = fb->pixels + (size_t)row * (size_t)fb->pitch_pixels + (size_t)x0;
        for (int col = x0; col < x1; ++col) {
            *dst++ = color;
        }
    }
}

static void intro_outline_rect(TecmoFramebuffer *fb, int x, int y, int w, int h, uint32_t color)
{
    intro_rect(fb, x, y, w, 1, color);
    intro_rect(fb, x, y + h - 1, w, 1, color);
    intro_rect(fb, x, y, 1, h, color);
    intro_rect(fb, x + w - 1, y, 1, h, color);
}

static const uint8_t INTRO_CAPTURED_TITLE_ATTRIBUTES[64] = {
    0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
    0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
    0x00U, 0x00U, 0xCCU, 0xB3U, 0xA0U, 0xA0U, 0x00U, 0x00U,
    0x00U, 0x00U, 0xCCU, 0xBBU, 0xAAU, 0xAAU, 0x02U, 0x00U,
    0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
    0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
    0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
    0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
};

static const uint8_t INTRO_CAPTURED_TITLE_PALETTE[5][4][4] = {
    {
        { 0x0FU, 0x0FU, 0x0FU, 0x0FU },
        { 0x0FU, 0x0FU, 0x0FU, 0x0FU },
        { 0x0FU, 0x0FU, 0x0FU, 0x0FU },
        { 0x0FU, 0x0FU, 0x0FU, 0x0FU },
    },
    {
        { 0x0FU, 0x06U, 0x05U, 0x07U },
        { 0x09U, 0x07U, 0x08U, 0x03U },
        { 0x09U, 0x05U, 0x02U, 0x00U },
        { 0x09U, 0x05U, 0x06U, 0x06U },
    },
    {
        { 0x0FU, 0x16U, 0x15U, 0x17U },
        { 0x19U, 0x17U, 0x18U, 0x03U },
        { 0x19U, 0x15U, 0x12U, 0x10U },
        { 0x19U, 0x05U, 0x16U, 0x16U },
    },
    {
        { 0x0FU, 0x16U, 0x15U, 0x17U },
        { 0x19U, 0x17U, 0x28U, 0x03U },
        { 0x19U, 0x15U, 0x12U, 0x20U },
        { 0x19U, 0x05U, 0x26U, 0x26U },
    },
    {
        { 0x0FU, 0x16U, 0x15U, 0x17U },
        { 0x19U, 0x17U, 0x38U, 0x03U },
        { 0x19U, 0x15U, 0x12U, 0x30U },
        { 0x19U, 0x05U, 0x26U, 0x36U },
    },
};

uint8_t tecmo_intro_title_palette_stage_for_frame(unsigned mode_frame_counter)
{
    if (mode_frame_counter < 4U) {
        return 0U;
    }
    if (mode_frame_counter < 8U) {
        return 1U;
    }
    if (mode_frame_counter < 12U) {
        return 2U;
    }
    if (mode_frame_counter < 16U) {
        return 3U;
    }
    return 4U;
}

size_t tecmo_intro_title_tile_count(void)
{
    return sizeof(INTRO_CAPTURED_TITLE_TILES) / sizeof(INTRO_CAPTURED_TITLE_TILES[0]);
}

bool tecmo_intro_title_draw(TecmoFramebuffer *fb,
                            const uint8_t *chr_bytes,
                            uint64_t chr_byte_count,
                            uint32_t chr_bank,
                            unsigned mode_frame_counter,
                            int origin_x,
                            int origin_y,
                            int scale,
                            bool draw_debug_bounds)
{
    size_t tile_count = tecmo_intro_title_tile_count();
    uint8_t palette_stage = tecmo_intro_title_palette_stage_for_frame(mode_frame_counter);

    if (fb == 0 || chr_bytes == 0 || chr_byte_count == 0 || scale <= 0) {
        return false;
    }

    (void)tecmo_nametable_draw_tiles(fb,
                                     chr_bytes,
                                     chr_byte_count,
                                     chr_bank,
                                     INTRO_CAPTURED_TITLE_TILES,
                                     tile_count,
                                     INTRO_CAPTURED_TITLE_ATTRIBUTES,
                                     &INTRO_CAPTURED_TITLE_PALETTE[palette_stage][0][0],
                                     tecmo_nametable_map_low_tiles_to_table1,
                                     0,
                                     origin_x,
                                     origin_y,
                                     scale);

    if (draw_debug_bounds) {
        intro_outline_rect(fb, origin_x + 10 * 8 * scale - 2, origin_y + 8 * 8 * scale - 2,
                           15 * 8 * scale + 4, 8 * 8 * scale + 4, 0xFF50606EU);
        intro_outline_rect(fb,
                           origin_x + TECMO_INTRO_PRESENTS_TILE_COL * 8 * scale - 2,
                           origin_y + TECMO_INTRO_PRESENTS_TILE_ROW * 8 * scale - 2,
                           (int)TECMO_INTRO_PRESENTS_LEN * 8 * scale + 4,
                           8 * scale + 4,
                           0xFF8EAEBEU);
    }

    return true;
}
