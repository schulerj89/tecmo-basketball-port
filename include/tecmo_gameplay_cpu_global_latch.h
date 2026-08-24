#ifndef TECMO_GAMEPLAY_CPU_GLOBAL_LATCH_H
#define TECMO_GAMEPLAY_CPU_GLOBAL_LATCH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TECMO_GAMEPLAY_CPU_GLOBAL_LATCH_TAG 0x4C474354U
#define TECMO_GAMEPLAY_CPU_GLOBAL_LATCH_SNAPSHOT_TAG 0x53474354U
#define TECMO_GAMEPLAY_CPU_GLOBAL_LATCH_WRITE_TAG 0x57474354U

typedef enum TecmoGameplayCpuGlobalLatchProducer {
    TECMO_GAMEPLAY_CPU_GLOBAL_LATCH_PRODUCER_NONE = 0,
    TECMO_GAMEPLAY_CPU_GLOBAL_LATCH_PRODUCER_A0F3 = 1,
    TECMO_GAMEPLAY_CPU_GLOBAL_LATCH_PRODUCER_A790 = 2,
    TECMO_GAMEPLAY_CPU_GLOBAL_LATCH_PRODUCER_A9DA = 3,
    TECMO_GAMEPLAY_CPU_GLOBAL_LATCH_PRODUCER_B721 = 4,
    TECMO_GAMEPLAY_CPU_GLOBAL_LATCH_PRODUCER_B783 = 5
} TecmoGameplayCpuGlobalLatchProducer;

typedef struct TecmoGameplayCpuGlobalLatch {
    uint32_t contract_tag;
    uint32_t write_serial;
    uint16_t raw_x_038d_038e;
    uint16_t raw_depth_038f_0390;
    TecmoGameplayCpuGlobalLatchProducer producer;
    bool valid;
} TecmoGameplayCpuGlobalLatch;

typedef struct TecmoGameplayCpuGlobalLatchWrite {
    uint32_t contract_tag;
    uint32_t expected_serial;
    uint16_t raw_x_038d_038e;
    uint16_t raw_depth_038f_0390;
    TecmoGameplayCpuGlobalLatchProducer producer;
} TecmoGameplayCpuGlobalLatchWrite;

typedef struct TecmoGameplayCpuGlobalLatchSnapshot {
    uint32_t contract_tag;
    uint32_t write_serial;
    uint16_t raw_x_038d_038e;
    uint16_t raw_depth_038f_0390;
    TecmoGameplayCpuGlobalLatchProducer producer;
    bool valid;
} TecmoGameplayCpuGlobalLatchSnapshot;

/* Initializes the native representation of the fixed full-reset clear. */
void tecmo_gameplay_cpu_global_latch_init(
    TecmoGameplayCpuGlobalLatch *latch);

/* Atomic last-writer-wins update. The serial admission prevents a stale
 * producer capture from overwriting a newer event. Rejection is byte-exact. */
bool tecmo_gameplay_cpu_global_latch_write(
    TecmoGameplayCpuGlobalLatch *latch,
    const TecmoGameplayCpuGlobalLatchWrite *write);

/* The only clearing operation. It also uses serial admission and rejects at
 * UINT32_MAX rather than wrapping provenance. */
bool tecmo_gameplay_cpu_global_latch_full_reset(
    TecmoGameplayCpuGlobalLatch *latch,
    uint32_t expected_serial);

/* These transitions validate the typed latch and deliberately retain every
 * byte. They are separate so tests cannot confuse retention with omission. */
bool tecmo_gameplay_cpu_global_latch_retain_period(
    const TecmoGameplayCpuGlobalLatch *latch);
bool tecmo_gameplay_cpu_global_latch_retain_possession(
    const TecmoGameplayCpuGlobalLatch *latch);

/* Produces an immutable value snapshot; it does not authorize a LIVE opcode
 * 13 input. Production remains fail-closed until every upstream scheduler and
 * gate in the opcode-13 call chain has a typed owner. */
bool tecmo_gameplay_cpu_global_latch_snapshot(
    const TecmoGameplayCpuGlobalLatch *latch,
    TecmoGameplayCpuGlobalLatchSnapshot *snapshot_out);

/* Exact wrapping arithmetic shared by the five producers/consumer where the
 * raw workspaces are already known. These are pure evidence helpers only. */
uint16_t tecmo_gameplay_cpu_global_latch_add_raw16(uint16_t base,
                                                   uint16_t offset);
uint16_t tecmo_gameplay_cpu_global_latch_delta_raw16(uint16_t target,
                                                     uint16_t actor);

bool tecmo_gameplay_cpu_global_latch_self_test(char *message,
                                               size_t message_size);

#endif
