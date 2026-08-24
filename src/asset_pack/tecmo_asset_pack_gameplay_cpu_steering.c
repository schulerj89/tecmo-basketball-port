#include "tecmo_asset_pack_gameplay_cpu_steering.h"

#include "tecmo_asset_pack_gameplay_movement.h"
#include "tecmo_asset_pack_import_layout.h"
#include "tecmo_asset_pack_util.h"

#include <stdlib.h>
#include <string.h>

#define CPU_STEERING_PRG_BANK_COUNT 8U
#define CPU_STEERING_CHR_BANK_COUNT 32U
#define CPU_STEERING_FIXED_CPU_BASE 0xC000U
#define CPU_STEERING_REV1_ROM_SIZE 393232U
#define CPU_STEERING_REV1_ROM_FNV1A32 0x0650F5B0U
#define CPU_STEERING_COMMAND_BASE_CPU 0x9F2EU

static const uint8_t cpu_steering_rev1_ines_header[16] = {
    'N','E','S',0x1AU,0x08U,0x20U,0x42U,0x00U,
    0x00U,0x00U,0x00U,0x00U,0x00U,0x00U,0x00U,0x00U
};

static const uint8_t cpu_steering_rev1_sha256[32] = {
    0x07U,0x6AU,0x6BU,0xEBU,0x27U,0x3FU,0xABU,0x39U,
    0x19U,0x8CU,0x87U,0xAEU,0x6AU,0xF6U,0x9FU,0x80U,
    0xAAU,0x54U,0x8DU,0x68U,0x17U,0x75U,0x38U,0x29U,
    0xF2U,0xC2U,0xBDU,0xE1U,0xF9U,0x74U,0x75U,0xC4U
};

static const uint16_t cpu_steering_handler_cpu[
    TECMO_GAMEPLAY_CPU_STEERING_OPCODE_COUNT] = {
    0x90E0U,0x934BU,0x9280U,0x905EU,0x8FFAU,0x8F92U,
    0x8F2DU,0x8F12U,0x8ED7U,0x8FC5U,0x8CD0U,0x8C40U,
    0x8E4FU,0x9125U,0x9146U,0x9172U,0x9085U,0x8C1AU,
    0x8C1AU,0x8C1AU,0x9032U,0x8BF6U,0x8BE1U,0x8F72U
};

static const uint8_t cpu_steering_direction_map[
    TECMO_GAMEPLAY_CPU_STEERING_DIRECTION_COUNT] = {
    3U,6U,4U,7U,0U,1U,2U,5U
};

/* These native lifecycle consumers use bytes outside the retained TGAI-3
   payload spans. The importer validates their canonical Rev1 source ranges;
   runtime code receives only the semantic constants derived from them. */
static const uint8_t cpu_steering_anchor_ac76_acf0_sha256[32] = {
    0xAAU,0x29U,0x6CU,0xBBU,0xF2U,0x26U,0x91U,0x30U,
    0xF1U,0x3CU,0x8DU,0x69U,0x83U,0xD8U,0x97U,0x45U,
    0x17U,0x71U,0x0BU,0x9AU,0x06U,0x41U,0xA6U,0xE9U,
    0x77U,0x0EU,0x50U,0x43U,0x8EU,0x07U,0xA2U,0x0AU
};

static const uint8_t cpu_steering_anchor_acd9_ace3_sha256[32] = {
    0x47U,0x61U,0xCFU,0x44U,0x14U,0x82U,0x47U,0xC6U,
    0xB9U,0x60U,0x46U,0xAEU,0x8FU,0xA2U,0xA9U,0xB8U,
    0x99U,0xBCU,0xDDU,0x2AU,0x3BU,0xCBU,0x2AU,0xD6U,
    0x1EU,0xFAU,0x3BU,0xEBU,0xBAU,0xD9U,0x41U,0x4DU
};

static const uint8_t cpu_steering_anchor_add6_addf_sha256[32] = {
    0x71U,0x0EU,0x20U,0x6AU,0x0EU,0x4AU,0x69U,0x19U,
    0xA8U,0x32U,0x3EU,0x87U,0xF4U,0x0DU,0x89U,0x1BU,
    0x73U,0xF8U,0xFBU,0xC2U,0x04U,0xEAU,0x28U,0x6CU,
    0xE7U,0x5DU,0xE5U,0xEDU,0x75U,0x44U,0x01U,0x55U
};

static const uint8_t cpu_steering_anchor_b05_route_sha256[32] = {
    0x30U,0x77U,0x15U,0xF2U,0x1DU,0x95U,0xCEU,0xEBU,
    0x50U,0x33U,0xEDU,0xD4U,0xDDU,0x77U,0xBEU,0x66U,
    0x52U,0x15U,0xE5U,0xF2U,0x99U,0x36U,0x63U,0xD9U,
    0xABU,0x81U,0xB1U,0x7AU,0x50U,0xD4U,0x0AU,0x48U
};

static const uint8_t cpu_steering_anchor_b06_shot_sha256[32] = {
    0x0EU,0x34U,0xFEU,0xFAU,0xC7U,0xDCU,0x76U,0x7BU,
    0x0AU,0x02U,0x86U,0xFDU,0x3BU,0xD7U,0xA8U,0x49U,
    0xA2U,0x49U,0x5DU,0x24U,0xF0U,0x03U,0xA1U,0x8DU,
    0xABU,0xFBU,0x18U,0x6FU,0x9BU,0xB4U,0x98U,0x1FU
};

static const uint8_t cpu_steering_anchor_b06_candidate_sha256[32] = {
    0xAAU,0xA9U,0x67U,0x0DU,0xA5U,0x94U,0x2FU,0xA2U,
    0x61U,0x4FU,0x92U,0x5AU,0x26U,0x66U,0x74U,0x89U,
    0x3AU,0x35U,0x2BU,0xB2U,0xDBU,0x3AU,0x8FU,0x41U,
    0x58U,0xF6U,0x1CU,0x8AU,0xE8U,0x91U,0xAEU,0x36U
};

/* The planar route arithmetic uses the exact Bank06 signed 16-by-16 divide
   helper without retaining another copy in TGAI. Keep the helper revision
   locked independently of the caller spans. */
static const uint8_t cpu_steering_anchor_b06_divide_sha256[32] = {
    0xF6U,0x21U,0x6FU,0xECU,0xFCU,0x69U,0x71U,0x14U,
    0xA2U,0xC8U,0x08U,0x3DU,0x8CU,0xECU,0xC7U,0x6BU,
    0xBAU,0x1EU,0x5EU,0xD6U,0x08U,0xA0U,0x57U,0x83U,
    0x69U,0x78U,0x58U,0x21U,0x53U,0x97U,0x9EU,0xD2U
};

/* Opcode-15's helper and handler are consumed only by the explicit raw
   harness. These anchors are separate from the broad retained command span:
   they prevent a compatible-looking surrounding payload from silently
   changing the exact branch or $88B0 pose/action contract. */
static const uint8_t cpu_steering_anchor_opcode15_helper_sha256[32] = {
    0x3BU,0x39U,0x4BU,0xE6U,0x4BU,0x0FU,0x27U,0xB2U,
    0x51U,0xBEU,0xE5U,0x55U,0x4DU,0x2DU,0xCCU,0xC8U,
    0xFAU,0x9EU,0xBDU,0x0EU,0xEEU,0x3AU,0x5AU,0x55U,
    0xEBU,0xE1U,0xF5U,0xF3U,0xBCU,0xCBU,0x99U,0x40U
};

static const uint8_t cpu_steering_anchor_opcode15_handler_sha256[32] = {
    0x3BU,0xECU,0x82U,0x71U,0x6EU,0x43U,0xD2U,0x3CU,
    0xBEU,0xAAU,0x3FU,0xB4U,0x31U,0xB1U,0x52U,0xB6U,
    0x5AU,0x31U,0x42U,0x6BU,0x65U,0xE6U,0x25U,0xE2U,
    0x74U,0x07U,0x6EU,0xD1U,0x41U,0x92U,0xC7U,0x94U
};

/* This is intentionally an overlapping semantic anchor, not an eleventh
   imported TGAI source span. The broad Bank06 handlers source retains the
   bytes; this verifies the Rev1-only tail that the lifted listing omits. */
static const uint8_t cpu_steering_anchor_opcode15_tail_sha256[32] = {
    0x3BU,0x13U,0x8EU,0xD7U,0x10U,0x99U,0xF3U,0x03U,
    0x9FU,0x13U,0xC6U,0x61U,0xDDU,0x77U,0x42U,0xF2U,
    0x88U,0xDAU,0xDCU,0x54U,0xB2U,0x96U,0x48U,0xA5U,
    0x47U,0xD6U,0xAFU,0x31U,0x5CU,0xDFU,0xADU,0x72U
};

static const uint8_t cpu_steering_anchor_opcode15_dispatch_sha256[32] = {
    0x90U,0x63U,0xB5U,0x8CU,0x6AU,0x27U,0x00U,0xB8U,
    0x09U,0x95U,0xA7U,0xF5U,0x3BU,0x9EU,0xF4U,0xE6U,
    0xF1U,0xB6U,0x4DU,0x30U,0xE2U,0xE5U,0x81U,0x31U,
    0x35U,0xDCU,0xE6U,0xE6U,0xCDU,0x0BU,0xBBU,0xC2U
};

static const uint8_t cpu_steering_anchor_opcode15_record_sha256[32] = {
    0xB7U,0xBEU,0x62U,0x4DU,0xCBU,0x1DU,0xFDU,0xACU,
    0xD7U,0x84U,0xF4U,0xA7U,0x96U,0x98U,0xEEU,0x22U,
    0xC0U,0x42U,0x2AU,0x16U,0x63U,0x67U,0xF2U,0xC5U,
    0xC9U,0x27U,0xF0U,0x1BU,0x37U,0xA2U,0xD9U,0x3EU
};

static const uint8_t cpu_steering_anchor_opcode15_primary_sha256[6][32] = {
    {0xC7U,0x33U,0x9EU,0xB1U,0xEAU,0xDDU,0x5FU,0xA6U,0x15U,0x4DU,0x57U,0x06U,0x23U,0x19U,0x2BU,0x35U,0xBFU,0x89U,0x14U,0xD8U,0x94U,0x71U,0x33U,0x9CU,0x34U,0x70U,0xB4U,0x35U,0xDFU,0x66U,0xCFU,0x90U},
    {0x40U,0x8AU,0xDFU,0xA5U,0xDEU,0x40U,0x08U,0x93U,0xB1U,0x19U,0x78U,0x20U,0xEBU,0x91U,0xE0U,0x90U,0x34U,0x36U,0x91U,0x0DU,0x0FU,0x0BU,0x7CU,0xE7U,0xD2U,0x8AU,0x56U,0x87U,0xEBU,0x48U,0xC9U,0x91U},
    {0x82U,0x71U,0x14U,0x3FU,0xB8U,0x37U,0x14U,0x6AU,0xA1U,0xD7U,0xB7U,0xDDU,0x42U,0x32U,0x75U,0x99U,0xB5U,0xCAU,0xBCU,0x08U,0xA3U,0x45U,0x44U,0x7BU,0x4AU,0x13U,0x35U,0x3BU,0xFFU,0xF1U,0xE1U,0xBFU},
    {0x28U,0x67U,0xE9U,0xCDU,0xC7U,0x3DU,0x46U,0x3FU,0x61U,0xE2U,0x1EU,0x66U,0x0BU,0x73U,0xB3U,0x41U,0xA3U,0x77U,0xE6U,0xF0U,0xE7U,0x02U,0x33U,0x6AU,0x51U,0xCBU,0xEAU,0x4DU,0x4BU,0xAFU,0x06U,0x97U},
    {0xA0U,0x64U,0xF5U,0xF0U,0x3DU,0xE6U,0x82U,0xD0U,0x93U,0x10U,0x90U,0x78U,0x3FU,0xC5U,0x15U,0x31U,0x51U,0xD2U,0xD2U,0x2DU,0xA0U,0x07U,0xF0U,0x63U,0x2FU,0xEFU,0x15U,0xB9U,0x6EU,0x87U,0x39U,0x16U},
    {0x64U,0xF7U,0x85U,0xB2U,0xF5U,0xFDU,0x6CU,0x49U,0xE4U,0xBDU,0x67U,0x00U,0xF7U,0xDDU,0x35U,0xD3U,0x70U,0x8DU,0xDFU,0xA4U,0x82U,0xF1U,0x0FU,0xEEU,0x8FU,0x48U,0x77U,0xD8U,0x5BU,0x7EU,0x99U,0x27U}
};

/* Persistent `$038D-$0390` latch provenance. These are semantic anchors, not
   copied TGAI payload: five Bank05 last-writer families, the Bank06 opcode-13
   consumer, and the two halves of the fixed full-reset/page-clear routine. */
static const uint8_t cpu_steering_anchor_global_latch_sha256[8][32] = {
    {0x65U,0x1CU,0x28U,0x50U,0x57U,0xD2U,0x56U,0xF2U,
     0x1FU,0xB4U,0x42U,0x02U,0x19U,0x68U,0xFEU,0x52U,
     0xFAU,0x7EU,0x44U,0x60U,0xF5U,0xACU,0x44U,0x34U,
     0x25U,0x8EU,0xEBU,0xF3U,0x79U,0x0FU,0x92U,0x19U},
    {0x25U,0xF2U,0xDFU,0xFFU,0xF7U,0x3AU,0x39U,0xC1U,
     0x9CU,0x8DU,0x19U,0x19U,0x65U,0xAEU,0x70U,0xA9U,
     0x93U,0xC4U,0x77U,0x81U,0xEEU,0x07U,0x59U,0x5EU,
     0x16U,0x6EU,0xC6U,0x09U,0xD3U,0x8FU,0xE9U,0x32U},
    {0x6FU,0x0CU,0x64U,0x4BU,0xDAU,0x3FU,0x2FU,0xBFU,
     0xF6U,0x50U,0xD0U,0xEAU,0xDCU,0xB6U,0xE0U,0x08U,
     0x58U,0x4EU,0x74U,0x71U,0xD5U,0xCFU,0xD8U,0x79U,
     0x89U,0x41U,0xAEU,0xFBU,0x24U,0x58U,0xB2U,0x18U},
    {0xE4U,0xD5U,0xB8U,0x85U,0xDCU,0xC6U,0xA7U,0x83U,
     0xD1U,0x21U,0x4FU,0x76U,0xBEU,0x95U,0x56U,0x2EU,
     0x55U,0xC7U,0x9CU,0xC0U,0x6AU,0xBCU,0xC2U,0x24U,
     0x6CU,0x37U,0xBBU,0xA5U,0xEFU,0xB2U,0xECU,0xB8U},
    {0xADU,0xBEU,0x42U,0xA1U,0xEBU,0x93U,0x5DU,0x5AU,
     0x4DU,0x85U,0x89U,0x2AU,0x6FU,0xC0U,0x4BU,0x75U,
     0xA1U,0xAFU,0x28U,0x73U,0x9DU,0x17U,0xA7U,0x9BU,
     0xECU,0x9BU,0xE8U,0xF6U,0xAAU,0x9AU,0x82U,0x55U},
    {0x2FU,0x88U,0x92U,0x39U,0xABU,0x57U,0x54U,0x9BU,
     0x30U,0xBEU,0xE9U,0xB5U,0xDEU,0x6AU,0x93U,0x77U,
     0xACU,0x9DU,0x97U,0x78U,0x72U,0xB8U,0x76U,0xDDU,
     0x0AU,0x98U,0x39U,0x6AU,0x1DU,0x4FU,0x3AU,0xE8U},
    {0x19U,0xA8U,0x80U,0xC5U,0xF8U,0x56U,0x19U,0xA7U,
     0x8BU,0x2FU,0xE9U,0xE8U,0x06U,0x92U,0x3DU,0x01U,
     0x13U,0xB7U,0xC2U,0x60U,0x81U,0x6FU,0xC1U,0x45U,
     0xE7U,0x80U,0xC4U,0x46U,0xF2U,0xA9U,0x9FU,0xD8U},
    {0x49U,0xDEU,0x48U,0xE4U,0xC0U,0x85U,0xF6U,0xCDU,
     0xEDU,0x1FU,0x16U,0x87U,0x37U,0x0BU,0xE2U,0x54U,
     0xFFU,0xC1U,0x56U,0x1DU,0x9AU,0x85U,0x18U,0x7EU,
     0x23U,0x6FU,0x96U,0x6EU,0x5CU,0xDFU,0x3EU,0x71U}
};

/* `$A8E9->$A9DA->$AAB8->$A993` scheduling, selector, target table, and
   assignment-store anchors. `$A9DA-$AA44` remains independently pinned by
   TGGL above; these four anchors cover its caller and downstream consumers. */
static const uint8_t cpu_steering_anchor_a9da_assignment_sha256[4][32] = {
    {0x77U,0x28U,0x3DU,0xBFU,0xF6U,0xC1U,0x63U,0xF9U,0xC8U,0xA8U,0x86U,0x0FU,0xACU,0xA3U,0xA4U,0x52U,0x12U,0x1AU,0xB7U,0x49U,0xBDU,0xEDU,0xD6U,0xA1U,0xFCU,0xE3U,0xB4U,0x2BU,0x61U,0x82U,0x07U,0xD3U},
    {0x66U,0x97U,0x31U,0x29U,0x98U,0x22U,0x13U,0x30U,0x67U,0xC4U,0x17U,0xB6U,0x0AU,0x3DU,0xEBU,0x16U,0x20U,0xDFU,0x62U,0x54U,0xBFU,0x5BU,0x76U,0xDFU,0x28U,0xA0U,0x1EU,0xC2U,0xD8U,0x4DU,0x9DU,0xCFU},
    {0xE0U,0x90U,0x32U,0x60U,0x8FU,0x34U,0xB0U,0x36U,0xF1U,0x7CU,0xFAU,0xB2U,0xEBU,0x3CU,0xC3U,0x40U,0x44U,0x3EU,0x08U,0x9DU,0x6BU,0x4DU,0x73U,0x16U,0x4CU,0x50U,0x76U,0x62U,0x66U,0x16U,0x9AU,0x80U},
    {0x3CU,0xCAU,0x2BU,0xADU,0x41U,0x99U,0xD8U,0x05U,0x28U,0x27U,0x4AU,0x2CU,0xBBU,0xA7U,0xC8U,0x04U,0x10U,0xFDU,0x85U,0x5DU,0x7AU,0x4BU,0x84U,0x09U,0x75U,0x99U,0x05U,0x03U,0xCDU,0xABU,0xA5U,0xE2U}
};

static const uint8_t cpu_steering_anchor_a8e9_velocity_sha256[2][32] = {
    {0x4BU,0x66U,0xA2U,0x38U,0x50U,0x09U,0x8EU,0xB6U,0x47U,0x77U,0x63U,0x00U,0x62U,0x61U,0x2DU,0xA2U,0x70U,0xF8U,0x7CU,0x92U,0xF3U,0x68U,0xDFU,0x35U,0xE8U,0x58U,0xB5U,0x74U,0xFDU,0x86U,0xA2U,0xB9U},
    {0x42U,0xA2U,0x3DU,0x23U,0xC8U,0x33U,0xABU,0xA0U,0x91U,0x4FU,0xEDU,0x4AU,0xD8U,0x65U,0xE2U,0x0DU,0x91U,0x5DU,0xAAU,0x62U,0xB8U,0x7BU,0x2EU,0xEFU,0x07U,0x31U,0x3DU,0x38U,0x8CU,0xC1U,0x2CU,0x5EU}
};

/* Pure direct-launch solver and its sanitized TGJS distance-table dependency.
   These anchors are validation-only and are not copied into TGAI payload. */
static const uint8_t cpu_steering_anchor_a0f3_launch_sha256[7][32] = {
    {0x54U,0xCBU,0xFAU,0xCBU,0x64U,0x06U,0x1AU,0x7CU,0x36U,0xF1U,0xEEU,0xA5U,0x92U,0xC1U,0xCDU,0x9FU,0x3BU,0x10U,0xD1U,0x57U,0x6BU,0xC7U,0xA3U,0xC5U,0xF1U,0x8EU,0x08U,0xD5U,0x2CU,0xE2U,0x8BU,0x24U},
    {0x1AU,0x55U,0xFEU,0x44U,0x50U,0xB9U,0xA5U,0xF7U,0x03U,0x3CU,0xD4U,0xAEU,0x43U,0x0DU,0x4EU,0xCAU,0x9EU,0xAAU,0xD7U,0x91U,0x75U,0x6AU,0x25U,0x66U,0x68U,0xDDU,0x5BU,0x65U,0x1AU,0x0FU,0x9AU,0xF3U},
    {0xC7U,0x8CU,0x58U,0xA1U,0xA5U,0x11U,0x26U,0x26U,0x3AU,0x0EU,0x5DU,0x11U,0xF5U,0xCBU,0x03U,0x6DU,0x6DU,0x63U,0x2AU,0xB0U,0xFFU,0x74U,0x37U,0xABU,0x5CU,0x3AU,0x5CU,0x52U,0xAFU,0x3BU,0x94U,0xAAU},
    {0xA5U,0x58U,0x56U,0xF1U,0xDDU,0x41U,0x1DU,0xC4U,0x72U,0xA2U,0xA8U,0x2CU,0x96U,0x9FU,0x76U,0x45U,0x26U,0xD0U,0x2DU,0x7FU,0x38U,0x79U,0xBFU,0xB6U,0xD2U,0x18U,0x3DU,0xD7U,0xEEU,0x2CU,0x40U,0xF9U},
    {0x2BU,0x26U,0xFBU,0x25U,0x1EU,0x55U,0xDCU,0xABU,0x92U,0x5BU,0x9EU,0xFBU,0xF2U,0xD4U,0x71U,0xE7U,0xBBU,0x6AU,0x32U,0x80U,0x04U,0xFCU,0x72U,0xA4U,0x2CU,0xFAU,0x12U,0x71U,0xADU,0x3DU,0x97U,0x6BU},
    {0xA2U,0xB6U,0x04U,0x2CU,0x74U,0x65U,0xE2U,0x18U,0xF7U,0x2DU,0xFFU,0xECU,0x68U,0x7DU,0x08U,0x94U,0xECU,0x32U,0x69U,0xD5U,0x8EU,0x3FU,0x25U,0x5CU,0x9CU,0xD1U,0xB5U,0x9DU,0x2DU,0x6FU,0x37U,0x18U},
    {0x4FU,0xD8U,0x00U,0x1FU,0xB2U,0xE2U,0x8BU,0xC9U,0x48U,0x54U,0x92U,0x3FU,0x70U,0x1EU,0x7DU,0xE4U,0xC2U,0xE9U,0x88U,0x66U,0x94U,0xC0U,0xB9U,0x52U,0xB2U,0x51U,0xF4U,0x3CU,0x9EU,0xFFU,0x74U,0xCAU}
};

const TecmoGameplayCpuSteeringExpectedSource
    tecmo_gameplay_cpu_steering_expected_sources[
        TECMO_GAMEPLAY_CPU_STEERING_SOURCE_COUNT] = {
        {TECMO_GAMEPLAY_CPU_STEERING_SOURCE_ACTOR_DISPATCH,
         6U,0U,0x81F7U,
         TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_ACTOR_DISPATCH_SIZE,
         TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_ACTOR_DISPATCH_FNV1A32,
         TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_ACTOR_DISPATCH_OFFSET},
        {TECMO_GAMEPLAY_CPU_STEERING_SOURCE_REFERENCE_DIRECTION,
         6U,0U,0x87AEU,
         TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_REFERENCE_DIRECTION_SIZE,
         TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_REFERENCE_DIRECTION_FNV1A32,
         TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_REFERENCE_DIRECTION_OFFSET},
        {TECMO_GAMEPLAY_CPU_STEERING_SOURCE_TARGET_DIRECTION,
         6U,0U,0x88DAU,
         TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_TARGET_DIRECTION_SIZE,
         TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_TARGET_DIRECTION_FNV1A32,
         TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_TARGET_DIRECTION_OFFSET},
        {TECMO_GAMEPLAY_CPU_STEERING_SOURCE_ROUTE_PROJECTION,
         6U,0U,0x8A96U,
         TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_ROUTE_PROJECTION_SIZE,
         TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_ROUTE_PROJECTION_FNV1A32,
         TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_ROUTE_PROJECTION_OFFSET},
        {TECMO_GAMEPLAY_CPU_STEERING_SOURCE_ROUTE_STEP,
         6U,0U,0x8AF4U,
         TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_ROUTE_STEP_SIZE,
         TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_ROUTE_STEP_FNV1A32,
         TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_ROUTE_STEP_OFFSET},
        {TECMO_GAMEPLAY_CPU_STEERING_SOURCE_COMMAND_DISPATCH,
         6U,0U,0x8B90U,
         TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_COMMAND_DISPATCH_SIZE,
         TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_COMMAND_DISPATCH_FNV1A32,
         TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_COMMAND_DISPATCH_OFFSET},
        {TECMO_GAMEPLAY_CPU_STEERING_SOURCE_COMMAND_HANDLERS,
         6U,0U,0x8BE1U,
         TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_COMMAND_HANDLERS_SIZE,
         TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_COMMAND_HANDLERS_FNV1A32,
         TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_COMMAND_HANDLERS_OFFSET},
        {TECMO_GAMEPLAY_CPU_STEERING_SOURCE_TARGET_APPLY,
         6U,0U,0x9280U,
         TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_TARGET_APPLY_SIZE,
         TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_TARGET_APPLY_FNV1A32,
         TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_TARGET_APPLY_OFFSET},
        {TECMO_GAMEPLAY_CPU_STEERING_SOURCE_FORMATION_STREAM_SELECT,
         6U,0U,0x938BU,
         TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_FORMATION_STREAM_SIZE,
         TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_FORMATION_STREAM_FNV1A32,
         TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_FORMATION_STREAM_OFFSET},
        {TECMO_GAMEPLAY_CPU_STEERING_SOURCE_COMMAND_TRAMPOLINE,
         7U,1U,0xC006U,
         TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_COMMAND_TRAMPOLINE_SIZE,
         TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_COMMAND_TRAMPOLINE_FNV1A32,
         TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_COMMAND_TRAMPOLINE_OFFSET},
        {TECMO_GAMEPLAY_CPU_STEERING_SOURCE_COMMAND_READER,
         7U,1U,0xCBE0U,
         TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_COMMAND_READER_SIZE,
         TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_COMMAND_READER_FNV1A32,
         TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_COMMAND_READER_OFFSET},
        {TECMO_GAMEPLAY_CPU_STEERING_SOURCE_PLAY_COMMANDS,
         4U,0U,0x9F2EU,
         TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_PLAY_COMMANDS_SIZE,
         TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_PLAY_COMMANDS_FNV1A32,
         TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_PLAY_COMMANDS_OFFSET}
    };

static int range_ok(uint64_t offset, uint64_t count, uint64_t total)
{
    return offset <= total && count <= total - offset;
}

static uint64_t source_offset(
    uint64_t prg_offset,
    uint32_t prg_banks,
    const TecmoGameplayCpuSteeringExpectedSource *source)
{
    uint16_t cpu_base = source->fixed_bank != 0U
        ? CPU_STEERING_FIXED_CPU_BASE
        : TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE;
    uint32_t bank = source->fixed_bank != 0U
        ? prg_banks - 1U
        : source->bank;
    return prg_offset +
           (uint64_t)bank * TECMO_ASSET_PACK_PRG_BANK_BYTES +
           (uint64_t)(source->cpu_start - cpu_base);
}

static uint64_t cpu_steering_switchable_rom_offset(
    uint64_t prg_offset,
    uint32_t bank,
    uint16_t cpu_address)
{
    return prg_offset +
           (uint64_t)bank * TECMO_ASSET_PACK_PRG_BANK_BYTES +
           (uint64_t)(cpu_address -
                      TECMO_ASSET_PACK_SWITCHED_PRG_CPU_BASE);
}

static int validate_lifecycle_anchor(
    const uint8_t *rom,
    uint64_t rom_size,
    uint64_t prg_offset,
    uint32_t bank,
    uint16_t cpu_start,
    uint16_t cpu_end,
    uint32_t expected_fingerprint,
    const uint8_t expected_sha256[32])
{
    uint8_t digest[32];
    uint64_t offset;
    uint64_t byte_count = (uint64_t)cpu_end - cpu_start + 1U;
    if (rom == NULL || cpu_end < cpu_start || bank >= CPU_STEERING_PRG_BANK_COUNT) {
        return 0;
    }
    offset = cpu_steering_switchable_rom_offset(
        prg_offset, bank, cpu_start);
    if (!range_ok(offset, byte_count, rom_size) ||
        (expected_fingerprint != 0U &&
         tecmo_asset_pack_fnv1a32(
             rom + (size_t)offset, (size_t)byte_count) !=
             expected_fingerprint) ||
        tecmo_asset_pack_sha256_digest(
            rom + (size_t)offset, (size_t)byte_count, digest) != 0 ||
        memcmp(digest, expected_sha256, sizeof(digest)) != 0) {
        return 0;
    }
    return 1;
}

static int validate_fixed_lifecycle_anchor(
    const uint8_t *rom,
    uint64_t rom_size,
    uint64_t prg_offset,
    uint16_t cpu_start,
    uint16_t cpu_end,
    const uint8_t expected_sha256[32])
{
    uint8_t digest[32];
    uint64_t byte_count = (uint64_t)cpu_end - cpu_start + 1U;
    uint64_t offset;
    if (rom == NULL || cpu_end < cpu_start || cpu_start < 0xC000U) return 0;
    offset = prg_offset +
             (uint64_t)(CPU_STEERING_PRG_BANK_COUNT - 1U) *
                 TECMO_ASSET_PACK_PRG_BANK_BYTES +
             (uint64_t)(cpu_start - 0xC000U);
    return range_ok(offset, byte_count, rom_size) &&
           tecmo_asset_pack_sha256_digest(
               rom + (size_t)offset, (size_t)byte_count, digest) == 0 &&
           memcmp(digest, expected_sha256, sizeof(digest)) == 0;
}

static int validate_lifecycle_anchors(
    const uint8_t *rom,
    uint64_t rom_size,
    uint64_t prg_offset)
{
    static const uint8_t route_table[2] = {0x00U,0x80U};
    uint64_t route_table_offset = cpu_steering_switchable_rom_offset(
        prg_offset, 5U, 0x9709U);
    if (!validate_lifecycle_anchor(
            rom, rom_size, prg_offset, 4U, 0xAC76U, 0xACF0U,
            0U,
            cpu_steering_anchor_ac76_acf0_sha256) ||
        !validate_lifecycle_anchor(
            rom, rom_size, prg_offset, 4U, 0xACD9U, 0xACE3U,
            0U,
            cpu_steering_anchor_acd9_ace3_sha256) ||
        !validate_lifecycle_anchor(
            rom, rom_size, prg_offset, 4U, 0xADD6U, 0xADDFU,
            0U,
            cpu_steering_anchor_add6_addf_sha256) ||
        !validate_lifecycle_anchor(
            rom, rom_size, prg_offset, 5U, 0x96B6U, 0x9708U,
            0U,
            cpu_steering_anchor_b05_route_sha256) ||
        !validate_lifecycle_anchor(
            rom, rom_size, prg_offset, 6U, 0x8374U, 0x84B6U,
            0U,
            cpu_steering_anchor_b06_shot_sha256) ||
        !validate_lifecycle_anchor(
            rom, rom_size, prg_offset, 6U, 0xB081U, 0xB365U,
            0U,
            cpu_steering_anchor_b06_candidate_sha256) ||
        !validate_lifecycle_anchor(
            rom, rom_size, prg_offset, 6U, 0x9BD8U, 0x9C6EU,
            0x74DD2AC6U,
            cpu_steering_anchor_b06_divide_sha256) ||
        !validate_lifecycle_anchor(
            rom, rom_size, prg_offset, 6U, 0x88B0U, 0x88D9U,
            TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_CONTRACT_FNV1A32,
            cpu_steering_anchor_opcode15_helper_sha256) ||
        !validate_lifecycle_anchor(
            rom, rom_size, prg_offset, 6U, 0x8B90U, 0x8BE0U,
            TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_COMMAND_DISPATCH_FNV1A32,
            cpu_steering_anchor_opcode15_dispatch_sha256) ||
        !validate_lifecycle_anchor(
            rom, rom_size, prg_offset, 6U, 0x9146U, 0x9216U,
            TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_HANDLER_FNV1A32,
            cpu_steering_anchor_opcode15_handler_sha256) ||
        !validate_lifecycle_anchor(
            rom, rom_size, prg_offset, 6U, 0x9208U, 0x9216U,
            TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_FINAL_TAIL_FNV1A32,
            cpu_steering_anchor_opcode15_tail_sha256) ||
        !validate_lifecycle_anchor(
            rom, rom_size, prg_offset, 4U, 0x9F65U, 0x9F69U,
            TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_RECORD_FNV1A32,
            cpu_steering_anchor_opcode15_record_sha256) ||
        !validate_lifecycle_anchor(
            rom, rom_size, prg_offset, 4U, 0x9F79U, 0x9F7DU,
            TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_RECORD_FNV1A32,
            cpu_steering_anchor_opcode15_record_sha256) ||
        !validate_lifecycle_anchor(
            rom, rom_size, prg_offset, 6U, 0x9187U, 0x91C1U,
            0xB33C7281U, cpu_steering_anchor_opcode15_primary_sha256[0]) ||
        !validate_lifecycle_anchor(
            rom, rom_size, prg_offset, 6U, 0x9248U, 0x926FU,
            0x96DD94EFU, cpu_steering_anchor_opcode15_primary_sha256[1]) ||
        !validate_lifecycle_anchor(
            rom, rom_size, prg_offset, 6U, 0x938BU, 0x9403U,
            0x4CB55522U, cpu_steering_anchor_opcode15_primary_sha256[2]) ||
        !validate_lifecycle_anchor(
            rom, rom_size, prg_offset, 6U, 0x943BU, 0x946EU,
            0x82C361B6U, cpu_steering_anchor_opcode15_primary_sha256[3]) ||
        !validate_fixed_lifecycle_anchor(
            rom, rom_size, prg_offset, 0xC060U, 0xC062U,
            cpu_steering_anchor_opcode15_primary_sha256[4]) ||
        !validate_fixed_lifecycle_anchor(
            rom, rom_size, prg_offset, 0xCBF7U, 0xCBFFU,
            cpu_steering_anchor_opcode15_primary_sha256[5]) ||
        !validate_lifecycle_anchor(
            rom, rom_size, prg_offset, 5U, 0xA0F3U, 0xA11AU, 0U,
            cpu_steering_anchor_global_latch_sha256[0]) ||
        !validate_lifecycle_anchor(
            rom, rom_size, prg_offset, 5U, 0xA790U, 0xA7A5U, 0U,
            cpu_steering_anchor_global_latch_sha256[1]) ||
        !validate_lifecycle_anchor(
            rom, rom_size, prg_offset, 5U, 0xA9DAU, 0xAA44U, 0U,
            cpu_steering_anchor_global_latch_sha256[2]) ||
        !validate_lifecycle_anchor(
            rom, rom_size, prg_offset, 5U, 0xB721U, 0xB736U, 0U,
            cpu_steering_anchor_global_latch_sha256[3]) ||
        !validate_lifecycle_anchor(
            rom, rom_size, prg_offset, 5U, 0xB783U, 0xB792U, 0U,
            cpu_steering_anchor_global_latch_sha256[4]) ||
        !validate_lifecycle_anchor(
            rom, rom_size, prg_offset, 6U, 0x9125U, 0x9145U, 0U,
            cpu_steering_anchor_global_latch_sha256[5]) ||
        !validate_fixed_lifecycle_anchor(
            rom, rom_size, prg_offset, 0xCC30U, 0xCC57U,
            cpu_steering_anchor_global_latch_sha256[6]) ||
        !validate_fixed_lifecycle_anchor(
            rom, rom_size, prg_offset, 0xCC58U, 0xCC85U,
            cpu_steering_anchor_global_latch_sha256[7]) ||
        !validate_lifecycle_anchor(
            rom, rom_size, prg_offset, 5U, 0xA8E9U, 0xA9D9U,
            0x8A09C556U, cpu_steering_anchor_a9da_assignment_sha256[0]) ||
        !validate_lifecycle_anchor(
            rom, rom_size, prg_offset, 5U, 0xAAB8U, 0xAB35U,
            0x16BFF0A5U, cpu_steering_anchor_a9da_assignment_sha256[1]) ||
        !validate_lifecycle_anchor(
            rom, rom_size, prg_offset, 5U, 0xBDEFU, 0xBDF4U,
            0xF4F6458FU, cpu_steering_anchor_a9da_assignment_sha256[2]) ||
        !validate_lifecycle_anchor(
            rom, rom_size, prg_offset, 5U, 0xA993U, 0xA9C4U,
            0x0E85BBD7U, cpu_steering_anchor_a9da_assignment_sha256[3]) ||
        !validate_lifecycle_anchor(
            rom, rom_size, prg_offset, 5U, 0xA8E9U, 0xA976U,
            0x815E6881U, cpu_steering_anchor_a8e9_velocity_sha256[0]) ||
        !validate_lifecycle_anchor(
            rom, rom_size, prg_offset, 5U, 0xAA87U, 0xAA9EU,
            0x6D37E9A0U, cpu_steering_anchor_a8e9_velocity_sha256[1]) ||
        !validate_lifecycle_anchor(
            rom, rom_size, prg_offset, 5U, 0xA0F3U, 0xA158U,
            0xE0D639BEU, cpu_steering_anchor_a0f3_launch_sha256[0]) ||
        !validate_lifecycle_anchor(
            rom, rom_size, prg_offset, 5U, 0xB32CU, 0xB390U,
            0xD3DB4014U, cpu_steering_anchor_a0f3_launch_sha256[1]) ||
        !validate_lifecycle_anchor(
            rom, rom_size, prg_offset, 5U, 0xBCF4U, 0xBD68U,
            0xA8A390BBU, cpu_steering_anchor_a0f3_launch_sha256[2]) ||
        !validate_lifecycle_anchor(
            rom, rom_size, prg_offset, 5U, 0x80A9U, 0x813DU,
            0x9A20473AU, cpu_steering_anchor_a0f3_launch_sha256[3]) ||
        !validate_lifecycle_anchor(
            rom, rom_size, prg_offset, 5U, 0xA15CU, 0xA183U,
            0x56696FEFU, cpu_steering_anchor_a0f3_launch_sha256[4]) ||
        !validate_lifecycle_anchor(
            rom, rom_size, prg_offset, 5U, 0xBDF7U, 0xBEF6U,
            0x93FCF6CBU, cpu_steering_anchor_a0f3_launch_sha256[5]) ||
        !validate_lifecycle_anchor(
            rom, rom_size, prg_offset, 5U, 0xBD6EU, 0xBDC6U,
            0x3F4FB637U, cpu_steering_anchor_a0f3_launch_sha256[6]) ||
        !range_ok(route_table_offset, sizeof(route_table), rom_size) ||
        memcmp(rom + (size_t)route_table_offset,
               route_table, sizeof(route_table)) != 0) {
        return 0;
    }
    return 1;
}

/* The full-ROM revision fingerprint rejects any mutated input before the
   importer reaches the nested anchor checks. Exercise each independent
   functional source or semantic anchor directly here as well, so the checks remain an
   independently enforced contract rather than metadata hidden behind the
   broader Rev1 gate. The opcode-15 tail intentionally overlaps the handler source and
   is tested separately because it is the canonical-ROM/lifted-source
   discrepancy boundary. */
static int validate_independent_anchor_mutation_rejection(
    const uint8_t *rom,
    uint64_t rom_size,
    uint64_t prg_offset)
{
    static const struct {
        uint32_t bank;
        uint16_t cpu_start;
    } anchors[] = {
        {6U, 0x9BD8U}, {6U, 0x88B0U}, {6U, 0x8B90U}, {6U, 0x9146U},
        {6U, 0x9208U}, {4U, 0x9F65U}, {4U, 0x9F79U},
        {6U, 0x9187U}, {6U, 0x9248U}, {6U, 0x938BU}, {6U, 0x943BU},
        {5U, 0xA0F3U}, {5U, 0xA790U}, {5U, 0xA9DAU},
        {5U, 0xB721U}, {5U, 0xB783U}, {6U, 0x9125U},
        {5U, 0xA8E9U}, {5U, 0xAAB8U}, {5U, 0xBDEFU}, {5U, 0xA993U},
        {5U, 0xAA87U}, {5U, 0xA0F3U}, {5U, 0xB32CU},
        {5U, 0xBCF4U}, {5U, 0x80A9U}, {5U, 0xA15CU}, {5U, 0xBDF7U},
        {5U, 0xBD6EU}
    };
    uint8_t *mutated;
    size_t index;
    if (rom == NULL || !validate_lifecycle_anchors(
            rom, rom_size, prg_offset)) {
        return 0;
    }
    if (rom_size > SIZE_MAX ||
        (mutated = (uint8_t *)malloc((size_t)rom_size)) == NULL) {
        return 0;
    }
    {
        static const uint16_t fixed_anchors[] = {
            0xCC30U, 0xCC58U, 0xC060U, 0xCBF7U
        };
        for (index = 0U;
             index < sizeof(fixed_anchors) / sizeof(fixed_anchors[0U]);
             ++index) {
            uint64_t offset = prg_offset +
                (uint64_t)(CPU_STEERING_PRG_BANK_COUNT - 1U) *
                    TECMO_ASSET_PACK_PRG_BANK_BYTES +
                (uint64_t)(fixed_anchors[index] - 0xC000U);
            if (!range_ok(offset, 1U, rom_size)) {
                free(mutated);
                return 0;
            }
            memcpy(mutated, rom, (size_t)rom_size);
            mutated[(size_t)offset] ^= 1U;
            if (validate_lifecycle_anchors(mutated, rom_size, prg_offset)) {
                free(mutated);
                return 0;
            }
        }
    }
    for (index = 0U; index < sizeof(anchors) / sizeof(anchors[0U]); ++index) {
        uint64_t offset = cpu_steering_switchable_rom_offset(
            prg_offset, anchors[index].bank, anchors[index].cpu_start);
        if (!range_ok(offset, 1U, rom_size)) {
            free(mutated);
            return 0;
        }
        memcpy(mutated, rom, (size_t)rom_size);
        mutated[(size_t)offset] ^= 1U;
        if (validate_lifecycle_anchors(mutated, rom_size, prg_offset)) {
            free(mutated);
            return 0;
        }
    }
    free(mutated);
    return 1;
}

static int play_commands_valid(const uint8_t *commands)
{
    static const uint8_t first_command[5] = {0x04U,0x0AU,0U,0U,0U};
    static const uint8_t free_throw_a[5] = {0x03U,0x08U,0U,0U,0U};
    static const uint8_t free_throw_b[5] = {0x02U,0xB4U,0U,0x96U,0U};
    static const uint8_t last_command[5] = {0x01U,0x80U,0x0CU,0U,0U};
    static const uint8_t opcode15_command[5] = {0x0FU,0U,0U,0U,0U};
    if (commands == NULL ||
        memcmp(commands, first_command, sizeof(first_command)) != 0 ||
        memcmp(commands + 0x007DU, free_throw_a,
               sizeof(free_throw_a)) != 0 ||
        memcmp(commands + 0x00D7U, free_throw_b,
               sizeof(free_throw_b)) != 0 ||
        memcmp(commands +
                   TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_RECORD_A_OFFSET,
               opcode15_command, sizeof(opcode15_command)) != 0 ||
        memcmp(commands +
                   TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_RECORD_B_OFFSET,
               opcode15_command, sizeof(opcode15_command)) != 0 ||
        tecmo_asset_pack_fnv1a32(
            commands +
                TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_RECORD_A_OFFSET,
            sizeof(opcode15_command)) !=
            TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_RECORD_FNV1A32 ||
        tecmo_asset_pack_fnv1a32(
            commands +
                TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_RECORD_B_OFFSET,
            sizeof(opcode15_command)) !=
            TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_RECORD_FNV1A32 ||
        memcmp(commands +
                   TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_PLAY_COMMANDS_SIZE -
                   TECMO_GAMEPLAY_CPU_STEERING_COMMAND_SIZE,
               last_command, sizeof(last_command)) != 0) {
        return 0;
    }
    for (size_t offset = 0U;
         offset < TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_PLAY_COMMANDS_SIZE;
         offset += TECMO_GAMEPLAY_CPU_STEERING_COMMAND_SIZE) {
        if (commands[offset] >=
            TECMO_GAMEPLAY_CPU_STEERING_OPCODE_COUNT) {
            return 0;
        }
    }
    return 1;
}

static int validate_semantics(const uint8_t *payload)
{
    const uint8_t *actor = payload +
        TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_ACTOR_DISPATCH_OFFSET;
    const uint8_t *reference = payload +
        TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_REFERENCE_DIRECTION_OFFSET;
    const uint8_t *target_direction = payload +
        TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_TARGET_DIRECTION_OFFSET;
    const uint8_t *route_projection = payload +
        TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_ROUTE_PROJECTION_OFFSET;
    const uint8_t *route_step = payload +
        TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_ROUTE_STEP_OFFSET;
    const uint8_t *dispatch = payload +
        TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_COMMAND_DISPATCH_OFFSET;
    const uint8_t *handlers = payload +
        TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_COMMAND_HANDLERS_OFFSET;
    const uint8_t *apply = payload +
        TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_TARGET_APPLY_OFFSET;
    const uint8_t *formation = payload +
        TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_FORMATION_STREAM_OFFSET;
    const uint8_t *trampoline = payload +
        TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_COMMAND_TRAMPOLINE_OFFSET;
    const uint8_t *reader = payload +
        TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_COMMAND_READER_OFFSET;
    const uint8_t *commands = payload +
        TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_PLAY_COMMANDS_OFFSET;
    const uint8_t *opcode15_descriptor = payload +
        TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_CONTRACT_DESCRIPTOR_OFFSET;
    const uint8_t *opcode15_raw = payload +
        TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_CONTRACT_RAW_OFFSET;
    uint8_t handler_low[TECMO_GAMEPLAY_CPU_STEERING_OPCODE_COUNT];
    uint8_t handler_high[TECMO_GAMEPLAY_CPU_STEERING_OPCODE_COUNT];

    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_CPU_STEERING_OPCODE_COUNT; ++index) {
        handler_low[index] =
            (uint8_t)(cpu_steering_handler_cpu[index] & 0xFFU);
        handler_high[index] =
            (uint8_t)(cpu_steering_handler_cpu[index] >> 8U);
    }

    return tecmo_asset_pack_read_u16(opcode15_descriptor) == 15U &&
           opcode15_descriptor[2U] ==
               TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_CONTRACT_BANK &&
           opcode15_descriptor[3U] == 0U &&
           tecmo_asset_pack_read_u16(opcode15_descriptor + 4U) ==
               TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_CONTRACT_CPU_START &&
           tecmo_asset_pack_read_u16(opcode15_descriptor + 6U) ==
               TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_CONTRACT_RAW_SIZE &&
           tecmo_asset_pack_read_u32(opcode15_descriptor + 8U) ==
               TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_CONTRACT_FNV1A32 &&
           tecmo_asset_pack_read_u16(opcode15_descriptor + 12U) ==
               TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_CONTRACT_RAW_OFFSET &&
           tecmo_asset_pack_read_u16(opcode15_descriptor + 14U) ==
               TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_CONTRACT_RAW_SIZE &&
           tecmo_asset_pack_fnv1a32(
               opcode15_raw,
               TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_CONTRACT_RAW_SIZE) ==
               TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_CONTRACT_FNV1A32 &&
           memcmp(opcode15_raw,
                  "\xBC\x63\x04\xB9\xCA\x88\x9D\x42\x04"
                  "\xB9\xD2\x88\x9D\x4D\x04\xA9\xC1\x9D"
                  "\x79\x04\xA9\x30\x9D\x58\x04\x60",
                  26U) == 0 &&
           memcmp(opcode15_raw +
                      TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_POSE_LOW_OFFSET,
                  "\x0C\x0A\x10\x0C\x0A\x0E\x0C\x0A", 8U) == 0 &&
           memcmp(opcode15_raw +
                      TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_POSE_HIGH_OFFSET,
                  "\x04\x04\x04\x04\x04\x04\x04\x04", 8U) == 0 &&
           memcmp(actor + (0x8284U - 0x81F7U),
                  "\xA2\x09\xEC\x08\x03\xF0", 6U) == 0 &&
           memcmp(actor + (0x82B6U - 0x81F7U),
                  "\x9F\x9F\x9F\x9F\x90", 5U) == 0 &&
           reference[0U] == 0xA6U && reference[1U] == 0xAAU &&
           memcmp(reference + (0x887BU - 0x87AEU),
                  "\xB9\x8E\x8A\x9D\x63\x04", 6U) == 0 &&
           memcmp(target_direction, "\xA5\xA4\x85\xAB", 4U) == 0 &&
           memcmp(target_direction + (0x899AU - 0x88DAU),
                  "\xB9\x8E\x8A\x9D\x63\x04", 6U) == 0 &&
           memcmp(target_direction + (0x8A8EU - 0x88DAU),
                  cpu_steering_direction_map,
                  sizeof(cpu_steering_direction_map)) == 0 &&
           memcmp(route_projection,
                  "\xA5\xA8\x85\x6D\xA5\xA9\x85\x6E", 8U) == 0 &&
           memcmp(route_projection + (0x8AE4U - 0x8A96U),
                  "\xA5\x9A\x18\x69\x01\x9D\x13\x05", 8U) == 0 &&
           memcmp(route_step,
                  "\xEC\x08\x03\xF0\x06\xAD\x89\x05", 8U) == 0 &&
           memcmp(route_step + (0x8B8AU - 0x8AF4U),
                  "\xA9\x04\x9D\x7C\x05\x60", 6U) == 0 &&
           memcmp(dispatch,
                  "\xBD\x47\x05\x18\x69\x2E\x85\xA8\xBD\x51\x05\x69\x9F",
                  13U) == 0 &&
           memcmp(dispatch + (0x8B9FU - 0x8B90U),
                  "\x20\x06\xC0\xA4\xC7", 5U) == 0 &&
           memcmp(dispatch + (0x8BB1U - 0x8B90U),
                  handler_low,
                  TECMO_GAMEPLAY_CPU_STEERING_OPCODE_COUNT) == 0 &&
           memcmp(dispatch + (0x8BC9U - 0x8B90U),
                  handler_high,
                  TECMO_GAMEPLAY_CPU_STEERING_OPCODE_COUNT) == 0 &&
           handlers[0U] == 0xA5U && handlers[1U] == 0xC8U &&
           memcmp(handlers + (0x8CD0U - 0x8BE1U),
                  "\xEC\xDF\x07", 3U) == 0 &&
           memcmp(handlers + (0x90E0U - 0x8BE1U),
                  "\xA5\xC8\x85\xA4", 4U) == 0 &&
           memcmp(apply,
                  "\xAD\x5A\x03\xF0\x23\xA9\x00\x38\xE5\xC8", 10U) == 0 &&
           memcmp(apply + (0x92A8U - 0x9280U),
                  "\xA5\xC8\x9D\x5B\x05\x38\xF5\x73", 8U) == 0 &&
           memcmp(apply + (0x92D4U - 0x9280U),
                  "\xA5\xA4\x05\xA5\x05\xA6\x05\xA7\xF0\x23",
                  10U) == 0 &&
           memcmp(apply + (0x92FEU - 0x9280U),
                  "\x4C\xDA\x88", 3U) == 0 &&
           memcmp(formation,
                  "\xAE\x08\x03\xB5\x73\x85\xA4", 7U) == 0 &&
           memcmp(formation + (0x9424U - 0x938BU),
                  "\xAD\x90\x07", 3U) == 0 &&
           memcmp(formation + (0x946FU - 0x938BU),
                  "\x86\x94\x20\x8B\x93", 5U) == 0 &&
           memcmp(trampoline, "\x4C\xE0\xCB", 3U) == 0 &&
           memcmp(reader,
                  "\xAD\xFF\xBF\x48\xA9\x04\x20\x6A\xD3\xA0\x04",
                  11U) == 0 &&
           memcmp(reader + 11U,
                  "\xB1\xA8\x99\xC7\x00\x88\x10\xF8\x68\x4C\x6A\xD3",
                  12U) == 0 &&
           play_commands_valid(commands);
}

int tecmo_asset_pack_build_gameplay_cpu_steering(
    const uint8_t *rom,
    uint64_t rom_size,
    uint64_t prg_offset,
    uint32_t prg_banks,
    int enforce_revision_fingerprints,
    uint8_t *payload,
    size_t payload_size,
    TecmoGameplayCpuSteeringProvenance *provenance,
    char *message,
    size_t message_size)
{
    uint8_t input_sha256[32];
    if (rom == NULL || payload == NULL || provenance == NULL ||
        payload_size != TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_SIZE ||
        prg_banks != CPU_STEERING_PRG_BANK_COUNT ||
        enforce_revision_fingerprints == 0 ||
        rom_size != CPU_STEERING_REV1_ROM_SIZE ||
        prg_offset != sizeof(cpu_steering_rev1_ines_header) ||
        memcmp(rom, cpu_steering_rev1_ines_header,
               sizeof(cpu_steering_rev1_ines_header)) != 0 ||
        tecmo_asset_pack_sha256_digest(
            rom, (size_t)rom_size, input_sha256) != 0 ||
        memcmp(input_sha256, cpu_steering_rev1_sha256,
               sizeof(input_sha256)) != 0) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TGAI-3 import requires the exact Rev1 ROM fingerprint.");
        return -1;
    }

    memset(payload, 0, payload_size);
    memset(provenance, 0, sizeof(*provenance));
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_CPU_STEERING_SOURCE_COUNT; ++index) {
        const TecmoGameplayCpuSteeringExpectedSource *expected =
            &tecmo_gameplay_cpu_steering_expected_sources[index];
        uint64_t offset = source_offset(prg_offset, prg_banks, expected);
        uint32_t cpu_end =
            (uint32_t)expected->cpu_start + expected->byte_count - 1U;
        uint8_t *record = payload +
            TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_SOURCES_OFFSET +
            index * TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_SOURCE_STRIDE;
        if (expected->bank >= prg_banks ||
            (expected->fixed_bank != 0U
                 ? cpu_end > 0xFFFFU || expected->cpu_start < 0xC000U
                 : cpu_end >= 0xC000U || expected->cpu_start < 0x8000U) ||
            !range_ok(offset, expected->byte_count, rom_size) ||
            tecmo_asset_pack_fnv1a32(
                rom + (size_t)offset, expected->byte_count) !=
                    expected->fingerprint) {
            tecmo_asset_pack_set_messagef(
                message, message_size,
                "TGAI-3 %s Bank%02u $%04X-$%04X fingerprint mismatch.",
                expected->fixed_bank != 0U ? "fixed" : "switchable",
                (unsigned)expected->bank,
                (unsigned)expected->cpu_start, (unsigned)cpu_end);
            return -1;
        }
        tecmo_asset_pack_store_u16(record, (uint16_t)expected->kind);
        record[2U] = expected->bank;
        record[3U] = expected->fixed_bank;
        tecmo_asset_pack_store_u16(record + 4U, expected->cpu_start);
        tecmo_asset_pack_store_u16(record + 6U, (uint16_t)cpu_end);
        tecmo_asset_pack_store_u32(record + 8U, expected->byte_count);
        tecmo_asset_pack_store_u32(record + 12U, expected->fingerprint);
        tecmo_asset_pack_store_u32(record + 16U,
                                   expected->payload_offset);
        memcpy(payload + expected->payload_offset,
               rom + (size_t)offset, expected->byte_count);
        provenance->source_offsets[index] = offset;
    }

    {
        uint64_t opcode15_offset = cpu_steering_switchable_rom_offset(
            prg_offset,
            TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_CONTRACT_BANK,
            TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_CONTRACT_CPU_START);
        uint8_t *descriptor = payload +
            TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_CONTRACT_DESCRIPTOR_OFFSET;
        uint8_t *raw = payload +
            TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_CONTRACT_RAW_OFFSET;
        if (!range_ok(
                opcode15_offset,
                TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_CONTRACT_RAW_SIZE,
                rom_size) ||
            tecmo_asset_pack_fnv1a32(
                rom + (size_t)opcode15_offset,
                TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_CONTRACT_RAW_SIZE) !=
                TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_CONTRACT_FNV1A32) {
            tecmo_asset_pack_set_message(
                message, message_size,
                "TGAI-3 Bank06 $88B0-$88D9 opcode-15 helper fingerprint mismatch.");
            return -1;
        }
        tecmo_asset_pack_store_u16(descriptor, 15U);
        descriptor[2U] =
            TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_CONTRACT_BANK;
        descriptor[3U] = 0U;
        tecmo_asset_pack_store_u16(
            descriptor + 4U,
            TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_CONTRACT_CPU_START);
        tecmo_asset_pack_store_u16(
            descriptor + 6U,
            TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_CONTRACT_RAW_SIZE);
        tecmo_asset_pack_store_u32(
            descriptor + 8U,
            TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_CONTRACT_FNV1A32);
        tecmo_asset_pack_store_u16(
            descriptor + 12U,
            TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_CONTRACT_RAW_OFFSET);
        tecmo_asset_pack_store_u16(
            descriptor + 14U,
            TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_CONTRACT_RAW_SIZE);
        memcpy(raw, rom + (size_t)opcode15_offset,
               TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_OPCODE15_CONTRACT_RAW_SIZE);
    }

    if (!validate_semantics(payload) ||
        !validate_lifecycle_anchors(rom, rom_size, prg_offset) ||
        /* The byte immediately after the 680th record resumes Bank04 code. */
        rom[16U + 4U * 0x4000U + (0xAC76U - 0x8000U)] != 0x20U ||
        tecmo_asset_pack_fnv1a32(rom, (size_t)rom_size) !=
            CPU_STEERING_REV1_ROM_FNV1A32) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TGAI-3 semantic or full-ROM revision contract rejected.");
        return -1;
    }

    memcpy(payload, "TGAI", 4U);
    tecmo_asset_pack_store_u16(
        payload + 4U, TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_VERSION);
    tecmo_asset_pack_store_u16(
        payload + 6U, TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_HEADER_SIZE);
    tecmo_asset_pack_store_u32(
        payload + 8U, TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_SIZE);
    tecmo_asset_pack_store_u16(
        payload + 12U, TECMO_GAMEPLAY_CPU_STEERING_SOURCE_COUNT);
    tecmo_asset_pack_store_u16(
        payload + 14U,
        TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_SOURCE_STRIDE);
    tecmo_asset_pack_store_u32(
        payload + 16U,
        TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_SOURCES_OFFSET);
    tecmo_asset_pack_store_u32(
        payload + 20U, TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_SIZE);
    tecmo_asset_pack_store_u32(
        payload + 24U, TECMO_ASSET_PACK_GAMEPLAY_MOVEMENT_FNV1A32);
    tecmo_asset_pack_store_u32(
        payload + 28U, CPU_STEERING_REV1_ROM_SIZE);
    tecmo_asset_pack_store_u32(
        payload + 32U, CPU_STEERING_REV1_ROM_FNV1A32);
    memcpy(payload + 36U, cpu_steering_rev1_sha256,
           sizeof(cpu_steering_rev1_sha256));
    tecmo_asset_pack_store_u16(payload + 68U,
                               CPU_STEERING_COMMAND_BASE_CPU);
    tecmo_asset_pack_store_u16(
        payload + 70U, TECMO_GAMEPLAY_CPU_STEERING_COMMAND_SIZE);
    tecmo_asset_pack_store_u16(
        payload + 72U, TECMO_GAMEPLAY_CPU_STEERING_COMMAND_COUNT);
    tecmo_asset_pack_store_u16(
        payload + 74U, TECMO_GAMEPLAY_CPU_STEERING_OPCODE_COUNT);
    tecmo_asset_pack_store_u16(
        payload + 76U, TECMO_GAMEPLAY_CPU_STEERING_DIRECTION_COUNT);
    tecmo_asset_pack_store_u16(payload + 78U, 10U);
    payload[80U] = 4U;
    payload[81U] = 6U;
    payload[82U] = 7U;
    payload[83U] = 4U;
    tecmo_asset_pack_store_u16(payload + 84U, 0x00C7U);
    memcpy(payload +
               TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_DIRECTION_OFFSET,
           cpu_steering_direction_map,
           sizeof(cpu_steering_direction_map));
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_CPU_STEERING_SOURCE_COUNT; ++index) {
        const TecmoGameplayCpuSteeringExpectedSource *expected =
            &tecmo_gameplay_cpu_steering_expected_sources[index];
        uint8_t *descriptor = payload +
            TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_DESCRIPTOR_OFFSET +
            index *
                TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_DESCRIPTOR_STRIDE;
        tecmo_asset_pack_store_u32(descriptor, expected->payload_offset);
        tecmo_asset_pack_store_u32(descriptor + 4U, expected->byte_count);
        tecmo_asset_pack_store_u32(descriptor + 8U,
                                   expected->fingerprint);
    }
    for (size_t index = 0U;
         index < TECMO_GAMEPLAY_CPU_STEERING_OPCODE_COUNT; ++index) {
        tecmo_asset_pack_store_u16(
            payload + TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_HANDLER_OFFSET +
                index * 2U,
            cpu_steering_handler_cpu[index]);
    }
    /* Target-producing command bits 0,2,4,10,12,13,16,20. Opcode 5 writes
       direction directly; the other entries remain control/state commands. */
    tecmo_asset_pack_store_u32(
        payload +
            TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_TARGET_EFFECT_MASK_OFFSET,
        0x00113415U);
    payload[TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_DIRECT_DIRECTION_OPCODE_OFFSET] =
        5U;

    if (tecmo_asset_pack_fnv1a32(payload, payload_size) !=
            TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_FNV1A32) {
        tecmo_asset_pack_set_messagef(
            message, message_size,
            "TGAI-3 canonical payload fingerprint mismatch (got %08X).",
            tecmo_asset_pack_fnv1a32(payload, payload_size));
        return -1;
    }
    tecmo_asset_pack_set_message(
        message, message_size,
        "Built strict ROM-derived TGAI-3 CPU steering evidence asset.");
    return 0;
}

int tecmo_asset_pack_gameplay_cpu_steering_source_test(
    const char *rom_path,
    char *message,
    size_t message_size)
{
    uint8_t *rom = NULL;
    uint64_t rom_size = 0U;
    uint64_t prg_offset = sizeof(cpu_steering_rev1_ines_header);
    uint64_t prg_size =
        (uint64_t)CPU_STEERING_PRG_BANK_COUNT *
        TECMO_ASSET_PACK_PRG_BANK_BYTES;
    uint64_t chr_size =
        (uint64_t)CPU_STEERING_CHR_BANK_COUNT *
        TECMO_ASSET_PACK_CHR_BANK_BYTES;
    uint8_t payload[TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_SIZE];
    TecmoGameplayCpuSteeringProvenance provenance;
    int result;
    if (rom_path == NULL ||
        tecmo_asset_pack_read_file(rom_path, &rom, &rom_size) != 0) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TGAI-3 direct source test could not read the ROM.");
        return -1;
    }
    if (rom_size != CPU_STEERING_REV1_ROM_SIZE ||
        prg_offset + prg_size + chr_size != rom_size) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TGAI-3 direct source test requires the exact Rev1 layout.");
        free(rom);
        return -1;
    }
    result = tecmo_asset_pack_build_gameplay_cpu_steering(
        rom, rom_size, prg_offset, CPU_STEERING_PRG_BANK_COUNT, 1,
        payload, sizeof(payload), &provenance, message, message_size);
    if (result == 0 && !validate_independent_anchor_mutation_rejection(
            rom, rom_size, prg_offset)) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TGAI-3 independent functional-anchor mutation rejection failed.");
        result = -1;
    }
    free(rom);
    return result;
}

int tecmo_asset_pack_gameplay_cpu_steering_self_test(
    char *message,
    size_t message_size)
{
    uint8_t truncated_rom[16] = {0U};
    uint8_t payload[TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_SIZE];
    TecmoGameplayCpuSteeringProvenance provenance;
    if (TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_ACTOR_DISPATCH_OFFSET !=
            TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_SOURCES_OFFSET +
                TECMO_GAMEPLAY_CPU_STEERING_SOURCE_COUNT *
                    TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_SOURCE_STRIDE ||
        TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_PLAY_COMMANDS_OFFSET +
                TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_PLAY_COMMANDS_SIZE >
            TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_SIZE ||
        TECMO_ASSET_PACK_GAMEPLAY_CPU_STEERING_PLAY_COMMANDS_SIZE !=
            TECMO_GAMEPLAY_CPU_STEERING_COMMAND_COUNT *
                TECMO_GAMEPLAY_CPU_STEERING_COMMAND_SIZE ||
        tecmo_asset_pack_build_gameplay_cpu_steering(
            truncated_rom, sizeof(truncated_rom), 16U,
            CPU_STEERING_PRG_BANK_COUNT, 1,
            payload, sizeof(payload), &provenance,
            NULL, 0U) == 0) {
        tecmo_asset_pack_set_message(
            message, message_size,
            "TGAI-3 importer layout/rejection self-test failed.");
        return -1;
    }
    tecmo_asset_pack_set_message(
        message, message_size,
        "TGAI-3 importer self-test passed.");
    return 0;
}
