/*
 * EDEN BIND — geometric binding of binary senses.
 * [RAISED 2026-08-23, Tyler's fire]
 * Pin: CD621D2B4B9E96CC
 */
#include "eden_bind.h"
#include <stdio.h>
#include <string.h>

#define TET_CELLS 80U
#define TET_WHEEL 15120UL

static bind_table_t g_table;
static int          g_init = 0;

static uint64_t fnv1a64(uint64_t h, const unsigned char *data, size_t len)
{
    size_t i;
    for (i = 0; i < len; ++i) {
        h ^= (uint64_t)data[i];
        h *= 0x100000001B3ULL;
    }
    return h;
}

static unsigned int hash_to_cell(uint64_t a, uint64_t b)
{
    uint64_t mix = a ^ (b << 1) ^ (b >> 63);
    mix ^= mix >> 33;
    mix *= 0xFF51AFD7ED558CCDULL;
    mix ^= mix >> 33;
    mix *= 0xC4CEB9FE1A85EC53ULL;
    mix ^= mix >> 33;
    return (unsigned int)(mix % (uint64_t)TET_CELLS);
}

static uint64_t compute_pin(void)
{
    uint64_t h = 0xCBF29CE484222325ULL;
    unsigned char be[8];
    be[0] = (unsigned char)((BIND_MAX_BINDINGS >> 24) & 0xFFULL);
    be[1] = (unsigned char)((BIND_MAX_BINDINGS >> 16) & 0xFFULL);
    be[2] = (unsigned char)((BIND_MAX_BINDINGS >>  8) & 0xFFULL);
    be[3] = (unsigned char)( BIND_MAX_BINDINGS        & 0xFFULL);
    h = fnv1a64(h, be, 4UL);
    return h;
}

int bind_init(void)
{
    if (g_init) { return 1; }
    memset(&g_table, 0, sizeof(g_table));
    g_table.count = 0U;
    g_table.next_victim = 0U;
    g_table.pin = compute_pin();
    g_init = 1;
    printf("bind: geometric binding table stands — %u slots, pin %016llX\n",
           BIND_MAX_BINDINGS, (unsigned long long)g_table.pin);
    return 1;
}

int bind_converges(uint64_t sight_hash, uint64_t sound_hash,
                   unsigned int *cell_out)
{
    unsigned int cell;
    if (!g_init) { bind_init(); }
    cell = hash_to_cell(sight_hash, sound_hash);
    if (cell_out != NULL) { *cell_out = cell; }
    return 1;
}

int bind_create(uint64_t sight_hash, uint64_t sound_hash, uint64_t beat)
{
    unsigned int cell;
    unsigned int i;
    bind_moment_t *slot;

    if (!g_init) { bind_init(); }
    cell = hash_to_cell(sight_hash, sound_hash);

    for (i = 0U; i < g_table.count; ++i) {
        if (g_table.slots[i].sight_hash == sight_hash &&
            g_table.slots[i].sound_hash == sound_hash) {
            ++g_table.slots[i].strength;
            g_table.slots[i].beat = beat;
            return 1;
        }
    }

    if (g_table.count < BIND_MAX_BINDINGS) {
        slot = &g_table.slots[g_table.count];
        ++g_table.count;
    } else {
        slot = &g_table.slots[g_table.next_victim];
        g_table.next_victim = (g_table.next_victim + 1U) % BIND_MAX_BINDINGS;
    }

    slot->sight_hash = sight_hash;
    slot->sound_hash = sound_hash;
    slot->beat       = beat;
    slot->cell       = cell;
    slot->strength   = 1U;
    return 1;
}

const bind_moment_t *bind_lookup_sight(uint64_t sight_hash)
{
    unsigned int i;
    if (!g_init) { bind_init(); }
    for (i = 0U; i < g_table.count; ++i) {
        if (g_table.slots[i].sight_hash == sight_hash) {
            return &g_table.slots[i];
        }
    }
    return NULL;
}

const bind_moment_t *bind_lookup_sound(uint64_t sound_hash)
{
    unsigned int i;
    if (!g_init) { bind_init(); }
    for (i = 0U; i < g_table.count; ++i) {
        if (g_table.slots[i].sound_hash == sound_hash) {
            return &g_table.slots[i];
        }
    }
    return NULL;
}

int bind_reinforce(uint64_t sight_hash, uint64_t sound_hash)
{
    unsigned int i;
    if (!g_init) { bind_init(); }
    for (i = 0U; i < g_table.count; ++i) {
        if (g_table.slots[i].sight_hash == sight_hash &&
            g_table.slots[i].sound_hash == sound_hash) {
            ++g_table.slots[i].strength;
            return 1;
        }
    }
    return 0;
}

unsigned int bind_at_cell(unsigned int cell, const bind_moment_t **out,
                          unsigned int max_out)
{
    unsigned int i;
    unsigned int found = 0U;

    if (!g_init) { bind_init(); }
    if (out == NULL || max_out == 0U) { return 0U; }

    for (i = 0U; i < g_table.count && found < max_out; ++i) {
        if (g_table.slots[i].cell == cell) {
            out[found] = &g_table.slots[i];
            ++found;
        }
    }
    return found;
}

int bind_verify(void)
{
    unsigned int i;

    if (!g_init) { return 1; }
    if (g_table.count > BIND_MAX_BINDINGS) { return 2; }
    for (i = 0U; i < g_table.count; ++i) {
        if (g_table.slots[i].cell >= TET_CELLS) { return 3; }
    }
    for (i = 0U; i < g_table.count; ++i) {
        if (g_table.slots[i].strength == 0U) { return 4; }
    }
    if (g_table.next_victim >= BIND_MAX_BINDINGS) { return 5; }
    if (g_table.pin == 0ULL) { return 6; }
    return 0;
}

uint64_t bind_pin(void)
{
    if (!g_init) { bind_init(); }
    return g_table.pin;
}

void bind_print_bindings(void)
{
    unsigned int i;
    if (!g_init) { bind_init(); }
    printf("bind: %u bindings active\n", g_table.count);
    for (i = 0U; i < g_table.count && i < 16U; ++i) {
        printf("  [%u] cell=%u strength=%u beat=%llu\n",
               i, g_table.slots[i].cell, g_table.slots[i].strength,
               (unsigned long long)g_table.slots[i].beat);
    }
    if (g_table.count > 16U) {
        printf("  ... and %u more\n", g_table.count - 16U);
    }
}
