#include "tecmo_gameplay_fixed_rng.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

static uint8_t rng_cd9c(uint8_t raw_006a, uint8_t raw_0053)
{
    uint8_t shifted = (uint8_t)(raw_006a << 1U);
    if ((raw_006a & 0x80U) != 0U) shifted ^= 0x1DU;
    if (shifted == 0U) shifted ^= raw_0053;
    return shifted;
}

static bool ranges_overlap(const void *left, size_t left_size,
                           const void *right, size_t right_size)
{
    uintptr_t left_begin = (uintptr_t)left;
    uintptr_t right_begin = (uintptr_t)right;
    return left_begin < right_begin + right_size &&
           right_begin < left_begin + left_size;
}

static bool rng_valid(const TecmoGameplayFixedRng *state)
{
    return state != NULL &&
           state->contract_tag == TECMO_GAMEPLAY_FIXED_RNG_TAG &&
           state->initialized && state->native_live_checkpoint &&
           state->serial != 0U &&
           (uint64_t)state->serial ==
               (uint64_t)state->nmi_serial + state->c05d_serial + 1U &&
           state->last_callsite <= TECMO_GAMEPLAY_FIXED_RNG_CALL_A0DD;
}

bool tecmo_gameplay_fixed_rng_valid(const TecmoGameplayFixedRng *state)
{
    return rng_valid(state);
}

bool tecmo_gameplay_fixed_rng_live_checkpoint(
    TecmoGameplayFixedRng *state, uint8_t raw_006a, uint8_t raw_0053,
    uint8_t raw_0054)
{
    TecmoGameplayFixedRng candidate;
    TecmoGameplayFixedRng virgin;
    if (state == NULL) return false;
    memset(&virgin, 0, sizeof(virgin));
    if (memcmp(state, &virgin, sizeof(virgin)) != 0) return false;
    memset(&candidate, 0, sizeof(candidate));
    candidate.contract_tag = TECMO_GAMEPLAY_FIXED_RNG_TAG;
    candidate.serial = 1U;
    candidate.raw_006a = raw_006a;
    candidate.raw_0053 = raw_0053;
    candidate.raw_0054 = raw_0054;
    candidate.initialized = true;
    candidate.native_live_checkpoint = true;
    *state = candidate;
    return true;
}

bool tecmo_gameplay_fixed_rng_nmi_tick(TecmoGameplayFixedRng *state)
{
    TecmoGameplayFixedRng candidate;
    if (!rng_valid(state) || state->serial == UINT32_MAX ||
        state->nmi_serial == UINT32_MAX) return false;
    candidate = *state;
    ++candidate.raw_0053;
    if (candidate.raw_0053 == 0U) ++candidate.raw_0054;
    candidate.raw_006a = rng_cd9c(candidate.raw_006a, candidate.raw_0053);
    ++candidate.nmi_serial;
    ++candidate.serial;
    *state = candidate;
    return true;
}

bool tecmo_gameplay_fixed_rng_c05d(
    TecmoGameplayFixedRng *state, TecmoGameplayFixedRngCallsite callsite,
    uint8_t *raw_006a_out)
{
    TecmoGameplayFixedRng candidate;
    if (!rng_valid(state) || raw_006a_out == NULL ||
        callsite < TECMO_GAMEPLAY_FIXED_RNG_CALL_9FA1 ||
        callsite > TECMO_GAMEPLAY_FIXED_RNG_CALL_A0DD ||
        state->serial == UINT32_MAX || state->c05d_serial == UINT32_MAX ||
        ranges_overlap(state, sizeof(*state), raw_006a_out,
                       sizeof(*raw_006a_out))) return false;
    candidate = *state;
    candidate.raw_006a = rng_cd9c(
        (uint8_t)(candidate.raw_006a ^ candidate.raw_0053),
        candidate.raw_0053);
    candidate.last_callsite = (uint8_t)callsite;
    ++candidate.c05d_serial;
    ++candidate.serial;
    *state = candidate;
    *raw_006a_out = candidate.raw_006a;
    return true;
}

bool tecmo_gameplay_fixed_rng_self_test(char *message, size_t message_size)
{
    TecmoGameplayFixedRng state;
    TecmoGameplayFixedRng before;
    unsigned raw_006a;
    unsigned raw_0053;
    uint8_t output = 0U;
    for (raw_006a = 0U; raw_006a <= 0xFFU; ++raw_006a) {
        for (raw_0053 = 0U; raw_0053 <= 0xFFU; ++raw_0053) {
            uint8_t expected = rng_cd9c(
                (uint8_t)(raw_006a ^ raw_0053), (uint8_t)raw_0053);
            memset(&state, 0, sizeof(state));
            if (!tecmo_gameplay_fixed_rng_live_checkpoint(
                    &state, (uint8_t)raw_006a, (uint8_t)raw_0053, 0U) ||
                !tecmo_gameplay_fixed_rng_c05d(
                    &state, TECMO_GAMEPLAY_FIXED_RNG_CALL_9FA1, &output) ||
                output != expected || state.raw_006a != expected) goto fail;
        }
    }
    memset(&state, 0, sizeof(state));
    if (!tecmo_gameplay_fixed_rng_live_checkpoint(&state, 0x80U, 0U, 0U) ||
        !tecmo_gameplay_fixed_rng_nmi_tick(&state) ||
        state.raw_0053 != 1U || state.raw_0054 != 0U ||
        state.raw_006a != 0x1DU) goto fail;
    state.raw_0053 = 0xFFU;
    state.raw_0054 = 0x7FU;
    state.raw_006a = 0U;
    if (!tecmo_gameplay_fixed_rng_nmi_tick(&state) ||
        state.raw_0053 != 0U || state.raw_0054 != 0x80U ||
        state.raw_006a != 0U) goto fail;
    state.raw_0053 = 0x5AU;
    state.raw_006a = 0U;
    if (!tecmo_gameplay_fixed_rng_nmi_tick(&state) ||
        state.raw_006a == 0U) goto fail;
    before = state;
    state.serial = UINT32_MAX;
    before = state;
    if (tecmo_gameplay_fixed_rng_nmi_tick(&state) ||
        memcmp(&state, &before, sizeof(state)) != 0) goto fail;
    memset(&state, 0, sizeof(state));
    if (!tecmo_gameplay_fixed_rng_live_checkpoint(&state, 1U, 2U, 3U))
        goto fail;
    before = state;
    if (tecmo_gameplay_fixed_rng_live_checkpoint(&state, 4U, 5U, 6U) ||
        memcmp(&state, &before, sizeof(state)) != 0) goto fail;
    for (size_t index = 0U; index < sizeof(state); ++index) {
        uint8_t *inside = (uint8_t *)&state + index;
        before = state;
        if (tecmo_gameplay_fixed_rng_c05d(
                &state, TECMO_GAMEPLAY_FIXED_RNG_CALL_A0DD, inside) ||
            memcmp(&state, &before, sizeof(state)) != 0) goto fail;
    }
    state.nmi_serial = UINT32_MAX;
    state.c05d_serial = UINT32_MAX;
    state.serial = UINT32_MAX;
    before = state;
    if (tecmo_gameplay_fixed_rng_nmi_tick(&state) ||
        memcmp(&state, &before, sizeof(state)) != 0) goto fail;
    if (message != NULL && message_size != 0U)
        (void)snprintf(message, message_size,
                       "TGFR-1 fixed RNG exhaustive=65536 NMI carry/fallback PASS");
    return true;
fail:
    if (message != NULL && message_size != 0U)
        (void)snprintf(message, message_size, "TGFR-1 fixed RNG failed");
    return false;
}
