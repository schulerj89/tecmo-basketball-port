#ifndef TECMO_INTRO_ARENA_SCENE_H
#define TECMO_INTRO_ARENA_SCENE_H

#include <stdbool.h>
#include <stddef.h>

#define TECMO_ARENA_INTRO_MAX_SPRITE_GROUPS 8U
#define TECMO_ARENA_SPRITE_GROUP_MAX_PIECES 16U
#define TECMO_ARENA_GOAL_PART_COUNT 5U

typedef enum TecmoArenaIntroPhase {
    TECMO_ARENA_INTRO_PHASE_ENTER,
    TECMO_ARENA_INTRO_PHASE_PAN_TO_GOAL,
    TECMO_ARENA_INTRO_PHASE_HOLD_GOAL,
    TECMO_ARENA_INTRO_PHASE_HANDOFF
} TecmoArenaIntroPhase;

typedef enum TecmoArenaGoalPart {
    TECMO_ARENA_GOAL_PART_BACKBOARD,
    TECMO_ARENA_GOAL_PART_RIM,
    TECMO_ARENA_GOAL_PART_NET,
    TECMO_ARENA_GOAL_PART_SUPPORT,
    TECMO_ARENA_GOAL_PART_POST
} TecmoArenaGoalPart;

typedef struct TecmoArenaPoint {
    int x;
    int y;
} TecmoArenaPoint;

typedef struct TecmoArenaRect {
    int x;
    int y;
    int width;
    int height;
} TecmoArenaRect;

typedef struct TecmoArenaCamera {
    TecmoArenaPoint position;
    int viewport_width;
    int viewport_height;
} TecmoArenaCamera;

typedef struct TecmoArenaSpritePiece {
    TecmoArenaGoalPart part;
    TecmoArenaPoint offset_from_anchor;
    int width;
    int height;
    unsigned frame_index;
} TecmoArenaSpritePiece;

typedef struct TecmoArenaSpriteGroup {
    TecmoArenaPoint anchor;
    TecmoArenaSpritePiece pieces[TECMO_ARENA_SPRITE_GROUP_MAX_PIECES];
    size_t piece_count;
    bool visible;
} TecmoArenaSpriteGroup;

typedef struct TecmoArenaGoal {
    TecmoArenaSpriteGroup sprite_group;
} TecmoArenaGoal;

typedef struct TecmoArenaIntro {
    unsigned frame;
    TecmoArenaIntroPhase phase;
    TecmoArenaCamera camera;
    TecmoArenaGoal goal;
    TecmoArenaSpriteGroup sprite_groups[TECMO_ARENA_INTRO_MAX_SPRITE_GROUPS];
    size_t sprite_group_count;
} TecmoArenaIntro;

void tecmo_arena_intro_init(TecmoArenaIntro *intro);
void tecmo_arena_intro_update(TecmoArenaIntro *intro);

void tecmo_arena_camera_init(TecmoArenaCamera *camera,
                             int world_x,
                             int world_y,
                             int viewport_width,
                             int viewport_height);

void tecmo_arena_goal_init(TecmoArenaGoal *goal, int anchor_x, int anchor_y);
TecmoArenaPoint tecmo_arena_goal_anchor_screen_point(const TecmoArenaGoal *goal,
                                                     const TecmoArenaCamera *camera);
size_t tecmo_arena_goal_project_parts(const TecmoArenaGoal *goal,
                                      const TecmoArenaCamera *camera,
                                      TecmoArenaRect *rects,
                                      size_t rect_capacity);
bool tecmo_arena_goal_parts_attached(const TecmoArenaGoal *goal,
                                     const TecmoArenaCamera *camera,
                                     char *message,
                                     size_t message_size);

const char *tecmo_arena_intro_phase_name(TecmoArenaIntroPhase phase);
bool tecmo_arena_intro_scene_self_test(char *message, size_t message_size);

#endif
