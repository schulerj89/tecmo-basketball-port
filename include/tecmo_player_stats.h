#ifndef TECMO_PLAYER_STATS_H
#define TECMO_PLAYER_STATS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Player-season statistics are deliberately independent of TTDT's static
 * ratings and identity records.  These dimensions are the native season
 * accumulator dimensions: two gameplay sides, twelve roster slots, nine
 * counters, and twenty-seven canonical teams. */
#define TECMO_PLAYER_STATS_GAME_SIDE_COUNT 2U
#define TECMO_PLAYER_STATS_ROSTER_COUNT 12U
#define TECMO_PLAYER_STATS_TEAM_COUNT 27U
#define TECMO_PLAYER_STATS_COUNTER_DIMENSION 9U
#define TECMO_PLAYER_STATS_IMPLEMENTED_COUNTER_COUNT 6U
#define TECMO_PLAYER_STATS_IMPLEMENTED_COVERAGE \
    ((uint16_t)((1U << TECMO_PLAYER_STATS_IMPLEMENTED_COUNTER_COUNT) - 1U))

/* The order is part of the TSAV-2 wire contract. */
typedef enum TecmoPlayerStatsCounter {
    TECMO_PLAYER_STATS_COUNTER_FTA = 0,
    TECMO_PLAYER_STATS_COUNTER_FGA,
    TECMO_PLAYER_STATS_COUNTER_THREE_PA,
    TECMO_PLAYER_STATS_COUNTER_FTM,
    TECMO_PLAYER_STATS_COUNTER_FGM,
    TECMO_PLAYER_STATS_COUNTER_THREE_PM,
    TECMO_PLAYER_STATS_COUNTER_STEALS,
    TECMO_PLAYER_STATS_COUNTER_BLOCKS,
    TECMO_PLAYER_STATS_COUNTER_REBOUNDS,
    TECMO_PLAYER_STATS_COUNTER_COUNT
} TecmoPlayerStatCounter;

typedef TecmoPlayerStatCounter TecmoPlayerStatsCounter;

/* Short aliases keep call sites readable while retaining the explicit wire
 * names above. */
#define TECMO_PLAYER_STATS_FTA TECMO_PLAYER_STATS_COUNTER_FTA
#define TECMO_PLAYER_STATS_FGA TECMO_PLAYER_STATS_COUNTER_FGA
#define TECMO_PLAYER_STATS_THREE_PA TECMO_PLAYER_STATS_COUNTER_THREE_PA
#define TECMO_PLAYER_STATS_FTM TECMO_PLAYER_STATS_COUNTER_FTM
#define TECMO_PLAYER_STATS_FGM TECMO_PLAYER_STATS_COUNTER_FGM
#define TECMO_PLAYER_STATS_THREE_PM TECMO_PLAYER_STATS_COUNTER_THREE_PM
#define TECMO_PLAYER_STATS_STEALS TECMO_PLAYER_STATS_COUNTER_STEALS
#define TECMO_PLAYER_STATS_BLOCKS TECMO_PLAYER_STATS_COUNTER_BLOCKS
#define TECMO_PLAYER_STATS_REBOUNDS TECMO_PLAYER_STATS_COUNTER_REBOUNDS

typedef uint8_t TecmoPlayerStatsGameCounters[
    TECMO_PLAYER_STATS_GAME_SIDE_COUNT][TECMO_PLAYER_STATS_ROSTER_COUNT]
    [TECMO_PLAYER_STATS_COUNTER_DIMENSION];
typedef uint16_t TecmoPlayerStatsSeasonTotals[
    TECMO_PLAYER_STATS_TEAM_COUNT][TECMO_PLAYER_STATS_ROSTER_COUNT]
    [TECMO_PLAYER_STATS_COUNTER_DIMENSION];

typedef TecmoPlayerStatsGameCounters TecmoPlayerGameStats;
typedef TecmoPlayerStatsSeasonTotals TecmoPlayerSeasonStats;

typedef struct TecmoPlayerStatsGameLedger {
    TecmoPlayerStatsGameCounters counters;
    /* Coverage is a counter-index bit mask.  It is global to the game ledger;
     * side/player identity remains in counters and is mapped at commit. */
    uint16_t coverage;
} TecmoPlayerStatsGameLedger;

static inline bool tecmo_player_stats_side_valid(uint8_t side)
{
    return side < TECMO_PLAYER_STATS_GAME_SIDE_COUNT;
}

static inline bool tecmo_player_stats_roster_valid(uint8_t roster)
{
    return roster < TECMO_PLAYER_STATS_ROSTER_COUNT;
}

static inline bool tecmo_player_stats_counter_valid(uint8_t counter)
{
    return counter < TECMO_PLAYER_STATS_COUNTER_DIMENSION;
}

static inline uint16_t tecmo_player_stats_counter_bit(uint8_t counter)
{
    return tecmo_player_stats_counter_valid(counter)
               ? (uint16_t)(1U << counter)
               : 0U;
}

static inline void tecmo_player_stats_game_ledger_clear(
    TecmoPlayerStatsGameLedger *ledger)
{
    if (ledger != NULL) memset(ledger, 0, sizeof(*ledger));
}

static inline void tecmo_player_stats_game_ledger_initialize(
    TecmoPlayerStatsGameLedger *ledger)
{
    tecmo_player_stats_game_ledger_clear(ledger);
    if (ledger != NULL)
        ledger->coverage = TECMO_PLAYER_STATS_IMPLEMENTED_COVERAGE;
}

/* Generic counter mutation is restricted to the six implemented emitters;
 * there is intentionally no gameplay path for steals/blocks/rebounds. */
static inline bool tecmo_player_stats_game_counter_add(
    TecmoPlayerStatsGameLedger *ledger,
    uint8_t side,
    uint8_t roster,
    uint8_t counter,
    uint8_t delta)
{
    if (ledger == NULL || !tecmo_player_stats_side_valid(side) ||
        !tecmo_player_stats_roster_valid(roster) ||
        counter >= TECMO_PLAYER_STATS_IMPLEMENTED_COUNTER_COUNT ||
        ledger->coverage != TECMO_PLAYER_STATS_IMPLEMENTED_COVERAGE)
        return false;
    ledger->counters[side][roster][counter] =
        (uint8_t)((uint16_t)ledger->counters[side][roster][counter] + delta);
    return true;
}

static inline bool tecmo_player_stats_record_shot_attempt(
    TecmoPlayerStatsGameLedger *ledger,
    uint8_t side,
    uint8_t roster,
    uint8_t point_value)
{
    if (point_value != 2U && point_value != 3U) return false;
    if (!tecmo_player_stats_game_counter_add(
            ledger, side, roster, TECMO_PLAYER_STATS_COUNTER_FGA, 1U))
        return false;
    return point_value != 3U || tecmo_player_stats_game_counter_add(
        ledger, side, roster, TECMO_PLAYER_STATS_COUNTER_THREE_PA, 1U);
}

static inline bool tecmo_player_stats_record_shot_make(
    TecmoPlayerStatsGameLedger *ledger,
    uint8_t side,
    uint8_t roster,
    uint8_t point_value)
{
    if (point_value != 2U && point_value != 3U) return false;
    if (!tecmo_player_stats_game_counter_add(
            ledger, side, roster, TECMO_PLAYER_STATS_COUNTER_FGM, 1U))
        return false;
    return point_value != 3U || tecmo_player_stats_game_counter_add(
        ledger, side, roster, TECMO_PLAYER_STATS_COUNTER_THREE_PM, 1U);
}

static inline bool tecmo_player_stats_record_free_throw(
    TecmoPlayerStatsGameLedger *ledger,
    uint8_t side,
    uint8_t roster,
    bool made)
{
    if (!tecmo_player_stats_game_counter_add(
            ledger, side, roster, TECMO_PLAYER_STATS_COUNTER_FTA, 1U))
        return false;
    return !made || tecmo_player_stats_game_counter_add(
        ledger, side, roster, TECMO_PLAYER_STATS_COUNTER_FTM, 1U);
}

static inline bool tecmo_player_stats_game_ledger_valid(
    const TecmoPlayerStatsGameLedger *ledger)
{
    if (ledger == NULL ||
        ledger->coverage != TECMO_PLAYER_STATS_IMPLEMENTED_COVERAGE)
        return false;
    for (size_t side = 0U; side < TECMO_PLAYER_STATS_GAME_SIDE_COUNT; ++side)
        for (size_t roster = 0U; roster < TECMO_PLAYER_STATS_ROSTER_COUNT;
             ++roster) {
            for (size_t counter = 0U;
                 counter < TECMO_PLAYER_STATS_IMPLEMENTED_COUNTER_COUNT;
                 ++counter)
                if (ledger->counters[side][roster][counter] != 0U &&
                    (ledger->coverage & (uint16_t)(1U << counter)) == 0U)
                    return false;
            for (size_t counter = TECMO_PLAYER_STATS_IMPLEMENTED_COUNTER_COUNT;
                 counter < TECMO_PLAYER_STATS_COUNTER_DIMENSION; ++counter)
                if (ledger->counters[side][roster][counter] != 0U)
                    return false;
        }
    return true;
}

static inline bool tecmo_player_stats_merge_game(
    TecmoPlayerStatsSeasonTotals totals,
    uint16_t *coverage,
    uint8_t away_team,
    uint8_t home_team,
    const TecmoPlayerStatsGameLedger *ledger)
{
    if (totals == NULL || coverage == NULL || ledger == NULL ||
        away_team >= TECMO_PLAYER_STATS_TEAM_COUNT ||
        home_team >= TECMO_PLAYER_STATS_TEAM_COUNT ||
        away_team == home_team || !tecmo_player_stats_game_ledger_valid(ledger))
        return false;
    for (size_t side = 0U; side < TECMO_PLAYER_STATS_GAME_SIDE_COUNT; ++side) {
        uint8_t team = side == 0U ? away_team : home_team;
        for (size_t roster = 0U; roster < TECMO_PLAYER_STATS_ROSTER_COUNT;
             ++roster)
            for (size_t counter = 0U;
                 counter < TECMO_PLAYER_STATS_COUNTER_COUNT; ++counter)
                totals[team][roster][counter] = (uint16_t)(
                    (uint32_t)totals[team][roster][counter] +
                    ledger->counters[side][roster][counter]);
    }
    /* Coverage is a proof intersection.  An in-progress migrated season with
     * no trusted bits must remain incomplete after later commits. */
    *coverage &= ledger->coverage;
    return true;
}

#endif
