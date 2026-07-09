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
    if (intro.sprite_group_count != 1U ||
        intro.sprite_groups[0].piece_count != TECMO_ARENA_GOAL_PART_COUNT) {
        arena_scene_message(message, message_size, "ARENA INTRO GROUP SETUP FAILED");
        return false;
    }

    for (unsigned step = 0U; step < 220U; ++step) {
        TecmoArenaRect rects[TECMO_ARENA_GOAL_PART_COUNT];
        TecmoArenaPoint anchor;
        size_t rect_count;

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
