#include "shakti_memory.h"

#include <ctype.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

/* SMC1 records commit a complete, length-delimited container and a DONE footer.
 * One Shakti instance owns this state and its append-only container file. */
static uint64_t hash_bytes(uint64_t hash, const void *data, size_t size)
{
    const unsigned char *bytes = data;
    size_t i;
    for (i = 0U; i < size; ++i) {
        hash ^= bytes[i];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static void put_number(unsigned char *out, uint64_t value, size_t size)
{
    size_t i;
    for (i = 0U; i < size; ++i) {
        out[i] = (unsigned char)(value & UINT64_C(255));
        value >>= 8;
    }
}

static uint64_t get_number(const unsigned char *in, size_t size)
{
    uint64_t value = 0U;
    size_t i;
    for (i = size; i > 0U; --i) value = (value << 8) | in[i - 1U];
    return value;
}

static int memory_bench(shakti_memory_state_t *memory, const char *reason)
{
    if (!memory->fault) fprintf(stderr, "BENCH memory: %s\n", reason);
    memory->fault = 1U;
    return 0;
}

static int load_text(const char *path, char *out, size_t capacity)
{
    FILE *file;
    size_t size;
    int extra;
    if (path == NULL || capacity == 0U) return 0;
    file = fopen(path, "rb");
    if (file == NULL) return 0;
    size = fread(out, 1U, capacity - 1U, file);
    extra = fgetc(file);
    if (ferror(file) || extra != EOF || memchr(out, 0, size) != NULL) {
        fclose(file);
        return 0;
    }
    out[size] = '\0';
    return fclose(file) == 0;
}

static int id_valid(const char *id)
{
    size_t i;
    size_t size = strlen(id);
    if (size == 0U || size >= SHAKTI_MEMORY_ID_BYTES) return 0;
    for (i = 0U; i < size; ++i) {
        unsigned char c = (unsigned char)id[i];
        if (!isalnum(c) && c != '_' && c != '-' && c != '.') return 0;
    }
    return 1;
}

static uint64_t record_header(const shakti_memory_item_t *item, unsigned char header[40])
{
    uint64_t hash;
    memset(header, 0, 40U);
    memcpy(header, "SMC1", 4U);
    header[4] = item->permanent;
    put_number(header + 8U, (uint64_t)item->tick.epoch_seconds, 8U);
    put_number(header + 16U, item->tick.frame, 4U);
    put_number(header + 20U, strlen(item->id), 4U);
    put_number(header + 24U, strlen(item->tags), 4U);
    put_number(header + 28U, item->size, 4U);
    hash = hash_bytes(UINT64_C(14695981039346656037), header, 32U);
    hash = hash_bytes(hash, item->id, strlen(item->id));
    hash = hash_bytes(hash, item->tags, strlen(item->tags));
    hash = hash_bytes(hash, item->text, item->size);
    put_number(header + 32U, hash, 8U);
    return hash;
}

/* 1 complete record; 0 clean end; -1 corrupt, incomplete, or over capacity. */
static int read_record(FILE *file, shakti_memory_item_t *item)
{
    unsigned char header[40], expected[40], footer[4];
    size_t n, ids, tags, bytes;
    uint64_t epoch, stored;
    n = fread(header, 1U, sizeof(header), file);
    if (n == 0U && feof(file) && !ferror(file)) return 0;
    if (n != sizeof(header) || memcmp(header, "SMC1", 4U) != 0 ||
        header[4] > 1U || header[5] || header[6] || header[7]) return -1;
    epoch = get_number(header + 8U, 8U);
    ids = (size_t)get_number(header + 20U, 4U);
    tags = (size_t)get_number(header + 24U, 4U);
    bytes = (size_t)get_number(header + 28U, 4U);
    if (epoch > (uint64_t)LONG_MAX || ids == 0U ||
        ids >= sizeof(item->id) || tags >= sizeof(item->tags) ||
        bytes >= sizeof(item->text)) return -1;
    memset(item, 0, sizeof(*item));
    item->permanent = header[4];
    item->tick.epoch_seconds = (time_t)epoch;
    item->tick.frame = (unsigned int)get_number(header + 16U, 4U);
    item->size = bytes;
    if (fread(item->id, 1U, ids, file) != ids ||
        fread(item->tags, 1U, tags, file) != tags ||
        fread(item->text, 1U, bytes, file) != bytes ||
        fread(footer, 1U, 4U, file) != 4U || memcmp(footer, "DONE", 4U) != 0 ||
        memchr(item->id, 0, ids) || memchr(item->tags, 0, tags) ||
        memchr(item->text, 0, bytes) || !id_valid(item->id)) return -1;
    stored = get_number(header + 32U, 8U);
    item->checksum = record_header(item, expected);
    if (item->checksum != stored || memcmp(header, expected, 40U) != 0) return -1;
    item->in_use = 1U;
    return 1;
}

static int word_character(unsigned char c)
{
    return isalnum(c) || c == '_';
}

static int contains_word(const char *text, const char *word, size_t size)
{
    size_t i, j;
    for (i = 0U; text[i] != '\0'; ++i) {
        if (i > 0U && word_character((unsigned char)text[i - 1U])) continue;
        for (j = 0U; j < size && text[i + j] != '\0'; ++j) {
            if (tolower((unsigned char)text[i + j]) != tolower((unsigned char)word[j])) break;
        }
        if (j == size && !word_character((unsigned char)text[i + j])) return 1;
    }
    return 0;
}

static int query_matches(const shakti_memory_item_t *item, const char *query)
{
    size_t start, end = 0U;
    while (query[end] != '\0') {
        while (query[end] && !word_character((unsigned char)query[end])) ++end;
        start = end;
        while (word_character((unsigned char)query[end])) ++end;
        if (end > start && (contains_word(item->tags, query + start, end - start) ||
            contains_word(item->text, query + start, end - start))) return 1;
    }
    return 0;
}

static size_t cache_slot(const shakti_memory_state_t *memory, const shakti_memory_item_t *item)
{
    size_t i, chosen = SHAKTI_SHORT_TERM_SLOTS, available = SHAKTI_SHORT_TERM_SLOTS;
    uint64_t oldest = UINT64_MAX;
    for (i = 0U; i < SHAKTI_SHORT_TERM_SLOTS; ++i) {
        const shakti_memory_item_t *slot = &memory->short_term[i];
        if (slot->in_use && strcmp(slot->id, item->id) == 0) return i;
        if (!slot->in_use) {
            if (available == SHAKTI_SHORT_TERM_SLOTS) available = i;
            continue;
        }
        if (!slot->permanent && slot->last_used_slot < oldest &&
            (memory->working_count == 0U || strcmp(slot->id, memory->working[0].id) != 0)) {
            chosen = i;
            oldest = slot->last_used_slot;
        }
    }
    return available != SHAKTI_SHORT_TERM_SLOTS ? available : chosen;
}

static void remove_working(shakti_memory_state_t *memory, const char *id)
{
    size_t read, write = 0U, old_count = memory->working_count;
    for (read = 0U; read < old_count; ++read) {
        if (strcmp(memory->working[read].id, id) != 0) {
            if (write != read) memory->working[write] = memory->working[read];
            ++write;
        }
    }
    memory->working_count = write;
    if (write < old_count) memset(memory->working + write, 0,
        (old_count - write) * sizeof(memory->working[0]));
}

static int install(shakti_memory_state_t *memory, const shakti_memory_item_t *item)
{
    size_t i = cache_slot(memory, item);
    if (i == SHAKTI_SHORT_TERM_SLOTS) return memory_bench(memory, "RAM has no replaceable whole-container slot");
    if (memory->short_term[i].in_use && strcmp(memory->short_term[i].id, item->id) == 0 &&
        memory->short_term[i].checksum != item->checksum) return memory_bench(memory, "immutable memory id conflict");
    if (!memory->short_term[i].in_use) ++memory->short_term_count;
    else if (strcmp(memory->short_term[i].id, item->id) != 0)
        remove_working(memory, memory->short_term[i].id);
    memory->short_term[i] = *item;
    memory->short_term[i].last_used_slot = memory->current_slot;
    memory->short_term[i].in_use = 1U;
    return shakti_memory_activate(memory, item->id);
}

void shakti_memory_init(shakti_memory_state_t *memory)
{
    if (memory == NULL) return;
    memset(memory, 0, sizeof(*memory));
    memcpy(memory->container_path, SHAKTI_CONTAINER_PATH, sizeof(SHAKTI_CONTAINER_PATH));
    /* Zero leaves the time-based decay rate for Shakti to choose. LRU RAM
     * replacement is active; no long-term container is deleted. */
}

int shakti_memory_load_always(shakti_memory_state_t *memory, const char *goal_path,
    const char *notebook_path, const char *menu_path, const char *long_term_path)
{
    FILE *file;
    (void)long_term_path;
    if (memory == NULL) return 0;
    memory->goal_loaded = (unsigned char)load_text(goal_path, memory->locked_goal, sizeof(memory->locked_goal));
    memory->notebook_loaded = (unsigned char)load_text(notebook_path, memory->locked_notebook, sizeof(memory->locked_notebook));
    memory->menu_loaded = (unsigned char)load_text(menu_path, memory->locked_menu, sizeof(memory->locked_menu));
    file = fopen(memory->container_path, "a+b");
    if (file == NULL || fseek(file, 0L, SEEK_SET) != 0) {
        if (file != NULL) fclose(file);
        return memory_bench(memory, "cannot audit long-term store");
    }
    memory->serial = 0U;
    memory->newest_epoch = (time_t)0;
    for (;;) {
        int result = read_record(file, &memory->recall_candidate);
        if (result == 0) break;
        if (result < 0 || memory->serial == UINT64_MAX) {
            fclose(file);
            return memory_bench(memory, "startup found a corrupt or incomplete container; writes held");
        }
        ++memory->serial;
        if (memory->recall_candidate.tick.epoch_seconds > memory->newest_epoch)
            memory->newest_epoch = memory->recall_candidate.tick.epoch_seconds;
        if (memory->recall_candidate.permanent && !install(memory, &memory->recall_candidate)) {
            fclose(file);
            return 0;
        }
    }
    memory->long_term_ready = (unsigned char)(fclose(file) == 0);
    return memory->goal_loaded && memory->notebook_loaded && memory->menu_loaded && memory->long_term_ready;
}

void shakti_memory_bind_eden(shakti_memory_state_t *memory, const shakti_reason_state_t *eden)
{
    if (memory != NULL) memory->eden = eden;
}

int shakti_memory_reload_notebook(shakti_memory_state_t *memory, const char *path)
{
    if (memory == NULL) return 0;
    memory->notebook_loaded = (unsigned char)load_text(path, memory->locked_notebook, sizeof(memory->locked_notebook));
    return memory->notebook_loaded;
}

int shakti_memory_reload_goal(shakti_memory_state_t *memory, const char *path)
{
    if (memory == NULL) return 0;
    memory->goal_loaded = (unsigned char)load_text(path, memory->locked_goal, sizeof(memory->locked_goal));
    return memory->goal_loaded;
}

int shakti_memory_activate(shakti_memory_state_t *memory, const char *id)
{
    size_t i, count;
    if (memory == NULL || id == NULL) return 0;
    for (i = 0U; i < SHAKTI_SHORT_TERM_SLOTS; ++i) {
        if (memory->short_term[i].in_use && strcmp(memory->short_term[i].id, id) == 0) {
            memory->short_term[i].last_used_slot = memory->current_slot;
            if (memory->working_count && strcmp(memory->working[0].id, id) == 0) {
                memory->working[0] = memory->short_term[i];
                return 1;
            }
            remove_working(memory, id);
            count = memory->working_count;
            if (count >= SHAKTI_WORKING_SLOTS) count = SHAKTI_WORKING_SLOTS - 1U;
            if (count) memmove(memory->working + 1U, memory->working, count * sizeof(memory->working[0]));
            memory->working[0] = memory->short_term[i];
            if (memory->working_count < SHAKTI_WORKING_SLOTS) ++memory->working_count;
            return 1;
        }
    }
    return 0;
}

/* The append-only file remains authoritative when an id has left RAM.
 * This scan is synchronous; native hosts must budget storage work accordingly. */
static int existing_id(shakti_memory_state_t *memory, const shakti_memory_item_t *item)
{
    shakti_memory_item_t previous;
    FILE *file = fopen(memory->container_path, "rb");
    int result, found = 0;
    if (file == NULL) return -1;
    while ((result = read_record(file, &previous)) > 0) {
        if (strcmp(previous.id, item->id) != 0) continue;
        if (previous.tick.epoch_seconds != item->tick.epoch_seconds ||
            previous.tick.frame != item->tick.frame || previous.permanent != item->permanent ||
            previous.size != item->size || strcmp(previous.tags, item->tags) != 0 ||
            memcmp(previous.text, item->text, item->size) != 0) {
            fclose(file);
            (void)memory_bench(memory, "immutable memory id conflict in long-term store");
            return -2;
        }
        found = 1;
    }
    if (fclose(file) != 0 || result < 0) return -1;
    return found;
}

int shakti_memory_store(shakti_memory_state_t *memory, const shakti_tick_t *tick,
    const char *id, const char *tags, const char *text, int permanent)
{
    shakti_memory_item_t *item;
    unsigned char header[40];
    FILE *file;
    size_t slot;
    int ok, exists;
    if (memory == NULL || tick == NULL || id == NULL || tags == NULL || text == NULL) return 0;
    if (!memory->long_term_ready || !id_valid(id) || strlen(tags) >= SHAKTI_MEMORY_TAG_BYTES ||
        strlen(text) >= SHAKTI_MEMORY_BYTES || tick->epoch_seconds < (time_t)0) return memory_bench(memory, "container is invalid or exceeds fixed capacity");
    item = &memory->recall_candidate;
    memset(item, 0, sizeof(*item));
    memcpy(item->id, id, strlen(id) + 1U);
    memcpy(item->tags, tags, strlen(tags) + 1U);
    item->size = strlen(text);
    memcpy(item->text, text, item->size + 1U);
    item->tick = *tick;
    item->permanent = permanent ? 1U : 0U;
    item->in_use = 1U;
    item->checksum = record_header(item, header);
    slot = cache_slot(memory, item);
    if (slot == SHAKTI_SHORT_TERM_SLOTS) return memory_bench(memory, "whole-container RAM capacity exhausted");
    if (memory->short_term[slot].in_use && strcmp(memory->short_term[slot].id, id) == 0) {
        if (memory->short_term[slot].checksum != item->checksum) return memory_bench(memory, "immutable memory id conflict");
        return shakti_memory_activate(memory, id);
    }
    exists = existing_id(memory, item);
    if (exists == -2) return 0;
    if (exists < 0) {
        memory->long_term_ready = 0U;
        return memory_bench(memory, "long-term integrity scan failed; writes held");
    }
    if (exists) return install(memory, item);
    if (memory->serial == UINT64_MAX) return memory_bench(memory, "memory serial exhausted");
    file = fopen(memory->container_path, "ab");
    if (file == NULL) return memory_bench(memory, "cannot open long-term store");
    ok = fwrite(header, 1U, 40U, file) == 40U &&
        fwrite(item->id, 1U, strlen(item->id), file) == strlen(item->id) &&
        fwrite(item->tags, 1U, strlen(item->tags), file) == strlen(item->tags) &&
        fwrite(item->text, 1U, item->size, file) == item->size &&
        fwrite("DONE", 1U, 4U, file) == 4U && fflush(file) == 0;
    if (fclose(file) != 0) ok = 0;
    if (!ok) {
        memory->long_term_ready = 0U;
        return memory_bench(memory, "incomplete long-term write; writes held and record not admitted");
    }
    ++memory->serial;
    if (tick->epoch_seconds > memory->newest_epoch) memory->newest_epoch = tick->epoch_seconds;
    return install(memory, item);
}

int shakti_memory_remember(shakti_memory_state_t *memory, const shakti_tick_t *tick, const char *text)
{
    char id[SHAKTI_MEMORY_ID_BYTES];
    int n;
    if (memory == NULL || tick == NULL || text == NULL) return 0;
    if (memory->serial == UINT64_MAX) return memory_bench(memory, "memory serial exhausted");
    n = snprintf(id, sizeof(id), "m-%ld-%u-%" PRIu64, (long)tick->epoch_seconds, tick->frame, memory->serial + 1U);
    if (n < 0 || (size_t)n >= sizeof(id)) return memory_bench(memory, "memory id capacity");
    return shakti_memory_store(memory, tick, id, "event", text, 0);
}

int shakti_memory_fetch(shakti_memory_state_t *memory, const char *id,
    shakti_memory_item_t *out)
{
    shakti_memory_item_t item, found_item;
    FILE *file;
    int result, found = 0;
    if (memory == NULL || id == NULL || out == NULL || !id_valid(id)) return -1;
    file = fopen(memory->container_path, "rb");
    if (file == NULL) return -1;
    while ((result = read_record(file, &item)) > 0) {
        if (strcmp(item.id, id) == 0) {
            if (found && item.checksum != found_item.checksum) { result = -1; break; }
            found_item = item;
            found = 1;
        }
    }
    if (fclose(file) != 0 || result < 0) {
        memory->long_term_ready = 0U;
        (void)memory_bench(memory, "whole-container fetch failed; writes held");
        return -1;
    }
    if (found) *out = found_item;
    return found;
}

int shakti_memory_request(shakti_memory_state_t *memory, const char *query)
{
    if (memory == NULL || query == NULL || query[0] == '\0' || strlen(query) >= sizeof(memory->recall_query)) return 0;
    if (memory->recall_file != NULL) return 0;
    memory->recall_file = fopen(memory->container_path, "rb");
    if (memory->recall_file == NULL) return memory_bench(memory, "cannot open requested memory store");
    memcpy(memory->recall_query, query, strlen(query) + 1U);
    memory->recalled_count = 0UL;
    return 1;
}

int shakti_memory_step(shakti_memory_state_t *memory, uint64_t slot)
{
    size_t i;
    int result;
    if (memory == NULL || slot < memory->current_slot) return 0;
    memory->current_slot = slot;
    if (memory->working_count) (void)shakti_memory_activate(memory, memory->working[0].id);
    if (memory->recent_decay_beats) {
        uint64_t ttl = (uint64_t)memory->recent_decay_beats > UINT64_MAX / SHAKTI_HEARING_SLOTS
            ? UINT64_MAX : (uint64_t)memory->recent_decay_beats * SHAKTI_HEARING_SLOTS;
        for (i = 0U; i < SHAKTI_SHORT_TERM_SLOTS; ++i) {
            shakti_memory_item_t *item = &memory->short_term[i];
            if (item->in_use && !item->permanent && slot - item->last_used_slot >= ttl) {
                remove_working(memory, item->id);
                memset(item, 0, sizeof(*item));
                --memory->short_term_count;
            }
        }
    }
    if (memory->recall_file == NULL) return 1;
    result = read_record(memory->recall_file, &memory->recall_candidate);
    if (result <= 0) {
        int close_ok = fclose(memory->recall_file) == 0;
        memory->recall_file = NULL;
        if (result < 0 || !close_ok) {
            memory->long_term_ready = 0U;
            return memory_bench(memory, "corrupt or incomplete container; writes held and record not admitted");
        }
        return 1;
    }
    if (query_matches(&memory->recall_candidate, memory->recall_query)) {
        if (!install(memory, &memory->recall_candidate)) return 0;
        ++memory->recalled_count;
    }
    return 1;
}

void shakti_memory_close(shakti_memory_state_t *memory)
{
    if (memory != NULL && memory->recall_file != NULL) {
        fclose(memory->recall_file);
        memory->recall_file = NULL;
    }
}

void shakti_memory_print_status(const shakti_memory_state_t *memory)
{
    if (memory == NULL) return;
    printf("Working memory: %lu whole containers. Short-term RAM: %lu.\n", (unsigned long)memory->working_count, (unsigned long)memory->short_term_count);
    printf("Eden resident: %s. Memory lookup: %s; %lu containers recalled.\n", memory->eden ? "yes" : "no", memory->recall_file ? "active" : "idle", memory->recalled_count);
    printf("Recent decay: %lu heartbeat beats (0 = rate not chosen). Long term is retained.\n", memory->recent_decay_beats);
    printf("Memory integrity: %s.\n", memory->fault ? "BENCH" : "ready");
}

/* Legacy readable-log inspection only. Live recall uses request/step above. */
int shakti_memory_recall_text_file(const char *path, const char *term, unsigned int maximum_matches)
{
    FILE *file;
    char line[SHAKTI_LINE_CAPACITY];
    unsigned int matches = 0U;
    if (path == NULL || term == NULL || !term[0]) return 0;
    file = fopen(path, "r");
    if (file == NULL) return 0;
    while (fgets(line, sizeof(line), file) != NULL) {
        size_t size = strlen(line);
        if (!size || (line[size - 1U] != '\n' && !feof(file))) { fclose(file); return 0; }
        if (strstr(line, term)) {
            fputs(line, stdout);
            if (++matches >= maximum_matches && maximum_matches) break;
        }
    }
    if (ferror(file)) { fclose(file); return 0; }
    return fclose(file) == 0;
}
