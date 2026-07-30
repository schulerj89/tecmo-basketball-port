#ifndef TECMO_GAMEPLAY_FREE_THROW_PROJECTION_TEST_H
#define TECMO_GAMEPLAY_FREE_THROW_PROJECTION_TEST_H

#include <stdbool.h>
#include <stddef.h>

/*
 * Test-only TGFL-1 -> TGCP-1 composition. This does not add either asset to
 * the live gameplay scene or create a production dependency between them.
 */
bool tecmo_gameplay_free_throw_projection_self_test(
    const char *asset_pack_path,
    char *message,
    size_t message_size);

#endif
