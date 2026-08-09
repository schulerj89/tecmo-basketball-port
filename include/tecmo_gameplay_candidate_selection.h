#ifndef TECMO_GAMEPLAY_CANDIDATE_SELECTION_H
#define TECMO_GAMEPLAY_CANDIDATE_SELECTION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TECMO_GAMEPLAY_CANDIDATE_ACTOR_COUNT 10U
#define TECMO_GAMEPLAY_CANDIDATE_SIDE_COUNT 2U
#define TECMO_GAMEPLAY_CANDIDATE_NO_ACTOR 0xFFU
#define TECMO_GAMEPLAY_CANDIDATE_POLARITY_MASK 0x10U
#define TECMO_GAMEPLAY_CANDIDATE_INPUT_TAG 0x54435349U
#define TECMO_GAMEPLAY_CANDIDATE_RESULT_TAG 0x54435352U

typedef enum TecmoGameplayCandidateFilter {
    TECMO_GAMEPLAY_CANDIDATE_FILTER_ACCEPTED = 0,
    TECMO_GAMEPLAY_CANDIDATE_FILTER_EXCLUDED,
    TECMO_GAMEPLAY_CANDIDATE_FILTER_POLARITY,
    TECMO_GAMEPLAY_CANDIDATE_FILTER_VIEWPORT,
    TECMO_GAMEPLAY_CANDIDATE_FILTER_HORIZONTAL_SIGN,
    TECMO_GAMEPLAY_CANDIDATE_FILTER_DEPTH_SIGN,
    TECMO_GAMEPLAY_CANDIDATE_FILTER_SCORE
} TecmoGameplayCandidateFilter;

typedef struct TecmoGameplayCandidateInput {
    uint32_t contract_tag;
    uint8_t direction_sector;
    uint8_t excluded_actor;
    uint8_t required_polarity;
    uint8_t reference_actor;
    uint16_t viewport_x;
    uint16_t actor_x[TECMO_GAMEPLAY_CANDIDATE_ACTOR_COUNT];
    uint8_t actor_depth[TECMO_GAMEPLAY_CANDIDATE_ACTOR_COUNT];
    uint8_t actor_flags[TECMO_GAMEPLAY_CANDIDATE_ACTOR_COUNT];
} TecmoGameplayCandidateInput;

typedef struct TecmoGameplayCandidateResult {
    uint32_t contract_tag;
    bool wrote_candidate;
    uint8_t candidate_actor;
    uint8_t scan_first;
    uint8_t scan_last;
    uint16_t candidate_score;
    uint16_t score[TECMO_GAMEPLAY_CANDIDATE_ACTOR_COUNT];
    uint8_t filter[TECMO_GAMEPLAY_CANDIDATE_ACTOR_COUNT];
} TecmoGameplayCandidateResult;

/* Exact Bank06 $9E48 bytes indexed by actor direction $0463. */
extern const uint8_t tecmo_gameplay_candidate_cpu_direction_sector[8];

/* Exact bounded Bank06 $B183-$B326 evaluator. A zero sector is the source
 * no-write path and returns a valid result with wrote_candidate=false. */
bool tecmo_gameplay_candidate_directional_select(
    const TecmoGameplayCandidateInput *input,
    TecmoGameplayCandidateResult *result_out);

bool tecmo_gameplay_candidate_selection_self_test(
    char *message, size_t message_size);
bool tecmo_gameplay_candidate_selection_source_test(
    const char *rom_path, char *message, size_t message_size);

#endif
