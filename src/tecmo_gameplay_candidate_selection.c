#include "tecmo_gameplay_candidate_selection.h"
#include "tecmo_gameplay_defense_contact.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Bank06 $9E48-$9E4F, Rev1 SHA256
   D989AA2ADBE02D459B3395D5F3C6D958E619BCF2894207D07F61828226441D30. */
const uint8_t tecmo_gameplay_candidate_cpu_direction_sector[8] = {
    0x01U,0x02U,0x04U,0x05U,0x06U,0x08U,0x09U,0x0AU
};

static const uint8_t table_b32f[11] =
    {0U,0U,0U,0U,0U,1U,0xFFU,0U,0U,0xFFU,1U};
static const uint8_t table_b33a[11] =
    {0U,1U,1U,0U,0U,1U,1U,0U,0U,1U,1U};
static const uint8_t table_b345[11] =
    {0U,0U,0U,0U,1U,1U,1U,0U,1U,1U,1U};
static const uint8_t table_b350[11] =
    {0U,0U,0x80U,0U,0U,0U,0x80U,0U,0U,0U,0x80U};
static const uint8_t table_b35b[11] =
    {0U,0U,0U,0U,0U,0U,0U,0U,0x80U,0x80U,0x80U};

static uint16_t neg16(uint16_t value) { return (uint16_t)(0U - value); }
static uint16_t abs16(uint16_t value)
{
    return (value & 0x8000U) != 0U ? neg16(value) : value;
}

/* This intentionally preserves the source's byte order for `lsr low; ror
   high`; it is not replaced by an intuitive uint16 divide. */
static uint16_t source_half(uint16_t value)
{
    uint8_t low = (uint8_t)value;
    uint8_t high = (uint8_t)(value >> 8U);
    uint8_t carry = (uint8_t)(low & 1U);
    low >>= 1U;
    high = (uint8_t)((high >> 1U) | (carry << 7U));
    return (uint16_t)low | (uint16_t)((uint16_t)high << 8U);
}

static uint16_t directional_score(
    const TecmoGameplayCandidateInput *input, uint8_t actor)
{
    uint8_t mode = input->direction_sector;
    uint16_t dx = (uint16_t)(input->actor_x[actor] -
                             input->actor_x[input->reference_actor]);
    uint16_t dz = (uint16_t)(uint8_t)(input->actor_depth[actor] -
                                      input->actor_depth[input->reference_actor]);
    if (input->actor_depth[actor] <
            input->actor_depth[input->reference_actor]) dz |= 0xFF00U;
    if (mode < 4U) return abs16(dz);
    if ((mode & 3U) == 0U) return abs16(dx);
    {
        bool negative_transform = (table_b32f[mode] & 0x80U) != 0U;
        uint16_t line_x = negative_transform
            ? (uint16_t)(input->actor_x[input->reference_actor] - dz)
            : (uint16_t)(input->actor_x[input->reference_actor] + dz);
        uint16_t line_depth = negative_transform
            ? (uint16_t)((uint16_t)input->actor_depth[actor] - dx)
            : (uint16_t)(input->actor_x[actor] + dx);
        uint16_t component_x = abs16(
            (uint16_t)(input->actor_x[actor] - line_x));
        uint16_t component_depth = abs16(
            (uint16_t)(line_depth - input->actor_depth[actor]));
        if (component_x >= component_depth)
            component_depth = source_half(component_depth);
        else
            component_x = source_half(component_x);
        return (uint16_t)(component_x + component_depth);
    }
}

bool tecmo_gameplay_candidate_directional_select(
    const TecmoGameplayCandidateInput *input,
    TecmoGameplayCandidateResult *result_out)
{
    TecmoGameplayCandidateResult result;
    uint16_t best = 0x0900U;
    int actor;
    if (input == NULL || result_out == NULL ||
        input->contract_tag != TECMO_GAMEPLAY_CANDIDATE_INPUT_TAG ||
        input->direction_sector > 10U ||
        input->excluded_actor >= TECMO_GAMEPLAY_CANDIDATE_ACTOR_COUNT ||
        input->reference_actor >= TECMO_GAMEPLAY_CANDIDATE_ACTOR_COUNT ||
        (input->required_polarity != 0U &&
         input->required_polarity != TECMO_GAMEPLAY_CANDIDATE_POLARITY_MASK)) {
        return false;
    }
    memset(&result, 0, sizeof(result));
    result.contract_tag = TECMO_GAMEPLAY_CANDIDATE_RESULT_TAG;
    result.candidate_actor = TECMO_GAMEPLAY_CANDIDATE_NO_ACTOR;
    result.candidate_score = best;
    result.scan_first = 9U;
    result.scan_last = 0U;
    for (actor = 0; actor < 10; ++actor) {
        result.score[actor] = 0xFFFFU;
        result.filter[actor] = TECMO_GAMEPLAY_CANDIDATE_FILTER_SCORE;
    }
    if (input->direction_sector == 0U) {
        *result_out = result;
        return true;
    }
    for (actor = 9; actor >= 0; --actor) {
        uint16_t dx;
        uint16_t dz;
        uint16_t score;
        if ((uint8_t)actor == input->excluded_actor) {
            result.filter[actor] = TECMO_GAMEPLAY_CANDIDATE_FILTER_EXCLUDED;
            continue;
        }
        if ((input->actor_flags[actor] &
             TECMO_GAMEPLAY_CANDIDATE_POLARITY_MASK) !=
                input->required_polarity) {
            result.filter[actor] = TECMO_GAMEPLAY_CANDIDATE_FILTER_POLARITY;
            continue;
        }
        if (input->required_polarity != 0U) {
            uint16_t distance = (uint16_t)(input->actor_x[actor] -
                                           input->viewport_x);
            if (input->viewport_x >= input->actor_x[actor] ||
                (distance >> 8U) != 0U) {
                result.filter[actor] = TECMO_GAMEPLAY_CANDIDATE_FILTER_VIEWPORT;
                continue;
            }
        }
        dx = (uint16_t)(input->actor_x[actor] -
                        input->actor_x[input->reference_actor]);
        dz = (uint16_t)(uint8_t)(input->actor_depth[actor] -
                                 input->actor_depth[input->reference_actor]);
        if (input->actor_depth[actor] <
                input->actor_depth[input->reference_actor]) dz |= 0xFF00U;
        if (table_b33a[input->direction_sector] != 0U &&
            (uint8_t)(dx >> 8U & 0x80U) !=
                table_b350[input->direction_sector]) {
            result.filter[actor] =
                TECMO_GAMEPLAY_CANDIDATE_FILTER_HORIZONTAL_SIGN;
            continue;
        }
        if (table_b345[input->direction_sector] != 0U &&
            (uint8_t)(dz >> 8U & 0x80U) !=
                table_b35b[input->direction_sector]) {
            result.filter[actor] = TECMO_GAMEPLAY_CANDIDATE_FILTER_DEPTH_SIGN;
            continue;
        }
        score = directional_score(input, (uint8_t)actor);
        result.score[actor] = score;
        if (score < best) {
            best = score;
            result.wrote_candidate = true;
            result.candidate_actor = (uint8_t)actor;
            result.candidate_score = score;
            result.filter[actor] = TECMO_GAMEPLAY_CANDIDATE_FILTER_ACCEPTED;
        } else {
            result.filter[actor] = TECMO_GAMEPLAY_CANDIDATE_FILTER_SCORE;
        }
    }
    *result_out = result;
    return true;
}

/* TEST-ONLY direct 6502-semantic reference. Kept separate from the shipping
   evaluator so the deterministic corpus detects scan/filter/score drift. */
static uint16_t reference_score(
    const TecmoGameplayCandidateInput *input, uint8_t actor)
{
    uint8_t mode = input->direction_sector;
    uint16_t dx = (uint16_t)(input->actor_x[actor] -
        input->actor_x[input->reference_actor]);
    uint16_t dz = (uint16_t)(uint8_t)(input->actor_depth[actor] -
        input->actor_depth[input->reference_actor]);
    if (input->actor_depth[actor] < input->actor_depth[input->reference_actor])
        dz |= 0xFF00U;
    if (mode < 4U) return abs16(dz);
    if ((mode & 3U) == 0U) return abs16(dx);
    {
        uint16_t a = (table_b32f[mode] & 0x80U) != 0U
            ? (uint16_t)(input->actor_x[input->reference_actor] - dz)
            : (uint16_t)(input->actor_x[input->reference_actor] + dz);
        uint16_t b = (table_b32f[mode] & 0x80U) != 0U
            ? (uint16_t)((uint16_t)input->actor_depth[actor] - dx)
            : (uint16_t)(input->actor_x[actor] + dx);
        uint16_t c = abs16((uint16_t)(input->actor_x[actor] - a));
        uint16_t d = abs16((uint16_t)(b - input->actor_depth[actor]));
        if (c >= d) d = source_half(d); else c = source_half(c);
        return (uint16_t)(c + d);
    }
}

static void reference_select(
    const TecmoGameplayCandidateInput *input,
    bool *wrote_out, uint8_t *actor_out, uint16_t *score_out)
{
    uint16_t best = 0x0900U;
    int actor;
    *wrote_out = false; *actor_out = 0xFFU; *score_out = best;
    if (input->direction_sector == 0U) return;
    for (actor = 9; actor >= 0; --actor) {
        uint16_t dx, dz, score;
        if ((uint8_t)actor == input->excluded_actor ||
            (input->actor_flags[actor] & 0x10U) != input->required_polarity)
            continue;
        if (input->required_polarity != 0U &&
            (input->viewport_x >= input->actor_x[actor] ||
             ((uint16_t)(input->actor_x[actor] - input->viewport_x) >> 8U) != 0U))
            continue;
        dx = (uint16_t)(input->actor_x[actor] -
            input->actor_x[input->reference_actor]);
        dz = (uint16_t)(uint8_t)(input->actor_depth[actor] -
            input->actor_depth[input->reference_actor]);
        if (input->actor_depth[actor] < input->actor_depth[input->reference_actor])
            dz |= 0xFF00U;
        if ((table_b33a[input->direction_sector] != 0U &&
             (uint8_t)(dx >> 8U & 0x80U) != table_b350[input->direction_sector]) ||
            (table_b345[input->direction_sector] != 0U &&
             (uint8_t)(dz >> 8U & 0x80U) != table_b35b[input->direction_sector]))
            continue;
        score = reference_score(input, (uint8_t)actor);
        if (score < best) {
            best = score; *wrote_out = true;
            *actor_out = (uint8_t)actor; *score_out = score;
        }
    }
}

bool tecmo_gameplay_candidate_selection_self_test(
    char *message, size_t message_size)
{
    static const uint8_t sectors[8] = {1U,2U,4U,5U,6U,8U,9U,10U};
    TecmoGameplayCandidateInput input;
    TecmoGameplayCandidateResult result;
    size_t case_index;
    memset(&input, 0, sizeof(input));
    input.contract_tag = TECMO_GAMEPLAY_CANDIDATE_INPUT_TAG;
    input.direction_sector = 1U;
    input.excluded_actor = 0U;
    input.reference_actor = 0U;
    input.actor_x[0U] = 200U; input.actor_depth[0U] = 100U;
    input.actor_x[1U] = 260U; input.actor_depth[1U] = 100U;
    input.actor_x[4U] = 260U; input.actor_depth[4U] = 100U;
    if (!tecmo_gameplay_candidate_directional_select(&input, &result) ||
        !result.wrote_candidate || result.candidate_actor != 4U ||
        result.scan_first != 9U || result.scan_last != 0U) {
        if (message != NULL && message_size > 0U)
            (void)snprintf(message, message_size,
                           "B183 descending/tie contract failed");
        return false;
    }
    input.direction_sector = 0U;
    if (!tecmo_gameplay_candidate_directional_select(&input, &result) ||
        result.wrote_candidate) {
        if (message != NULL && message_size > 0U)
            (void)snprintf(message, message_size,
                           "B183 neutral no-write contract failed");
        return false;
    }
    if (memcmp(tecmo_gameplay_candidate_cpu_direction_sector,
               sectors, sizeof(sectors)) != 0) {
        if (message != NULL && message_size > 0U)
            (void)snprintf(message, message_size, "$9E48 map mismatch");
        return false;
    }
    /* Golden sign-sector cases. Only slot 1 is eligible, so every accepted
       result directly proves its table gate rather than a fallback score. */
    for (case_index = 0U; case_index < 8U; ++case_index) {
        memset(&input, 0, sizeof(input));
        input.contract_tag = TECMO_GAMEPLAY_CANDIDATE_INPUT_TAG;
        input.direction_sector = sectors[case_index];
        input.excluded_actor = 0U;
        input.reference_actor = 0U;
        input.actor_x[0U] = 300U; input.actor_depth[0U] = 120U;
        input.actor_x[1U] = (case_index == 1U || case_index == 4U ||
                             case_index == 7U)
            ? 260U : (case_index == 2U || case_index == 5U ? 300U : 340U);
        input.actor_depth[1U] =
            (case_index == 5U || case_index == 6U || case_index == 7U)
                ? 80U
                : (case_index == 0U || case_index == 1U ? 120U : 160U);
        for (size_t actor = 2U; actor < 10U; ++actor)
            input.actor_flags[actor] = 0x10U;
        if (!tecmo_gameplay_candidate_directional_select(&input, &result) ||
            !result.wrote_candidate || result.candidate_actor != 1U) {
            if (message != NULL && message_size > 0U)
                (void)snprintf(message, message_size,
                    "B183 golden sector %u failed", sectors[case_index]);
            return false;
        }
    }
    /* Wrapped X subtraction: 4-65530 is +10 in source uint16 arithmetic. */
    memset(&input, 0, sizeof(input));
    input.contract_tag = TECMO_GAMEPLAY_CANDIDATE_INPUT_TAG;
    input.direction_sector = 1U; input.excluded_actor = 0U;
    input.reference_actor = 0U; input.actor_x[0U] = 65530U;
    input.actor_x[1U] = 4U;
    for (size_t actor = 2U; actor < 10U; ++actor)
        input.actor_flags[actor] = 0x10U;
    if (!tecmo_gameplay_candidate_directional_select(&input, &result) ||
        result.candidate_actor != 1U) {
        if (message != NULL && message_size > 0U)
            (void)snprintf(message, message_size, "B183 uint16 wrap failed");
        return false;
    }
    /* Invalid source-facing inputs are rejected before touching the caller's
       result, preserving the per-frame candidate transaction. */
    memset(&result, 0xA5, sizeof(result));
    input.direction_sector = 11U;
    {
        TecmoGameplayCandidateResult sentinel = result;
        if (tecmo_gameplay_candidate_directional_select(&input, &result) ||
            memcmp(&result, &sentinel, sizeof(result)) != 0) {
            if (message != NULL && message_size > 0U)
                (void)snprintf(message, message_size,
                    "B183 invalid-input transaction failed");
            return false;
        }
    }
    {
        TecmoGameplayDefenseContactB06ScanInput scan;
        TecmoGameplayDefenseContactB06ScanResult scan_result;
        uint8_t low[10] = {0U}, high[10] = {0U}, depth[10] = {0U};
        uint8_t flags[10] = {0U};
        memset(&scan, 0, sizeof(scan));
        low[6U] = 110U; low[7U] = 110U;
        depth[6U] = 100U; depth[7U] = 100U;
        flags[6U] = 0x10U; flags[7U] = 0x10U;
        scan.contract_tag = TECMO_GAMEPLAY_DEFENSE_CONTACT_B06_SCAN_INPUT_TAG;
        scan.routine_cpu = TECMO_GAMEPLAY_DEFENSE_CONTACT_B06_ROUTINE_CPU;
        scan.raw_007d = 100U; scan.raw_00fd = 100U;
        scan.raw_0309 = 5U; scan.raw_030b = 1U;
        scan.raw_037f_at_030b = 6U;
        scan.raw_0073_low=low; scan.raw_0073_low_count=10U;
        scan.raw_00e8_high=high; scan.raw_00e8_high_count=10U;
        scan.raw_00f3_depth=depth; scan.raw_00f3_depth_count=10U;
        scan.raw_04b0_by_slot=flags; scan.raw_04b0_by_slot_count=10U;
        if (!tecmo_gameplay_defense_contact_b06_candidate_scan_b081(
                &scan, &scan_result) ||
            scan_result.raw_037f_at_030b != 7U) {
            if (message != NULL && message_size > 0U)
                (void)snprintf(message, message_size,
                               "B081 descending tie/exclusion failed");
            return false;
        }
    }
    {
        uint32_t random = 0xB183B081U;
        size_t corpus;
        for (corpus = 0U; corpus < 1024U; ++corpus) {
            bool reference_wrote;
            uint8_t reference_actor;
            uint16_t reference_best;
            memset(&input, 0, sizeof(input));
            input.contract_tag = TECMO_GAMEPLAY_CANDIDATE_INPUT_TAG;
            input.direction_sector = sectors[corpus & 7U];
            input.excluded_actor = (uint8_t)(corpus % 10U);
            input.reference_actor = input.excluded_actor;
            input.required_polarity = (corpus & 8U) != 0U ? 0x10U : 0U;
            input.viewport_x = 16U;
            for (size_t actor = 0U; actor < 10U; ++actor) {
                random = random * 1664525U + 1013904223U;
                input.actor_x[actor] = (uint16_t)(32U + (random & 0xBFU));
                random = random * 1664525U + 1013904223U;
                input.actor_depth[actor] = (uint8_t)(32U + (random & 0xBFU));
                input.actor_flags[actor] = actor >= 5U ? 0x10U : 0U;
            }
            reference_select(&input, &reference_wrote,
                             &reference_actor, &reference_best);
            if (!tecmo_gameplay_candidate_directional_select(&input, &result) ||
                result.wrote_candidate != reference_wrote ||
                result.candidate_actor != reference_actor ||
                result.candidate_score != reference_best) {
                if (message != NULL && message_size > 0U)
                    (void)snprintf(message, message_size,
                        "B183 reference corpus mismatch at %u",
                        (unsigned)corpus);
                return false;
            }
        }
    }
    if (message != NULL && message_size > 0U)
        (void)snprintf(message, message_size,
                       "candidate selection B081/B183 source contract pass");
    return true;
}

static uint32_t fnv1a32(const uint8_t *bytes, size_t count)
{
    uint32_t value = 2166136261U;
    size_t index;
    for (index = 0U; index < count; ++index) {
        value ^= bytes[index];
        value *= 16777619U;
    }
    return value;
}

bool tecmo_gameplay_candidate_selection_source_test(
    const char *rom_path, char *message, size_t message_size)
{
    static const uint8_t header[16] = {
        'N','E','S',0x1AU,0x08U,0x20U,0x42U,0U,
        0U,0U,0U,0U,0U,0U,0U,0U};
    static const uint8_t fixed_loop[9] = {
        0x20U,0x6AU,0xD3U,0x20U,0x39U,0xB1U,0x20U,0x04U,0xB1U};
    FILE *file;
    uint8_t *bytes;
    long length;
    bool ok;
    const size_t b06 = 16U + 6U * 16384U;
    const size_t b05 = 16U + 5U * 16384U;
    if (rom_path == NULL || fopen_s(&file, rom_path, "rb") != 0 ||
        file == NULL)
        return false;
    if (fseek(file, 0L, SEEK_END) != 0 ||
        (length = ftell(file)) != 393232L ||
        fseek(file, 0L, SEEK_SET) != 0 ||
        (bytes = (uint8_t *)malloc((size_t)length)) == NULL) {
        fclose(file); return false;
    }
    ok = fread(bytes, 1U, (size_t)length, file) == (size_t)length;
    fclose(file);
    ok = ok && memcmp(bytes, header, sizeof(header)) == 0 &&
        fnv1a32(bytes, (size_t)length) == 0x0650F5B0U &&
        memcmp(bytes + 0x1F05BU, fixed_loop, sizeof(fixed_loop)) == 0 &&
        fnv1a32(bytes + b06 + (0xB081U - 0x8000U),
                0xB326U - 0xB081U + 1U) == 0x11B1E26EU &&
        fnv1a32(bytes + b06 + (0xB32FU - 0x8000U), 55U) == 0x6488E745U &&
        fnv1a32(bytes + b06 + (0x9E48U - 0x8000U), 8U) == 0x008DEAE4U &&
        memcmp(bytes + b06 + (0x9E48U - 0x8000U),
               tecmo_gameplay_candidate_cpu_direction_sector, 8U) == 0 &&
        fnv1a32(bytes + b05 + (0xB074U - 0x8000U),
                0xB0FDU - 0xB074U + 1U) == 0xD444B867U;
    free(bytes);
    if (message != NULL && message_size > 0U)
        (void)snprintf(message, message_size, "%s",
            ok ? "candidate selector source spans/fixed-loop order pass"
               : "candidate selector source fingerprint mismatch");
    return ok;
}
