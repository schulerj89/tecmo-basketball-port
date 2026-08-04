param(
    [string]$ProjectRoot,
    [string]$RomPath
)

$ErrorActionPreference = "Stop"

if (!$ProjectRoot) {
    $ProjectRoot = Split-Path -Parent $PSScriptRoot
}
$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path
if (!$RomPath) {
    $RomPath = $env:TECMO_ROM_PATH
}
if (!$RomPath -or !(Test-Path -LiteralPath $RomPath -PathType Leaf)) {
    throw "Pass -RomPath or set TECMO_ROM_PATH to the local Rev1 iNES ROM."
}
$RomPath = (Resolve-Path -LiteralPath $RomPath).Path
$ExpectedRomSha256 =
    "076A6BEB273FAB39198C87AE6AF69F80AA548D6817753829F2C2BDE1F97475C4"
$ActualRomSha256 = (Get-FileHash -LiteralPath $RomPath -Algorithm SHA256).Hash.ToUpperInvariant()
if ($ActualRomSha256 -cne $ExpectedRomSha256) {
    throw "R2 defense/contact tests require the exact Tecmo NBA Basketball Rev1 ROM."
}

function Get-Fnv1a32 {
    param([byte[]]$Bytes)
    [uint32]$Hash = 2166136261
    foreach ($Byte in $Bytes) {
        [uint64]$Product = [uint64]($Hash -bxor [uint32]$Byte) *
            [uint64]16777619
        $Hash = [uint32]($Product % [uint64]4294967296)
    }
    return ("{0:X8}" -f $Hash)
}

function Get-Sha256Hex {
    param([byte[]]$Bytes)
    $Hasher = [Security.Cryptography.SHA256]::Create()
    try {
        return ([BitConverter]::ToString($Hasher.ComputeHash($Bytes))).
            Replace("-", "").ToUpperInvariant()
    } finally {
        $Hasher.Dispose()
    }
}

function Get-RomSpan {
    param(
        [byte[]]$RomBytes,
        [int]$PrgStart,
        [int]$PrgBankCount,
        [int]$Bank,
        [int]$CpuStart,
        [int]$CpuEnd
    )
    $PrgBankBytes = 0x4000
    $CpuBase = 0x8000
    if ($Bank -lt 0 -or $Bank -ge $PrgBankCount -or
        $CpuStart -lt $CpuBase -or $CpuEnd -lt $CpuStart -or
        $CpuEnd -ge ($CpuBase + $PrgBankBytes)) {
        throw ("Raw span {0:X4}-{1:X4} bank {2} is outside the " +
               "switchable iNES PRG mapping." -f $CpuStart, $CpuEnd, $Bank)
    }
    $Offset = $PrgStart + $Bank * $PrgBankBytes + ($CpuStart - $CpuBase)
    $Count = $CpuEnd - $CpuStart + 1
    $PrgEnd = $PrgStart + $PrgBankCount * $PrgBankBytes
    if ($Offset -lt $PrgStart -or $Count -le 0 -or
        $Offset -gt $PrgEnd -or $Count -gt $PrgEnd - $Offset -or
        $Offset -gt $RomBytes.Length -or
        $Count -gt $RomBytes.Length - $Offset) {
        throw ("Mapped raw span {0:X4}-{1:X4} bank {2} is out of file bounds." -f
               $CpuStart, $CpuEnd, $Bank)
    }
    $Data = New-Object byte[] $Count
    [Array]::Copy($RomBytes, $Offset, $Data, 0, $Count)
    return [pscustomobject]@{
        bank = $Bank
        cpu_start = $CpuStart
        cpu_end = $CpuEnd
        offset = $Offset
        count = $Count
        bytes = $Data
    }
}

function Get-HarnessSource {
    return @'
#include "tecmo_gameplay_defense_contact.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK_CASE(name, expression) \
    do { \
        if (!(expression)) { \
            printf("FAIL %s: %s\n", (name), #expression); \
            return 0; \
        } \
    } while (0)

static uint16_t oracle_x_absolute(uint16_t reference, uint16_t entry)
{
    uint32_t wrapped = (uint32_t)reference +
                       (reference < entry ? 0x10000U : 0U) -
                       (uint32_t)entry;
    if (reference < entry) {
        return (uint16_t)(0x10000U - wrapped);
    }
    return (uint16_t)wrapped;
}

static uint8_t oracle_depth_absolute(uint8_t reference, uint8_t entry)
{
    int difference = (int)reference - (int)entry;
    if (difference < 0) difference = -difference;
    return (uint8_t)difference;
}

static uint16_t oracle_metric(uint16_t raw_x_delta,
                              uint8_t raw_depth_delta)
{
    uint32_t maximum = raw_x_delta > (uint16_t)raw_depth_delta
                           ? (uint32_t)raw_x_delta
                           : (uint32_t)raw_depth_delta;
    uint32_t minimum = raw_x_delta > (uint16_t)raw_depth_delta
                           ? (uint32_t)raw_depth_delta
                           : (uint32_t)raw_x_delta;
    return (uint16_t)(maximum + minimum / 2U);
}

static uint16_t oracle_metric_from_coordinates(uint16_t reference_x,
                                               uint16_t entry_x,
                                               uint8_t reference_depth,
                                               uint8_t entry_depth)
{
    return oracle_metric(oracle_x_absolute(reference_x, entry_x),
                         oracle_depth_absolute(reference_depth, entry_depth));
}

static void oracle_scan_pass(
    const TecmoGameplayDefenseContactB06ScanInput *input,
    TecmoGameplayDefenseContactB06ScanResult *result)
{
    uint8_t cursor = TECMO_GAMEPLAY_DEFENSE_CONTACT_B06_CANDIDATE_COUNT;
    uint16_t reference_x = (uint16_t)input->raw_007d |
                           (uint16_t)((uint16_t)input->raw_00f2 << 8U);

    while (cursor > 0U) {
        uint8_t candidate;
        uint16_t entry_x;
        uint16_t metric;
        uint16_t threshold;

        --cursor;
        candidate = cursor;
        if (candidate == input->raw_0309 ||
            (input->raw_04b0_by_slot[candidate] & 0x10U) == 0U) {
            continue;
        }
        entry_x = (uint16_t)input->raw_0073_low[candidate] |
                  (uint16_t)((uint16_t)input->raw_00e8_high[candidate]
                             << 8U);
        metric = oracle_metric_from_coordinates(
            reference_x, entry_x, input->raw_00fd,
            input->raw_00f3_depth[candidate]);
        result->raw_0094 = (uint8_t)(metric & 0xFFU);
        result->raw_0095 = (uint8_t)(metric >> 8U);
        threshold = (uint16_t)result->raw_06d7 |
                    (uint16_t)((uint16_t)result->raw_06d8 << 8U);
        if (metric < threshold) {
            result->raw_06d7 = result->raw_0094;
            result->raw_06d8 = result->raw_0095;
            result->raw_06d5 = candidate;
            result->raw_037f_at_030b = candidate;
            result->raw_flags = (uint8_t)(result->raw_flags |
                                           TECMO_GAMEPLAY_DEFENSE_CONTACT_B06_RESULT_FLAG_IMPROVEMENT);
        }
    }
}

static void oracle_scan(
    const TecmoGameplayDefenseContactB06ScanInput *input,
    TecmoGameplayDefenseContactB06ScanResult *result)
{
    memset(result, 0, sizeof(*result));
    result->contract_tag = TECMO_GAMEPLAY_DEFENSE_CONTACT_B06_SCAN_RESULT_TAG;
    result->routine_cpu = TECMO_GAMEPLAY_DEFENSE_CONTACT_B06_ROUTINE_CPU;
    result->raw_030b = input->raw_030b;
    result->raw_0094 = input->raw_0094;
    result->raw_0095 = input->raw_0095;
    result->raw_06d5 = input->raw_06d5;
    result->raw_06d7 = input->raw_06d7;
    result->raw_06d8 =
        TECMO_GAMEPLAY_DEFENSE_CONTACT_B06_THRESHOLD_HIGH;
    result->raw_037f_at_030b = input->raw_037f_at_030b;
    oracle_scan_pass(input, result);
}

static void initialize_scan(
    TecmoGameplayDefenseContactB06ScanInput *input,
    const uint8_t *low,
    const uint8_t *high,
    const uint8_t *depth,
    const uint8_t *gate)
{
    memset(input, 0, sizeof(*input));
    input->contract_tag = TECMO_GAMEPLAY_DEFENSE_CONTACT_B06_SCAN_INPUT_TAG;
    input->routine_cpu = TECMO_GAMEPLAY_DEFENSE_CONTACT_B06_ROUTINE_CPU;
    input->raw_007d = 0x00U;
    input->raw_00f2 = 0x01U;
    input->raw_00fd = 100U;
    input->raw_0309 = 5U;
    input->raw_030b = 2U;
    input->raw_06d5 = 0xA6U;
    input->raw_06d7 = 0x34U;
    input->raw_0094 = 0xCCU;
    input->raw_0095 = 0xDDU;
    input->raw_037f_at_030b = 0xB7U;
    input->raw_0073_low = low;
    input->raw_0073_low_count = TECMO_GAMEPLAY_DEFENSE_CONTACT_B06_CANDIDATE_COUNT;
    input->raw_00e8_high = high;
    input->raw_00e8_high_count = TECMO_GAMEPLAY_DEFENSE_CONTACT_B06_CANDIDATE_COUNT;
    input->raw_00f3_depth = depth;
    input->raw_00f3_depth_count = TECMO_GAMEPLAY_DEFENSE_CONTACT_B06_CANDIDATE_COUNT;
    input->raw_04b0_by_slot = gate;
    input->raw_04b0_by_slot_count = TECMO_GAMEPLAY_DEFENSE_CONTACT_B06_CANDIDATE_COUNT;
}

static void set_candidate_x(uint8_t *low,
                            uint8_t *high,
                            uint8_t candidate,
                            uint16_t reference,
                            uint16_t delta)
{
    uint16_t entry = (uint16_t)(reference - delta);
    low[candidate] = (uint8_t)(entry & 0xFFU);
    high[candidate] = (uint8_t)(entry >> 8U);
}

static int test_metric(void)
{
    TecmoGameplayDefenseContactB06MetricInput input;
    TecmoGameplayDefenseContactB06MetricInput input_before;
    TecmoGameplayDefenseContactB06MetricResult result;
    TecmoGameplayDefenseContactB06MetricResult expected;
    TecmoGameplayDefenseContactB06MetricResult repeat;
    TecmoGameplayDefenseContactB06MetricResult result_before;
    uint16_t metric;

    memset(&input, 0, sizeof(input));
    input.contract_tag = TECMO_GAMEPLAY_DEFENSE_CONTACT_B06_METRIC_INPUT_TAG;
    input.routine_cpu = TECMO_GAMEPLAY_DEFENSE_CONTACT_B06_ROUTINE_CPU;
    input.raw_x_reference = 100U;
    input.raw_x_entry = 90U;
    input.raw_depth_reference = 20U;
    input.raw_depth_entry = 14U;
    memset(&expected, 0, sizeof(expected));
    expected.contract_tag = TECMO_GAMEPLAY_DEFENSE_CONTACT_B06_METRIC_RESULT_TAG;
    expected.routine_cpu = TECMO_GAMEPLAY_DEFENSE_CONTACT_B06_ROUTINE_CPU;
    metric = oracle_metric_from_coordinates(
        input.raw_x_reference, input.raw_x_entry,
        input.raw_depth_reference, input.raw_depth_entry);
    expected.raw_0094 = (uint8_t)(metric & 0xFFU);
    expected.raw_0095 = (uint8_t)(metric >> 8U);
    expected.raw_metric = metric;
    CHECK_CASE("B081 metric", tecmo_gameplay_defense_contact_b06_weighted_relative_metric(
        &input, &result));
    CHECK_CASE("B081 metric", memcmp(&result, &expected, sizeof(result)) == 0);
    repeat = result;
    CHECK_CASE("B081 metric", tecmo_gameplay_defense_contact_b06_weighted_relative_metric(
        &input, &result));
    CHECK_CASE("B081 metric", memcmp(&result, &repeat, sizeof(result)) == 0);

    /* Reversing both raw coordinate pairs in a non-wrapped case preserves
       the same absolute metric without assigning an orientation meaning. */
    input.raw_x_reference = 90U;
    input.raw_x_entry = 100U;
    input.raw_depth_reference = 14U;
    input.raw_depth_entry = 20U;
    metric = oracle_metric_from_coordinates(
        input.raw_x_reference, input.raw_x_entry,
        input.raw_depth_reference, input.raw_depth_entry);
    CHECK_CASE("B081 reversed-coordinate mirror", metric == repeat.raw_metric);
    CHECK_CASE("B081 reversed-coordinate mirror", tecmo_gameplay_defense_contact_b06_weighted_relative_metric(
        &input, &result));
    CHECK_CASE("B081 reversed-coordinate mirror", result.raw_metric ==
        repeat.raw_metric && result.raw_metric == metric);

    input.raw_x_reference = 0x0001U;
    input.raw_x_entry = 0xFFFFU;
    input.raw_depth_reference = 0xFFU;
    input.raw_depth_entry = 0x00U;
    metric = oracle_metric_from_coordinates(
        input.raw_x_reference, input.raw_x_entry,
        input.raw_depth_reference, input.raw_depth_entry);
    CHECK_CASE("B081 metric", metric == 0x007DU);
    CHECK_CASE("B081 metric", tecmo_gameplay_defense_contact_b06_weighted_relative_metric(
        &input, &result));
    CHECK_CASE("B081 metric", result.raw_metric == metric &&
        result.raw_0094 == 0x7DU && result.raw_0095 == 0x00U);

    memset(&result, 0xA5, sizeof(result));
    result_before = result;
    CHECK_CASE("B081 metric rollback", !tecmo_gameplay_defense_contact_b06_weighted_relative_metric(
        NULL, &result));
    CHECK_CASE("B081 metric rollback", memcmp(&result, &result_before,
                                              sizeof(result)) == 0);
    CHECK_CASE("B081 metric rollback", !tecmo_gameplay_defense_contact_b06_weighted_relative_metric(
        &input, NULL));
    input_before = input;
    input.contract_tag ^= 1U;
    CHECK_CASE("B081 metric rollback", !tecmo_gameplay_defense_contact_b06_weighted_relative_metric(
        &input, &result));
    CHECK_CASE("B081 metric rollback", memcmp(&result, &result_before,
                                              sizeof(result)) == 0);
    input = input_before;
    input.routine_cpu = (uint16_t)(TECMO_GAMEPLAY_DEFENSE_CONTACT_B06_ROUTINE_CPU + 1U);
    CHECK_CASE("B081 metric rollback", !tecmo_gameplay_defense_contact_b06_weighted_relative_metric(
        &input, &result));
    CHECK_CASE("B081 metric rollback", memcmp(&result, &result_before,
                                              sizeof(result)) == 0);
    input = input_before;
    input_before = input;
    CHECK_CASE("B081 metric rollback", !tecmo_gameplay_defense_contact_b06_weighted_relative_metric(
        &input, (TecmoGameplayDefenseContactB06MetricResult *)&input));
    CHECK_CASE("B081 metric rollback", memcmp(&input, &input_before,
                                              sizeof(input)) == 0);
    return 1;
}

static int test_scan(void)
{
    uint8_t low[TECMO_GAMEPLAY_DEFENSE_CONTACT_B06_CANDIDATE_COUNT];
    uint8_t high[TECMO_GAMEPLAY_DEFENSE_CONTACT_B06_CANDIDATE_COUNT];
    uint8_t depth[TECMO_GAMEPLAY_DEFENSE_CONTACT_B06_CANDIDATE_COUNT];
    uint8_t gate[TECMO_GAMEPLAY_DEFENSE_CONTACT_B06_CANDIDATE_COUNT];
    TecmoGameplayDefenseContactB06ScanInput input;
    TecmoGameplayDefenseContactB06ScanInput input_before;
    TecmoGameplayDefenseContactB06ScanResult result;
    TecmoGameplayDefenseContactB06ScanResult expected;
    TecmoGameplayDefenseContactB06ScanResult repeat;
    TecmoGameplayDefenseContactB06ScanResult result_before;
    uint16_t reference = 0x0100U;
    size_t index;

    memset(low, 0, sizeof(low));
    memset(high, 0, sizeof(high));
    memset(depth, 100, sizeof(depth));
    memset(gate, 0, sizeof(gate));
    initialize_scan(&input, low, high, depth, gate);
    CHECK_CASE("B081 stale no-match", tecmo_gameplay_defense_contact_b06_candidate_scan_b081(
        &input, &result));
    oracle_scan(&input, &expected);
    CHECK_CASE("B081 stale no-match", memcmp(&result, &expected,
                                              sizeof(result)) == 0);
    CHECK_CASE("B081 high threshold init", result.raw_06d8 == 0x07U);
    CHECK_CASE("B081 stale no-match", result.raw_06d5 == 0xA6U &&
        result.raw_037f_at_030b == 0xB7U && result.raw_06d7 == 0x34U);

    /* With high=7 and stale low=FF, 0x0800 is above the 0x07FF
       threshold. This rejects the old '<' versus '|' precedence bug,
       which would have treated the nonzero high byte as true. */
    memset(low, 0, sizeof(low));
    memset(high, 0, sizeof(high));
    memset(depth, 0, sizeof(depth));
    memset(gate, 0, sizeof(gate));
    gate[0] = 0x10U;
    input.raw_007d = 0x00U;
    input.raw_00f2 = 0x10U;
    input.raw_00fd = 0x00U;
    input.raw_0309 = 9U;
    input.raw_030b = 0U;
    input.raw_06d5 = 0xA6U;
    input.raw_06d7 = 0xFFU;
    input.raw_0094 = 0xCCU;
    input.raw_0095 = 0xDDU;
    input.raw_037f_at_030b = 0xB7U;
    set_candidate_x(low, high, 0U, 0x1000U, 0x0800U);
    CHECK_CASE("B081 threshold precedence oracle", tecmo_gameplay_defense_contact_b06_candidate_scan_b081(
        &input, &result));
    oracle_scan(&input, &expected);
    CHECK_CASE("B081 threshold precedence oracle", memcmp(&result, &expected,
                                                            sizeof(result)) == 0);
    CHECK_CASE("B081 threshold precedence oracle", result.raw_06d5 == 0xA6U &&
        result.raw_037f_at_030b == 0xB7U && result.raw_06d7 == 0xFFU &&
        result.raw_06d8 == 0x07U);

    input.raw_007d = 0x00U;
    input.raw_00f2 = 0x01U;
    input.raw_00fd = 100U;
    memset(depth, 100, sizeof(depth));
    gate[9] = 0x10U;
    gate[8] = 0x10U;
    set_candidate_x(low, high, 9U, reference, 100U);
    set_candidate_x(low, high, 8U, reference, 100U);
    input.raw_06d7 = 0xFFU;
    input.raw_0309 = 4U;
    input.raw_030b = 2U;
    CHECK_CASE("B081 strict tie", tecmo_gameplay_defense_contact_b06_candidate_scan_b081(
        &input, &result));
    oracle_scan(&input, &expected);
    CHECK_CASE("B081 strict tie", memcmp(&result, &expected,
                                          sizeof(result)) == 0);
    CHECK_CASE("B081 high-index tie retention", result.raw_06d5 == 9U &&
        result.raw_037f_at_030b == 9U && result.raw_06d7 == 100U &&
        result.raw_06d8 == 0U);
    CHECK_CASE("B081 equal tie same metric", oracle_metric_from_coordinates(
        reference, (uint16_t)low[8] |
        (uint16_t)((uint16_t)high[8] << 8U), input.raw_00fd, depth[8]) ==
        oracle_metric_from_coordinates(reference, (uint16_t)low[9] |
        (uint16_t)((uint16_t)high[9] << 8U), input.raw_00fd, depth[9]));

    memset(gate, 0, sizeof(gate));
    memset(low, 0, sizeof(low));
    memset(high, 0, sizeof(high));
    memset(depth, 100, sizeof(depth));
    gate[9] = 0x10U;
    gate[8] = 0x10U;
    gate[7] = 0x00U;
    gate[6] = 0x10U;
    gate[5] = 0x10U;
    gate[4] = 0x10U;
    gate[2] = 0x10U;
    set_candidate_x(low, high, 9U, reference, 100U);
    set_candidate_x(low, high, 8U, reference, 100U);
    set_candidate_x(low, high, 7U, reference, 1U);
    set_candidate_x(low, high, 6U, reference, 20U);
    set_candidate_x(low, high, 5U, reference, 1U);
    set_candidate_x(low, high, 4U, reference, 30U);
    set_candidate_x(low, high, 2U, reference, 10U);
    input.raw_0309 = 5U;
    input.raw_030b = 2U;
    input.raw_06d7 = 0xFFU;
    CHECK_CASE("B081 descending", tecmo_gameplay_defense_contact_b06_candidate_scan_b081(
        &input, &result));
    oracle_scan(&input, &expected);
    CHECK_CASE("B081 descending", memcmp(&result, &expected,
                                                  sizeof(result)) == 0);
    CHECK_CASE("B081 one pass final", result.raw_06d5 == 2U &&
        result.raw_037f_at_030b == 2U && result.raw_06d7 == 10U &&
        result.raw_06d8 == 0U);
    CHECK_CASE("B081 self skip", input.raw_0309 == 5U &&
        (gate[5] & TECMO_GAMEPLAY_DEFENSE_CONTACT_B06_GATE_MASK) != 0U &&
        oracle_metric_from_coordinates(reference,
            (uint16_t)low[5] | (uint16_t)((uint16_t)high[5] << 8U),
            input.raw_00fd, depth[5]) == 1U && result.raw_06d5 != 5U);
    CHECK_CASE("B081 bit-$10 gate", (gate[7] &
        TECMO_GAMEPLAY_DEFENSE_CONTACT_B06_GATE_MASK) == 0U &&
        oracle_metric_from_coordinates(reference,
            (uint16_t)low[7] | (uint16_t)((uint16_t)high[7] << 8U),
            input.raw_00fd, depth[7]) == 1U && result.raw_06d5 != 7U);
    CHECK_CASE("B081 output mirror", result.raw_06d5 == result.raw_037f_at_030b);

    memset(gate, 0, sizeof(gate));
    memset(low, 0, sizeof(low));
    memset(high, 0, sizeof(high));
    memset(depth, 100, sizeof(depth));
    gate[3] = 0x10U;
    input.raw_007d = 0x01U;
    input.raw_00f2 = 0x00U;
    input.raw_00fd = 0xFFU;
    input.raw_06d7 = 0x34U;
    input.raw_0309 = 9U;
    input.raw_030b = 3U;
    low[3] = 0xFFU;
    high[3] = 0xFFU;
    depth[3] = 0U;
    CHECK_CASE("B081 wrapped metric", tecmo_gameplay_defense_contact_b06_candidate_scan_b081(
        &input, &result));
    oracle_scan(&input, &expected);
    CHECK_CASE("B081 wrapped metric", memcmp(&result, &expected,
                                              sizeof(result)) == 0);
    CHECK_CASE("B081 wrapped metric", result.raw_06d7 == 0x7DU &&
        result.raw_06d8 == 0x00U && result.raw_06d5 == 3U);

    memset(gate, 0, sizeof(gate));
    memset(low, 0, sizeof(low));
    memset(high, 0, sizeof(high));
    memset(depth, 100, sizeof(depth));
    gate[0] = 0x10U;
    input.raw_007d = 0x00U;
    input.raw_00f2 = 0x10U;
    input.raw_00fd = 0U;
    input.raw_0309 = 9U;
    input.raw_030b = 0U;
    input.raw_06d7 = 0x64U;
    input.raw_0094 = 0x11U;
    input.raw_0095 = 0x22U;
    set_candidate_x(low, high, 0U, 0x1000U, 0x0764U);
    depth[0] = 0U;
    CHECK_CASE("B081 equal threshold", tecmo_gameplay_defense_contact_b06_candidate_scan_b081(
        &input, &result));
    oracle_scan(&input, &expected);
    CHECK_CASE("B081 equal threshold", memcmp(&result, &expected,
                                               sizeof(result)) == 0);
    CHECK_CASE("B081 equal threshold", result.raw_06d5 == 0xA6U &&
        result.raw_037f_at_030b == 0xB7U && result.raw_06d7 == 0x64U &&
        result.raw_06d8 == 0x07U);

    input_before = input;
    memset(&result, 0xA5, sizeof(result));
    result_before = result;
    CHECK_CASE("B081 rollback", !tecmo_gameplay_defense_contact_b06_candidate_scan_b081(
        NULL, &result));
    CHECK_CASE("B081 rollback", memcmp(&result, &result_before,
                                       sizeof(result)) == 0);
    CHECK_CASE("B081 rollback", !tecmo_gameplay_defense_contact_b06_candidate_scan_b081(
        &input, NULL));
    input = input_before;
    input.raw_0309 = TECMO_GAMEPLAY_DEFENSE_CONTACT_B06_CANDIDATE_COUNT;
    CHECK_CASE("B081 rollback", !tecmo_gameplay_defense_contact_b06_candidate_scan_b081(
        &input, &result));
    CHECK_CASE("B081 rollback", memcmp(&result, &result_before,
                                       sizeof(result)) == 0);
    input = input_before;
    input.raw_030b = 10U;
    CHECK_CASE("B081 rollback", !tecmo_gameplay_defense_contact_b06_candidate_scan_b081(
        &input, &result));
    CHECK_CASE("B081 rollback", memcmp(&result, &result_before,
                                       sizeof(result)) == 0);
    input = input_before;
    input.routine_cpu = (uint16_t)(TECMO_GAMEPLAY_DEFENSE_CONTACT_B06_ROUTINE_CPU + 1U);
    CHECK_CASE("B081 rollback", !tecmo_gameplay_defense_contact_b06_candidate_scan_b081(
        &input, &result));
    CHECK_CASE("B081 rollback", memcmp(&result, &result_before,
                                       sizeof(result)) == 0);
    input = input_before;
    input.raw_0073_low_count = 9U;
    CHECK_CASE("B081 rollback", !tecmo_gameplay_defense_contact_b06_candidate_scan_b081(
        &input, &result));
    CHECK_CASE("B081 rollback", memcmp(&result, &result_before,
                                       sizeof(result)) == 0);
    input = input_before;
    input.raw_00e8_high_count = 9U;
    CHECK_CASE("B081 rollback", !tecmo_gameplay_defense_contact_b06_candidate_scan_b081(
        &input, &result));
    CHECK_CASE("B081 rollback", memcmp(&result, &result_before,
                                       sizeof(result)) == 0);
    input = input_before;
    input.raw_00f3_depth_count = 9U;
    CHECK_CASE("B081 rollback", !tecmo_gameplay_defense_contact_b06_candidate_scan_b081(
        &input, &result));
    CHECK_CASE("B081 rollback", memcmp(&result, &result_before,
                                       sizeof(result)) == 0);
    input = input_before;
    input.raw_04b0_by_slot_count = 9U;
    CHECK_CASE("B081 rollback", !tecmo_gameplay_defense_contact_b06_candidate_scan_b081(
        &input, &result));
    CHECK_CASE("B081 rollback", memcmp(&result, &result_before,
                                       sizeof(result)) == 0);
    input = input_before;
    input.raw_04b0_by_slot = NULL;
    CHECK_CASE("B081 rollback", !tecmo_gameplay_defense_contact_b06_candidate_scan_b081(
        &input, &result));
    CHECK_CASE("B081 rollback", memcmp(&result, &result_before,
                                       sizeof(result)) == 0);
    input = input_before;
    input.raw_0073_low = NULL;
    CHECK_CASE("B081 rollback", !tecmo_gameplay_defense_contact_b06_candidate_scan_b081(
        &input, &result));
    CHECK_CASE("B081 rollback", memcmp(&result, &result_before,
                                       sizeof(result)) == 0);
    input = input_before;
    input.raw_00e8_high = NULL;
    CHECK_CASE("B081 rollback", !tecmo_gameplay_defense_contact_b06_candidate_scan_b081(
        &input, &result));
    CHECK_CASE("B081 rollback", memcmp(&result, &result_before,
                                       sizeof(result)) == 0);
    input = input_before;
    input.raw_00f3_depth = NULL;
    CHECK_CASE("B081 rollback", !tecmo_gameplay_defense_contact_b06_candidate_scan_b081(
        &input, &result));
    CHECK_CASE("B081 rollback", memcmp(&result, &result_before,
                                       sizeof(result)) == 0);
    input = input_before;
    input.contract_tag ^= 1U;
    CHECK_CASE("B081 rollback", !tecmo_gameplay_defense_contact_b06_candidate_scan_b081(
        &input, &result));
    CHECK_CASE("B081 rollback", memcmp(&result, &result_before,
                                       sizeof(result)) == 0);
    input = input_before;
    CHECK_CASE("B081 rollback", !tecmo_gameplay_defense_contact_b06_candidate_scan_b081(
        &input, (TecmoGameplayDefenseContactB06ScanResult *)&input));
    CHECK_CASE("B081 rollback", memcmp(&input, &input_before,
                                       sizeof(input)) == 0);
    input = input_before;
    input.raw_0073_low = (const uint8_t *)&result;
    input.raw_0073_low_count = TECMO_GAMEPLAY_DEFENSE_CONTACT_B06_CANDIDATE_COUNT;
    CHECK_CASE("B081 rollback", !tecmo_gameplay_defense_contact_b06_candidate_scan_b081(
        &input, &result));
    CHECK_CASE("B081 rollback", memcmp(&result, &result_before,
                                       sizeof(result)) == 0);
    input = input_before;
    for (index = 0U; index < sizeof(low); ++index) {
        low[index] = (uint8_t)index;
    }
    input.raw_0073_low = low;
    CHECK_CASE("B081 deterministic", tecmo_gameplay_defense_contact_b06_candidate_scan_b081(
        &input, &result));
    repeat = result;
    CHECK_CASE("B081 deterministic", tecmo_gameplay_defense_contact_b06_candidate_scan_b081(
        &input, &result));
    CHECK_CASE("B081 deterministic", memcmp(&result, &repeat,
                                             sizeof(result)) == 0);
    return 1;
}

static uint16_t oracle_geometry_x(uint16_t candidate,
                                  uint16_t reference,
                                  uint8_t *borrow_out)
{
    if (candidate < reference) {
        *borrow_out = 1U;
        return (uint16_t)(0x10000U -
                          ((uint32_t)reference - (uint32_t)candidate));
    }
    *borrow_out = 0U;
    return (uint16_t)(candidate - reference);
}

static uint8_t oracle_geometry_depth(uint8_t candidate,
                                     uint8_t reference,
                                     uint8_t *borrow_out)
{
    if (candidate < reference) {
        *borrow_out = 1U;
        return (uint8_t)(0x100U -
                         ((uint16_t)reference - (uint16_t)candidate));
    }
    *borrow_out = 0U;
    return (uint8_t)(candidate - reference);
}

static void oracle_geometry(uint16_t x_candidate,
                            uint16_t x_reference,
                            uint8_t depth_candidate,
                            uint8_t depth_reference,
                            uint16_t *x_delta_out,
                            uint8_t *depth_delta_out,
                            uint8_t *flags_out,
                            uint8_t *gate_out)
{
    uint8_t x_borrow;
    uint8_t depth_borrow;
    uint16_t x_delta = oracle_geometry_x(x_candidate, x_reference,
                                         &x_borrow);
    uint8_t depth_delta = oracle_geometry_depth(depth_candidate,
                                                depth_reference,
                                                &depth_borrow);
    uint8_t x_pass = (uint8_t)((!x_borrow && x_delta <= 0x0007U) ||
                               (x_borrow && x_delta >= 0xFFF8U));
    uint8_t depth_pass = (uint8_t)((!depth_borrow && depth_delta <= 0x05U) ||
                                   (depth_borrow && depth_delta >= 0xFAU));
    *x_delta_out = x_delta;
    *depth_delta_out = depth_delta;
    *flags_out = (uint8_t)((x_borrow ? 0x02U : 0U) |
                           (depth_borrow ? 0x04U : 0U));
    *gate_out = (uint8_t)(x_pass && depth_pass ? 0x01U : 0U);
}

static uint16_t oracle_x_offset(uint16_t base, int offset)
{
    return offset < 0 ? (uint16_t)(base - (uint16_t)(-offset))
                      : (uint16_t)(base + (uint16_t)offset);
}

static uint8_t oracle_depth_offset(uint8_t base, int offset)
{
    return offset < 0 ? (uint8_t)(base - (uint8_t)(-offset))
                      : (uint8_t)(base + (uint8_t)offset);
}

static int test_geometry(void)
{
    TecmoGameplayDefenseContactB05GeometryInput input;
    TecmoGameplayDefenseContactB05GeometryInput input_before;
    TecmoGameplayDefenseContactB05GeometryResult result;
    TecmoGameplayDefenseContactB05GeometryResult result_before;
    uint16_t expected_x_delta;
    uint8_t expected_depth_delta;
    uint8_t expected_flags;
    uint8_t expected_gate;
    int x;
    int depth;

    memset(&input, 0, sizeof(input));
    input.contract_tag = TECMO_GAMEPLAY_DEFENSE_CONTACT_B05_GEOMETRY_INPUT_TAG;
    input.routine_cpu = TECMO_GAMEPLAY_DEFENSE_CONTACT_B05_GEOMETRY_ROUTINE_CPU;
    for (x = -16; x <= 16; ++x) {
        for (depth = -16; depth <= 16; ++depth) {
            input.raw_x_reference = 0x4000U;
            input.raw_x_candidate = oracle_x_offset(0x4000U, x);
            input.raw_depth_reference = 0x40U;
            input.raw_depth_candidate = oracle_depth_offset(0x40U, depth);
            oracle_geometry(input.raw_x_candidate, input.raw_x_reference,
                            input.raw_depth_candidate,
                            input.raw_depth_reference, &expected_x_delta,
                            &expected_depth_delta, &expected_flags,
                            &expected_gate);
            CHECK_CASE("B05 geometry matrix", tecmo_gameplay_defense_contact_b05_geometry_gate_9968(
                &input, &result));
            CHECK_CASE("B05 geometry matrix", result.raw_x_delta ==
                expected_x_delta && result.raw_depth_delta ==
                expected_depth_delta && result.raw_flags == expected_flags &&
                result.raw_gate == expected_gate);
        }
    }

    /* The same wrapped X delta has opposite native carry/borrow outcomes. */
    input.raw_x_candidate = 0x0000U;
    input.raw_x_reference = 0x0001U;
    input.raw_depth_candidate = 0x40U;
    input.raw_depth_reference = 0x40U;
    CHECK_CASE("B05 geometry X borrow FFFF", tecmo_gameplay_defense_contact_b05_geometry_gate_9968(
        &input, &result));
    CHECK_CASE("B05 geometry X borrow FFFF", result.raw_x_delta == 0xFFFFU &&
        (result.raw_flags & 0x02U) != 0U && result.raw_gate == 1U);
    input.raw_x_candidate = 0xFFFFU;
    input.raw_x_reference = 0x0000U;
    CHECK_CASE("B05 geometry X no-borrow FFFF", tecmo_gameplay_defense_contact_b05_geometry_gate_9968(
        &input, &result));
    CHECK_CASE("B05 geometry X no-borrow FFFF", result.raw_x_delta == 0xFFFFU &&
        (result.raw_flags & 0x02U) == 0U && result.raw_gate == 0U);

    /* Depth has the same carry-sensitive distinction at raw $FF. */
    input.raw_x_candidate = 0x4000U;
    input.raw_x_reference = 0x4000U;
    input.raw_depth_candidate = 0x00U;
    input.raw_depth_reference = 0x01U;
    CHECK_CASE("B05 geometry depth borrow FF", tecmo_gameplay_defense_contact_b05_geometry_gate_9968(
        &input, &result));
    CHECK_CASE("B05 geometry depth borrow FF", result.raw_depth_delta == 0xFFU &&
        (result.raw_flags & 0x04U) != 0U && result.raw_gate == 1U);
    input.raw_depth_candidate = 0xFFU;
    input.raw_depth_reference = 0x00U;
    CHECK_CASE("B05 geometry depth no-borrow FF", tecmo_gameplay_defense_contact_b05_geometry_gate_9968(
        &input, &result));
    CHECK_CASE("B05 geometry depth no-borrow FF", result.raw_depth_delta == 0xFFU &&
        (result.raw_flags & 0x04U) == 0U && result.raw_gate == 0U);

    /* Explicit high-byte and mirrored boundary rejection vectors. */
    input.raw_x_candidate = 0x0100U;
    input.raw_x_reference = 0x0000U;
    input.raw_depth_candidate = 0x40U;
    input.raw_depth_reference = 0x40U;
    CHECK_CASE("B05 geometry X 0100 rejection", tecmo_gameplay_defense_contact_b05_geometry_gate_9968(
        &input, &result) && result.raw_gate == 0U);
    input.raw_x_candidate = 0x0107U;
    CHECK_CASE("B05 geometry X 0107 rejection", tecmo_gameplay_defense_contact_b05_geometry_gate_9968(
        &input, &result) && result.raw_gate == 0U);
    input.raw_x_candidate = 0xFEFFU;
    CHECK_CASE("B05 geometry X FEFF rejection", tecmo_gameplay_defense_contact_b05_geometry_gate_9968(
        &input, &result) && result.raw_gate == 0U);
    input.raw_x_candidate = 0x0000U;
    input.raw_x_reference = 0x0008U;
    CHECK_CASE("B05 geometry X FFF8 boundary", tecmo_gameplay_defense_contact_b05_geometry_gate_9968(
        &input, &result) && result.raw_x_delta == 0xFFF8U &&
        result.raw_gate == 1U);
    input.raw_x_reference = 0x0009U;
    CHECK_CASE("B05 geometry X FFF7 rejection", tecmo_gameplay_defense_contact_b05_geometry_gate_9968(
        &input, &result) && result.raw_x_delta == 0xFFF7U &&
        result.raw_gate == 0U);

    memset(&result, 0xA5, sizeof(result));
    result_before = result;
    CHECK_CASE("B05 geometry rollback", !tecmo_gameplay_defense_contact_b05_geometry_gate_9968(
        NULL, &result));
    CHECK_CASE("B05 geometry rollback", memcmp(&result, &result_before,
                                               sizeof(result)) == 0);
    CHECK_CASE("B05 geometry rollback", !tecmo_gameplay_defense_contact_b05_geometry_gate_9968(
        &input, NULL));
    input_before = input;
    input.contract_tag ^= 1U;
    CHECK_CASE("B05 geometry rollback", !tecmo_gameplay_defense_contact_b05_geometry_gate_9968(
        &input, &result));
    CHECK_CASE("B05 geometry rollback", memcmp(&result, &result_before,
                                               sizeof(result)) == 0);
    input = input_before;
    input.routine_cpu = (uint16_t)(TECMO_GAMEPLAY_DEFENSE_CONTACT_B05_GEOMETRY_ROUTINE_CPU + 1U);
    CHECK_CASE("B05 geometry rollback", !tecmo_gameplay_defense_contact_b05_geometry_gate_9968(
        &input, &result));
    CHECK_CASE("B05 geometry rollback", memcmp(&result, &result_before,
                                               sizeof(result)) == 0);
    input = input_before;
    CHECK_CASE("B05 geometry rollback", !tecmo_gameplay_defense_contact_b05_geometry_gate_9968(
        &input, (TecmoGameplayDefenseContactB05GeometryResult *)&input));
    CHECK_CASE("B05 geometry rollback", memcmp(&input, &input_before,
                                               sizeof(input)) == 0);
    return 1;
}

static int test_state17(void)
{
    uint8_t raw_0754[TECMO_GAMEPLAY_DEFENSE_CONTACT_B05_STATE17_SLOT_COUNT];
    uint8_t raw_0754_before[TECMO_GAMEPLAY_DEFENSE_CONTACT_B05_STATE17_SLOT_COUNT];
    TecmoGameplayDefenseContactB05State17Input input;
    TecmoGameplayDefenseContactB05State17Input input_before;
    TecmoGameplayDefenseContactB05State17Result result;
    TecmoGameplayDefenseContactB05State17Result expected;
    TecmoGameplayDefenseContactB05State17Result repeat;
    TecmoGameplayDefenseContactB05State17Result result_before;
    size_t index;

    for (index = 0U; index < sizeof(raw_0754); ++index)
        raw_0754[index] = (uint8_t)(index * 17U);
    memcpy(raw_0754_before, raw_0754, sizeof(raw_0754));
    memset(&input, 0, sizeof(input));
    input.contract_tag = TECMO_GAMEPLAY_DEFENSE_CONTACT_B05_STATE17_INPUT_TAG;
    input.routine_cpu = TECMO_GAMEPLAY_DEFENSE_CONTACT_B05_STATE17_ROUTINE_CPU;
    input.raw_route_established = 1U;
    input.raw_030b = 3U;
    input.raw_0754_by_slot = raw_0754;
    input.raw_0754_by_slot_count = sizeof(raw_0754);
    input.raw_030c_be = 1U;
    input.raw_0588 = 0xFFU;
    input.raw_ba = 0x01U;
    input.raw_0458_bf = 0xABU;
    CHECK_CASE("B05 raw-$17 plan", tecmo_gameplay_defense_contact_b05_state17_plan_9a24(
        &input, &result));
    memset(&expected, 0, sizeof(expected));
    expected.contract_tag = TECMO_GAMEPLAY_DEFENSE_CONTACT_B05_STATE17_RESULT_TAG;
    expected.routine_cpu = TECMO_GAMEPLAY_DEFENSE_CONTACT_B05_STATE17_ROUTINE_CPU;
    expected.raw_helper_cpu = 0xC042U;
    expected.raw_helper_x = 0x07U;
    expected.raw_state_value = 0x17U;
    expected.raw_flags = TECMO_GAMEPLAY_DEFENSE_CONTACT_B05_STATE17_RESULT_FLAG_HELPER |
                         TECMO_GAMEPLAY_DEFENSE_CONTACT_B05_STATE17_RESULT_FLAG_NIBBLE;
    expected.raw_030b = 3U;
    expected.raw_030c_be = 1U;
    expected.raw_0754_before = raw_0754[3];
    expected.raw_0754_after = (uint8_t)(raw_0754[3] + 1U);
    expected.raw_0588_before = 0xFFU;
    expected.raw_0588_after = 0xEFU;
    expected.raw_ba_before = 0x01U;
    expected.raw_ba_after = 0x81U;
    expected.raw_0478_after = 0x17U;
    expected.raw_0528_after = 0x17U;
    expected.raw_0743_after = 0U;
    expected.raw_0458_bf_before = 0xABU;
    expected.raw_0458_bf_after = 0xA5U;
    CHECK_CASE("B05 raw-$17 plan", memcmp(&result, &expected,
                                           sizeof(result)) == 0);
    CHECK_CASE("B05 raw-$17 plan", result.raw_helper_cpu == 0xC042U &&
        result.raw_helper_x == 7U && result.raw_state_value == 0x17U &&
        result.raw_0478_after == 0x17U && result.raw_0528_after == 0x17U);
    CHECK_CASE("B05 raw-$17 plan", memcmp(raw_0754, raw_0754_before,
                                           sizeof(raw_0754)) == 0);
    repeat = result;
    CHECK_CASE("B05 raw-$17 deterministic", tecmo_gameplay_defense_contact_b05_state17_plan_9a24(
        &input, &result));
    CHECK_CASE("B05 raw-$17 deterministic", memcmp(&result, &repeat,
                                                    sizeof(result)) == 0);

    input.raw_030c_be = 0U;
    input.raw_0458_bf = 0xA7U;
    CHECK_CASE("B05 raw-$17 conditional false", tecmo_gameplay_defense_contact_b05_state17_plan_9a24(
        &input, &result));
    CHECK_CASE("B05 raw-$17 conditional false", result.raw_0458_bf_after == 0xA7U &&
        (result.raw_flags & TECMO_GAMEPLAY_DEFENSE_CONTACT_B05_STATE17_RESULT_FLAG_NIBBLE) == 0U);
    input.raw_030b = 9U;
    raw_0754[9] = 0xFFU;
    CHECK_CASE("B05 raw-$17 counter wrap", tecmo_gameplay_defense_contact_b05_state17_plan_9a24(
        &input, &result) && result.raw_0754_before == 0xFFU &&
        result.raw_0754_after == 0U);

    memset(&result, 0xA5, sizeof(result));
    result_before = result;
    input_before = input;
    CHECK_CASE("B05 raw-$17 rollback", !tecmo_gameplay_defense_contact_b05_state17_plan_9a24(
        NULL, &result));
    CHECK_CASE("B05 raw-$17 rollback", memcmp(&result, &result_before,
                                               sizeof(result)) == 0);
    CHECK_CASE("B05 raw-$17 rollback", !tecmo_gameplay_defense_contact_b05_state17_plan_9a24(
        &input, NULL));
    CHECK_CASE("B05 raw-$17 rollback", memcmp(&input, &input_before,
                                               sizeof(input)) == 0);
    input.routine_cpu = (uint16_t)(TECMO_GAMEPLAY_DEFENSE_CONTACT_B05_STATE17_ROUTINE_CPU + 1U);
    CHECK_CASE("B05 raw-$17 rollback", !tecmo_gameplay_defense_contact_b05_state17_plan_9a24(
        &input, &result));
    CHECK_CASE("B05 raw-$17 rollback", memcmp(&result, &result_before,
                                               sizeof(result)) == 0);
    input = input_before;
    input.raw_route_established = 0U;
    CHECK_CASE("B05 raw-$17 rollback", !tecmo_gameplay_defense_contact_b05_state17_plan_9a24(
        &input, &result));
    CHECK_CASE("B05 raw-$17 rollback", memcmp(&result, &result_before,
                                               sizeof(result)) == 0);
    input = input_before;
    input.raw_route_established = 2U;
    CHECK_CASE("B05 raw-$17 rollback", !tecmo_gameplay_defense_contact_b05_state17_plan_9a24(
        &input, &result));
    CHECK_CASE("B05 raw-$17 rollback", memcmp(&result, &result_before,
                                               sizeof(result)) == 0);
    input = input_before;
    input.raw_030b = 10U;
    CHECK_CASE("B05 raw-$17 rollback", !tecmo_gameplay_defense_contact_b05_state17_plan_9a24(
        &input, &result));
    CHECK_CASE("B05 raw-$17 rollback", memcmp(&result, &result_before,
                                               sizeof(result)) == 0);
    input = input_before;
    input.raw_0754_by_slot_count = 9U;
    CHECK_CASE("B05 raw-$17 rollback", !tecmo_gameplay_defense_contact_b05_state17_plan_9a24(
        &input, &result));
    CHECK_CASE("B05 raw-$17 rollback", memcmp(&result, &result_before,
                                               sizeof(result)) == 0);
    input = input_before;
    input.raw_0754_by_slot = NULL;
    CHECK_CASE("B05 raw-$17 rollback", !tecmo_gameplay_defense_contact_b05_state17_plan_9a24(
        &input, &result));
    CHECK_CASE("B05 raw-$17 rollback", memcmp(&result, &result_before,
                                               sizeof(result)) == 0);
    input = input_before;
    input.contract_tag ^= 1U;
    CHECK_CASE("B05 raw-$17 rollback", !tecmo_gameplay_defense_contact_b05_state17_plan_9a24(
        &input, &result));
    CHECK_CASE("B05 raw-$17 rollback", memcmp(&result, &result_before,
                                               sizeof(result)) == 0);
    input = input_before;
    CHECK_CASE("B05 raw-$17 rollback", !tecmo_gameplay_defense_contact_b05_state17_plan_9a24(
        &input, (TecmoGameplayDefenseContactB05State17Result *)&input));
    CHECK_CASE("B05 raw-$17 rollback", memcmp(&input, &input_before,
                                               sizeof(input)) == 0);
    input = input_before;
    input.raw_0754_by_slot = (const uint8_t *)&result;
    input.raw_0754_by_slot_count = sizeof(raw_0754);
    CHECK_CASE("B05 raw-$17 rollback", !tecmo_gameplay_defense_contact_b05_state17_plan_9a24(
        &input, &result));
    CHECK_CASE("B05 raw-$17 rollback", memcmp(&result, &result_before,
                                               sizeof(result)) == 0);
    return 1;
}

int main(void)
{
    if (!test_metric() || !test_scan() || !test_geometry() || !test_state17())
        return 1;
    printf("R2 defense/contact raw tests passed: B081 metric+scan, B05 $9968 gate, B05 raw-$17 plan, transactional rollback, deterministic oracle\n");
    return 0;
}
'@
}

$RomBytes = [IO.File]::ReadAllBytes($RomPath)
if ($RomBytes.Length -ne 393232 -or $RomBytes.Length -lt 16) {
    throw "Rev1 ROM must have the canonical validated file size."
}
if ($RomBytes[0] -ne 0x4E -or $RomBytes[1] -ne 0x45 -or
    $RomBytes[2] -ne 0x53 -or $RomBytes[3] -ne 0x1A) {
    throw "Rev1 ROM does not have the iNES magic."
}
$Flags6 = [int]$RomBytes[6]
$Flags7 = [int]$RomBytes[7]
if (($Flags7 -band 0x0C) -eq 0x08) {
    throw "NES 2.0 layout is not supported by this legacy iNES mapper check."
}
$TrainerBytes = if (($Flags6 -band 0x04) -ne 0) { 0x0200 } else { 0 }
$PrgBankCount = [int]$RomBytes[4]
$ChrBankCount = [int]$RomBytes[5]
if ($PrgBankCount -ne 8) {
    throw "Rev1 ROM must declare exactly eight PRG banks for the accepted mapping."
}
$PrgStart = 16 + $TrainerBytes
$PrgBytes = $PrgBankCount * 0x4000
$ChrBytes = $ChrBankCount * 0x2000
$ExpectedImageBytes = 16 + $TrainerBytes + $PrgBytes + $ChrBytes
if ($ExpectedImageBytes -ne $RomBytes.Length) {
    throw "Rev1 ROM has unsupported trailing bytes or an inconsistent PRG/CHR layout."
}
$PrgEnd = $PrgStart + $PrgBytes
if ($PrgEnd -gt $RomBytes.Length -or $PrgEnd -gt $ExpectedImageBytes) {
    throw "Rev1 ROM PRG range is not bounded inside the validated iNES image."
}

$SpanSpecs = @(
    [pscustomobject]@{
        name = 'B06 $B081-$B108'
        bank = 6; start = 0xB081; end = 0xB108
        fnv = "87A88720"
        sha = "6BD687EABED16010B1DB4A0D81F532680DD3503F4757F3B5DCEA584A910CAF6D"
    },
    [pscustomobject]@{
        name = 'B05 $9968-$999E'
        bank = 5; start = 0x9968; end = 0x999E
        fnv = "FF699FE9"
        sha = "5F3742E3D833700C25811333B3B4B9FE737FFC2FB62414CDA9B1FCC206CEEBF3"
    },
    [pscustomobject]@{
        name = 'B05 $9A24-$9A5F'
        bank = 5; start = 0x9A24; end = 0x9A5F
        fnv = "953B37A4"
        sha = "1751F2A4AAC9A23A385BF172BC419260D7EFB20650B360FDB604DF67A7A5A66B"
    }
)
$Spans = @{}
foreach ($Spec in $SpanSpecs) {
    $Span = Get-RomSpan -RomBytes $RomBytes -PrgStart $PrgStart `
        -PrgBankCount $PrgBankCount -Bank $Spec.bank `
        -CpuStart $Spec.start -CpuEnd $Spec.end
    if ((Get-Fnv1a32 $Span.bytes) -cne $Spec.fnv -or
        (Get-Sha256Hex $Span.bytes) -cne $Spec.sha) {
        throw "Raw provenance fingerprint mismatch for $($Spec.name)."
    }
    $Spans[$Spec.name] = $Span
}

function Get-MappedByte {
    param([int]$Bank, [int]$Cpu)
    $Span = Get-RomSpan -RomBytes $RomBytes -PrgStart $PrgStart `
        -PrgBankCount $PrgBankCount -Bank $Bank -CpuStart $Cpu -CpuEnd $Cpu
    return [int]$Span.bytes[0]
}

if ((Get-MappedByte 6 0xB103) -ne 0x60 -or
    (Get-MappedByte 6 0xB104) -ne 0xAC -or
    (Get-MappedByte 6 0xB105) -ne 0x0B -or
    (Get-MappedByte 6 0xB106) -ne 0x03 -or
    (Get-MappedByte 6 0xB107) -ne 0xB9 -or
    (Get-MappedByte 6 0xB108) -ne 0x0C -or
    (Get-MappedByte 6 0xB109) -ne 0x03 -or
    (Get-MappedByte 5 0x999E) -ne 0xAD) {
    throw "Raw routine-boundary bytes did not match the documented Rev1 mapping."
}

function Assert-RomSpanRejected {
    param(
        [byte[]]$Bytes,
        [int]$PrgStartValue,
        [int]$PrgBankCountValue,
        [int]$BankValue,
        [int]$CpuStartValue,
        [int]$CpuEndValue
    )
    $Rejected = $false
    try {
        $null = Get-RomSpan -RomBytes $Bytes -PrgStart $PrgStartValue `
            -PrgBankCount $PrgBankCountValue -Bank $BankValue `
            -CpuStart $CpuStartValue -CpuEnd $CpuEndValue
    } catch {
        $Rejected = $true
    }
    if (!$Rejected) {
        throw ("Invalid iNES span was accepted: bank {0} {1:X4}-{2:X4}" -f
               $BankValue, $CpuStartValue, $CpuEndValue)
    }
}
Assert-RomSpanRejected -Bytes $RomBytes -PrgStartValue $PrgStart `
    -PrgBankCountValue $PrgBankCount -BankValue $PrgBankCount `
    -CpuStartValue 0x8000 -CpuEndValue 0x8000
Assert-RomSpanRejected -Bytes $RomBytes -PrgStartValue $PrgStart `
    -PrgBankCountValue $PrgBankCount -BankValue 5 `
    -CpuStartValue 0x7FFF -CpuEndValue 0x8000
Assert-RomSpanRejected -Bytes $RomBytes -PrgStartValue $PrgStart `
    -PrgBankCountValue $PrgBankCount -BankValue 5 `
    -CpuStartValue 0xBFFF -CpuEndValue 0xC000
Assert-RomSpanRejected -Bytes $RomBytes -PrgStartValue $PrgStart `
    -PrgBankCountValue $PrgBankCount -BankValue 5 `
    -CpuStartValue 0x9000 -CpuEndValue 0x8FFF

$BoundaryFiles = @(
    "include\tecmo_gameplay_cpu_steering.h",
    "src\tecmo_gameplay_cpu_steering.c",
    "include\tecmo_gameplay_penalties.h",
    "src\tecmo_gameplay_penalties.c",
    "include\tecmo_gameplay_shot_resolution.h",
    "src\tecmo_gameplay_shot_resolution.c",
    "include\tecmo_gameplay_live_foundation.h",
    "src\tecmo_gameplay_live_foundation.c",
    "include\tecmo_gameplay_pretip.h",
    "src\tecmo_gameplay_pretip.c"
)
$BoundaryHashes = @{}
foreach ($RelativePath in $BoundaryFiles) {
    $FullPath = Join-Path $ProjectRoot $RelativePath
    if (!(Test-Path -LiteralPath $FullPath -PathType Leaf)) {
        throw "Read-only boundary file is missing: $RelativePath"
    }
    $BoundaryHashes[$RelativePath] =
        (Get-FileHash -LiteralPath $FullPath -Algorithm SHA256).Hash
}
$DefenseHeader = Get-Content -Raw -LiteralPath `
    (Join-Path $ProjectRoot "include\tecmo_gameplay_defense_contact.h")
$DefenseSource = Get-Content -Raw -LiteralPath `
    (Join-Path $ProjectRoot "src\tecmo_gameplay_defense_contact.c")
if ($DefenseHeader -match "tecmo_gameplay_scene|tecmo_gameplay_audio|" +
    "tecmo_gameplay_penalties|tecmo_gameplay_shot_resolution" -or
    $DefenseSource -match "tecmo_gameplay_scene|tecmo_gameplay_audio|" +
    "tecmo_gameplay_penalties|tecmo_gameplay_shot_resolution") {
    throw "Raw foundation acquired a forbidden runtime/cross-contract dependency."
}
if ($DefenseHeader -match "raw_030c_by_slot|raw_pass_count|WRAPPER_RERUN" -or
    $DefenseSource -match "raw_030c_by_slot|raw_pass_count|WRAPPER_RERUN") {
    throw "B081 public/source contract retained synthetic wrapper-pass state."
}
$ScanPassCalls = [regex]::Matches(
    $DefenseSource, 'b06_scan_pass\s*\(\s*input\s*,\s*&result\s*\)')
if ($ScanPassCalls.Count -ne 1) {
    throw "B081 source must model exactly one scan-pass call."
}

$SemanticBoundaryChecks = @(
    [pscustomobject]@{
        path = "include\tecmo_gameplay_penalties.h"
        pattern = 'The API does not\s+\*\s+infer contact, collision, possession, or shooting state\.'
    },
    [pscustomobject]@{
        path = "include\tecmo_gameplay_shot_resolution.h"
        pattern = 'the claimant action as a rebound, steal, block, or recovery\.'
    },
    [pscustomobject]@{
        path = "include\tecmo_gameplay_live_foundation.h"
        pattern = 'not claims\s+\*\s+about the incomplete ROM dynamic candidate vector\.'
    },
    [pscustomobject]@{
        path = "docs\gameplay-cpu-steering.md"
        pattern = '`\$B081-\$B32E` candidate scan as ordinary movement targeting\.'
    },
    [pscustomobject]@{
        path = "docs\gameplay-state-foundation.md"
        pattern = 'candidate scan\s+remain outside this evidence boundary\.'
    }
)
foreach ($Assertion in $SemanticBoundaryChecks) {
    $AssertionPath = Join-Path $ProjectRoot $Assertion.path
    if (!(Test-Path -LiteralPath $AssertionPath -PathType Leaf)) {
        throw "Semantic boundary file is missing: $($Assertion.path)"
    }
    $AssertionText = Get-Content -Raw -LiteralPath $AssertionPath
    if ($AssertionText -notmatch $Assertion.pattern) {
        throw "Semantic boundary statement is missing: $($Assertion.path)"
    }
}

$TempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
$TempRoot = $TempRoot.TrimEnd([IO.Path]::DirectorySeparatorChar,
                               [IO.Path]::AltDirectorySeparatorChar)
$ScratchPrefix = "tecmo-gameplay-defense-contact-"
$Scratch = [IO.Path]::GetFullPath((Join-Path $TempRoot `
    ($ScratchPrefix + [Guid]::NewGuid().ToString("N"))))
function Test-SafeScratchPath {
    param(
        [string]$Candidate,
        [string]$ResolvedTempRoot,
        [string]$RequiredPrefix
    )
    try {
        $CandidateFull = [IO.Path]::GetFullPath($Candidate)
        $RootFull = [IO.Path]::GetFullPath($ResolvedTempRoot).TrimEnd(
            [IO.Path]::DirectorySeparatorChar,
            [IO.Path]::AltDirectorySeparatorChar)
    } catch {
        return $false
    }
    $RootBoundary = $RootFull + [IO.Path]::DirectorySeparatorChar
    $Leaf = [IO.Path]::GetFileName($CandidateFull)
    return $CandidateFull.StartsWith($RootBoundary,
                                     [StringComparison]::OrdinalIgnoreCase) -and
           $Leaf.StartsWith($RequiredPrefix,
                            [StringComparison]::Ordinal)
}
if (!(Test-SafeScratchPath -Candidate $Scratch -ResolvedTempRoot $TempRoot `
      -RequiredPrefix $ScratchPrefix)) {
    throw "Transient scratch path failed its temp-root/prefix safety invariant."
}
New-Item -ItemType Directory -Force -Path $Scratch | Out-Null
$HarnessPath = Join-Path $Scratch "defense_contact_harness.c"
$Executable = Join-Path $Scratch "defense_contact_harness.exe"
try {
    [IO.File]::WriteAllText($HarnessPath, (Get-HarnessSource),
                            [Text.Encoding]::ASCII)
    $VsWhere = Join-Path ([Environment]::GetEnvironmentVariable("ProgramFiles(x86)")) `
        "Microsoft Visual Studio\Installer\vswhere.exe"
    if (!(Test-Path -LiteralPath $VsWhere -PathType Leaf)) {
        throw "vswhere.exe was not found."
    }
    $VsPath = (& $VsWhere -latest -products * `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -property installationPath).Trim()
    if (!$VsPath) { throw "MSVC C tools were not found." }
    $VcVars = Join-Path $VsPath "VC\Auxiliary\Build\vcvars64.bat"
    if (!(Test-Path -LiteralPath $VcVars -PathType Leaf)) {
        throw "vcvars64.bat was not found."
    }
    $HeaderPath = Join-Path $ProjectRoot "include"
    $SourcePath = Join-Path $ProjectRoot "src\tecmo_gameplay_defense_contact.c"
    $CompileCommand =
        'call "' + $VcVars + '" >nul && cl /nologo /std:c11 /W4 /WX /TC /I"' +
        $HeaderPath + '" /Fe:"' + $Executable + '" "' + $HarnessPath +
        '" "' + $SourcePath + '"'
    $CompileOutput = @(& cmd.exe /d /c $CompileCommand 2>&1)
    if ($LASTEXITCODE -ne 0) {
        throw ("Focused MSVC /W4 /WX compile failed.`n" +
               ($CompileOutput -join [Environment]::NewLine))
    }
    if (!(Test-Path -LiteralPath $Executable -PathType Leaf)) {
        throw "Focused harness executable was not produced."
    }
    $RunOutput = @(& $Executable 2>&1)
    $RunExitCode = $LASTEXITCODE
    $RunText = ($RunOutput -join [Environment]::NewLine).Trim()
    if ($RunExitCode -ne 0 -or
        $RunText -ne
        "R2 defense/contact raw tests passed: B081 metric+scan, B05 `$9968 gate, B05 raw-`$17 plan, transactional rollback, deterministic oracle") {
        throw ("Focused raw harness failed.`n" + $RunText)
    }
    foreach ($RelativePath in $BoundaryFiles) {
        $Hash = (Get-FileHash -LiteralPath (Join-Path $ProjectRoot $RelativePath) `
                              -Algorithm SHA256).Hash
        if ($Hash -cne $BoundaryHashes[$RelativePath]) {
            throw "Read-only CPU/LIVE/TIP/TPNL/TGSR boundary changed during the focused run: $RelativePath"
        }
    }
    Write-Output "Focused isolated MSVC compile used /std:c11 /W4 /WX; no normal build integration."
    Write-Output ("R2 defense/contact tests passed: ROM size/SHA, iNES bank mapping, " +
                  "three raw-span FNV32/SHA fingerprints, B081 scan oracle, " +
                  "B05 `$9968 matrix, B05 raw-`$17 plan, rollback, repeatability, " +
                  "and read-only CPU/LIVE/TIP/TPNL/TGSR boundary checks.")
} finally {
    if (Test-Path -LiteralPath $Scratch) {
        if (!(Test-SafeScratchPath -Candidate $Scratch -ResolvedTempRoot $TempRoot `
              -RequiredPrefix $ScratchPrefix)) {
            throw "Refusing recursive cleanup outside the validated task scratch path."
        }
        Remove-Item -LiteralPath $Scratch -Recurse -Force
    }
}
