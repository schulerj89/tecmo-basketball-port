#ifndef TECMO_GAMEPLAY_DEFENSE_CONTACT_H
#define TECMO_GAMEPLAY_DEFENSE_CONTACT_H

/*
 * Raw, bounded Rev. 1 evidence contracts only.
 *
 * These APIs preserve the byte-width arithmetic and the caller-visible RAM
 * addresses of the cited routines.  They do not expose a semantic result
 * vocabulary, do not call the native scene, and do not call the external
 * helper named by the raw-$17 tail.  The module is intentionally independent
 * of the ROM, asset packs, decompilation files, and runtime integration.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TECMO_GAMEPLAY_DEFENSE_CONTACT_B06_CANDIDATE_COUNT 10U
#define TECMO_GAMEPLAY_DEFENSE_CONTACT_B05_STATE17_SLOT_COUNT 10U

#define TECMO_GAMEPLAY_DEFENSE_CONTACT_B06_ROUTINE_CPU 0xB081U
#define TECMO_GAMEPLAY_DEFENSE_CONTACT_B06_FUNCTION_END_CPU 0xB103U
/* B104-$B108 is a separate wrapper/dispatcher observation, not B081 input. */
#define TECMO_GAMEPLAY_DEFENSE_CONTACT_B06_WRAPPER_CPU 0xB104U
#define TECMO_GAMEPLAY_DEFENSE_CONTACT_B06_THRESHOLD_HIGH 0x07U
#define TECMO_GAMEPLAY_DEFENSE_CONTACT_B06_GATE_MASK 0x10U

#define TECMO_GAMEPLAY_DEFENSE_CONTACT_B05_GEOMETRY_ROUTINE_CPU 0x9968U
#define TECMO_GAMEPLAY_DEFENSE_CONTACT_B05_GEOMETRY_END_CPU 0x999DU
#define TECMO_GAMEPLAY_DEFENSE_CONTACT_B05_GEOMETRY_RAW_X_LOW_MAX 0x0007U
#define TECMO_GAMEPLAY_DEFENSE_CONTACT_B05_GEOMETRY_RAW_X_HIGH_MIN 0xFFF8U
#define TECMO_GAMEPLAY_DEFENSE_CONTACT_B05_GEOMETRY_RAW_DEPTH_LOW_MAX 0x05U
#define TECMO_GAMEPLAY_DEFENSE_CONTACT_B05_GEOMETRY_RAW_DEPTH_HIGH_MIN 0xFAU

#define TECMO_GAMEPLAY_DEFENSE_CONTACT_B05_STATE17_ROUTINE_CPU 0x9A24U
#define TECMO_GAMEPLAY_DEFENSE_CONTACT_B05_STATE17_END_CPU 0x9A5FU
#define TECMO_GAMEPLAY_DEFENSE_CONTACT_B05_STATE17_HELPER_CPU 0xC042U
#define TECMO_GAMEPLAY_DEFENSE_CONTACT_B05_STATE17_HELPER_X 0x07U
#define TECMO_GAMEPLAY_DEFENSE_CONTACT_B05_STATE17_VALUE 0x17U

#define TECMO_GAMEPLAY_DEFENSE_CONTACT_B06_METRIC_INPUT_TAG 0x314D3642U
#define TECMO_GAMEPLAY_DEFENSE_CONTACT_B06_METRIC_RESULT_TAG 0x31523642U
#define TECMO_GAMEPLAY_DEFENSE_CONTACT_B06_SCAN_INPUT_TAG 0x31493642U
#define TECMO_GAMEPLAY_DEFENSE_CONTACT_B06_SCAN_RESULT_TAG 0x31533642U
#define TECMO_GAMEPLAY_DEFENSE_CONTACT_B05_GEOMETRY_INPUT_TAG 0x31493542U
#define TECMO_GAMEPLAY_DEFENSE_CONTACT_B05_GEOMETRY_RESULT_TAG 0x31523542U
#define TECMO_GAMEPLAY_DEFENSE_CONTACT_B05_STATE17_INPUT_TAG 0x31493742U
#define TECMO_GAMEPLAY_DEFENSE_CONTACT_B05_STATE17_RESULT_TAG 0x31523742U

#define TECMO_GAMEPLAY_DEFENSE_CONTACT_B06_RESULT_FLAG_IMPROVEMENT 0x01U
#define TECMO_GAMEPLAY_DEFENSE_CONTACT_B05_GEOMETRY_RESULT_FLAG_PASS 0x01U
#define TECMO_GAMEPLAY_DEFENSE_CONTACT_B05_GEOMETRY_RESULT_FLAG_X_BORROW 0x02U
#define TECMO_GAMEPLAY_DEFENSE_CONTACT_B05_GEOMETRY_RESULT_FLAG_DEPTH_BORROW 0x04U
#define TECMO_GAMEPLAY_DEFENSE_CONTACT_B05_STATE17_RESULT_FLAG_HELPER 0x01U
#define TECMO_GAMEPLAY_DEFENSE_CONTACT_B05_STATE17_RESULT_FLAG_NIBBLE 0x02U

/*
 * The metric helper models the B081 scratch pair $0094/$0095.  X arithmetic
 * first wraps the 16-bit subtraction and then applies the 6502 two-byte
 * absolute-value path.  Depth is an ordinary absolute byte difference.
 */
typedef struct TecmoGameplayDefenseContactB06MetricInput {
    uint32_t contract_tag;
    uint16_t routine_cpu;
    uint16_t raw_x_reference;
    uint16_t raw_x_entry;
    uint8_t raw_depth_reference;
    uint8_t raw_depth_entry;
} TecmoGameplayDefenseContactB06MetricInput;

typedef struct TecmoGameplayDefenseContactB06MetricResult {
    uint32_t contract_tag;
    uint16_t routine_cpu;
    uint8_t raw_0094;
    uint8_t raw_0095;
    uint16_t raw_metric;
} TecmoGameplayDefenseContactB06MetricResult;

/*
 * The scan input is a bounded ten-byte view of the raw tables addressed by
 * B081.  All pointed-to bytes are read-only.  Those read-only table views may
 * alias one another; the result object may not overlap the input object or
 * any pointed-to table view.
 *
 * raw_06d7 is the caller/stale low threshold byte.  B081 always overwrites
 * raw_06d8 with 7 on entry, while raw_06d5 and raw_037f_at_030b are preserved
 * until a strict improvement.  This contract performs exactly one descending
 * 9-to-0 pass and stops at RTS $B103.  The separate B104-$B108 wrapper can
 * dispatch one B081 pass when its own predicate is nonzero; that control-flow
 * observation is deliberately not an input, output, or synthetic second pass.
 */
typedef struct TecmoGameplayDefenseContactB06ScanInput {
    uint32_t contract_tag;
    uint16_t routine_cpu;
    uint8_t raw_007d;
    uint8_t raw_00f2;
    uint8_t raw_00fd;
    uint8_t raw_0309;
    uint8_t raw_030b;
    uint8_t raw_06d5;
    uint8_t raw_06d7;
    uint8_t raw_0094;
    uint8_t raw_0095;
    uint8_t raw_037f_at_030b;
    const uint8_t *raw_0073_low;
    size_t raw_0073_low_count;
    const uint8_t *raw_00e8_high;
    size_t raw_00e8_high_count;
    const uint8_t *raw_00f3_depth;
    size_t raw_00f3_depth_count;
    const uint8_t *raw_04b0_by_slot;
    size_t raw_04b0_by_slot_count;
} TecmoGameplayDefenseContactB06ScanInput;

typedef struct TecmoGameplayDefenseContactB06ScanResult {
    uint32_t contract_tag;
    uint16_t routine_cpu;
    uint8_t raw_flags;
    uint8_t raw_030b;
    uint8_t raw_0094;
    uint8_t raw_0095;
    uint8_t raw_06d5;
    uint8_t raw_06d7;
    uint8_t raw_06d8;
    uint8_t raw_037f_at_030b;
} TecmoGameplayDefenseContactB06ScanResult;

/* B05 $9968-$999D takes raw candidate/reference coordinate pairs.  The
 * result preserves wrapped deltas and the native X/depth borrow branches. */
typedef struct TecmoGameplayDefenseContactB05GeometryInput {
    uint32_t contract_tag;
    uint16_t routine_cpu;
    uint16_t raw_x_candidate;
    uint16_t raw_x_reference;
    uint8_t raw_depth_candidate;
    uint8_t raw_depth_reference;
} TecmoGameplayDefenseContactB05GeometryInput;

typedef struct TecmoGameplayDefenseContactB05GeometryResult {
    uint32_t contract_tag;
    uint16_t routine_cpu;
    uint16_t raw_x_delta;
    uint8_t raw_depth_delta;
    uint8_t raw_flags;
    uint8_t raw_gate;
} TecmoGameplayDefenseContactB05GeometryResult;

/*
 * B05 $9A24-$9A5F is represented as a plan.  raw_route_established is an
 * explicit caller-supplied raw context bit; the tail is not callable without
 * that bit.  The ten-byte $0754 view is read-only and may alias no mutable
 * result storage.  The plan records the $C042 request but never invokes it.
 * Its fixed slot count is owned by this B05 contract, independently of B06.
 */
typedef struct TecmoGameplayDefenseContactB05State17Input {
    uint32_t contract_tag;
    uint16_t routine_cpu;
    uint8_t raw_route_established;
    uint8_t raw_030b;
    const uint8_t *raw_0754_by_slot;
    size_t raw_0754_by_slot_count;
    uint8_t raw_030c_be;
    uint8_t raw_0588;
    uint8_t raw_ba;
    uint8_t raw_0458_bf;
} TecmoGameplayDefenseContactB05State17Input;

typedef struct TecmoGameplayDefenseContactB05State17Result {
    uint32_t contract_tag;
    uint16_t routine_cpu;
    uint16_t raw_helper_cpu;
    uint8_t raw_helper_x;
    uint8_t raw_state_value;
    uint8_t raw_flags;
    uint8_t raw_030b;
    uint8_t raw_030c_be;
    uint8_t raw_0754_before;
    uint8_t raw_0754_after;
    uint8_t raw_0588_before;
    uint8_t raw_0588_after;
    uint8_t raw_ba_before;
    uint8_t raw_ba_after;
    uint8_t raw_0478_after;
    uint8_t raw_0528_after;
    uint8_t raw_0743_after;
    uint8_t raw_0458_bf_before;
    uint8_t raw_0458_bf_after;
} TecmoGameplayDefenseContactB05State17Result;

bool tecmo_gameplay_defense_contact_b06_weighted_relative_metric(
    const TecmoGameplayDefenseContactB06MetricInput *input,
    TecmoGameplayDefenseContactB06MetricResult *result_out);

bool tecmo_gameplay_defense_contact_b06_candidate_scan_b081(
    const TecmoGameplayDefenseContactB06ScanInput *input,
    TecmoGameplayDefenseContactB06ScanResult *result_out);

bool tecmo_gameplay_defense_contact_b05_geometry_gate_9968(
    const TecmoGameplayDefenseContactB05GeometryInput *input,
    TecmoGameplayDefenseContactB05GeometryResult *result_out);

bool tecmo_gameplay_defense_contact_b05_state17_plan_9a24(
    const TecmoGameplayDefenseContactB05State17Input *input,
    TecmoGameplayDefenseContactB05State17Result *result_out);

#endif
