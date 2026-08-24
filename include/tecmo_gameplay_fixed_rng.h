#ifndef TECMO_GAMEPLAY_FIXED_RNG_H
#define TECMO_GAMEPLAY_FIXED_RNG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TECMO_GAMEPLAY_FIXED_RNG_TAG 0x52474654U

typedef enum TecmoGameplayFixedRngCallsite {
    TECMO_GAMEPLAY_FIXED_RNG_CALL_9FA1 = 1,
    TECMO_GAMEPLAY_FIXED_RNG_CALL_A0DD = 2
} TecmoGameplayFixedRngCallsite;

typedef struct TecmoGameplayFixedRng {
    uint32_t contract_tag;
    uint32_t serial;
    uint32_t nmi_serial;
    uint32_t c05d_serial;
    uint8_t raw_006a;
    uint8_t raw_0053;
    uint8_t raw_0054;
    uint8_t last_callsite;
    bool initialized;
    bool native_live_checkpoint;
} TecmoGameplayFixedRng;

/* This is a native LIVE continuity checkpoint, not a claim that PRETIP owns
 * the canonical global fixed-bank RNG stream before the handoff. */
bool tecmo_gameplay_fixed_rng_live_checkpoint(
    TecmoGameplayFixedRng *state, uint8_t raw_006a, uint8_t raw_0053,
    uint8_t raw_0054);

/* Fixed NMI `$CD7A->$CD8F->$CD9C`: increment 53 with carry into 54, then
 * advance the 6A LFSR. Rejection is byte-exact. */
bool tecmo_gameplay_fixed_rng_nmi_tick(TecmoGameplayFixedRng *state);

/* `$C05D->$CD96-$CDAB`: XOR 6A with 53 and apply the same LFSR advance.
 * The tag records which translated source caller consumed the returned byte. */
bool tecmo_gameplay_fixed_rng_c05d(
    TecmoGameplayFixedRng *state, TecmoGameplayFixedRngCallsite callsite,
    uint8_t *raw_006a_out);
bool tecmo_gameplay_fixed_rng_valid(const TecmoGameplayFixedRng *state);

bool tecmo_gameplay_fixed_rng_self_test(char *message, size_t message_size);

#endif
