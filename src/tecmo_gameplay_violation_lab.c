#include "tecmo_gameplay_violation_lab.h"

#include <stdio.h>
#include <string.h>

typedef struct TecmoGameplayViolationLabItemSpec {
    const char *label;
    const char *token;
    TecmoGameplayViolation violation;
    uint8_t sequence_id;
    bool fixed_foul;
    bool state_supported;
} TecmoGameplayViolationLabItemSpec;

static const TecmoGameplayViolationLabItemSpec violation_lab_items[
    TECMO_GAMEPLAY_VIOLATION_LAB_ITEM_COUNT] = {
    {"OUT OF BOUNDS", "out-of-bounds",
     TECMO_GAMEPLAY_VIOLATION_OUT_OF_BOUNDS, 1U, false, true},
    {"BACKCOURT", "backcourt", TECMO_GAMEPLAY_VIOLATION_BACKCOURT,
     1U, false, true},
    {"5 SECOND VIOLATION", "five-seconds",
     TECMO_GAMEPLAY_VIOLATION_FIVE_SECONDS, 3U, false, false},
    {"10 SECOND VIOLATION", "ten-seconds",
     TECMO_GAMEPLAY_VIOLATION_TEN_SECONDS, 3U, false, false},
    {"SHOT CLOCK VIOLATION", "shot-clock",
     TECMO_GAMEPLAY_VIOLATION_SHOT_CLOCK, 3U, false, true},
    {"TRAVELING", "traveling", TECMO_GAMEPLAY_VIOLATION_TRAVELING,
     4U, false, false},
    {"GOALTENDING", "goaltending", TECMO_GAMEPLAY_VIOLATION_GOALTENDING,
     1U, false, false},
    {"FIXED FOUL PRESENTATION", "foul", TECMO_GAMEPLAY_VIOLATION_NONE,
     0U, true, true}
};

static bool violation_lab_item_valid(TecmoGameplayViolationLabItem item)
{
    return item >= TECMO_GAMEPLAY_VIOLATION_LAB_OUT_OF_BOUNDS &&
           item < TECMO_GAMEPLAY_VIOLATION_LAB_ITEM_COUNT;
}

static TecmoGameplayViolationLabItem violation_lab_selected_item(
    const TecmoGameplayViolationLab *lab)
{
    if (lab == NULL || lab->selection >=
                           TECMO_GAMEPLAY_VIOLATION_LAB_ITEM_COUNT) {
        return TECMO_GAMEPLAY_VIOLATION_LAB_OUT_OF_BOUNDS;
    }
    return (TecmoGameplayViolationLabItem)lab->selection;
}

static bool violation_lab_scene_state_path_available(
    const TecmoGameplayScene *scene)
{
    /* Production-state preview delegates to tecmo_gameplay_scene_draw().
       That compositor gives pre-tip cards/cutaways and a dunk cutaway
       priority over TGVR.  Never install a temporary TGVR state unless the
       frozen source scene is an ordinary idle LIVE frame: source preview is
       still safe for every otherwise-active presentation. */
    return scene != NULL && scene->state.initialized &&
           scene->state.phase == TECMO_GAMEPLAY_PHASE_LIVE &&
           scene->state.banner == TECMO_GAMEPLAY_BANNER_NONE &&
           scene->state.free_throws.attempts_remaining == 0U &&
           !tecmo_gameplay_pretip_is_presentation(&scene->pretip_state) &&
           scene->shot_kind == TECMO_GAMEPLAY_SCENE_SHOT_NONE &&
           !scene->free_throw_lineup_active &&
           !tecmo_gameplay_scene_in_dunk_presentation(scene);
}

static void violation_lab_restore(TecmoGameplayViolationLab *lab,
                                  TecmoGameplayScene *scene)
{
    if (lab == NULL || scene == NULL || !lab->snapshot_valid) return;
    scene->state = lab->saved_state;
    scene->foul_presentation = lab->saved_foul_presentation;
}

static uint16_t violation_lab_max_frame(
    TecmoGameplayViolationLabItem item)
{
    uint16_t count = tecmo_gameplay_violation_lab_frame_count(item);
    return count == 0U ? 0U : (uint16_t)(count - 1U);
}

static bool violation_lab_apply_production_state(
    TecmoGameplayViolationLab *lab,
    TecmoGameplayScene *scene)
{
    TecmoGameplayViolationLabItem item;
    TecmoGameplayFoulRequest foul_request;
    TecmoGameplayTeam restart;

    if (lab == NULL || scene == NULL || !lab->snapshot_valid ||
        !lab->state_path_available ||
        lab->path != TECMO_GAMEPLAY_VIOLATION_LAB_PRODUCTION_STATE_PREVIEW) {
        return false;
    }
    item = violation_lab_selected_item(lab);
    if (!tecmo_gameplay_violation_lab_item_state_supported(item)) return false;

    violation_lab_restore(lab, scene);
    if (tecmo_gameplay_violation_lab_item_is_foul(item)) {
        memset(&foul_request, 0, sizeof(foul_request));
        foul_request.fouling_team = TECMO_GAMEPLAY_TEAM_HOME;
        foul_request.free_throw_team = TECMO_GAMEPLAY_TEAM_AWAY;
        /* This temp state is solely for the fixed presentation; no counter
           effect or free-throw settlement is represented by the preview. */
        foul_request.counter_effect = TECMO_GAMEPLAY_FOUL_COUNTER_NONE;
        foul_request.player_index = 0U;
        foul_request.free_throw_attempts = 0U;
        if (!tecmo_gameplay_request_foul(&scene->state, &foul_request)) {
            violation_lab_restore(lab, scene);
            return false;
        }
        memset(&scene->foul_presentation, 0,
               sizeof(scene->foul_presentation));
    } else {
        TecmoGameplayViolation violation;
        if (!tecmo_gameplay_violation_lab_item_violation(item, &violation)) {
            violation_lab_restore(lab, scene);
            return false;
        }
        restart = lab->saved_state.possession == TECMO_GAMEPLAY_TEAM_AWAY
                      ? TECMO_GAMEPLAY_TEAM_HOME
                      : TECMO_GAMEPLAY_TEAM_AWAY;
        if (!tecmo_gameplay_request_violation(&scene->state, violation,
                                              restart)) {
            violation_lab_restore(lab, scene);
            return false;
        }
    }
    scene->state.phase_frame = lab->phase_frame;
    if (!tecmo_gameplay_state_valid(&scene->state)) {
        violation_lab_restore(lab, scene);
        return false;
    }
    return true;
}

static bool violation_lab_refresh_preview(TecmoGameplayViolationLab *lab,
                                          TecmoGameplayScene *scene)
{
    TecmoGameplayViolationLabItem item;
    if (lab == NULL || scene == NULL || !lab->active) return false;
    item = violation_lab_selected_item(lab);
    if (lab->path == TECMO_GAMEPLAY_VIOLATION_LAB_SOURCE_PREVIEW) {
        violation_lab_restore(lab, scene);
        return true;
    }
    if (!lab->state_path_available ||
        !tecmo_gameplay_violation_lab_item_state_supported(item) ||
        !violation_lab_apply_production_state(lab, scene)) {
        lab->path = TECMO_GAMEPLAY_VIOLATION_LAB_SOURCE_PREVIEW;
        lab->state_path_rejected = true;
        violation_lab_restore(lab, scene);
    }
    return true;
}

void tecmo_gameplay_violation_lab_init(TecmoGameplayViolationLab *lab)
{
    if (lab != NULL) memset(lab, 0, sizeof(*lab));
}

bool tecmo_gameplay_violation_lab_open(TecmoGameplayViolationLab *lab,
                                       TecmoGameplayScene *scene)
{
    if (lab == NULL || scene == NULL || lab->active || !scene->available ||
        !scene->active || !scene->state.initialized ||
        !scene->violation_referee_assets.available) {
        return false;
    }
    tecmo_gameplay_violation_lab_init(lab);
    lab->active = true;
    lab->paused = true;
    lab->snapshot_valid = true;
    lab->state_path_available = violation_lab_scene_state_path_available(scene);
    lab->selection = TECMO_GAMEPLAY_VIOLATION_LAB_OUT_OF_BOUNDS;
    lab->path = TECMO_GAMEPLAY_VIOLATION_LAB_SOURCE_PREVIEW;
    lab->saved_state = scene->state;
    lab->saved_foul_presentation = scene->foul_presentation;
    return true;
}

void tecmo_gameplay_violation_lab_close(TecmoGameplayViolationLab *lab,
                                        TecmoGameplayScene *scene)
{
    if (lab == NULL) return;
    violation_lab_restore(lab, scene);
    tecmo_gameplay_violation_lab_init(lab);
}

bool tecmo_gameplay_violation_lab_set_item(
    TecmoGameplayViolationLab *lab,
    TecmoGameplayScene *scene,
    TecmoGameplayViolationLabItem item)
{
    if (lab == NULL || scene == NULL || !lab->active ||
        !violation_lab_item_valid(item)) {
        return false;
    }
    lab->selection = (uint8_t)item;
    lab->state_path_rejected = false;
    if (lab->path == TECMO_GAMEPLAY_VIOLATION_LAB_PRODUCTION_STATE_PREVIEW &&
        !tecmo_gameplay_violation_lab_item_state_supported(item)) {
        lab->path = TECMO_GAMEPLAY_VIOLATION_LAB_SOURCE_PREVIEW;
    }
    return violation_lab_refresh_preview(lab, scene);
}

bool tecmo_gameplay_violation_lab_set_path(
    TecmoGameplayViolationLab *lab,
    TecmoGameplayScene *scene,
    TecmoGameplayViolationLabPath path)
{
    TecmoGameplayViolationLabItem item;
    if (lab == NULL || scene == NULL || !lab->active ||
        path >= TECMO_GAMEPLAY_VIOLATION_LAB_PATH_COUNT) {
        return false;
    }
    item = violation_lab_selected_item(lab);
    if (path == TECMO_GAMEPLAY_VIOLATION_LAB_PRODUCTION_STATE_PREVIEW &&
        (!lab->state_path_available ||
         !tecmo_gameplay_violation_lab_item_state_supported(item))) {
        lab->path = TECMO_GAMEPLAY_VIOLATION_LAB_SOURCE_PREVIEW;
        lab->state_path_rejected = true;
        violation_lab_restore(lab, scene);
        return false;
    }
    lab->path = path;
    lab->state_path_rejected = false;
    return violation_lab_refresh_preview(lab, scene);
}

bool tecmo_gameplay_violation_lab_set_frame(
    TecmoGameplayViolationLab *lab,
    TecmoGameplayScene *scene,
    uint16_t phase_frame)
{
    uint16_t maximum;
    if (lab == NULL || scene == NULL || !lab->active) return false;
    maximum = violation_lab_max_frame(violation_lab_selected_item(lab));
    lab->phase_frame = phase_frame > maximum ? maximum : phase_frame;
    return violation_lab_refresh_preview(lab, scene);
}

bool tecmo_gameplay_violation_lab_update(TecmoGameplayViolationLab *lab,
                                         TecmoGameplayScene *scene,
                                         const TecmoControlFrame *controls)
{
    TecmoGameplayViolationLabItem item;
    uint16_t maximum;
    bool changed = false;

    if (lab == NULL || scene == NULL || controls == NULL || !lab->active) {
        return false;
    }
    item = violation_lab_selected_item(lab);
    if (controls->pressed.violation_lab_previous) {
        item = item == TECMO_GAMEPLAY_VIOLATION_LAB_OUT_OF_BOUNDS
                   ? (TecmoGameplayViolationLabItem)(
                         TECMO_GAMEPLAY_VIOLATION_LAB_ITEM_COUNT - 1U)
                   : (TecmoGameplayViolationLabItem)(item - 1);
        lab->selection = (uint8_t)item;
        lab->state_path_rejected = false;
        changed = true;
    }
    if (controls->pressed.violation_lab_next) {
        item = item + 1U >= TECMO_GAMEPLAY_VIOLATION_LAB_ITEM_COUNT
                   ? TECMO_GAMEPLAY_VIOLATION_LAB_OUT_OF_BOUNDS
                   : (TecmoGameplayViolationLabItem)(item + 1);
        lab->selection = (uint8_t)item;
        lab->state_path_rejected = false;
        changed = true;
    }
    item = violation_lab_selected_item(lab);
    if (lab->path == TECMO_GAMEPLAY_VIOLATION_LAB_PRODUCTION_STATE_PREVIEW &&
        !tecmo_gameplay_violation_lab_item_state_supported(item)) {
        lab->path = TECMO_GAMEPLAY_VIOLATION_LAB_SOURCE_PREVIEW;
        changed = true;
    }
    if (controls->pressed.violation_lab_path) {
        TecmoGameplayViolationLabPath requested =
            lab->path == TECMO_GAMEPLAY_VIOLATION_LAB_SOURCE_PREVIEW
                ? TECMO_GAMEPLAY_VIOLATION_LAB_PRODUCTION_STATE_PREVIEW
                : TECMO_GAMEPLAY_VIOLATION_LAB_SOURCE_PREVIEW;
        (void)tecmo_gameplay_violation_lab_set_path(lab, scene, requested);
        changed = true;
    }
    if (controls->pressed.violation_lab_play_pause) {
        lab->paused = !lab->paused;
        changed = true;
    }
    if (controls->pressed.violation_lab_restart) {
        lab->phase_frame = 0U;
        lab->paused = true;
        changed = true;
    }
    maximum = violation_lab_max_frame(violation_lab_selected_item(lab));
    if (controls->pressed.violation_lab_step) {
        lab->paused = true;
        if (lab->phase_frame < maximum) ++lab->phase_frame;
        changed = true;
    } else if (!lab->paused) {
        if (lab->phase_frame < maximum) {
            ++lab->phase_frame;
        } else {
            lab->paused = true;
        }
        changed = true;
    }
    if (changed) return violation_lab_refresh_preview(lab, scene);
    return true;
}

bool tecmo_gameplay_violation_lab_draw(
    const TecmoGameplayViolationLab *lab,
    const TecmoGameplayScene *scene,
    TecmoFramebuffer *framebuffer,
    int origin_x,
    int origin_y,
    int scale)
{
    TecmoGameplayViolationLabItem item;
    TecmoGameplayViolation violation;
    if (lab == NULL || scene == NULL || framebuffer == NULL || !lab->active ||
        !scene->violation_referee_assets.available) {
        return false;
    }
    if (lab->path == TECMO_GAMEPLAY_VIOLATION_LAB_PRODUCTION_STATE_PREVIEW) {
        return tecmo_gameplay_scene_draw(scene, framebuffer, origin_x, origin_y,
                                         scale, true);
    }
    item = violation_lab_selected_item(lab);
    if (tecmo_gameplay_violation_lab_item_is_foul(item)) {
        return tecmo_gameplay_violation_referee_draw_foul(
            &scene->violation_referee_assets, scene->assets.chr_storage,
            scene->assets.chr_storage_size, framebuffer, origin_x, origin_y,
            scale, lab->phase_frame);
    }
    if (!tecmo_gameplay_violation_lab_item_violation(item, &violation)) {
        return false;
    }
    return tecmo_gameplay_violation_referee_draw(
        &scene->violation_referee_assets, scene->assets.chr_storage,
        scene->assets.chr_storage_size, framebuffer, origin_x, origin_y,
        scale, violation, lab->phase_frame);
}

const char *tecmo_gameplay_violation_lab_item_label(
    TecmoGameplayViolationLabItem item)
{
    return violation_lab_item_valid(item) ? violation_lab_items[item].label
                                          : "INVALID";
}

const char *tecmo_gameplay_violation_lab_item_token(
    TecmoGameplayViolationLabItem item)
{
    return violation_lab_item_valid(item) ? violation_lab_items[item].token
                                          : "invalid";
}

const char *tecmo_gameplay_violation_lab_path_label(
    TecmoGameplayViolationLabPath path)
{
    switch (path) {
    case TECMO_GAMEPLAY_VIOLATION_LAB_SOURCE_PREVIEW:
        return "SOURCE PREVIEW";
    case TECMO_GAMEPLAY_VIOLATION_LAB_PRODUCTION_STATE_PREVIEW:
        return "PRODUCTION STATE PREVIEW";
    case TECMO_GAMEPLAY_VIOLATION_LAB_PATH_COUNT:
    default:
        return "INVALID PATH";
    }
}

bool tecmo_gameplay_violation_lab_item_violation(
    TecmoGameplayViolationLabItem item,
    TecmoGameplayViolation *violation_out)
{
    if (!violation_lab_item_valid(item) || violation_out == NULL ||
        violation_lab_items[item].fixed_foul) {
        return false;
    }
    *violation_out = violation_lab_items[item].violation;
    return true;
}

bool tecmo_gameplay_violation_lab_item_is_foul(
    TecmoGameplayViolationLabItem item)
{
    return violation_lab_item_valid(item) && violation_lab_items[item].fixed_foul;
}

bool tecmo_gameplay_violation_lab_item_state_supported(
    TecmoGameplayViolationLabItem item)
{
    return violation_lab_item_valid(item) &&
           violation_lab_items[item].state_supported;
}

uint8_t tecmo_gameplay_violation_lab_item_sequence_id(
    TecmoGameplayViolationLabItem item)
{
    return violation_lab_item_valid(item) ? violation_lab_items[item].sequence_id
                                          : 0U;
}

uint16_t tecmo_gameplay_violation_lab_frame_count(
    TecmoGameplayViolationLabItem item)
{
    if (!violation_lab_item_valid(item)) return 0U;
    return tecmo_gameplay_violation_lab_item_is_foul(item)
               ? TECMO_GAMEPLAY_FOUL_PRESENTATION_FRAMES
               : TECMO_GAMEPLAY_VIOLATION_PRESENTATION_FRAMES;
}

bool tecmo_gameplay_violation_lab_group_id(
    const TecmoGameplayViolationLab *lab,
    const TecmoGameplayScene *scene,
    uint8_t *group_id_out)
{
    TecmoGameplayViolationLabItem item;
    TecmoGameplayViolation violation;
    if (lab == NULL || scene == NULL || group_id_out == NULL || !lab->active) {
        return false;
    }
    item = violation_lab_selected_item(lab);
    if (tecmo_gameplay_violation_lab_item_is_foul(item)) {
        return tecmo_gameplay_violation_referee_foul_group_for_frame(
            &scene->violation_referee_assets, lab->phase_frame, group_id_out);
    }
    if (!tecmo_gameplay_violation_lab_item_violation(item, &violation)) {
        return false;
    }
    return tecmo_gameplay_violation_referee_group_for_frame(
        &scene->violation_referee_assets, violation, lab->phase_frame,
        group_id_out);
}

static void violation_lab_test_message(char *message,
                                       size_t message_size,
                                       const char *text)
{
    if (message == NULL || message_size == 0U) return;
    (void)snprintf(message, message_size, "%s", text != NULL ? text : "");
}

static void violation_lab_test_press(TecmoControlFrame *controls,
                                     TecmoControlButton button)
{
    memset(controls, 0, sizeof(*controls));
    tecmo_input_set_button(&controls->pressed, button, true);
}

static bool violation_lab_test_rejects_superseding_scene_presentation(
    TecmoGameplayScene *scene,
    const TecmoGameplayState *initial_state,
    const TecmoGameplaySceneFoulPresentation *initial_foul,
    char *message,
    size_t message_size)
{
    static const struct {
        const char *label;
        TecmoGameplaySceneShotKind shot_kind;
        uint16_t shot_frame;
        bool free_throw_lineup_active;
    } cases[] = {
        {"DUNK CUTAWAY", TECMO_GAMEPLAY_SCENE_SHOT_DUNK,
         TECMO_GAMEPLAY_DUNK_BLACK_START_FRAME, false},
        {"JUMP SHOT", TECMO_GAMEPLAY_SCENE_SHOT_JUMP, 1U, false},
        {"FREE THROW LINEUP", TECMO_GAMEPLAY_SCENE_SHOT_NONE, 0U, true}
    };
    size_t index;

    if (scene == NULL || initial_state == NULL || initial_foul == NULL) {
        violation_lab_test_message(message, message_size,
                                   "LAB SUPERSEDING SETUP FAILED");
        return false;
    }
    for (index = 0U; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        TecmoGameplayViolationLab lab;
        TecmoControlFrame controls;
        char failure[128];

        scene->state = *initial_state;
        scene->foul_presentation = *initial_foul;
        scene->pretip_state.phase = TECMO_GAMEPLAY_PRETIP_LIVE;
        scene->shot_kind = cases[index].shot_kind;
        scene->shot_frame = cases[index].shot_frame;
        scene->free_throw_lineup_active =
            cases[index].free_throw_lineup_active;
        tecmo_gameplay_violation_lab_init(&lab);
        if (!tecmo_gameplay_violation_lab_open(&lab, scene) ||
            lab.state_path_available ||
            lab.path != TECMO_GAMEPLAY_VIOLATION_LAB_SOURCE_PREVIEW) {
            (void)snprintf(failure, sizeof(failure),
                           "LAB %s OPEN DID NOT FORCE SOURCE", cases[index].label);
            violation_lab_test_message(message, message_size, failure);
            return false;
        }
        violation_lab_test_press(&controls, TECMO_CONTROL_VIOLATION_LAB_PATH);
        if (!tecmo_gameplay_violation_lab_update(&lab, scene, &controls) ||
            !lab.state_path_rejected ||
            lab.path != TECMO_GAMEPLAY_VIOLATION_LAB_SOURCE_PREVIEW ||
            memcmp(&scene->state, initial_state, sizeof(*initial_state)) != 0 ||
            memcmp(&scene->foul_presentation, initial_foul,
                   sizeof(*initial_foul)) != 0) {
            (void)snprintf(failure, sizeof(failure),
                           "LAB %s F7 STATE PREVIEW NOT REJECTED",
                           cases[index].label);
            violation_lab_test_message(message, message_size, failure);
            return false;
        }
        tecmo_gameplay_violation_lab_close(&lab, scene);
        if (lab.active ||
            memcmp(&scene->state, initial_state, sizeof(*initial_state)) != 0 ||
            memcmp(&scene->foul_presentation, initial_foul,
                   sizeof(*initial_foul)) != 0 ||
            scene->shot_kind != cases[index].shot_kind ||
            scene->shot_frame != cases[index].shot_frame ||
            scene->free_throw_lineup_active !=
                cases[index].free_throw_lineup_active) {
            (void)snprintf(failure, sizeof(failure),
                           "LAB %s CLOSE RESTORE FAILED", cases[index].label);
            violation_lab_test_message(message, message_size, failure);
            return false;
        }
    }
    return true;
}

bool tecmo_gameplay_violation_lab_self_test(char *message,
                                            size_t message_size)
{
    TecmoGameplayConfig config;
    TecmoGameplayState initial_state;
    TecmoGameplayScene scene;
    TecmoGameplayViolationLab lab;
    TecmoControlFrame controls;
    TecmoGameplaySceneFoulPresentation initial_foul;
    TecmoGameplayViolation expected_violations[
        TECMO_GAMEPLAY_VIOLATION_REFEREE_MESSAGE_COUNT] = {
        TECMO_GAMEPLAY_VIOLATION_OUT_OF_BOUNDS,
        TECMO_GAMEPLAY_VIOLATION_BACKCOURT,
        TECMO_GAMEPLAY_VIOLATION_FIVE_SECONDS,
        TECMO_GAMEPLAY_VIOLATION_TEN_SECONDS,
        TECMO_GAMEPLAY_VIOLATION_SHOT_CLOCK,
        TECMO_GAMEPLAY_VIOLATION_TRAVELING,
        TECMO_GAMEPLAY_VIOLATION_GOALTENDING
    };
    uint8_t expected_sequences[
        TECMO_GAMEPLAY_VIOLATION_REFEREE_MESSAGE_COUNT] =
        {1U, 1U, 3U, 3U, 3U, 4U, 1U};
    size_t item;

    for (item = 0U;
         item < TECMO_GAMEPLAY_VIOLATION_REFEREE_MESSAGE_COUNT; ++item) {
        TecmoGameplayViolation violation = TECMO_GAMEPLAY_VIOLATION_NONE;
        if (!tecmo_gameplay_violation_lab_item_violation(
                (TecmoGameplayViolationLabItem)item, &violation) ||
            violation != expected_violations[item] ||
            tecmo_gameplay_violation_lab_item_sequence_id(
                (TecmoGameplayViolationLabItem)item) !=
                expected_sequences[item] ||
            tecmo_gameplay_violation_lab_item_state_supported(
                (TecmoGameplayViolationLabItem)item) !=
                (item == TECMO_GAMEPLAY_VIOLATION_LAB_OUT_OF_BOUNDS ||
                 item == TECMO_GAMEPLAY_VIOLATION_LAB_BACKCOURT ||
                 item == TECMO_GAMEPLAY_VIOLATION_LAB_SHOT_CLOCK)) {
            violation_lab_test_message(message, message_size,
                                       "VIOLATION ITEM MAP FAILED");
            return false;
        }
    }
    if (!tecmo_gameplay_violation_lab_item_is_foul(
            TECMO_GAMEPLAY_VIOLATION_LAB_FIXED_FOUL) ||
        tecmo_gameplay_violation_lab_item_sequence_id(
            TECMO_GAMEPLAY_VIOLATION_LAB_FIXED_FOUL) != 0U ||
        !tecmo_gameplay_violation_lab_item_state_supported(
            TECMO_GAMEPLAY_VIOLATION_LAB_FIXED_FOUL) ||
        tecmo_gameplay_violation_lab_frame_count(
            TECMO_GAMEPLAY_VIOLATION_LAB_FIXED_FOUL) !=
            TECMO_GAMEPLAY_FOUL_PRESENTATION_FRAMES) {
        violation_lab_test_message(message, message_size,
                                   "FIXED FOUL ITEM MAP FAILED");
        return false;
    }

    if (!tecmo_gameplay_config_init(&config, 3U) ||
        !tecmo_gameplay_state_init(&initial_state, &config,
                                   TECMO_GAMEPLAY_TEAM_AWAY)) {
        violation_lab_test_message(message, message_size,
                                   "LAB STATE SETUP FAILED");
        return false;
    }
    tecmo_gameplay_scene_init(&scene);
    scene.available = true;
    scene.active = true;
    scene.state = initial_state;
    scene.violation_referee_assets.available = true;
    scene.pretip_state.phase = TECMO_GAMEPLAY_PRETIP_LIVE;
    initial_foul = scene.foul_presentation;
    tecmo_gameplay_violation_lab_init(&lab);
    if (!tecmo_gameplay_violation_lab_open(&lab, &scene) || !lab.active ||
        !lab.paused || lab.path !=
            TECMO_GAMEPLAY_VIOLATION_LAB_SOURCE_PREVIEW ||
        !lab.state_path_available) {
        violation_lab_test_message(message, message_size,
                                   "LAB OPEN CONTRACT FAILED");
        return false;
    }

    violation_lab_test_press(&controls, TECMO_CONTROL_VIOLATION_LAB_NEXT);
    if (!tecmo_gameplay_violation_lab_update(&lab, &scene, &controls) ||
        violation_lab_selected_item(&lab) !=
            TECMO_GAMEPLAY_VIOLATION_LAB_BACKCOURT ||
        memcmp(&scene.state, &initial_state, sizeof(initial_state)) != 0) {
        violation_lab_test_message(message, message_size,
                                   "LAB SOURCE SELECTION MUTATED STATE");
        return false;
    }
    violation_lab_test_press(&controls, TECMO_CONTROL_VIOLATION_LAB_PATH);
    if (!tecmo_gameplay_violation_lab_update(&lab, &scene, &controls) ||
        lab.path != TECMO_GAMEPLAY_VIOLATION_LAB_PRODUCTION_STATE_PREVIEW ||
        scene.state.phase != TECMO_GAMEPLAY_PHASE_VIOLATION_PRESENTATION ||
        scene.state.violation != TECMO_GAMEPLAY_VIOLATION_BACKCOURT ||
        scene.state.phase_frame != 0U) {
        violation_lab_test_message(message, message_size,
                                   "LAB BACKCOURT STATE PREVIEW FAILED");
        return false;
    }
    violation_lab_test_press(&controls, TECMO_CONTROL_VIOLATION_LAB_STEP);
    if (!tecmo_gameplay_violation_lab_update(&lab, &scene, &controls) ||
        !lab.paused || lab.phase_frame != 1U || scene.state.phase_frame != 1U) {
        violation_lab_test_message(message, message_size,
                                   "LAB SINGLE STEP FAILED");
        return false;
    }
    violation_lab_test_press(&controls, TECMO_CONTROL_VIOLATION_LAB_NEXT);
    if (!tecmo_gameplay_violation_lab_update(&lab, &scene, &controls) ||
        violation_lab_selected_item(&lab) !=
            TECMO_GAMEPLAY_VIOLATION_LAB_FIVE_SECONDS ||
        lab.path != TECMO_GAMEPLAY_VIOLATION_LAB_SOURCE_PREVIEW ||
        memcmp(&scene.state, &initial_state, sizeof(initial_state)) != 0) {
        violation_lab_test_message(message, message_size,
                                   "LAB UNSUPPORTED STATE ROUTE FAILED");
        return false;
    }
    violation_lab_test_press(&controls, TECMO_CONTROL_VIOLATION_LAB_PATH);
    if (!tecmo_gameplay_violation_lab_update(&lab, &scene, &controls) ||
        !lab.state_path_rejected || lab.path !=
            TECMO_GAMEPLAY_VIOLATION_LAB_SOURCE_PREVIEW ||
        memcmp(&scene.state, &initial_state, sizeof(initial_state)) != 0) {
        violation_lab_test_message(message, message_size,
                                   "LAB UNSUPPORTED PATH REJECTION FAILED");
        return false;
    }
    if (!tecmo_gameplay_violation_lab_set_item(
            &lab, &scene, TECMO_GAMEPLAY_VIOLATION_LAB_FIXED_FOUL) ||
        !tecmo_gameplay_violation_lab_set_path(
            &lab, &scene,
            TECMO_GAMEPLAY_VIOLATION_LAB_PRODUCTION_STATE_PREVIEW) ||
        !tecmo_gameplay_violation_lab_set_frame(&lab, &scene, 23U) ||
        scene.state.phase != TECMO_GAMEPLAY_PHASE_FOUL_PRESENTATION ||
        scene.state.phase_frame != 23U) {
        violation_lab_test_message(message, message_size,
                                   "LAB FOUL STATE PREVIEW FAILED");
        return false;
    }
    tecmo_gameplay_violation_lab_close(&lab, &scene);
    if (lab.active || memcmp(&scene.state, &initial_state,
                             sizeof(initial_state)) != 0 ||
        memcmp(&scene.foul_presentation, &initial_foul,
               sizeof(initial_foul)) != 0) {
        violation_lab_test_message(message, message_size,
                                   "LAB TRANSACTION RESTORE FAILED");
        return false;
    }
    if (!violation_lab_test_rejects_superseding_scene_presentation(
            &scene, &initial_state, &initial_foul, message, message_size)) {
        return false;
    }
    violation_lab_test_message(message, message_size,
                               "VIOLATION LAB SELF TEST PASS");
    return true;
}
