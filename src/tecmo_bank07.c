#include "tecmo_bank07.h"

#include <stdio.h>
#include <string.h>

#define BANK07_CPU_OAM_BASE 0x0200U
#define BANK07_SPRITE_COUNT_ADDR 0x058DU
#define BANK07_SETUP_FULL_BASE 0x033EU
#define BANK07_SETUP_NIBBLE_BASE 0x031EU
#define BANK07_MAX_OAM_SPRITES 64U

typedef struct Bank07L88E7IrqProof {
    uint8_t irq_latch;
    uint8_t irq_reload;
    uint16_t irq_vector;
    bool irq_enabled;
    uint8_t phase0_latch;
    uint8_t phase1_latch;
    uint16_t phase1_ppu_address;
    uint16_t phase2_ppu_address;
} Bank07L88E7IrqProof;

static void set_bank07_message(char *dest, size_t dest_size, const char *text)
{
    if (dest == NULL || dest_size == 0) {
        return;
    }
    if (text == NULL) {
        text = "";
    }
    (void)snprintf(dest, dest_size, "%s", text);
}

size_t tecmo_bank07_d861_stage_sprite_records(const TecmoBank07SpriteRecord *records,
                                               size_t record_count,
                                               const TecmoBank07SpriteStageConfig *config,
                                               TecmoBank07OamSprite *entries,
                                               size_t entry_capacity)
{
    size_t staged_count = 0;
    if (records == NULL || config == NULL || entries == NULL || entry_capacity == 0) {
        return 0;
    }

    for (size_t i = 0; i < record_count && staged_count < entry_capacity; ++i) {
        const TecmoBank07SpriteRecord *record = &records[i];
        TecmoBank07OamSprite *entry = &entries[staged_count++];
        entry->y = (uint8_t)(config->base_y + record->relative_y);
        entry->tile = (uint8_t)(record->tile + config->tile_offset);
        entry->attributes = (uint8_t)(record->attributes | config->attribute_or);
        entry->x = (uint8_t)(config->base_x + record->relative_x);
    }

    return staged_count;
}

void tecmo_bank07_d2d2_hide_unused_oam(TecmoGameMemory *memory)
{
    uint8_t sprite_count;
    if (memory == NULL) {
        return;
    }

    sprite_count = tecmo_cpu_ram_read(memory, BANK07_SPRITE_COUNT_ADDR);
    if (sprite_count >= BANK07_MAX_OAM_SPRITES) {
        return;
    }

    for (uint16_t sprite = sprite_count; sprite < BANK07_MAX_OAM_SPRITES; ++sprite) {
        tecmo_cpu_ram_write(memory, (uint16_t)(BANK07_CPU_OAM_BASE + sprite * 4U), 0xF7U);
    }
}

void tecmo_bank07_d700_copy_setup_bytes(TecmoGameMemory *memory, const uint8_t source[16])
{
    if (memory == NULL || source == NULL) {
        return;
    }

    for (uint16_t index = 0; index < 16U; ++index) {
        uint8_t value = source[index];
        tecmo_cpu_ram_write(memory, (uint16_t)(BANK07_SETUP_FULL_BASE + index), value);
        tecmo_cpu_ram_write(memory, (uint16_t)(BANK07_SETUP_NIBBLE_BASE + index), (uint8_t)(value & 0x0FU));
    }
}

void tecmo_bank07_sprite_8x16_pair_for_table(uint8_t oam_tile_low,
                                             uint32_t chr_table,
                                             uint16_t out_tiles[2])
{
    if (out_tiles == NULL) {
        return;
    }

    out_tiles[0] = (uint16_t)((chr_table & 1U) * 0x100U + (uint16_t)(oam_tile_low & 0xFEU));
    out_tiles[1] = (uint16_t)(out_tiles[0] + 1U);
}

static bool model_bank07_l88e7_cdac_irq_setup(const TecmoGameMemory *memory, Bank07L88E7IrqProof *proof)
{
    uint8_t vector_index;
    if (memory == NULL || proof == NULL) {
        return false;
    }

    memset(proof, 0, sizeof(*proof));
    proof->irq_enabled = (tecmo_cpu_ram_read(memory, 0x05B6U) & 0x01U) != 0U;

    if (tecmo_cpu_ram_read(memory, 0x0305U) != 0U) {
        proof->irq_latch = 0x1FU;
        proof->irq_reload = 0x1FU;
        proof->irq_vector = 0xFE92U;
        return true;
    }

    proof->irq_latch = tecmo_cpu_ram_read(memory, 0x0352U);
    proof->irq_reload = proof->irq_latch;
    vector_index = tecmo_cpu_ram_read(memory, 0x0100U);
    if (vector_index != 0x05U) {
        return false;
    }

    proof->irq_vector = 0xFCF6U;
    proof->phase0_latch = (uint8_t)(0x78U - tecmo_cpu_ram_read(memory, 0x0301U));
    proof->phase1_latch = (uint8_t)(tecmo_cpu_ram_read(memory, 0x0088U) + tecmo_cpu_ram_read(memory, 0x0301U));
    proof->phase1_ppu_address = 0x0400U;
    proof->phase2_ppu_address = 0x0200U;
    return true;
}

static bool check_oam_entry(const TecmoBank07OamSprite *entry,
                            uint8_t y,
                            uint8_t tile,
                            uint8_t attributes,
                            uint8_t x)
{
    return entry->y == y &&
           entry->tile == tile &&
           entry->attributes == attributes &&
           entry->x == x;
}

bool tecmo_bank07_self_test(char *message, size_t message_size)
{
    const TecmoBank07SpriteRecord records[] = {
        {2, 0x10U, 0x01U, 3},
        {-30, 0x20U, 0x02U, 4},
        {5, 0xF8U, 0x03U, 300},
    };
    const TecmoBank07SpriteStageConfig config = {10, 20, 0x0DU, 0x10U};
    TecmoBank07OamSprite entries[3];
    TecmoGameMemory memory;
    Bank07L88E7IrqProof irq_proof;
    uint8_t setup[16];
    uint16_t pair[2] = {0, 0};
    size_t count;

    if (message != NULL && message_size > 0) {
        message[0] = '\0';
    }

    if (tecmo_bank07_d861_stage_sprite_records(NULL, 1, &config, entries, 3) != 0 ||
        tecmo_bank07_d861_stage_sprite_records(records, 1, NULL, entries, 3) != 0 ||
        tecmo_bank07_d861_stage_sprite_records(records, 1, &config, NULL, 3) != 0 ||
        tecmo_bank07_d861_stage_sprite_records(records, 1, &config, entries, 0) != 0) {
        set_bank07_message(message, message_size, "BANK07 D861 NULL CONTRACT FAILED");
        return false;
    }

    memset(entries, 0, sizeof(entries));
    count = tecmo_bank07_d861_stage_sprite_records(records, 3, &config, entries, 2);
    if (count != 2) {
        set_bank07_message(message, message_size, "BANK07 D861 CAPACITY CONTRACT FAILED");
        return false;
    }
    if (!check_oam_entry(&entries[0], 0x16U, 0x1DU, 0x11U, 0x0DU) ||
        !check_oam_entry(&entries[1], 0xF6U, 0x2DU, 0x12U, 0x0EU)) {
        set_bank07_message(message, message_size, "BANK07 D861 STAGING CONTRACT FAILED");
        return false;
    }

    memset(entries, 0, sizeof(entries));
    count = tecmo_bank07_d861_stage_sprite_records(records, 3, &config, entries, 3);
    if (count != 3 || !check_oam_entry(&entries[2], 0x19U, 0x05U, 0x13U, 0x36U)) {
        set_bank07_message(message, message_size, "BANK07 D861 BYTE WRAP CONTRACT FAILED");
        return false;
    }

    memset(&memory, 0, sizeof(memory));
    tecmo_cpu_ram_write(&memory, BANK07_SPRITE_COUNT_ADDR, 2U);
    tecmo_bank07_d2d2_hide_unused_oam(&memory);
    if (tecmo_cpu_ram_read(&memory, 0x0200U) != 0x00U ||
        tecmo_cpu_ram_read(&memory, 0x0204U) != 0x00U ||
        tecmo_cpu_ram_read(&memory, 0x0208U) != 0xF7U ||
        tecmo_cpu_ram_read(&memory, 0x02FCU) != 0xF7U) {
        set_bank07_message(message, message_size, "BANK07 D2D2 UNUSED OAM CONTRACT FAILED");
        return false;
    }

    memset(&memory, 0, sizeof(memory));
    tecmo_cpu_ram_write(&memory, BANK07_SPRITE_COUNT_ADDR, 0x40U);
    tecmo_bank07_d2d2_hide_unused_oam(&memory);
    if (tecmo_cpu_ram_read(&memory, 0x0200U) != 0x00U ||
        tecmo_cpu_ram_read(&memory, 0x02FCU) != 0x00U) {
        set_bank07_message(message, message_size, "BANK07 D2D2 FULL OAM CONTRACT FAILED");
        return false;
    }

    memset(&memory, 0, sizeof(memory));
    for (uint8_t i = 0; i < 16U; ++i) {
        setup[i] = (uint8_t)(0xA0U + i);
    }
    tecmo_bank07_d700_copy_setup_bytes(&memory, setup);
    for (uint16_t i = 0; i < 16U; ++i) {
        if (tecmo_cpu_ram_read(&memory, (uint16_t)(BANK07_SETUP_FULL_BASE + i)) != setup[i] ||
            tecmo_cpu_ram_read(&memory, (uint16_t)(BANK07_SETUP_NIBBLE_BASE + i)) != (setup[i] & 0x0FU)) {
            set_bank07_message(message, message_size, "BANK07 D700 SETUP COPY CONTRACT FAILED");
            return false;
        }
    }

    tecmo_bank07_sprite_8x16_pair_for_table(0x25U, 1U, pair);
    if (pair[0] != 0x124U || pair[1] != 0x125U) {
        set_bank07_message(message, message_size, "BANK07 SPRITE TABLE ONE PAIR CONTRACT FAILED");
        return false;
    }
    tecmo_bank07_sprite_8x16_pair_for_table(0x80U, 0U, pair);
    if (pair[0] != 0x080U || pair[1] != 0x081U) {
        set_bank07_message(message, message_size, "BANK07 SPRITE TABLE ZERO PAIR CONTRACT FAILED");
        return false;
    }

    memset(&memory, 0, sizeof(memory));
    tecmo_cpu_ram_write(&memory, 0x0352U, 0x01U);
    tecmo_cpu_ram_write(&memory, 0x0100U, 0x05U);
    tecmo_cpu_ram_write(&memory, 0x05B6U, 0x01U);
    tecmo_cpu_ram_write(&memory, 0x0305U, 0x00U);
    tecmo_cpu_ram_write(&memory, 0x0301U, 0x32U);
    tecmo_cpu_ram_write(&memory, 0x0088U, 0xA8U);
    if (!model_bank07_l88e7_cdac_irq_setup(&memory, &irq_proof) ||
        irq_proof.irq_latch != 0x01U ||
        irq_proof.irq_reload != 0x01U ||
        irq_proof.irq_vector != 0xFCF6U ||
        !irq_proof.irq_enabled ||
        irq_proof.phase0_latch != 0x46U ||
        irq_proof.phase1_latch != 0xDAU ||
        irq_proof.phase1_ppu_address != 0x0400U ||
        irq_proof.phase2_ppu_address != 0x0200U) {
        set_bank07_message(message, message_size, "BANK07 L88E7 IRQ SETUP CONTRACT FAILED");
        return false;
    }

    set_bank07_message(message, message_size, "BANK07 C HELPER SELF TEST PASS");
    return true;
}
