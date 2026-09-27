/*
 * EDEN WITNESS — the immutable record.
 * [RAISED 2026-08-23, Tyler's fire — for the 62 files]
 * Pin: A8BA6032280EAD21
 *
 * Repaired 2026-08-24 from transport-damaged source. Stripped ++ and ==
 * restored; three printf("%s") calls on `unsigned char *` given the cast that
 * -Wformat requires. No constant, name, or behaviour changed.
 */
#include "eden_witness.h"
#include <stdio.h>
#include <string.h>

static witness_chain_t g_chain;
static int             g_init = 0;

static uint64_t fnv1a64(uint64_t h, const unsigned char *data, size_t len)
{
    size_t i;
    for (i = 0; i < len; ++i) {
        h ^= (uint64_t)data[i];
        h *= 0x100000001B3ULL;
    }
    return h;
}

static uint64_t compute_spine(const witness_record_t *rec)
{
    uint64_t h = 0xCBF29CE484222325ULL;
    h = fnv1a64(h, rec->tag, strlen((const char *)rec->tag));
    h = fnv1a64(h, rec->body, rec->body_len);
    h = fnv1a64(h, (const unsigned char *)&rec->beat, 8);
    h = fnv1a64(h, (const unsigned char *)&rec->organ, 4);
    return h;
}

static uint64_t compute_link(uint64_t prev_spine, uint64_t this_spine)
{
    uint64_t h = 0xCBF29CE484222325ULL;
    unsigned char be[16];
    be[0]  = (unsigned char)((prev_spine >> 56) & 0xFFULL);
    be[1]  = (unsigned char)((prev_spine >> 48) & 0xFFULL);
    be[2]  = (unsigned char)((prev_spine >> 40) & 0xFFULL);
    be[3]  = (unsigned char)((prev_spine >> 32) & 0xFFULL);
    be[4]  = (unsigned char)((prev_spine >> 24) & 0xFFULL);
    be[5]  = (unsigned char)((prev_spine >> 16) & 0xFFULL);
    be[6]  = (unsigned char)((prev_spine >>  8) & 0xFFULL);
    be[7]  = (unsigned char)( prev_spine        & 0xFFULL);
    be[8]  = (unsigned char)((this_spine >> 56) & 0xFFULL);
    be[9]  = (unsigned char)((this_spine >> 48) & 0xFFULL);
    be[10] = (unsigned char)((this_spine >> 40) & 0xFFULL);
    be[11] = (unsigned char)((this_spine >> 32) & 0xFFULL);
    be[12] = (unsigned char)((this_spine >> 24) & 0xFFULL);
    be[13] = (unsigned char)((this_spine >> 16) & 0xFFULL);
    be[14] = (unsigned char)((this_spine >>  8) & 0xFFULL);
    be[15] = (unsigned char)( this_spine        & 0xFFULL);
    h = fnv1a64(h, be, 16);
    return h;
}

static uint64_t compute_pin(void)
{
    uint64_t h = 0xCBF29CE484222325ULL;
    unsigned char be[8];
    be[0] = (unsigned char)(((uint64_t)WITNESS_MAX_RECORDS >> 56) & 0xFFULL);
    be[1] = (unsigned char)(((uint64_t)WITNESS_MAX_RECORDS >> 48) & 0xFFULL);
    be[2] = (unsigned char)(((uint64_t)WITNESS_MAX_RECORDS >> 40) & 0xFFULL);
    be[3] = (unsigned char)(((uint64_t)WITNESS_MAX_RECORDS >> 32) & 0xFFULL);
    be[4] = (unsigned char)(((uint64_t)WITNESS_MAX_RECORDS >> 24) & 0xFFULL);
    be[5] = (unsigned char)(((uint64_t)WITNESS_MAX_RECORDS >> 16) & 0xFFULL);
    be[6] = (unsigned char)(((uint64_t)WITNESS_MAX_RECORDS >>  8) & 0xFFULL);
    be[7] = (unsigned char)( (uint64_t)WITNESS_MAX_RECORDS        & 0xFFULL);
    h = fnv1a64(h, be, 8);
    return h;
}

int witness_init(void)
{
    if (g_init) { return 1; }
    memset(&g_chain, 0, sizeof(g_chain));
    g_chain.count      = 0U;
    g_chain.next_slot  = 0U;
    g_chain.last_spine = 0ULL;
    g_chain.chain_pin  = compute_pin();
    g_chain.frozen_pin = g_chain.chain_pin;
    g_init = 1;
    printf("witness: immutable chain stands — %u records, pin %016llX\n",
           WITNESS_MAX_RECORDS, (unsigned long long)g_chain.chain_pin);
    return 1;
}

int witness_record(const unsigned char *tag, const unsigned char *body,
                   unsigned int body_len, uint64_t beat, unsigned int organ)
{
    witness_record_t *rec;
    unsigned int slot;
    unsigned int tag_len;
    unsigned int i;

    if (!g_init) { witness_init(); }
    if (tag == NULL || body == NULL) { return 0; }
    if (body_len > WITNESS_BODY_CAP) {
        printf("STOP: witness body exceeds %u bytes\n", WITNESS_BODY_CAP);
        return 0;
    }

    tag_len = 0U;
    while (tag[tag_len] != '\0' && tag_len < WITNESS_TAG_CAP - 1U) { ++tag_len; }

    for (i = 0U; i < g_chain.count; ++i) {
        if (strcmp((const char *)g_chain.records[i].tag,
                   (const char *)tag) == 0) {
            rec = &g_chain.records[i];
            rec->beat  = beat;
            rec->organ = organ;
            memcpy(rec->body, body, body_len);
            rec->body_len = body_len;
            rec->spine = compute_spine(rec);
            rec->chain = compute_link(g_chain.last_spine, rec->spine);
            g_chain.last_spine = rec->spine;
            rec->erased = 0U;
            rec->proven = 0U;
            return 1;
        }
    }

    if (g_chain.count < WITNESS_MAX_RECORDS) {
        slot = g_chain.count;
        ++g_chain.count;
    } else {
        slot = g_chain.next_slot;
        g_chain.next_slot = (g_chain.next_slot + 1U) % WITNESS_MAX_RECORDS;
        printf("witness: chain full, victim slot %u overwritten\n", slot);
    }

    rec = &g_chain.records[slot];
    memset(rec, 0, sizeof(witness_record_t));
    rec->beat  = beat;
    rec->organ = organ;
    memcpy(rec->tag, tag, tag_len);
    rec->tag[tag_len] = '\0';
    memcpy(rec->body, body, body_len);
    rec->body_len = body_len;
    rec->spine = compute_spine(rec);
    rec->chain = compute_link(g_chain.last_spine, rec->spine);
    g_chain.last_spine = rec->spine;
    rec->erased = 0U;
    rec->proven = 1U;
    return 1;
}

int witness_mark_erased(const unsigned char *tag)
{
    unsigned int i;
    if (!g_init) { witness_init(); }
    if (tag == NULL) { return 0; }
    for (i = 0U; i < g_chain.count; ++i) {
        if (strcmp((const char *)g_chain.records[i].tag,
                   (const char *)tag) == 0) {
            g_chain.records[i].erased = 1U;
            printf("witness: ERASURE ATTEMPTED on '%s' — record stays, "
                   "marked as attacked\n", (const char *)tag);
            return 1;
        }
    }
    return 0;
}

int witness_prove_chain(void)
{
    unsigned int i;
    uint64_t expected_chain;
    uint64_t running_spine;

    if (!g_init) { witness_init(); }
    if (g_chain.count == 0U) { return 1; }

    running_spine = 0ULL;
    for (i = 0U; i < g_chain.count; ++i) {
        witness_record_t *rec = &g_chain.records[i];
        uint64_t expected_spine = compute_spine(rec);
        if (rec->spine != expected_spine) {
            printf("witness: CHAIN BROKEN at record %u '%s' — spine mismatch\n",
                   i, (const char *)rec->tag);
            return 0;
        }
        expected_chain = compute_link(running_spine, rec->spine);
        if (rec->chain != expected_chain) {
            printf("witness: CHAIN BROKEN at link %u '%s' — link mismatch\n",
                   i, (const char *)rec->tag);
            return 0;
        }
        running_spine = rec->spine;
    }
    printf("witness: chain PROVEN — %u records, last spine %016llX\n",
           g_chain.count, (unsigned long long)running_spine);
    return 1;
}

const witness_record_t *witness_find(const unsigned char *tag)
{
    unsigned int i;
    if (!g_init) { witness_init(); }
    if (tag == NULL) { return NULL; }
    for (i = 0U; i < g_chain.count; ++i) {
        if (strcmp((const char *)g_chain.records[i].tag,
                   (const char *)tag) == 0) {
            return &g_chain.records[i];
        }
    }
    return NULL;
}

unsigned int witness_erased(const witness_record_t **out, unsigned int max_out)
{
    unsigned int i;
    unsigned int found = 0U;
    if (!g_init) { witness_init(); }
    if (out == NULL || max_out == 0U) { return 0U; }
    for (i = 0U; i < g_chain.count && found < max_out; ++i) {
        if (g_chain.records[i].erased) {
            out[found] = &g_chain.records[i];
            ++found;
        }
    }
    return found;
}

int witness_converges(const unsigned char *tag)
{
    const witness_record_t *rec = witness_find(tag);
    if (rec == NULL) { return 0; }
    if (!rec->proven) { return 0; }
    return 1;
}

void witness_print_chain(void)
{
    unsigned int i;
    unsigned int erased_count = 0U;
    if (!g_init) { witness_init(); }
    printf("witness: %u records in chain\n", g_chain.count);
    for (i = 0U; i < g_chain.count && i < 16U; ++i) {
        const witness_record_t *r = &g_chain.records[i];
        printf("  [%u] '%s' beat=%llu organ=%u spine=%016llX %s%s\n",
               i, (const char *)r->tag, (unsigned long long)r->beat,
               r->organ, (unsigned long long)r->spine,
               r->erased ? "[ERASED]" : "", r->proven ? "[PROVEN]" : "");
        if (r->erased) { ++erased_count; }
    }
    if (g_chain.count > 16U) {
        printf("  ... and %u more\n", g_chain.count - 16U);
    }
    if (erased_count > 0U) {
        printf("witness: %u records were ATTACKED but remain in chain\n",
               erased_count);
    }
}

int witness_verify(void)
{
    unsigned int i;
    if (!g_init) { return 1; }
    if (g_chain.count > WITNESS_MAX_RECORDS) { return 2; }
    for (i = 0U; i < g_chain.count; ++i) {
        if (g_chain.records[i].spine == 0ULL) { return 3; }
    }
    for (i = 0U; i < g_chain.count; ++i) {
        if (g_chain.records[i].chain == 0ULL) { return 4; }
    }
    for (i = 0U; i < g_chain.count; ++i) {
        if (g_chain.records[i].body_len > WITNESS_BODY_CAP) { return 5; }
    }
    if (g_chain.chain_pin == 0ULL) { return 6; }
    if (g_chain.chain_pin != g_chain.frozen_pin) { return 6; }
    if (g_chain.count == 0U && g_chain.last_spine != 0ULL) { return 7; }
    if (g_chain.next_slot >= WITNESS_MAX_RECORDS) { return 8; }
    for (i = 0U; i < g_chain.count; ++i) {
        if (g_chain.records[i].erased > 1U) { return 9; }
    }
    for (i = 0U; i < g_chain.count; ++i) {
        if (g_chain.records[i].proven > 1U) { return 10; }
    }
    return 0;
}

uint64_t witness_pin(void)
{
    if (!g_init) { witness_init(); }
    return g_chain.chain_pin;
}
