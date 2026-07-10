#define _CRT_SECURE_NO_WARNINGS

#include "tecmo_asset_pack_d9f6.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static void set_message(char *message, size_t message_size, const char *text)
{
    if (message == NULL || message_size == 0U) return;
    (void)snprintf(message, message_size, "%s", text != NULL ? text : "");
}

static void set_messagef(char *message,
                         size_t message_size,
                         const char *format,
                         ...)
{
    va_list args;
    if (message == NULL || message_size == 0U) return;
    if (format == NULL) {
        message[0] = '\0';
        return;
    }
    va_start(args, format);
    (void)vsnprintf(message, message_size, format, args);
    va_end(args);
}

int tecmo_asset_pack_decode_d9f6_stream(const uint8_t *bank_bytes,
                                         size_t bank_size,
                                         size_t stream_offset,
                                         uint8_t *decoded,
                                         size_t decoded_size,
                                         size_t *encoded_size_out,
                                         char *message,
                                         size_t message_size)
{
    size_t source = stream_offset;
    size_t output = 0U;

    if (bank_bytes == NULL || decoded == NULL || stream_offset >= bank_size) {
        set_message(message, message_size, "Arena screen stream starts outside its PRG bank.");
        return -1;
    }

    for (;;) {
        uint8_t control;

        if (source >= bank_size) {
            set_message(message, message_size, "Arena screen stream ended before a terminator.");
            return -1;
        }
        control = bank_bytes[source++];

        for (unsigned int slot = 0U; slot < 4U; ++slot) {
            unsigned int operation = (unsigned int)(control & 0x03U);
            size_t count;

            control >>= 2U;
            if (source >= bank_size) {
                set_message(message, message_size, "Arena screen stream is missing an operation count.");
                return -1;
            }
            count = bank_bytes[source++];
            if (count == 0U) count = 256U;

            if (operation == 0U) {
                if (output != decoded_size) {
                    set_messagef(message,
                                 message_size,
                                 "Arena screen stream terminated after %llu decoded bytes; expected %llu.",
                                 (unsigned long long)output,
                                 (unsigned long long)decoded_size);
                    return -1;
                }
                if (encoded_size_out != NULL) {
                    *encoded_size_out = source - stream_offset;
                }
                return 0;
            }
            if (count > decoded_size - output) {
                set_message(message, message_size, "Arena screen stream decodes past two nametables.");
                return -1;
            }

            if (operation == 1U) {
                if (count > bank_size - source) {
                    set_message(message, message_size, "Arena screen literal crosses its PRG bank.");
                    return -1;
                }
                memcpy(decoded + output, bank_bytes + source, count);
                source += count;
            } else if (operation == 2U) {
                if (source >= bank_size) {
                    set_message(message, message_size, "Arena screen repeat is missing its byte.");
                    return -1;
                }
                memset(decoded + output, bank_bytes[source++], count);
            } else {
                size_t delta_offset = source;
                size_t delta;
                size_t copy_source;

                if (bank_size - source < 2U) {
                    set_message(message, message_size, "Arena screen back-copy is missing its delta.");
                    return -1;
                }
                delta = (size_t)bank_bytes[source] |
                        ((size_t)bank_bytes[source + 1U] << 8U);
                source += 2U;

                /* D9F6 subtracts from the address of the delta, then resumes after it. */
                if (delta > delta_offset) {
                    set_message(message, message_size, "Arena screen back-copy underflows its PRG bank.");
                    return -1;
                }
                copy_source = delta_offset - delta;
                if (count > bank_size - copy_source) {
                    set_message(message, message_size, "Arena screen back-copy crosses its PRG bank.");
                    return -1;
                }
                memcpy(decoded + output, bank_bytes + copy_source, count);
            }
            output += count;
        }
    }
}
