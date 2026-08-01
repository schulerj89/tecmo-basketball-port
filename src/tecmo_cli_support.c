#include "tecmo_gameplay_court.h"
#include "tecmo_gameplay_movement.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "tecmo_cli_internal.h"

bool tecmo_cli_render_mode_requires_roster_data(const char *mode_name)
{
    return mode_name != NULL &&
           (strcmp(mode_name, "rosters") == 0 ||
            strcmp(mode_name, "play-setup") == 0);
}

bool tecmo_cli_parse_render_frame_suffix(const char *mode_name,
                                      const char *prefix,
                                      unsigned *frame)
{
    const char *suffix;
    char *end;
    unsigned long value;
    size_t prefix_length;

    if (mode_name == NULL || prefix == NULL || frame == NULL) {
        return false;
    }
    prefix_length = strlen(prefix);
    if (strncmp(mode_name, prefix, prefix_length) != 0) {
        return false;
    }
    suffix = mode_name + prefix_length;
    if (*suffix < '0' || *suffix > '9') {
        return false;
    }

    errno = 0;
    value = strtoul(suffix, &end, 10);
    if (errno == ERANGE || value > UINT_MAX || *end != '\0') {
        return false;
    }
    *frame = (unsigned)value;
    return true;
}

bool tecmo_cli_parse_u32_argument(const char *text,
                               uint32_t maximum,
                               uint32_t *value_out)
{
    char *end = NULL;
    unsigned long value;
    if (text == NULL || text[0] == '\0' || value_out == NULL) return false;
    errno = 0;
    value = strtoul(text, &end, 0);
    if (errno != 0 || end == text || *end != '\0' ||
        value > maximum) {
        return false;
    }
    *value_out = (uint32_t)value;
    return true;
}

bool tecmo_cli_parse_i16_argument(const char *text, int16_t *value_out)
{
    char *end = NULL;
    long value;
    if (text == NULL || text[0] == '\0' || value_out == NULL) return false;
    errno = 0;
    value = strtol(text, &end, 0);
    if (errno != 0 || end == text || *end != '\0' ||
        value < INT16_MIN || value > INT16_MAX) {
        return false;
    }
    *value_out = (int16_t)value;
    return true;
}

bool tecmo_cli_parse_court_coordinate_argument(
    const char *text,
    TecmoGameplayCourtCoordinate *coordinate_out)
{
    char *end = NULL;
    char *depth_end = NULL;
    long x;
    long y;
    if (text == NULL || text[0] == '\0' || coordinate_out == NULL) {
        return false;
    }
    errno = 0;
    x = strtol(text, &end, 0);
    if (errno != 0 || end == text || *end != ',') return false;
    errno = 0;
    y = strtol(end + 1, &depth_end, 0);
    if (errno != 0 || depth_end == end + 1 || *depth_end != '\0' ||
        x < TECMO_GAMEPLAY_COURT_WORLD_MIN_X ||
        x > TECMO_GAMEPLAY_COURT_WORLD_MAX_X ||
        y < TECMO_GAMEPLAY_COURT_WORLD_MIN_Y ||
        y > TECMO_GAMEPLAY_COURT_WORLD_MAX_Y) {
        return false;
    }
    coordinate_out->x = (int16_t)x;
    coordinate_out->y = (int16_t)y;
    return true;
}

bool tecmo_cli_parse_movement_input(const char *text, uint8_t *input_out)
{
    static const struct MovementInputName {
        const char *name;
        uint8_t value;
    } names[] = {
        {"neutral", TECMO_GAMEPLAY_MOVEMENT_INPUT_NEUTRAL},
        {"right", TECMO_GAMEPLAY_MOVEMENT_INPUT_RIGHT},
        {"left", TECMO_GAMEPLAY_MOVEMENT_INPUT_LEFT},
        {"down", TECMO_GAMEPLAY_MOVEMENT_INPUT_DOWN},
        {"down-right", TECMO_GAMEPLAY_MOVEMENT_INPUT_DOWN_RIGHT},
        {"down-left", TECMO_GAMEPLAY_MOVEMENT_INPUT_DOWN_LEFT},
        {"up", TECMO_GAMEPLAY_MOVEMENT_INPUT_UP},
        {"up-right", TECMO_GAMEPLAY_MOVEMENT_INPUT_UP_RIGHT},
        {"up-left", TECMO_GAMEPLAY_MOVEMENT_INPUT_UP_LEFT}
    };
    uint32_t numeric;
    if (text == NULL || input_out == NULL) return false;
    for (size_t index = 0U; index < sizeof(names) / sizeof(names[0]);
         ++index) {
        if (strcmp(text, names[index].name) == 0) {
            *input_out = names[index].value;
            return true;
        }
    }
    if (!tecmo_cli_parse_u32_argument(text, 0x0FU, &numeric) ||
        !tecmo_gameplay_movement_input_valid((uint8_t)numeric)) {
        return false;
    }
    *input_out = (uint8_t)numeric;
    return true;
}

const char *tecmo_cli_movement_input_name(uint8_t input)
{
    switch (input) {
    case TECMO_GAMEPLAY_MOVEMENT_INPUT_NEUTRAL: return "neutral";
    case TECMO_GAMEPLAY_MOVEMENT_INPUT_RIGHT: return "right";
    case TECMO_GAMEPLAY_MOVEMENT_INPUT_LEFT: return "left";
    case TECMO_GAMEPLAY_MOVEMENT_INPUT_DOWN: return "down";
    case TECMO_GAMEPLAY_MOVEMENT_INPUT_DOWN_RIGHT: return "down-right";
    case TECMO_GAMEPLAY_MOVEMENT_INPUT_DOWN_LEFT: return "down-left";
    case TECMO_GAMEPLAY_MOVEMENT_INPUT_UP: return "up";
    case TECMO_GAMEPLAY_MOVEMENT_INPUT_UP_RIGHT: return "up-right";
    case TECMO_GAMEPLAY_MOVEMENT_INPUT_UP_LEFT: return "up-left";
    default: return "invalid";
    }
}
