#include "tecmo_intro_arena_scene.h"

#include <stdio.h>
#include <string.h>

#define TECMO_ARENA_INTRO_PAN_START_X 0
#define TECMO_ARENA_INTRO_PAN_END_X 40
#define TECMO_ARENA_INTRO_PAN_START_Y 0
#define TECMO_ARENA_INTRO_PAN_END_Y 72
#define TECMO_ARENA_INTRO_PAN_FRAMES 96U
#define TECMO_ARENA_INTRO_HOLD_START_FRAME 144U
#define TECMO_ARENA_INTRO_HANDOFF_FRAME 192U
#define TECMO_ARENA_INTRO_VIEW_WIDTH 256
#define TECMO_ARENA_INTRO_VIEW_HEIGHT 240
#define TECMO_ARENA_INTRO_GOAL_ANCHOR_X 184
#define TECMO_ARENA_INTRO_GOAL_ANCHOR_Y 128

static unsigned arena_min_unsigned(unsigned a, unsigned b)
{
    return a < b ? a : b;
}

static uint32_t arena_rgb(unsigned r, unsigned g, unsigned b)
{
    return 0xFF000000U |
           ((uint32_t)(r & 0xFFU) << 16U) |
           ((uint32_t)(g & 0xFFU) << 8U) |
           (uint32_t)(b & 0xFFU);
}

static void arena_scene_message(char *dest, size_t dest_size, const char *text)
{
    if (dest == NULL || dest_size == 0U) {
        return;
    }
    if (text == NULL) {
        text = "";
    }
    (void)snprintf(dest, dest_size, "%s", text);
}

static void arena_fill_rect(TecmoFramebuffer *fb, int x, int y, int width, int height, uint32_t color)
{
    int x0;
    int y0;
    int x1;
    int y1;

    if (fb == NULL || fb->pixels == NULL || width <= 0 || height <= 0) {
        return;
    }

    x0 = x < 0 ? 0 : x;
    y0 = y < 0 ? 0 : y;
    x1 = x + width > fb->width ? fb->width : x + width;
    y1 = y + height > fb->height ? fb->height : y + height;
    if (x1 <= x0 || y1 <= y0) {
        return;
    }

    for (int row = y0; row < y1; ++row) {
        uint32_t *pixels = fb->pixels + (size_t)row * (size_t)fb->pitch_pixels;
        for (int col = x0; col < x1; ++col) {
            pixels[col] = color;
        }
    }
}

static void arena_outline_rect(TecmoFramebuffer *fb,
                               int x,
                               int y,
                               int width,
                               int height,
                               uint32_t color)
{
    arena_fill_rect(fb, x, y, width, 1, color);
    arena_fill_rect(fb, x, y + height - 1, width, 1, color);
    arena_fill_rect(fb, x, y, 1, height, color);
    arena_fill_rect(fb, x + width - 1, y, 1, height, color);
}

static void arena_draw_world_rect(TecmoFramebuffer *fb,
                                  const TecmoArenaCamera *camera,
                                  int origin_x,
                                  int origin_y,
                                  int scale,
                                  const TecmoArenaRect *world_rect,
                                  uint32_t color)
{
    if (camera == NULL || world_rect == NULL) {
        return;
    }

    arena_fill_rect(fb,
                    origin_x + (world_rect->x - camera->position.x) * scale,
                    origin_y + (world_rect->y - camera->position.y) * scale,
                    world_rect->width * scale,
                    world_rect->height * scale,
                    color);
}

static uint32_t arena_goal_part_color(TecmoArenaGoalPart part)
{
    switch (part) {
    case TECMO_ARENA_GOAL_PART_BACKBOARD:
        return arena_rgb(238U, 240U, 224U);
    case TECMO_ARENA_GOAL_PART_RIM:
        return arena_rgb(236U, 82U, 48U);
    case TECMO_ARENA_GOAL_PART_NET:
        return arena_rgb(232U, 232U, 220U);
    case TECMO_ARENA_GOAL_PART_SUPPORT:
        return arena_rgb(92U, 102U, 106U);
    case TECMO_ARENA_GOAL_PART_POST:
        return arena_rgb(202U, 204U, 192U);
    default:
        return arena_rgb(180U, 180U, 180U);
    }
}

static int arena_lerp_int(int start, int end, unsigned step, unsigned step_count)
{
    int delta;

    if (step_count == 0U || step >= step_count) {
        return end;
    }

    delta = end - start;
    return start + (int)(((long long)delta * (long long)step) / (long long)step_count);
}

static bool arena_sprite_group_add_piece(TecmoArenaSpriteGroup *group,
                                         TecmoArenaGoalPart part,
                                         int offset_x,
                                         int offset_y,
                                         int width,
                                         int height,
                                         unsigned frame_index)
{
    TecmoArenaSpritePiece *piece;

    if (group == NULL || group->piece_count >= TECMO_ARENA_SPRITE_GROUP_MAX_PIECES) {
        return false;
    }

    piece = &group->pieces[group->piece_count++];
    piece->part = part;
    piece->offset_from_anchor.x = offset_x;
    piece->offset_from_anchor.y = offset_y;
    piece->width = width;
    piece->height = height;
    piece->frame_index = frame_index;
    return true;
}

static bool arena_sprite_piece_matches(const TecmoArenaSpritePiece *actual,
                                       const TecmoArenaSpritePiece *expected)
{
    if (actual == NULL || expected == NULL) {
        return false;
    }

    return actual->part == expected->part &&
           actual->offset_from_anchor.x == expected->offset_from_anchor.x &&
           actual->offset_from_anchor.y == expected->offset_from_anchor.y &&
           actual->width == expected->width &&
           actual->height == expected->height &&
           actual->frame_index == expected->frame_index;
}

static bool arena_sprite_group_matches(const TecmoArenaSpriteGroup *actual,
                                       const TecmoArenaSpriteGroup *expected)
{
    if (actual == NULL || expected == NULL) {
        return false;
    }

    if (actual->anchor.x != expected->anchor.x ||
        actual->anchor.y != expected->anchor.y ||
        actual->piece_count != expected->piece_count ||
        actual->visible != expected->visible) {
        return false;
    }

    for (size_t i = 0; i < expected->piece_count; ++i) {
        if (!arena_sprite_piece_matches(&actual->pieces[i], &expected->pieces[i])) {
            return false;
        }
    }

    return true;
}

static bool arena_intro_render_list_matches_goal(const TecmoArenaIntro *intro,
                                                 char *message,
                                                 size_t message_size)
{
    if (intro == NULL) {
        arena_scene_message(message, message_size, "ARENA INTRO INPUT MISSING");
        return false;
    }

    if (intro->sprite_group_count != 1U) {
        arena_scene_message(message, message_size, "ARENA INTRO GROUP COUNT FAILED");
        return false;
    }

    if (!arena_sprite_group_matches(&intro->sprite_groups[0], &intro->goal.sprite_group)) {
        arena_scene_message(message, message_size, "ARENA INTRO RENDER GROUP DESYNC");
        return false;
    }

    return true;
}

static TecmoArenaIntroPhase arena_intro_expected_phase(unsigned frame)
{
    if (frame == 0U) {
        return TECMO_ARENA_INTRO_PHASE_ENTER;
    }
    if (frame < TECMO_ARENA_INTRO_PAN_FRAMES) {
        return TECMO_ARENA_INTRO_PHASE_PAN_TO_GOAL;
    }
    if (frame < TECMO_ARENA_INTRO_HANDOFF_FRAME) {
        return TECMO_ARENA_INTRO_PHASE_HOLD_GOAL;
    }
    return TECMO_ARENA_INTRO_PHASE_HANDOFF;
}

static TecmoArenaPoint arena_intro_expected_camera_position(unsigned frame)
{
    TecmoArenaPoint point;
    unsigned pan_step = frame < TECMO_ARENA_INTRO_PAN_FRAMES
                            ? frame
                            : TECMO_ARENA_INTRO_PAN_FRAMES;

    point.x = arena_lerp_int(TECMO_ARENA_INTRO_PAN_START_X,
                             TECMO_ARENA_INTRO_PAN_END_X,
                             pan_step,
                             TECMO_ARENA_INTRO_PAN_FRAMES);
    point.y = arena_lerp_int(TECMO_ARENA_INTRO_PAN_START_Y,
                             TECMO_ARENA_INTRO_PAN_END_Y,
                             pan_step,
                             TECMO_ARENA_INTRO_PAN_FRAMES);
    return point;
}

static bool arena_intro_state_matches_frame(const TecmoArenaIntro *intro,
                                            unsigned expected_frame,
                                            char *message,
                                            size_t message_size)
{
    TecmoArenaPoint expected_camera;
    TecmoArenaIntroPhase expected_phase;

    if (intro == NULL) {
        arena_scene_message(message, message_size, "ARENA INTRO INPUT MISSING");
        return false;
    }

    expected_camera = arena_intro_expected_camera_position(expected_frame);
    expected_phase = arena_intro_expected_phase(expected_frame);

    if (intro->frame != expected_frame) {
        arena_scene_message(message, message_size, "ARENA INTRO FRAME CONTRACT FAILED");
        return false;
    }
    if (intro->phase != expected_phase) {
        arena_scene_message(message, message_size, "ARENA INTRO PHASE BOUNDARY FAILED");
        return false;
    }
    if (intro->camera.position.x != expected_camera.x ||
        intro->camera.position.y != expected_camera.y ||
        intro->camera.viewport_width != TECMO_ARENA_INTRO_VIEW_WIDTH ||
        intro->camera.viewport_height != TECMO_ARENA_INTRO_VIEW_HEIGHT) {
        arena_scene_message(message, message_size, "ARENA INTRO CAMERA BOUNDARY FAILED");
        return false;
    }

    return true;
}

void tecmo_arena_camera_init(TecmoArenaCamera *camera,
                             int world_x,
                             int world_y,
                             int viewport_width,
                             int viewport_height)
{
    if (camera == NULL) {
        return;
    }

    camera->position.x = world_x;
    camera->position.y = world_y;
    camera->viewport_width = viewport_width;
    camera->viewport_height = viewport_height;
}

void tecmo_arena_goal_init(TecmoArenaGoal *goal, int anchor_x, int anchor_y)
{
    TecmoArenaSpriteGroup *group;

    if (goal == NULL) {
        return;
    }

    memset(goal, 0, sizeof(*goal));
    group = &goal->sprite_group;
    group->anchor.x = anchor_x;
    group->anchor.y = anchor_y;
    group->visible = true;

    (void)arena_sprite_group_add_piece(group,
                                       TECMO_ARENA_GOAL_PART_BACKBOARD,
                                       -32,
                                       -40,
                                       64,
                                       36,
                                       0U);
    (void)arena_sprite_group_add_piece(group,
                                       TECMO_ARENA_GOAL_PART_RIM,
                                       -13,
                                       -8,
                                       26,
                                       8,
                                       0U);
    (void)arena_sprite_group_add_piece(group,
                                       TECMO_ARENA_GOAL_PART_NET,
                                       -10,
                                       0,
                                       20,
                                       24,
                                       0U);
    (void)arena_sprite_group_add_piece(group,
                                       TECMO_ARENA_GOAL_PART_SUPPORT,
                                       22,
                                       -28,
                                       34,
                                       10,
                                       0U);
    (void)arena_sprite_group_add_piece(group,
                                       TECMO_ARENA_GOAL_PART_POST,
                                       48,
                                       -24,
                                       10,
                                       96,
                                       0U);
}

void tecmo_arena_intro_init(TecmoArenaIntro *intro)
{
    if (intro == NULL) {
        return;
    }

    memset(intro, 0, sizeof(*intro));
    intro->phase = TECMO_ARENA_INTRO_PHASE_ENTER;
    tecmo_arena_camera_init(&intro->camera,
                            TECMO_ARENA_INTRO_PAN_START_X,
                            TECMO_ARENA_INTRO_PAN_START_Y,
                            TECMO_ARENA_INTRO_VIEW_WIDTH,
                            TECMO_ARENA_INTRO_VIEW_HEIGHT);
    tecmo_arena_goal_init(&intro->goal,
                          TECMO_ARENA_INTRO_GOAL_ANCHOR_X,
                          TECMO_ARENA_INTRO_GOAL_ANCHOR_Y);
    intro->sprite_groups[0] = intro->goal.sprite_group;
    intro->sprite_group_count = 1U;
}

void tecmo_arena_intro_update(TecmoArenaIntro *intro)
{
    unsigned pan_step;

    if (intro == NULL) {
        return;
    }

    ++intro->frame;
    if (intro->frame < TECMO_ARENA_INTRO_PAN_FRAMES) {
        intro->phase = TECMO_ARENA_INTRO_PHASE_PAN_TO_GOAL;
        pan_step = intro->frame;
    } else if (intro->frame < TECMO_ARENA_INTRO_HOLD_START_FRAME) {
        intro->phase = TECMO_ARENA_INTRO_PHASE_HOLD_GOAL;
        pan_step = TECMO_ARENA_INTRO_PAN_FRAMES;
    } else if (intro->frame < TECMO_ARENA_INTRO_HANDOFF_FRAME) {
        intro->phase = TECMO_ARENA_INTRO_PHASE_HOLD_GOAL;
        pan_step = TECMO_ARENA_INTRO_PAN_FRAMES;
    } else {
        intro->phase = TECMO_ARENA_INTRO_PHASE_HANDOFF;
        pan_step = TECMO_ARENA_INTRO_PAN_FRAMES;
    }

    intro->camera.position.x = arena_lerp_int(TECMO_ARENA_INTRO_PAN_START_X,
                                              TECMO_ARENA_INTRO_PAN_END_X,
                                              pan_step,
                                              TECMO_ARENA_INTRO_PAN_FRAMES);
    intro->camera.position.y = arena_lerp_int(TECMO_ARENA_INTRO_PAN_START_Y,
                                              TECMO_ARENA_INTRO_PAN_END_Y,
                                              pan_step,
                                              TECMO_ARENA_INTRO_PAN_FRAMES);
    intro->sprite_groups[0] = intro->goal.sprite_group;
}

TecmoArenaPoint tecmo_arena_goal_anchor_screen_point(const TecmoArenaGoal *goal,
                                                     const TecmoArenaCamera *camera)
{
    TecmoArenaPoint point = {0, 0};

    if (goal == NULL || camera == NULL) {
        return point;
    }

    point.x = goal->sprite_group.anchor.x - camera->position.x;
    point.y = goal->sprite_group.anchor.y - camera->position.y;
    return point;
}

size_t tecmo_arena_goal_project_parts(const TecmoArenaGoal *goal,
                                      const TecmoArenaCamera *camera,
                                      TecmoArenaRect *rects,
                                      size_t rect_capacity)
{
    const TecmoArenaSpriteGroup *group;
    TecmoArenaPoint anchor_screen;
    size_t count;

    if (goal == NULL || camera == NULL || rects == NULL || rect_capacity == 0U) {
        return 0U;
    }

    group = &goal->sprite_group;
    if (!group->visible) {
        return 0U;
    }

    anchor_screen = tecmo_arena_goal_anchor_screen_point(goal, camera);
    count = group->piece_count < rect_capacity ? group->piece_count : rect_capacity;
    for (size_t i = 0; i < count; ++i) {
        const TecmoArenaSpritePiece *piece = &group->pieces[i];
        rects[i].x = anchor_screen.x + piece->offset_from_anchor.x;
        rects[i].y = anchor_screen.y + piece->offset_from_anchor.y;
        rects[i].width = piece->width;
        rects[i].height = piece->height;
    }
    return count;
}

bool tecmo_arena_goal_parts_attached(const TecmoArenaGoal *goal,
                                     const TecmoArenaCamera *camera,
                                     char *message,
                                     size_t message_size)
{
    TecmoArenaRect rects[TECMO_ARENA_GOAL_PART_COUNT];
    TecmoArenaPoint anchor_screen;
    const TecmoArenaSpriteGroup *group;
    size_t projected_count;

    if (goal == NULL || camera == NULL) {
        arena_scene_message(message, message_size, "GOAL ATTACHMENT INPUT MISSING");
        return false;
    }

    group = &goal->sprite_group;
    if (group->piece_count != TECMO_ARENA_GOAL_PART_COUNT) {
        arena_scene_message(message, message_size, "GOAL PART COUNT CONTRACT FAILED");
        return false;
    }

    projected_count = tecmo_arena_goal_project_parts(goal,
                                                     camera,
                                                     rects,
                                                     sizeof(rects) / sizeof(rects[0]));
    if (projected_count != group->piece_count) {
        arena_scene_message(message, message_size, "GOAL PROJECTION COUNT CONTRACT FAILED");
        return false;
    }

    anchor_screen = tecmo_arena_goal_anchor_screen_point(goal, camera);
    for (size_t i = 0; i < projected_count; ++i) {
        const TecmoArenaSpritePiece *piece = &group->pieces[i];
        if (rects[i].x - anchor_screen.x != piece->offset_from_anchor.x ||
            rects[i].y - anchor_screen.y != piece->offset_from_anchor.y ||
            rects[i].width != piece->width ||
            rects[i].height != piece->height) {
            arena_scene_message(message, message_size, "GOAL PART ANCHOR CONTRACT FAILED");
            return false;
        }
    }

    arena_scene_message(message, message_size, "GOAL PARTS ATTACHED");
    return true;
}

void tecmo_arena_intro_scene_draw_preview(TecmoFramebuffer *fb, const TecmoArenaIntro *intro)
{
    const int scale = 2;
    const int viewport_width = TECMO_ARENA_INTRO_VIEW_WIDTH * scale;
    const int viewport_height = TECMO_ARENA_INTRO_VIEW_HEIGHT * scale;
    int origin_x;
    int origin_y;
    unsigned phase_index;
    TecmoArenaRect rects[TECMO_ARENA_GOAL_PART_COUNT];
    size_t rect_count;

    if (fb == NULL || fb->pixels == NULL || intro == NULL) {
        return;
    }

    origin_x = (fb->width - viewport_width) / 2;
    origin_y = (fb->height - viewport_height) / 2;
    phase_index = intro->phase == TECMO_ARENA_INTRO_PHASE_ENTER
                      ? 0U
                      : intro->phase == TECMO_ARENA_INTRO_PHASE_PAN_TO_GOAL
                            ? 1U
                            : intro->phase == TECMO_ARENA_INTRO_PHASE_HOLD_GOAL ? 2U : 3U;

    arena_fill_rect(fb, 0, 0, fb->width, fb->height, arena_rgb(8U, 10U, 12U));
    arena_fill_rect(fb, origin_x, origin_y, viewport_width, viewport_height, arena_rgb(18U, 20U, 26U));

    {
        const TecmoArenaRect crowd_top = {-16, -18, 300, 118};
        const TecmoArenaRect jumbotron = {26, 4, 94, 54};
        const TecmoArenaRect railing = {-16, 102, 300, 8};
        const TecmoArenaRect lower_crowd = {-16, 120, 300, 94};
        const TecmoArenaRect floor = {-16, 214, 300, 90};
        const TecmoArenaRect baseline = {-16, 204, 300, 4};
        const TecmoArenaRect phase_bar = {
            0,
            0,
            (int)arena_min_unsigned(intro->frame, TECMO_ARENA_INTRO_HANDOFF_FRAME) *
                TECMO_ARENA_INTRO_VIEW_WIDTH / (int)TECMO_ARENA_INTRO_HANDOFF_FRAME,
            4};

        arena_draw_world_rect(fb, &intro->camera, origin_x, origin_y, scale, &crowd_top, arena_rgb(36U, 40U, 66U));
        arena_draw_world_rect(fb, &intro->camera, origin_x, origin_y, scale, &jumbotron, arena_rgb(18U, 22U, 32U));
        arena_draw_world_rect(fb, &intro->camera, origin_x, origin_y, scale, &railing, arena_rgb(28U, 72U, 156U));
        arena_draw_world_rect(fb, &intro->camera, origin_x, origin_y, scale, &lower_crowd, arena_rgb(136U, 62U, 42U));
        arena_draw_world_rect(fb, &intro->camera, origin_x, origin_y, scale, &floor, arena_rgb(196U, 112U, 54U));
        arena_draw_world_rect(fb, &intro->camera, origin_x, origin_y, scale, &baseline, arena_rgb(236U, 38U, 80U));
        arena_fill_rect(fb,
                        origin_x,
                        origin_y,
                        phase_bar.width * scale,
                        phase_bar.height * scale,
                        phase_index == 0U
                            ? arena_rgb(132U, 142U, 154U)
                            : phase_index == 1U
                                  ? arena_rgb(80U, 180U, 226U)
                                  : phase_index == 2U ? arena_rgb(248U, 210U, 88U) : arena_rgb(96U, 214U, 124U));
    }

    rect_count = tecmo_arena_goal_project_parts(&intro->goal,
                                                &intro->camera,
                                                rects,
                                                sizeof(rects) / sizeof(rects[0]));
    for (size_t i = 0; i < rect_count; ++i) {
        const TecmoArenaSpritePiece *piece = &intro->goal.sprite_group.pieces[i];
        uint32_t color = arena_goal_part_color(piece->part);
        TecmoArenaRect screen_rect = rects[i];
        arena_fill_rect(fb,
                        origin_x + screen_rect.x * scale,
                        origin_y + screen_rect.y * scale,
                        screen_rect.width * scale,
                        screen_rect.height * scale,
                        color);
        arena_outline_rect(fb,
                           origin_x + screen_rect.x * scale,
                           origin_y + screen_rect.y * scale,
                           screen_rect.width * scale,
                           screen_rect.height * scale,
                           arena_rgb(24U, 28U, 30U));
    }

    {
        TecmoArenaPoint anchor = tecmo_arena_goal_anchor_screen_point(&intro->goal, &intro->camera);
        arena_fill_rect(fb,
                        origin_x + anchor.x * scale - 3,
                        origin_y + anchor.y * scale - 3,
                        6,
                        6,
                        arena_rgb(252U, 236U, 118U));
    }

    arena_outline_rect(fb, origin_x, origin_y, viewport_width, viewport_height, arena_rgb(82U, 94U, 104U));
}

const char *tecmo_arena_intro_phase_name(TecmoArenaIntroPhase phase)
{
    switch (phase) {
    case TECMO_ARENA_INTRO_PHASE_ENTER:
        return "ENTER";
    case TECMO_ARENA_INTRO_PHASE_PAN_TO_GOAL:
        return "PAN_TO_GOAL";
    case TECMO_ARENA_INTRO_PHASE_HOLD_GOAL:
        return "HOLD_GOAL";
    case TECMO_ARENA_INTRO_PHASE_HANDOFF:
        return "HANDOFF";
    default:
        return "UNKNOWN";
    }
}

bool tecmo_arena_intro_scene_self_test(char *message, size_t message_size)
{
    TecmoArenaIntro intro;
    TecmoArenaCamera camera;
    TecmoArenaRect previous_rects[TECMO_ARENA_GOAL_PART_COUNT];
    TecmoArenaPoint previous_anchor = {0, 0};
    bool have_previous = false;

    if (message != NULL && message_size > 0U) {
        message[0] = '\0';
    }

    tecmo_arena_intro_init(&intro);
    if (!arena_intro_render_list_matches_goal(&intro, message, message_size)) {
        return false;
    }
    if (intro.sprite_groups[0].piece_count != TECMO_ARENA_GOAL_PART_COUNT) {
        arena_scene_message(message, message_size, "ARENA INTRO GROUP SETUP FAILED");
        return false;
    }

    for (unsigned step = 0U; step < 220U; ++step) {
        TecmoArenaRect rects[TECMO_ARENA_GOAL_PART_COUNT];
        TecmoArenaPoint anchor;
        size_t rect_count;

        if (!arena_intro_state_matches_frame(&intro, step, message, message_size)) {
            return false;
        }
        if (!arena_intro_render_list_matches_goal(&intro, message, message_size)) {
            return false;
        }

        camera = intro.camera;
        if (!tecmo_arena_goal_parts_attached(&intro.goal, &camera, message, message_size)) {
            return false;
        }

        rect_count = tecmo_arena_goal_project_parts(&intro.goal,
                                                    &camera,
                                                    rects,
                                                    sizeof(rects) / sizeof(rects[0]));
        if (rect_count != TECMO_ARENA_GOAL_PART_COUNT) {
            arena_scene_message(message, message_size, "ARENA INTRO PROJECTION COUNT FAILED");
            return false;
        }

        anchor = tecmo_arena_goal_anchor_screen_point(&intro.goal, &camera);
        if (have_previous) {
            int anchor_delta_x = anchor.x - previous_anchor.x;
            int anchor_delta_y = anchor.y - previous_anchor.y;

            for (size_t i = 0; i < rect_count; ++i) {
                if (rects[i].x - previous_rects[i].x != anchor_delta_x ||
                    rects[i].y - previous_rects[i].y != anchor_delta_y) {
                    arena_scene_message(message,
                                        message_size,
                                        "GOAL PARTS MOVED SEPARATELY FROM ANCHOR");
                    return false;
                }
            }
        }

        memcpy(previous_rects, rects, sizeof(previous_rects));
        previous_anchor = anchor;
        have_previous = true;
        tecmo_arena_intro_update(&intro);

        if (!arena_intro_render_list_matches_goal(&intro, message, message_size)) {
            return false;
        }
        if (intro.frame == 1U ||
            intro.frame == TECMO_ARENA_INTRO_PAN_FRAMES - 1U ||
            intro.frame == TECMO_ARENA_INTRO_PAN_FRAMES ||
            intro.frame == TECMO_ARENA_INTRO_HOLD_START_FRAME ||
            intro.frame == TECMO_ARENA_INTRO_HANDOFF_FRAME) {
            if (!arena_intro_state_matches_frame(&intro, intro.frame, message, message_size)) {
                return false;
            }
        }
    }

    tecmo_arena_camera_init(&camera, -80, 37, 320, 180);
    if (!tecmo_arena_goal_parts_attached(&intro.goal, &camera, message, message_size)) {
        return false;
    }

    if (intro.phase != TECMO_ARENA_INTRO_PHASE_HANDOFF) {
        arena_scene_message(message, message_size, "ARENA INTRO HANDOFF PHASE FAILED");
        return false;
    }

    arena_scene_message(message, message_size, "ARENA INTRO SCENE SELF TEST PASS");
    return true;
}
