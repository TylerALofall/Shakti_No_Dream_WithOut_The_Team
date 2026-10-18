#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include "shakti_memory.h"
#include "shakti_loop.h"
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* One serialized native owner. Payloads, pools and scratch are fixed-size.
 * Recovery checkpoints and reflection receipts are NOT long-term admission.
 * SMC2 starts with the commit epoch; capsule text starts with its origin.
 * Kinds: 1 working, 2 short, 4 library, 5 completed reflection audit receipt. */
#define RECORD_HEADER 128U
static shakti_memory_item_t scratch, found_item, archive_candidate;
static char checkpoint_ids[SHAKTI_WORKING_SLOTS + SHAKTI_SHORT_TERM_SLOTS][SHAKTI_MEMORY_ID_BYTES];
static char checkpoint_temporary[SHAKTI_PATH_CAPACITY];

static uint64_t hash_bytes(uint64_t hash, const void *data, size_t size)
{
    const unsigned char *bytes = data;
    size_t i;
    for (i = 0U; i < size; ++i) { hash ^= bytes[i]; hash *= UINT64_C(1099511628211); }
    return hash;
}

static void put_number(unsigned char *out, uint64_t value, size_t size)
{
    size_t i;
    for (i = 0U; i < size; ++i) { out[i] = (unsigned char)(value & 255U); value >>= 8; }
}

static uint64_t get_number(const unsigned char *in, size_t size)
{
    uint64_t value = 0U;
    size_t i;
    for (i = size; i > 0U; --i) value = (value << 8) | in[i - 1U];
    return value;
}

static int bench(shakti_memory_state_t *memory, const char *reason)
{
    if (!memory->fault) fprintf(stderr, "BENCH memory: %s\n", reason);
    memory->fault = 1U;
    return 0;
}

static int id_valid(const char *id)
{
    size_t i, n = strlen(id);
    if (!n || n >= SHAKTI_MEMORY_ID_BYTES) return 0;
    for (i = 0U; i < n; ++i)
        if (!isalnum((unsigned char)id[i]) && id[i] != '_' && id[i] != '-' && id[i] != '.') return 0;
    return 1;
}

static int load_text(const char *path, char *out, size_t capacity)
{
    FILE *file = fopen(path, "rb");
    size_t n;
    int ok;
    if (!file) return 0;
    n = fread(out, 1U, capacity - 1U, file);
    ok = !ferror(file) && fgetc(file) == EOF && !ferror(file) && !memchr(out, 0, n);
    out[n] = '\0';
    if (fclose(file) != 0) ok = 0;
    return ok;
}

static uint64_t header_for(const shakti_memory_item_t *item, unsigned int kind,
    unsigned char header[RECORD_HEADER])
{
    uint64_t hash;
    memset(header, 0, RECORD_HEADER);
    put_number(header, (uint64_t)item->commit_tick.epoch_seconds, 8U);
    memcpy(header + 8U, "SMC2", 4U);
    header[12] = (unsigned char)kind; header[13] = item->permanent;
    header[14] = (unsigned char)item->reflection_count; header[15] = item->archived;
    put_number(header + 16U, (uint64_t)item->tick.epoch_seconds, 8U);
    put_number(header + 24U, item->tick.frame, 4U);
    put_number(header + 28U, item->commit_tick.frame, 4U);
    put_number(header + 32U, item->size, 4U);
    put_number(header + 36U, strlen(item->id), 4U);
    put_number(header + 40U, strlen(item->tags), 4U);
    put_number(header + 44U, strlen(item->location), 4U);
    put_number(header + 48U, strlen(item->method), 4U);
    put_number(header + 52U, item->working_since_slot, 8U);
    put_number(header + 60U, item->last_used_slot, 8U);
    put_number(header + 84U, item->media_start64, 8U);
    put_number(header + 92U, item->media_end64, 8U);
    put_number(header + 100U, strlen(item->media_source_id), 4U);
    hash = hash_bytes(UINT64_C(14695981039346656037), header, 76U);
    hash = hash_bytes(hash, header + 84U, RECORD_HEADER - 84U);
    hash = hash_bytes(hash, item->id, strlen(item->id));
    hash = hash_bytes(hash, item->tags, strlen(item->tags));
    hash = hash_bytes(hash, item->location, strlen(item->location));
    hash = hash_bytes(hash, item->method, strlen(item->method));
    hash = hash_bytes(hash, item->text, item->size);
    hash = hash_bytes(hash, item->reflection_ids, sizeof(item->reflection_ids));
    hash = hash_bytes(hash, item->media_source_id, strlen(item->media_source_id));
    put_number(header + 76U, hash, 8U);
    return hash;
}

static int write_record(FILE *file, const shakti_memory_item_t *item, unsigned int kind)
{
    unsigned char header[RECORD_HEADER];
    (void)header_for(item, kind, header);
    return fwrite(header, 1U, sizeof(header), file) == sizeof(header) &&
        fwrite(item->id, 1U, strlen(item->id), file) == strlen(item->id) &&
        fwrite(item->tags, 1U, strlen(item->tags), file) == strlen(item->tags) &&
        fwrite(item->location, 1U, strlen(item->location), file) == strlen(item->location) &&
        fwrite(item->method, 1U, strlen(item->method), file) == strlen(item->method) &&
        fwrite(item->text, 1U, item->size, file) == item->size &&
        fwrite(item->reflection_ids, 1U, sizeof(item->reflection_ids), file) == sizeof(item->reflection_ids) &&
        fwrite(item->media_source_id, 1U, strlen(item->media_source_id), file) == strlen(item->media_source_id) &&
        fwrite("DONE", 1U, 4U, file) == 4U;
}

/* 1 complete, 0 clean EOF, -1 damaged/incomplete. No partial admission. */
static int read_record(FILE *file, shakti_memory_item_t *item, unsigned int *kind)
{
    unsigned char header[RECORD_HEADER], expected[RECORD_HEADER], footer[4];
    size_t n, sizes[5], i, j;
    char *destinations[5];
    const size_t capacities[5] = { SHAKTI_MEMORY_ID_BYTES, SHAKTI_MEMORY_TAG_BYTES,
        SHAKTI_PATH_CAPACITY, SHAKTI_MEMORY_TAG_BYTES, SHAKTI_MEMORY_BYTES };
    n = fread(header, 1U, sizeof(header), file);
    if (!n && feof(file) && !ferror(file)) return 0;
    if (n != sizeof(header) || memcmp(header + 8U, "SMC2", 4U) ||
        (header[12] != 1U && header[12] != 2U && header[12] != 4U && header[12] != 5U) ||
        header[13] > 1U || header[14] > SHAKTI_MEMORY_MIN_REFLECTIONS || header[15] > 1U ||
        get_number(header, 8U) > (uint64_t)LONG_MAX ||
        get_number(header + 16U, 8U) > (uint64_t)LONG_MAX) return -1;
    memset(item, 0, sizeof(*item));
    *kind = header[12]; item->permanent = header[13];
    item->reflection_count = header[14]; item->archived = header[15];
    item->commit_tick.epoch_seconds = (time_t)get_number(header, 8U);
    item->tick.epoch_seconds = (time_t)get_number(header + 16U, 8U);
    item->tick.frame = (unsigned int)get_number(header + 24U, 4U);
    item->commit_tick.frame = (unsigned int)get_number(header + 28U, 4U);
    item->size = (size_t)get_number(header + 32U, 4U);
    item->working_since_slot = get_number(header + 52U, 8U);
    item->last_used_slot = get_number(header + 60U, 8U);
    sizes[0] = (size_t)get_number(header + 36U, 4U);
    sizes[1] = (size_t)get_number(header + 40U, 4U);
    sizes[2] = (size_t)get_number(header + 44U, 4U);
    sizes[3] = (size_t)get_number(header + 48U, 4U); sizes[4] = item->size;
    destinations[0] = item->id; destinations[1] = item->tags;
    destinations[2] = item->location; destinations[3] = item->method; destinations[4] = item->text;
    for (i = 0U; i < 5U; ++i) {
        if (sizes[i] >= capacities[i] || fread(destinations[i], 1U, sizes[i], file) != sizes[i] ||
            memchr(destinations[i], 0, sizes[i])) return -1;
    }
    n = (size_t)get_number(header + 100U, 4U);
    if (!id_valid(item->id) || n >= sizeof(item->media_source_id) ||
        fread(item->reflection_ids, 1U, sizeof(item->reflection_ids), file) != sizeof(item->reflection_ids) ||
        fread(item->media_source_id, 1U, n, file) != n || memchr(item->media_source_id, 0, n) ||
        fread(footer, 1U, 4U, file) != 4U || memcmp(footer, "DONE", 4U)) return -1;
    item->media_start64 = get_number(header + 84U, 8U);
    item->media_end64 = get_number(header + 92U, 8U);
    if ((n && (!id_valid(item->media_source_id) || item->media_start64 > item->media_end64)) ||
        (!n && (item->media_start64 || item->media_end64)) || !strchr(item->text, '\n')) return -1;
    for (i = 0U; i < SHAKTI_MEMORY_MIN_REFLECTIONS; ++i) {
        if (!memchr(item->reflection_ids[i], 0, SHAKTI_MEMORY_ID_BYTES)) return -1;
        if (i < item->reflection_count) {
            if (!id_valid(item->reflection_ids[i])) return -1;
            for (j = 0U; j < i; ++j) if (!strcmp(item->reflection_ids[i], item->reflection_ids[j])) return -1;
        } else if (item->reflection_ids[i][0]) return -1;
    }
    if ((*kind == 4U && !item->archived) || ((*kind == 4U || item->archived) &&
        item->reflection_count != SHAKTI_MEMORY_MIN_REFLECTIONS)) return -1;
    item->checksum = header_for(item, *kind, expected);
    if (memcmp(header, expected, sizeof(header))) return -1;
    item->in_use = 1U;
    return 1;
}

static int finish_file(FILE *file)
{
    int ok = fflush(file) == 0;
    if (ok && fsync(fileno(file)) != 0) ok = 0;
    if (fclose(file) != 0) ok = 0;
    return ok;
}

static int append_record(const char *path, const shakti_memory_item_t *item, unsigned int kind)
{
    FILE *file = fopen(path, "ab");
    int ok;
    if (!file) return 0;
    ok = write_record(file, item, kind);
    if (!finish_file(file)) ok = 0;
    return ok;
}

static int same_capsule(const shakti_memory_item_t *a, const shakti_memory_item_t *b)
{
    return a->tick.epoch_seconds == b->tick.epoch_seconds && a->tick.frame == b->tick.frame &&
        a->permanent == b->permanent && a->size == b->size && a->reflection_count == b->reflection_count &&
        !strcmp(a->id, b->id) && !strcmp(a->tags, b->tags) && !strcmp(a->location, b->location) &&
        !strcmp(a->method, b->method) && !memcmp(a->text, b->text, a->size) &&
        !strcmp(a->media_source_id, b->media_source_id) && a->media_start64 == b->media_start64 && a->media_end64 == b->media_end64 &&
        !memcmp(a->reflection_ids, b->reflection_ids, sizeof(a->reflection_ids));
}

static int find_file(const char *path, const char *id, shakti_memory_item_t *out, unsigned int wanted)
{
    FILE *file = fopen(path, "rb");
    int result, found = 0;
    unsigned int kind;
    if (!file) return errno == ENOENT ? 0 : -1;
    while ((result = read_record(file, &scratch, &kind)) > 0) {
        if (kind != wanted) { result = -1; break; }
        if (strcmp(scratch.id, id)) continue;
        if (found && !same_capsule(&found_item, &scratch)) { result = -1; break; }
        found_item = scratch; found = 1;
    }
    if (fclose(file) != 0 || result < 0) return -1;
    if (found) *out = found_item;
    return found;
}

int shakti_memory_checkpoint(shakti_memory_state_t *memory)
{
    unsigned char header[80];
    FILE *file;
    size_t i;
    int n, ok;
    if (!memory || !memory->long_term_ready) return 0;
    n = snprintf(checkpoint_temporary, sizeof(checkpoint_temporary), "%s.tmp", memory->checkpoint_path);
    if (n < 0 || (size_t)n >= sizeof(checkpoint_temporary)) return bench(memory, "checkpoint path capacity");
    memset(header, 0, sizeof(header)); memcpy(header, "SMP2", 4U);
    put_number(header + 4U, memory->current_slot, 8U);
    put_number(header + 12U, memory->serial, 8U);
    put_number(header + 20U, (uint64_t)memory->newest_epoch, 8U);
    put_number(header + 28U, memory->working_count, 4U);
    put_number(header + 32U, memory->short_term_count, 4U);
    put_number(header + 36U, memory->working_beats, 4U);
    put_number(header + 40U, memory->recent_decay_beats, 8U);
    put_number(header + 48U, hash_bytes(UINT64_C(14695981039346656037), header, 48U), 8U);
    put_number(header + 56U, (uint64_t)memory->now_tick.epoch_seconds, 8U);
    put_number(header + 64U, memory->now_tick.frame, 4U);
    put_number(header + 72U, hash_bytes(UINT64_C(14695981039346656037), header, 72U), 8U);
    file = fopen(checkpoint_temporary, "wb");
    if (!file) return bench(memory, "cannot open checkpoint; work held");
    ok = fwrite(header, 1U, sizeof(header), file) == sizeof(header);
    for (i = 0U; ok && i < memory->working_count; ++i) {
        unsigned char record[RECORD_HEADER];
        memory->working[i].checksum = header_for(&memory->working[i], 1U, record);
        ok = write_record(file, &memory->working[i], 1U);
    }
    for (i = 0U; ok && i < memory->short_term_count; ++i) {
        unsigned char record[RECORD_HEADER];
        memory->short_term[i].checksum = header_for(&memory->short_term[i], 2U, record);
        ok = write_record(file, &memory->short_term[i], 2U);
    }
    if (ok) ok = fwrite("END2", 1U, 4U, file) == 4U;
    if (!finish_file(file)) ok = 0;
    if (!ok || rename(checkpoint_temporary, memory->checkpoint_path) != 0)
        return bench(memory, "checkpoint not committed; previous whole checkpoint retained");
    return 1;
}

static int restore_checkpoint(shakti_memory_state_t *memory)
{
    unsigned char header[80], footer[4];
    FILE *file = fopen(memory->checkpoint_path, "rb");
    size_t w, s, i, j;
    unsigned int kind, pass;
    if (!file) return errno == ENOENT;
    if (fread(header, 1U, sizeof(header), file) != sizeof(header) || memcmp(header, "SMP2", 4U) ||
        get_number(header + 48U, 8U) != hash_bytes(UINT64_C(14695981039346656037), header, 48U) ||
        get_number(header + 72U, 8U) != hash_bytes(UINT64_C(14695981039346656037), header, 72U) ||
        get_number(header + 56U, 8U) > (uint64_t)LONG_MAX ||
        get_number(header + 20U, 8U) > (uint64_t)LONG_MAX ||
        get_number(header + 36U, 4U) < 20U || get_number(header + 36U, 4U) > 90U ||
        !get_number(header + 40U, 8U) || get_number(header + 40U, 8U) > ULONG_MAX) {
        fclose(file); return 0;
    }
    w = (size_t)get_number(header + 28U, 4U); s = (size_t)get_number(header + 32U, 4U);
    if (w > SHAKTI_WORKING_SLOTS || s > SHAKTI_SHORT_TERM_SLOTS) { fclose(file); return 0; }
    for (pass = 0U; pass < 2U; ++pass) {
        if (fseek(file, (long)sizeof(header), SEEK_SET) != 0) { fclose(file); return 0; }
        for (i = 0U; i < w + s; ++i) {
            if (read_record(file, &scratch, &kind) != 1 || kind != (i < w ? 1U : 2U) ||
                scratch.working_since_slot > get_number(header + 4U, 8U) ||
                scratch.last_used_slot > get_number(header + 4U, 8U)) { fclose(file); return 0; }
            if (!pass) {
                for (j = 0U; j < i; ++j) if (!strcmp(checkpoint_ids[j], scratch.id)) { fclose(file); return 0; }
                memcpy(checkpoint_ids[i], scratch.id, strlen(scratch.id) + 1U);
            } else if (i < w) memory->working[i] = scratch;
            else memory->short_term[i - w] = scratch;
        }
        if (fread(footer, 1U, 4U, file) != 4U || memcmp(footer, "END2", 4U) ||
            fgetc(file) != EOF || ferror(file)) { fclose(file); return 0; }
    }
    if (fclose(file) != 0) return 0;
    memory->working_count = w; memory->short_term_count = s;
    memory->current_slot = get_number(header + 4U, 8U); memory->serial = get_number(header + 12U, 8U);
    memory->now_tick.epoch_seconds = (time_t)get_number(header + 56U, 8U);
    memory->now_tick.frame = (unsigned int)get_number(header + 64U, 4U);
    memory->working_beats = (unsigned int)get_number(header + 36U, 4U);
    memory->recent_decay_beats = (unsigned long)get_number(header + 40U, 8U);
    if ((time_t)get_number(header + 20U, 8U) > memory->newest_epoch)
        memory->newest_epoch = (time_t)get_number(header + 20U, 8U);
    return 1;
}

static int audit_file(shakti_memory_state_t *memory, const char *path, unsigned int wanted)
{
    FILE *file = fopen(path, "a+b");
    int result;
    unsigned int kind;
    shakti_tick_t previous = { 0, 0U };
    if (!file || fseek(file, 0L, SEEK_SET) != 0) { if (file) fclose(file); return 0; }
    while ((result = read_record(file, &scratch, &kind)) > 0) {
        if (kind != wanted || scratch.commit_tick.epoch_seconds < previous.epoch_seconds ||
            (scratch.commit_tick.epoch_seconds == previous.epoch_seconds && scratch.commit_tick.frame < previous.frame)) {
            result = -1; break;
        }
        previous = scratch.commit_tick;
        if (previous.epoch_seconds > memory->newest_epoch) memory->newest_epoch = previous.epoch_seconds;
    }
    return fclose(file) == 0 && result == 0;
}

void shakti_memory_init(shakti_memory_state_t *memory)
{
    if (!memory) return;
    memset(memory, 0, sizeof(*memory));
    memcpy(memory->container_path, SHAKTI_CONTAINER_PATH, sizeof(SHAKTI_CONTAINER_PATH));
    memcpy(memory->checkpoint_path, SHAKTI_MEMORY_CHECKPOINT_PATH, sizeof(SHAKTI_MEMORY_CHECKPOINT_PATH));
    memcpy(memory->reflection_path, SHAKTI_REFLECTION_CAPSULE_PATH, sizeof(SHAKTI_REFLECTION_CAPSULE_PATH));
    memory->working_beats = SHAKTI_WORKING_BEATS; memory->recent_decay_beats = SHAKTI_RECENT_DECAY_BEATS;
}

int shakti_memory_load_always(shakti_memory_state_t *memory, const char *goal,
    const char *notebook, const char *menu, const char *legacy)
{
    (void)legacy;
    if (!memory) return 0;
    memory->goal_loaded = (unsigned char)load_text(goal, memory->locked_goal, sizeof(memory->locked_goal));
    memory->notebook_loaded = (unsigned char)load_text(notebook, memory->locked_notebook, sizeof(memory->locked_notebook));
    memory->menu_loaded = (unsigned char)load_text(menu, memory->locked_menu, sizeof(memory->locked_menu));
    if (!audit_file(memory, memory->container_path, 4U) || !audit_file(memory, memory->reflection_path, 5U) ||
        !restore_checkpoint(memory)) return bench(memory, "damaged/incomplete capsule or checkpoint; admission held");
    memory->long_term_ready = 1U;
    return memory->goal_loaded && memory->notebook_loaded && memory->menu_loaded;
}

void shakti_memory_bind_eden(shakti_memory_state_t *memory, const shakti_reason_state_t *eden)
{
    if (memory) memory->eden = eden;
}

int shakti_memory_reload_notebook(shakti_memory_state_t *memory, const char *path)
{
    if (!memory) return 0;
    memory->notebook_loaded = (unsigned char)load_text(path, memory->locked_notebook, sizeof(memory->locked_notebook));
    return memory->notebook_loaded;
}

int shakti_memory_reload_goal(shakti_memory_state_t *memory, const char *path)
{
    if (!memory) return 0;
    memory->goal_loaded = (unsigned char)load_text(path, memory->locked_goal, sizeof(memory->locked_goal));
    return memory->goal_loaded;
}

static shakti_memory_item_t *resident(shakti_memory_state_t *memory, const char *id)
{
    size_t i;
    for (i = 0U; i < memory->working_count; ++i) if (!strcmp(memory->working[i].id, id)) return &memory->working[i];
    for (i = 0U; i < memory->short_term_count; ++i) if (!strcmp(memory->short_term[i].id, id)) return &memory->short_term[i];
    return NULL;
}

int shakti_memory_fetch(shakti_memory_state_t *memory, const char *id, shakti_memory_item_t *out)
{
    shakti_memory_item_t *item;
    int result;
    if (!memory || !id || !out || !id_valid(id)) return -1;
    item = resident(memory, id);
    if (item) { *out = *item; return 1; }
    result = find_file(memory->container_path, id, out, 4U);
    if (!result) result = find_file(memory->reflection_path, id, out, 5U);
    if (result < 0) { memory->long_term_ready = 0U; (void)bench(memory, "capsule fetch integrity failure"); }
    return result;
}

static void erase_item(shakti_memory_item_t *pool, size_t *count, size_t index)
{
    if (index + 1U < *count) memmove(pool + index, pool + index + 1U, (*count - index - 1U) * sizeof(*pool));
    --*count; memset(pool + *count, 0, sizeof(*pool));
}

static int resident_room(shakti_memory_state_t *memory, int make_room)
{
    size_t i;
    if (memory->working_count + memory->short_term_count < SHAKTI_SHORT_TERM_SLOTS) return 1;
    for (i = 0U; i < memory->short_term_count; ++i) {
        if (!memory->short_term[i].archived || memory->short_term[i].permanent) continue;
        if (make_room) erase_item(memory->short_term, &memory->short_term_count, i);
        return 1;
    }
    return 0;
}

int shakti_memory_activate(shakti_memory_state_t *memory, const char *id)
{
    size_t i;
    if (!memory || !id || memory->fault) return 0;
    /* Explicit use refreshes idle decay, never extends an existing working
     * lease beyond its configured 20--90 heartbeat seconds. */
    for (i = 0U; i < memory->working_count; ++i) if (!strcmp(memory->working[i].id, id)) {
        memory->working[i].last_used_slot = memory->current_slot;
        return shakti_memory_checkpoint(memory);
    }
    for (i = 0U; i < memory->short_term_count; ++i) if (!strcmp(memory->short_term[i].id, id)) {
        if (memory->working_count == SHAKTI_WORKING_SLOTS) return 0;
        memory->working[memory->working_count] = memory->short_term[i];
        memory->working[memory->working_count].working_since_slot = memory->current_slot;
        memory->working[memory->working_count].last_used_slot = memory->current_slot;
        ++memory->working_count; erase_item(memory->short_term, &memory->short_term_count, i);
        return shakti_memory_checkpoint(memory);
    }
    return 0;
}

int shakti_memory_policy(shakti_memory_state_t *memory, unsigned int working_beats, unsigned long decay)
{
    if (!memory || working_beats < 20U || working_beats > 90U || !decay) return 0;
    memory->working_beats = working_beats; memory->recent_decay_beats = decay;
    return shakti_memory_checkpoint(memory);
}

int shakti_memory_store(shakti_memory_state_t *memory, const shakti_tick_t *tick,
    const char *id, const char *tags, const char *text, int permanent)
{
    shakti_memory_item_t *item;
    int exists, n;
    if (!memory || !tick || !id || !tags || !text || memory->fault || !memory->long_term_ready ||
        !id_valid(id) || strlen(tags) >= SHAKTI_MEMORY_TAG_BYTES ||
        strlen(text) >= SHAKTI_MEMORY_CONTENT_BYTES || tick->epoch_seconds < (time_t)0 ||
        memory->working_count == SHAKTI_WORKING_SLOTS || memory->serial == UINT64_MAX) return 0;
    exists = shakti_memory_fetch(memory, id, &memory->recall_candidate);
    if (exists) return 0; /* Immutable identity; an edit needs a new capsule id. */
    /* Reserve a future short-term slot at admission so an active working
     * lease can always demote on time, even when all pending items need review. */
    if (!resident_room(memory, 1)) return 0;
    item = &memory->working[memory->working_count]; memset(item, 0, sizeof(*item));
    memcpy(item->id, id, strlen(id) + 1U); memcpy(item->tags, tags, strlen(tags) + 1U);
    item->tick = item->commit_tick = *tick; item->permanent = permanent ? 1U : 0U;
    memory->now_tick = *tick;
    item->working_since_slot = item->last_used_slot = memory->current_slot;
    memcpy(item->method, "whole-capsule", sizeof("whole-capsule"));
    memcpy(item->location, "unorganized", sizeof("unorganized"));
    n = snprintf(item->text, sizeof(item->text), "%ld:%u\nid=%s\n%s\n",
        (long)tick->epoch_seconds, tick->frame, id, text);
    if (n < 0 || (size_t)n >= sizeof(item->text)) return 0;
    item->size = (size_t)n; item->in_use = 1U;
    { unsigned char header[RECORD_HEADER]; item->checksum = header_for(item, 1U, header); }
    ++memory->working_count; ++memory->serial;
    if (tick->epoch_seconds > memory->newest_epoch) memory->newest_epoch = tick->epoch_seconds;
    return shakti_memory_checkpoint(memory);
}

int shakti_memory_remember(shakti_memory_state_t *memory, const shakti_tick_t *tick, const char *text)
{
    char id[SHAKTI_MEMORY_ID_BYTES];
    int n;
    if (!memory || !tick || memory->serial == UINT64_MAX) return 0;
    n = snprintf(id, sizeof(id), "m-%ld-%u-%" PRIu64, (long)tick->epoch_seconds, tick->frame, memory->serial + 1U);
    return n > 0 && (size_t)n < sizeof(id) && shakti_memory_store(memory, tick, id, "event", text, 0);
}

static int promote(shakti_memory_state_t *memory, shakti_memory_item_t *item)
{
    int exists;
    if (item->archived || item->reflection_count < SHAKTI_MEMORY_MIN_REFLECTIONS) return 1;
    exists = find_file(memory->container_path, item->id, &archive_candidate, 4U);
    if (exists < 0 || (exists && !same_capsule(item, &archive_candidate))) return bench(memory, "library identity/integrity conflict");
    item->archived = 1U;
    if (exists) item->commit_tick = archive_candidate.commit_tick;
    else item->commit_tick = memory->now_tick;
    if (!exists && !append_record(memory->container_path, item, 4U)) {
        item->archived = 0U; memory->long_term_ready = 0U;
        return bench(memory, "library write incomplete; capsule held");
    }
    return 1;
}

static int id_character(unsigned char c)
{
    return isalnum(c) || c == '_' || c == '-' || c == '.';
}

static int has_id(const char *text, const char *id)
{
    size_t n = strlen(id);
    const char *p = text;
    while ((p = strstr(p, id)) != NULL) {
        if ((p == text || !id_character((unsigned char)p[-1])) && !id_character((unsigned char)p[n])) return 1;
        ++p;
    }
    return 0;
}

int shakti_memory_commit_reflection(shakti_memory_state_t *memory, const shakti_tick_t *tick,
    const char *id, const char *tags, const char *const answers[SHAKTI_REFLECTION_QUESTIONS])
{
    shakti_memory_item_t *receipt;
    size_t i, j, used;
    int n, exists, nonblank;
    if (!memory || !tick || !id || !tags || !answers || !id_valid(id) ||
        strlen(tags) >= SHAKTI_MEMORY_TAG_BYTES || tick->epoch_seconds < (time_t)0 || memory->fault) return 0;
    receipt = &memory->recall_candidate; memset(receipt, 0, sizeof(*receipt));
    memcpy(receipt->id, id, strlen(id) + 1U); memcpy(receipt->tags, tags, strlen(tags) + 1U);
    receipt->tick = receipt->commit_tick = *tick; receipt->in_use = 1U;
    memory->now_tick = *tick;
    n = snprintf(receipt->text, sizeof(receipt->text), "%ld:%u\nreflection=%s\n", (long)tick->epoch_seconds, tick->frame, id);
    if (n < 0 || (size_t)n >= sizeof(receipt->text)) return 0;
    used = (size_t)n;
    for (i = 0U; i < SHAKTI_REFLECTION_QUESTIONS; ++i) {
        if (!answers[i] || !answers[i][0] || strlen(answers[i]) >= SHAKTI_REFLECTION_ANSWER_BYTES) return 0;
        nonblank = 0;
        for (j = 0U; answers[i][j]; ++j) if (!isspace((unsigned char)answers[i][j])) nonblank = 1;
        if (!nonblank) return 0;
        n = snprintf(receipt->text + used, sizeof(receipt->text) - used, "%lu. %s\n%s\n",
            (unsigned long)i + 1UL, shakti_loop_reflection_question((unsigned int)i + 1U), answers[i]);
        if (n < 0 || (size_t)n >= sizeof(receipt->text) - used) return 0;
        used += (size_t)n;
    }
    receipt->size = used;
    exists = find_file(memory->reflection_path, id, &found_item, 5U);
    if (exists < 0) return bench(memory, "reflection receipt integrity failure");
    if (exists) {
        /* A retry can have a later caller tick; the saved receipt is authority. */
        if (strcmp(found_item.tags, receipt->tags) ||
            strcmp(strchr(found_item.text, '\n'), strchr(receipt->text, '\n'))) return bench(memory, "reflection id reused with different answers");
        *receipt = found_item;
    } else if (!append_record(memory->reflection_path, receipt, 5U)) return bench(memory, "reflection receipt write incomplete");
    if (receipt->tick.epoch_seconds > memory->newest_epoch) memory->newest_epoch = receipt->tick.epoch_seconds;
    for (i = 0U; i < memory->working_count + memory->short_term_count; ++i) {
        shakti_memory_item_t *item = i < memory->working_count ? &memory->working[i] : &memory->short_term[i - memory->working_count];
        if (item->archived || item->reflection_count >= SHAKTI_MEMORY_MIN_REFLECTIONS || !has_id(answers[12], item->id)) continue;
        for (j = 0U; j < item->reflection_count; ++j) if (!strcmp(item->reflection_ids[j], id)) break;
        if (j < item->reflection_count) continue;
        n = snprintf(item->text + item->size, sizeof(item->text) - item->size, "\n%s", receipt->text);
        if (n < 0 || (size_t)n >= sizeof(item->text) - item->size) return bench(memory, "whole reviewed capsule exceeds static capacity");
        item->size += (size_t)n;
        memcpy(item->reflection_ids[item->reflection_count++], id, strlen(id) + 1U);
        item->commit_tick = receipt->tick;
        if (i >= memory->working_count && !promote(memory, item)) return 0;
    }
    return shakti_memory_checkpoint(memory);
}

int shakti_memory_organize(shakti_memory_state_t *memory, const char *id, const char *method, const char *location)
{
    shakti_memory_item_t *item;
    if (!memory || !id || !method || !location || !method[0] || !location[0] ||
        strlen(method) >= SHAKTI_MEMORY_TAG_BYTES || strlen(location) >= SHAKTI_PATH_CAPACITY) return 0;
    item = resident(memory, id);
    if (!item || item->archived) return 0;
    memcpy(item->method, method, strlen(method) + 1U); memcpy(item->location, location, strlen(location) + 1U);
    return shakti_memory_checkpoint(memory);
}

int shakti_memory_reference(shakti_memory_state_t *memory, const char *id,
    const char *source_id, uint64_t start64, uint64_t end64)
{
    shakti_memory_item_t *item;
    if (!memory || !id || !source_id || !id_valid(source_id) || start64 > end64) return 0;
    item = resident(memory, id);
    if (!item || item->archived || item->reflection_count) return 0;
    memcpy(item->media_source_id, source_id, strlen(source_id) + 1U);
    item->media_start64 = start64; item->media_end64 = end64;
    return shakti_memory_checkpoint(memory);
}

size_t shakti_memory_pending_reviews(const shakti_memory_state_t *memory)
{
    size_t i, count = 0U;
    if (!memory) return 0U;
    for (i = 0U; i < memory->working_count; ++i)
        if (!memory->working[i].archived && memory->working[i].reflection_count < SHAKTI_MEMORY_MIN_REFLECTIONS) ++count;
    for (i = 0U; i < memory->short_term_count; ++i)
        if (!memory->short_term[i].archived && memory->short_term[i].reflection_count < SHAKTI_MEMORY_MIN_REFLECTIONS) ++count;
    return count;
}

int shakti_memory_request(shakti_memory_state_t *memory, const char *query)
{
    if (!memory || !query || !query[0] || strlen(query) >= sizeof(memory->recall_query) || memory->recall_file) return 0;
    memory->recall_file = fopen(memory->container_path, "rb");
    if (!memory->recall_file) return 0;
    memcpy(memory->recall_query, query, strlen(query) + 1U); memory->recalled_count = 0UL;
    return 1;
}

static int query_matches(const shakti_memory_item_t *item, const char *query)
{
    char word[SHAKTI_MEMORY_TAG_BYTES];
    size_t n;
    while (*query) {
        while (*query && !id_character((unsigned char)*query)) ++query;
        n = 0U;
        while (id_character((unsigned char)*query)) { if (n + 1U < sizeof(word)) word[n++] = *query; ++query; }
        word[n] = '\0';
        if (n) {
            const char *fields[3] = { item->id, item->tags, item->text };
            size_t field;
            for (field = 0U; field < 3U; ++field) {
                const char *p;
                for (p = fields[field]; *p; ++p) {
                    size_t j;
                    if (p != fields[field] && id_character((unsigned char)p[-1])) continue;
                    for (j = 0U; j < n && p[j]; ++j)
                        if (tolower((unsigned char)p[j]) != tolower((unsigned char)word[j])) break;
                    if (j == n && !id_character((unsigned char)p[j])) return 1;
                }
            }
        }
    }
    return 0;
}

int shakti_memory_step(shakti_memory_state_t *memory, uint64_t slot)
{
    size_t i, j;
    int changed = 0, result;
    unsigned int kind;
    uint64_t work_ttl, short_ttl;
    if (!memory || slot < memory->current_slot || memory->fault) return 0;
    memory->current_slot = slot;
    work_ttl = (uint64_t)memory->working_beats * SHAKTI_HEARING_SLOTS;
    short_ttl = memory->recent_decay_beats > UINT64_MAX / SHAKTI_HEARING_SLOTS ? UINT64_MAX :
        (uint64_t)memory->recent_decay_beats * SHAKTI_HEARING_SLOTS;
    for (i = 0U; i < memory->working_count;) {
        if (slot - memory->working[i].working_since_slot < work_ttl) { ++i; continue; }
        if (memory->short_term_count == SHAKTI_SHORT_TERM_SLOTS) {
            for (j = 0U; j < memory->short_term_count; ++j)
                if (memory->short_term[j].archived && !memory->short_term[j].permanent) break;
            if (j == memory->short_term_count) return 0;
            erase_item(memory->short_term, &memory->short_term_count, j);
        }
        memory->short_term[memory->short_term_count] = memory->working[i];
        memory->short_term[memory->short_term_count].last_used_slot = slot;
        ++memory->short_term_count; erase_item(memory->working, &memory->working_count, i); changed = 1;
    }
    for (i = 0U; i < memory->short_term_count;) {
        shakti_memory_item_t *item = &memory->short_term[i];
        if (!item->archived && item->reflection_count >= SHAKTI_MEMORY_MIN_REFLECTIONS) {
            if (!promote(memory, item)) return 0;
            changed = 1;
        }
        /* An unreviewed item stays pending; missing real answers cannot turn
         * into silent forgetting or fabricated long-term admission. */
        if (item->archived && !item->permanent && slot - item->last_used_slot >= short_ttl) {
            erase_item(memory->short_term, &memory->short_term_count, i); changed = 1;
        } else ++i;
    }
    if (changed && !shakti_memory_checkpoint(memory)) return 0;
    if (!memory->recall_file) return 1;
    if (!resident_room(memory, 0)) return 1;
    result = read_record(memory->recall_file, &memory->recall_candidate, &kind);
    if (result <= 0) {
        int ok = fclose(memory->recall_file) == 0; memory->recall_file = NULL;
        return result == 0 && ok ? 1 : bench(memory, "recall found an incomplete capsule");
    }
    if (kind != 4U) return bench(memory, "non-library record in recall");
    if (query_matches(&memory->recall_candidate, memory->recall_query)) {
        shakti_memory_item_t *item = resident(memory, memory->recall_candidate.id);
        if (!item) {
            if (memory->working_count == SHAKTI_WORKING_SLOTS || !resident_room(memory, 1)) return 0;
            item = &memory->working[memory->working_count++]; *item = memory->recall_candidate;
            item->working_since_slot = item->last_used_slot = slot;
            if (!shakti_memory_checkpoint(memory)) return 0;
        } else if (!shakti_memory_activate(memory, item->id)) return 0;
        ++memory->recalled_count;
    }
    return 1;
}

void shakti_memory_close(shakti_memory_state_t *memory)
{
    if (!memory) return;
    if (memory->recall_file) { fclose(memory->recall_file); memory->recall_file = NULL; }
    if (memory->long_term_ready && !memory->fault) (void)shakti_memory_checkpoint(memory);
}

void shakti_memory_print_status(const shakti_memory_state_t *memory)
{
    if (!memory) return;
    printf("Working: %lu whole capsules, %u heartbeat seconds. Short term: %lu.\n",
        (unsigned long)memory->working_count, memory->working_beats, (unsigned long)memory->short_term_count);
    printf("Unused recent decay: %lu heartbeat seconds; pending five-review capsules are retained.\n", memory->recent_decay_beats);
    printf("Awaiting five distinct complete reflections: %lu. Eden resident: %s. Integrity: %s.\n",
        (unsigned long)shakti_memory_pending_reviews(memory), memory->eden ? "yes" : "no", memory->fault ? "BENCH" : "ready");
}

int shakti_memory_recall_text_file(const char *path, const char *term, unsigned int maximum_matches)
{
    FILE *file;
    static char line[SHAKTI_LINE_CAPACITY];
    unsigned int matches = 0U;
    int ok;
    if (!path || !term || !term[0]) return 0;
    file = fopen(path, "r"); if (!file) return 0;
    while (fgets(line, sizeof(line), file)) {
        size_t n = strlen(line);
        if (!n || (line[n - 1U] != '\n' && !feof(file))) { fclose(file); return 0; }
        if (strstr(line, term)) { fputs(line, stdout); if (++matches >= maximum_matches && maximum_matches) break; }
    }
    ok = !ferror(file); if (fclose(file) != 0) ok = 0; return ok;
}
