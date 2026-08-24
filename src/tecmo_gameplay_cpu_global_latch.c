#include "tecmo_gameplay_cpu_global_latch.h"

#include <stdio.h>
#include <string.h>

static bool producer_valid(TecmoGameplayCpuGlobalLatchProducer producer)
{
    return producer >= TECMO_GAMEPLAY_CPU_GLOBAL_LATCH_PRODUCER_A0F3 &&
           producer <= TECMO_GAMEPLAY_CPU_GLOBAL_LATCH_PRODUCER_B783;
}

static bool latch_valid(const TecmoGameplayCpuGlobalLatch *latch)
{
    return latch != NULL &&
           latch->contract_tag == TECMO_GAMEPLAY_CPU_GLOBAL_LATCH_TAG &&
           ((latch->valid && producer_valid(latch->producer) &&
             latch->write_serial != 0U) ||
            (!latch->valid &&
             latch->producer == TECMO_GAMEPLAY_CPU_GLOBAL_LATCH_PRODUCER_NONE &&
             latch->raw_x_038d_038e == 0U &&
             latch->raw_depth_038f_0390 == 0U));
}

bool tecmo_gameplay_cpu_global_latch_init(TecmoGameplayCpuGlobalLatch *latch)
{
    static const TecmoGameplayCpuGlobalLatch virgin = {0};
    TecmoGameplayCpuGlobalLatch candidate;
    if (latch == NULL || memcmp(latch, &virgin, sizeof(*latch)) != 0) {
        return false;
    }
    candidate = virgin;
    candidate.contract_tag = TECMO_GAMEPLAY_CPU_GLOBAL_LATCH_TAG;
    *latch = candidate;
    return true;
}

bool tecmo_gameplay_cpu_global_latch_write(
    TecmoGameplayCpuGlobalLatch *latch,
    const TecmoGameplayCpuGlobalLatchWrite *write)
{
    TecmoGameplayCpuGlobalLatch candidate;
    if (!latch_valid(latch) || write == NULL ||
        write->contract_tag != TECMO_GAMEPLAY_CPU_GLOBAL_LATCH_WRITE_TAG ||
        !producer_valid(write->producer) ||
        write->expected_serial != latch->write_serial ||
        latch->write_serial == UINT32_MAX) {
        return false;
    }
    candidate = *latch;
    candidate.write_serial++;
    candidate.raw_x_038d_038e = write->raw_x_038d_038e;
    candidate.raw_depth_038f_0390 = write->raw_depth_038f_0390;
    candidate.producer = write->producer;
    candidate.valid = true;
    *latch = candidate;
    return true;
}

bool tecmo_gameplay_cpu_global_latch_full_reset(
    TecmoGameplayCpuGlobalLatch *latch,
    uint32_t expected_serial)
{
    TecmoGameplayCpuGlobalLatch candidate;
    if (!latch_valid(latch) || expected_serial != latch->write_serial ||
        latch->write_serial == UINT32_MAX) {
        return false;
    }
    candidate = *latch;
    candidate.write_serial++;
    candidate.raw_x_038d_038e = 0U;
    candidate.raw_depth_038f_0390 = 0U;
    candidate.producer = TECMO_GAMEPLAY_CPU_GLOBAL_LATCH_PRODUCER_NONE;
    candidate.valid = false;
    *latch = candidate;
    return true;
}

bool tecmo_gameplay_cpu_global_latch_retain_period(
    const TecmoGameplayCpuGlobalLatch *latch)
{
    return latch_valid(latch);
}

bool tecmo_gameplay_cpu_global_latch_retain_possession(
    const TecmoGameplayCpuGlobalLatch *latch)
{
    return latch_valid(latch);
}

bool tecmo_gameplay_cpu_global_latch_snapshot(
    const TecmoGameplayCpuGlobalLatch *latch,
    TecmoGameplayCpuGlobalLatchSnapshot *snapshot_out)
{
    TecmoGameplayCpuGlobalLatchSnapshot snapshot;
    if (!latch_valid(latch) || snapshot_out == NULL) return false;
    memset(&snapshot, 0, sizeof(snapshot));
    snapshot.contract_tag = TECMO_GAMEPLAY_CPU_GLOBAL_LATCH_SNAPSHOT_TAG;
    snapshot.write_serial = latch->write_serial;
    snapshot.raw_x_038d_038e = latch->raw_x_038d_038e;
    snapshot.raw_depth_038f_0390 = latch->raw_depth_038f_0390;
    snapshot.producer = latch->producer;
    snapshot.valid = latch->valid;
    *snapshot_out = snapshot;
    return true;
}

uint16_t tecmo_gameplay_cpu_global_latch_add_raw16(uint16_t base,
                                                   uint16_t offset)
{
    return (uint16_t)(base + offset);
}

uint16_t tecmo_gameplay_cpu_global_latch_delta_raw16(uint16_t target,
                                                     uint16_t actor)
{
    return (uint16_t)(target - actor);
}

bool tecmo_gameplay_cpu_global_latch_self_test(char *message,
                                               size_t message_size)
{
    TecmoGameplayCpuGlobalLatch latch;
    TecmoGameplayCpuGlobalLatch before;
    TecmoGameplayCpuGlobalLatch malformed;
    TecmoGameplayCpuGlobalLatchWrite write;
    TecmoGameplayCpuGlobalLatchSnapshot snapshot;
    TecmoGameplayCpuGlobalLatchSnapshot snapshot_before;
    unsigned producer;
    if (message == NULL || message_size == 0U) return false;
    memset(&latch, 0, sizeof(latch));
    if (!tecmo_gameplay_cpu_global_latch_init(&latch)) {
        (void)snprintf(message, message_size, "one-shot construction failed");
        return false;
    }
    before = latch;
    if (tecmo_gameplay_cpu_global_latch_init(&latch) ||
        memcmp(&latch, &before, sizeof(latch)) != 0 ||
        tecmo_gameplay_cpu_global_latch_init(NULL)) {
        (void)snprintf(message, message_size,
                       "initialized-latch reinit rejection failed");
        return false;
    }
    memset(&malformed, 0xA5, sizeof(malformed));
    before = malformed;
    if (tecmo_gameplay_cpu_global_latch_init(&malformed) ||
        memcmp(&malformed, &before, sizeof(malformed)) != 0) {
        (void)snprintf(message, message_size,
                       "malformed construction rollback failed");
        return false;
    }
    if (!tecmo_gameplay_cpu_global_latch_retain_period(&latch) ||
        !tecmo_gameplay_cpu_global_latch_retain_possession(&latch)) {
        (void)snprintf(message, message_size, "empty latch validation failed");
        return false;
    }
    memset(&write, 0, sizeof(write));
    write.contract_tag = TECMO_GAMEPLAY_CPU_GLOBAL_LATCH_WRITE_TAG;
    for (producer = TECMO_GAMEPLAY_CPU_GLOBAL_LATCH_PRODUCER_A0F3;
         producer <= TECMO_GAMEPLAY_CPU_GLOBAL_LATCH_PRODUCER_B783;
         ++producer) {
        write.expected_serial = latch.write_serial;
        write.raw_x_038d_038e = (uint16_t)(0xFF00U + producer);
        write.raw_depth_038f_0390 = (uint16_t)(0x00F0U + producer);
        write.producer = (TecmoGameplayCpuGlobalLatchProducer)producer;
        if (!tecmo_gameplay_cpu_global_latch_write(&latch, &write) ||
            latch.write_serial != producer ||
            latch.producer != write.producer ||
            latch.raw_x_038d_038e != write.raw_x_038d_038e ||
            latch.raw_depth_038f_0390 != write.raw_depth_038f_0390) {
            (void)snprintf(message, message_size,
                           "producer overwrite %u failed", producer);
            return false;
        }
    }
    before = latch;
    if (tecmo_gameplay_cpu_global_latch_init(&latch) ||
        memcmp(&latch, &before, sizeof(latch)) != 0) {
        (void)snprintf(message, message_size,
                       "populated-latch reinit rejection failed");
        return false;
    }
    write.expected_serial--;
    if (tecmo_gameplay_cpu_global_latch_init(&latch) ||
        tecmo_gameplay_cpu_global_latch_write(&latch, &write) ||
        memcmp(&latch, &before, sizeof(latch)) != 0) {
        (void)snprintf(message, message_size,
                       "stale writer rollback failed");
        return false;
    }
    write.expected_serial = latch.write_serial;
    write.producer = TECMO_GAMEPLAY_CPU_GLOBAL_LATCH_PRODUCER_NONE;
    if (tecmo_gameplay_cpu_global_latch_write(&latch, &write) ||
        memcmp(&latch, &before, sizeof(latch)) != 0) {
        (void)snprintf(message, message_size,
                       "malformed writer rollback failed");
        return false;
    }
    memset(&snapshot, 0xA5, sizeof(snapshot));
    if (!tecmo_gameplay_cpu_global_latch_snapshot(&latch, &snapshot) ||
        snapshot.contract_tag != TECMO_GAMEPLAY_CPU_GLOBAL_LATCH_SNAPSHOT_TAG ||
        snapshot.write_serial != latch.write_serial || !snapshot.valid ||
        snapshot.producer != latch.producer ||
        snapshot.raw_x_038d_038e != 0xFF05U ||
        snapshot.raw_depth_038f_0390 != 0x00F5U) {
        (void)snprintf(message, message_size, "snapshot failed");
        return false;
    }
    snapshot_before = snapshot;
    malformed = latch;
    malformed.contract_tag ^= 1U;
    if (tecmo_gameplay_cpu_global_latch_snapshot(&malformed, &snapshot) ||
        memcmp(&snapshot, &snapshot_before, sizeof(snapshot)) != 0) {
        (void)snprintf(message, message_size,
                       "malformed snapshot rollback failed");
        return false;
    }
    before = latch;
    if (!tecmo_gameplay_cpu_global_latch_retain_period(&latch) ||
        !tecmo_gameplay_cpu_global_latch_retain_possession(&latch) ||
        memcmp(&latch, &before, sizeof(latch)) != 0 ||
        memcmp(&snapshot, &snapshot_before, sizeof(snapshot)) != 0) {
        (void)snprintf(message, message_size, "retention mutated latch");
        return false;
    }
    if (tecmo_gameplay_cpu_global_latch_add_raw16(0xFFFFU, 2U) != 1U ||
        tecmo_gameplay_cpu_global_latch_add_raw16(0x00FFU, 1U) != 0x0100U ||
        tecmo_gameplay_cpu_global_latch_delta_raw16(0U, 1U) != 0xFFFFU ||
        tecmo_gameplay_cpu_global_latch_delta_raw16(0x0100U, 0x00FFU) != 1U) {
        (void)snprintf(message, message_size,
                       "raw16 carry/wrap arithmetic failed");
        return false;
    }
    if (!tecmo_gameplay_cpu_global_latch_full_reset(
            &latch, latch.write_serial) || latch.valid ||
        latch.producer != TECMO_GAMEPLAY_CPU_GLOBAL_LATCH_PRODUCER_NONE ||
        latch.raw_x_038d_038e != 0U || latch.raw_depth_038f_0390 != 0U ||
        latch.write_serial != 6U) {
        (void)snprintf(message, message_size, "full reset clear failed");
        return false;
    }
    before = latch;
    if (tecmo_gameplay_cpu_global_latch_full_reset(&latch, 5U) ||
        memcmp(&latch, &before, sizeof(latch)) != 0) {
        (void)snprintf(message, message_size, "reset rollback failed");
        return false;
    }
    latch.write_serial = UINT32_MAX;
    before = latch;
    write.expected_serial = UINT32_MAX;
    write.producer = TECMO_GAMEPLAY_CPU_GLOBAL_LATCH_PRODUCER_A0F3;
    if (tecmo_gameplay_cpu_global_latch_write(&latch, &write) ||
        tecmo_gameplay_cpu_global_latch_full_reset(&latch, UINT32_MAX) ||
        memcmp(&latch, &before, sizeof(latch)) != 0) {
        (void)snprintf(message, message_size, "serial overflow rollback failed");
        return false;
    }
    (void)snprintf(message, message_size,
                   "TGGL-1 global latch: producers=5 overwrite=atomic "
                   "retention=period+possession reset=full-only live=unbound");
    return true;
}
