#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include "shakti_retention.h"
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static unsigned char arena[SHAKTI_ARENA_BYTES];
static shakti_retention_state_t retained;
static shakti_retained_window_t incoming;
static shakti_retained_window_t historical;
static char transcript_text[SHAKTI_TRANSCRIPT_BYTES];
static unsigned char header[256];
#define LATEST (arena + SHAKTI_RETENTION_HISTORY_BYTES)
#define WORK (LATEST + SHAKTI_RETENTION_EDGE_BYTES)

static void put(unsigned char *p, uint64_t value, size_t n)
{
    size_t i;
    for (i = 0U; i < n; ++i) { p[i] = (unsigned char)(value & 255U); value >>= 8; }
}

static uint64_t get(const unsigned char *p, size_t n)
{
    uint64_t value = 0U;
    while (n) value = (value << 8) | p[--n];
    return value;
}

static int hold(const char *reason)
{
    if (!retained.fault) fprintf(stderr, "BENCH retention: %s\n", reason);
    retained.fault = 1U;
    return 0;
}

static int transcribed(const shakti_retained_window_t *entry)
{
    size_t i;
    uint64_t end = entry->end_position64 + 1U, next = entry->position64;
    for (i = 0U; i < retained.transcript_count; ++i) {
        if (retained.transcripts[i].start64 > next) return 0;
        if (retained.transcripts[i].end64 >= end) return 1;
        if (retained.transcripts[i].end64 >= next) next = retained.transcripts[i].end64 + 1U;
    }
    return 0;
}

static const unsigned char *select_payload(shakti_retained_window_t *entry,
    const unsigned char *data, uint64_t stride)
{
    int sight = (entry->mask & 1U) && (!entry->ordinal || entry->ordinal % stride == 0U);
    int audio = (entry->mask & 6U) && (!entry->ordinal || !transcribed(entry));
    if (sight && audio) return data;
    if (sight) {
        entry->mask = 1U; entry->bytes = entry->lanes[0].encoded_bytes;
        memset(entry->lanes + 1U, 0, 2U * sizeof(entry->lanes[0])); return data;
    }
    if (audio) {
        size_t offset = entry->lanes[1].payload_offset;
        entry->mask = 6U; entry->bytes -= offset;
        entry->lanes[1].payload_offset -= offset; entry->lanes[2].payload_offset -= offset;
        memset(&entry->lanes[0], 0, sizeof(entry->lanes[0])); return data + offset;
    }
    entry->mask = 0U; entry->bytes = 0U; return data;
}

static void thin(uint64_t stride)
{
    size_t i, used = 0U, count = 0U;
    for (i = 0U; i < retained.count; ++i) {
        shakti_retained_window_t entry = retained.windows[i];
        const unsigned char *data = select_payload(&entry, arena + entry.offset, stride);
        if (!entry.mask) continue;
        if (count) {
            shakti_retained_window_t *previous = &retained.windows[count - 1U];
            if (previous->mask == entry.mask && previous->bytes == entry.bytes &&
                previous->end_position64 <= UINT64_MAX - 2U && previous->end_position64 + 2U == entry.position64 &&
                !memcmp(previous->lanes, entry.lanes, sizeof(entry.lanes)) &&
                !memcmp(arena + previous->offset, data, entry.bytes)) {
                previous->end_position64 = entry.end_position64; continue;
            }
        }
        memmove(arena + used, data, entry.bytes); retained.windows[count] = entry;
        retained.windows[count++].offset = used; used += entry.bytes;
    }
    retained.count = count; retained.bytes_used = used; retained.stride = stride;
}

static size_t rle_size(const unsigned char *data, size_t bytes, size_t unit)
{
    size_t offset = 0U, encoded = 0U;
    while (offset < bytes) {
        size_t count = 1U;
        while (count < UINT32_MAX && count < (bytes - offset) / unit &&
            !memcmp(data + offset, data + offset + count * unit, unit)) ++count;
        if (encoded > SIZE_MAX - 4U - unit) return SIZE_MAX;
        encoded += 4U + unit; offset += count * unit;
    }
    return encoded;
}

static void encode(const unsigned char *data, size_t bytes, size_t unit, unsigned char *out)
{
    size_t offset = 0U, used = 0U;
    while (offset < bytes) {
        size_t count = 1U;
        while (count < UINT32_MAX && count < (bytes - offset) / unit &&
            !memcmp(data + offset, data + offset + count * unit, unit)) ++count;
        put(out + used, count, 4U); memcpy(out + used + 4U, data + offset, unit);
        used += 4U + unit; offset += count * unit;
    }
}

static int decode(const shakti_retained_lane_t *part, const unsigned char *data,
    unsigned char *out, size_t capacity)
{
    size_t offset = 0U, used = 0U, unit = (size_t)part->channels * part->sample_bytes;
    if (!out || capacity < part->raw_bytes || !unit || unit > 64U) return 0;
    if (!part->codec) {
        if (part->encoded_bytes != part->raw_bytes) return 0;
        memcpy(out, data, part->raw_bytes);
    } else {
        while (offset < part->encoded_bytes) {
            uint64_t count;
            size_t i;
            if (part->encoded_bytes - offset < 4U + unit) return 0;
            count = get(data + offset, 4U);
            if (!count || count > (part->raw_bytes - used) / unit) return 0;
            for (i = 0U; i < (size_t)count; ++i) memcpy(out + used + i * unit, data + offset + 4U, unit);
            used += (size_t)count * unit; offset += 4U + unit;
        }
        if (used != part->raw_bytes) return 0;
    }
    return shakti_media_hash(out, part->raw_bytes) == part->hash;
}

static uint64_t record_hash(const unsigned char *data, size_t bytes)
{
    uint64_t hash = shakti_media_hash(header, sizeof(header));
    size_t i;
    for (i = 0U; i < bytes; ++i) { hash ^= data[i]; hash *= UINT64_C(1099511628211); }
    return hash;
}

static int write_record(FILE *file, const shakti_retained_window_t *entry, const unsigned char *data)
{
    size_t i;
    memset(header, 0, sizeof(header));
    put(header, entry->epoch, 8U); put(header + 8U, entry->event_frame, 4U);
    put(header + 12U, entry->mask, 4U); put(header + 16U, entry->position64, 8U);
    put(header + 24U, entry->ordinal, 8U); put(header + 32U, entry->bytes, 8U);
    put(header + 40U, entry->end_position64, 8U); memcpy(header + 48U, "SRF1", 4U);
    for (i = 0U; i < 3U; ++i) {
        unsigned char *h = header + 64U + i * 64U;
        const shakti_retained_lane_t *p = &entry->lanes[i];
        put(h, p->width, 4U); put(h + 4U, p->height, 4U); put(h + 8U, p->channels, 4U);
        put(h + 12U, p->sample_bytes, 4U); put(h + 16U, p->sample_rate, 4U); put(h + 20U, p->codec, 4U);
        put(h + 24U, p->raw_bytes, 8U); put(h + 32U, p->encoded_bytes, 8U);
        put(h + 40U, p->payload_offset, 8U); put(h + 48U, p->hash, 8U);
    }
    put(header + 56U, record_hash(data, entry->bytes), 8U);
    return fwrite(header, 1U, sizeof(header), file) == sizeof(header) &&
        fwrite(data, 1U, entry->bytes, file) == entry->bytes && fwrite("DONE", 1U, 4U, file) == 4U;
}

static int read_record(FILE *file)
{
    size_t i, n = fread(header, 1U, sizeof(header), file), used = 0U;
    uint64_t stored;
    unsigned char footer[4];
    if (!n && feof(file) && !ferror(file)) return 0;
    if (n != sizeof(header) || (memcmp(header + 48U, "SRF1", 4U) && memcmp(header + 48U, "STX1", 4U)) ||
        get(header + 32U, 8U) > SHAKTI_RETENTION_EDGE_BYTES) return -1;
    if (!memcmp(header + 48U, "STX1", 4U)) {
        shakti_transcript_segment_t *segment;
        size_t bytes = (size_t)get(header + 32U, 8U);
        if (retained.transcript_count == SHAKTI_TRANSCRIPT_SEGMENTS ||
            bytes >= SHAKTI_TRANSCRIPT_BYTES - retained.transcript_bytes ||
            get(header + 16U, 8U) > get(header + 40U, 8U) ||
            fread(WORK, 1U, bytes, file) != bytes || memchr(WORK, 0, bytes) ||
            fread(footer, 1U, 4U, file) != 4U || memcmp(footer, "DONE", 4U)) return -1;
        stored = get(header + 56U, 8U); memset(header + 56U, 0, 8U);
        if (record_hash(WORK, bytes) != stored) return -1;
        segment = &retained.transcripts[retained.transcript_count++];
        segment->epoch = get(header, 8U); segment->event_frame = (unsigned int)get(header + 8U, 4U);
        segment->start64 = get(header + 16U, 8U); segment->end64 = get(header + 40U, 8U);
        segment->offset = retained.transcript_bytes; segment->bytes = bytes;
        memcpy(transcript_text + segment->offset, WORK, bytes); transcript_text[segment->offset + bytes] = '\0';
        retained.transcript_bytes += bytes + 1U; return 2;
    }
    memset(&incoming, 0, sizeof(incoming));
    incoming.epoch = get(header, 8U); incoming.event_frame = (unsigned int)get(header + 8U, 4U);
    incoming.mask = (unsigned int)get(header + 12U, 4U); incoming.position64 = get(header + 16U, 8U);
    incoming.ordinal = get(header + 24U, 8U); incoming.bytes = (size_t)get(header + 32U, 8U);
    incoming.end_position64 = get(header + 40U, 8U);
    if ((incoming.mask != 1U && incoming.mask != 6U && incoming.mask != 7U) ||
        (incoming.position64 & 1U) || (incoming.end_position64 & 1U) || incoming.end_position64 < incoming.position64) return -1;
    for (i = 0U; i < 3U; ++i) {
        const unsigned char *h = header + 64U + i * 64U;
        shakti_retained_lane_t *p = &incoming.lanes[i];
        uint64_t raw = get(h + 24U, 8U), encoded = get(h + 32U, 8U), offset = get(h + 40U, 8U);
        p->width = (unsigned int)get(h, 4U); p->height = (unsigned int)get(h + 4U, 4U);
        p->channels = (unsigned int)get(h + 8U, 4U); p->sample_bytes = (unsigned int)get(h + 12U, 4U);
        p->sample_rate = (unsigned int)get(h + 16U, 4U); p->codec = (unsigned int)get(h + 20U, 4U);
        if (!(incoming.mask & (1U << i))) { if (raw || encoded || offset) return -1; continue; }
        if (!raw || raw > SHAKTI_RETENTION_EDGE_BYTES || !encoded || encoded > incoming.bytes ||
            offset != used || encoded > incoming.bytes - used || !p->channels || p->channels > 8U ||
            !p->sample_bytes || p->sample_bytes > 8U || p->codec > 1U) return -1;
        p->raw_bytes = (size_t)raw; p->encoded_bytes = (size_t)encoded;
        p->payload_offset = (size_t)offset; p->hash = get(h + 48U, 8U); used += p->encoded_bytes;
    }
    if (used != incoming.bytes || fread(WORK, 1U, incoming.bytes, file) != incoming.bytes ||
        fread(footer, 1U, 4U, file) != 4U || memcmp(footer, "DONE", 4U)) return -1;
    stored = get(header + 56U, 8U); memset(header + 56U, 0, 8U);
    if (record_hash(WORK, incoming.bytes) != stored) return -1;
    for (i = 0U; i < 3U; ++i) if (incoming.mask & (1U << i))
        if (!decode(&incoming.lanes[i], WORK + incoming.lanes[i].payload_offset, LATEST, SHAKTI_RETENTION_EDGE_BYTES)) return -1;
    return 1;
}

static void install(void)
{
    uint64_t elapsed = incoming.position64 - retained.first_position64;
    uint64_t floor_stride = elapsed >= UINT64_C(2400) * 64U ? 4U : elapsed >= UINT64_C(1200) * 64U ? 2U : 1U;
    if (retained.stride < floor_stride) thin(floor_stride);
    memcpy(LATEST, WORK, incoming.bytes); retained.latest = incoming;
    retained.latest.offset = SHAKTI_RETENTION_HISTORY_BYTES;
    retained.windows_seen = incoming.ordinal + 1U;
    retained.blocked = 0U;
    for (;;) {
        size_t old_bytes = retained.bytes_used;
        const unsigned char *data;
        historical = incoming; data = select_payload(&historical, WORK, retained.stride);
        if (!historical.mask) return;
        if (historical.bytes <= retained.history_budget - retained.bytes_used && retained.count < SHAKTI_RETENTION_WINDOWS) {
            if (data != WORK) memmove(WORK, data, historical.bytes);
            break;
        }
        if (retained.stride > UINT64_MAX / 2U) { retained.blocked = 1U; return; }
        thin(retained.stride * 2U);
        if (retained.bytes_used == old_bytes) {
            size_t i;
            for (i = 0U; i < retained.count; ++i)
                if (retained.windows[i].ordinal && (retained.windows[i].mask & 1U)) break;
            if (i == retained.count) { retained.blocked = 1U; return; }
        }
    }
    if (retained.count) {
        shakti_retained_window_t *previous = &retained.windows[retained.count - 1U];
        if (previous->bytes == historical.bytes && previous->mask == historical.mask &&
            previous->end_position64 <= UINT64_MAX - 2U && previous->end_position64 + 2U == incoming.position64 &&
            !memcmp(previous->lanes, historical.lanes, sizeof(historical.lanes)) &&
            !memcmp(arena + previous->offset, WORK, historical.bytes)) {
            previous->end_position64 = incoming.end_position64; return;
        }
    }
    memcpy(arena + retained.bytes_used, WORK, historical.bytes);
    retained.windows[retained.count] = historical;
    retained.windows[retained.count++].offset = retained.bytes_used;
    retained.bytes_used += historical.bytes;
}

static int finish(FILE *file)
{
    int ok = fflush(file) == 0;
    if (ok && fsync(fileno(file))) ok = 0;
    if (fclose(file)) ok = 0;
    return ok;
}

static int write_file_header(FILE *file)
{
    memset(header, 0, 128U); memcpy(header, "SMAR1", 5U);
    memcpy(header + 8U, retained.source_id, sizeof(retained.source_id));
    put(header + 72U, retained.first_position64, 8U);
    put(header + 80U, retained.history_budget, 8U);
    put(header + 88U, shakti_media_hash(header, 88U), 8U);
    return fwrite(header, 1U, 128U, file) == 128U;
}

static int write_transcript(FILE *file, const shakti_transcript_segment_t *segment)
{
    const unsigned char *text = (const unsigned char *)transcript_text + segment->offset;
    memset(header, 0, sizeof(header)); put(header, segment->epoch, 8U);
    put(header + 8U, segment->event_frame, 4U); put(header + 16U, segment->start64, 8U);
    put(header + 40U, segment->end64, 8U); put(header + 32U, segment->bytes, 8U);
    memcpy(header + 48U, "STX1", 4U); put(header + 56U, record_hash(text, segment->bytes), 8U);
    return fwrite(header, 1U, sizeof(header), file) == sizeof(header) &&
        fwrite(text, 1U, segment->bytes, file) == segment->bytes && fwrite("DONE", 1U, 4U, file) == 4U;
}

static int snapshot(void)
{
    FILE *file = fopen(SHAKTI_RETENTION_PATH ".tmp", "wb");
    size_t i;
    int ok;
    if (!file) return 0;
    ok = write_file_header(file);
    for (i = 0U; ok && i < retained.transcript_count; ++i) ok = write_transcript(file, &retained.transcripts[i]);
    for (i = 0U; ok && i < retained.count; ++i)
        ok = write_record(file, &retained.windows[i], arena + retained.windows[i].offset);
    if (ok && retained.active)
        ok = write_record(file, &retained.latest, LATEST);
    if (!finish(file)) ok = 0;
    return ok && !rename(SHAKTI_RETENTION_PATH ".tmp", SHAKTI_RETENTION_PATH);
}

int shakti_retention_restore(void)
{
    FILE *file;
    int result;
    memset(&retained, 0, sizeof(retained)); retained.stride = 1U;
    retained.history_budget = SHAKTI_RETENTION_HISTORY_BYTES;
    file = fopen(SHAKTI_RETENTION_PATH, "rb");
    if (!file) return errno == ENOENT ? 1 : hold("cannot restore native arena journal");
    if (fread(header, 1U, 128U, file) != 128U || memcmp(header, "SMAR1", 5U) ||
        !memchr(header + 8U, 0, 64U) || get(header + 88U, 8U) != shakti_media_hash(header, 88U) ||
        get(header + 80U, 8U) < 65536U || get(header + 80U, 8U) > SHAKTI_RETENTION_HISTORY_BYTES) {
        fclose(file); return hold("damaged arena journal header");
    }
    memcpy(retained.source_id, header + 8U, 64U);
    retained.first_position64 = get(header + 72U, 8U);
    retained.history_budget = (size_t)get(header + 80U, 8U);
    while ((result = read_record(file)) > 0) {
        if (result == 2) {
            if (retained.active) {
                int was_blocked = retained.blocked;
                thin(retained.stride);
                if (was_blocked) { incoming = retained.latest; memcpy(WORK, LATEST, incoming.bytes); install(); }
            }
            continue;
        }
        if (!retained.source_id[0] || incoming.position64 < retained.first_position64 || incoming.ordinal == UINT64_MAX ||
            (retained.active && incoming.ordinal < retained.latest.ordinal)) { result = -1; break; }
        if (retained.active && incoming.ordinal == retained.latest.ordinal) {
            if (incoming.position64 != retained.latest.position64 || incoming.epoch != retained.latest.epoch ||
                incoming.event_frame != retained.latest.event_frame) { result = -1; break; }
            memcpy(LATEST, WORK, incoming.bytes); retained.latest = incoming;
            retained.latest.offset = SHAKTI_RETENTION_HISTORY_BYTES; continue;
        }
        retained.active = 1U; install();
    }
    if (fclose(file) || result < 0) return hold("incomplete arena journal; no retained view admitted");
    if (retained.transcript_count && (!retained.active || retained.transcripts[0].start64 < retained.first_position64 ||
        retained.transcripts[retained.transcript_count - 1U].end64 > retained.latest.position64 + 1U))
        return hold("transcript range is outside its source");
    return 1;
}

const shakti_retention_state_t *shakti_retention_view(void)
{
    return retained.fault ? NULL : &retained;
}

int shakti_retention_accepts(const char *source_id)
{
    return source_id && !retained.fault && !retained.blocked && (!retained.active || !strcmp(retained.source_id, source_id));
}

int shakti_retention_capture(const shakti_sensory_group_t *group, uint64_t epoch, unsigned int event_frame)
{
    size_t i, used = 0U;
    const shakti_sensory_frame_t *first;
    FILE *file;
    long journal_bytes;
    int ok;
    if (!group || retained.fault || (group->checked_mask != 1U && group->checked_mask != 6U && group->checked_mask != 7U)) return 0;
    first = &group->parts[(group->checked_mask & 1U) ? 0U : 1U];
    if (!shakti_retention_accepts(first->source_id) || (retained.active && first->position64 <= retained.latest.position64)) return 0;
    memset(&incoming, 0, sizeof(incoming)); incoming.epoch = epoch; incoming.event_frame = event_frame;
    incoming.position64 = incoming.end_position64 = first->position64; incoming.mask = group->checked_mask;
    incoming.ordinal = retained.windows_seen;
    for (i = 0U; i < 3U; ++i) if (group->checked_mask & (1U << i)) {
        const shakti_sensory_frame_t *from = &group->parts[i];
        shakti_retained_lane_t *to = &incoming.lanes[i];
        size_t compressed, unit = (size_t)from->channels * from->sample_bytes;
        if (!unit || unit > 64U || !from->data || !from->bytes || from->bytes % unit ||
            strcmp(first->source_id, from->source_id) || shakti_media_hash(from->data, from->bytes) != from->validated_hash) return hold("source or rebuilt bytes changed before retention");
        compressed = rle_size(from->data, from->bytes, unit);
        to->codec = compressed < from->bytes ? 1U : 0U;
        to->encoded_bytes = to->codec ? compressed : from->bytes;
        if (to->encoded_bytes > SHAKTI_RETENTION_EDGE_BYTES - used) return hold("whole source window exceeds fixed 32 MiB edge region");
        to->width = from->width; to->height = from->height; to->channels = from->channels;
        to->sample_bytes = from->sample_bytes; to->sample_rate = from->sample_rate;
        to->raw_bytes = from->bytes; to->hash = from->validated_hash; to->payload_offset = used;
        if (to->codec) encode(from->data, from->bytes, unit, WORK + used);
        else memcpy(WORK + used, from->data, from->bytes);
        used += to->encoded_bytes;
    }
    incoming.bytes = used;
    if (!retained.active) {
        memcpy(retained.source_id, first->source_id, strlen(first->source_id) + 1U);
        retained.first_position64 = first->position64; retained.active = 1U;
        /* Initial header must precede every durable frame. */
        file = fopen(SHAKTI_RETENTION_PATH, "wb");
        if (!file) return hold("cannot open native short-term journal");
        ok = write_file_header(file); if (!finish(file)) ok = 0;
        if (!ok) return hold("cannot commit short-term journal header");
    }
    install();
    file = fopen(SHAKTI_RETENTION_PATH, "ab");
    if (!file) return hold("cannot append native short-term journal");
    ok = write_record(file, &retained.latest, LATEST);
    journal_bytes = ftell(file);
    if (!finish(file)) ok = 0;
    if (!ok) return hold("short-term journal write incomplete");
    if ((journal_bytes < 0L || (uint64_t)journal_bytes > SHAKTI_ARENA_BYTES) && !snapshot()) return hold("arena compaction checkpoint failed");
    return 1;
}

int shakti_retention_read(size_t index, unsigned int lane, unsigned char *out, size_t capacity)
{
    const shakti_retained_window_t *entry;
    if (retained.fault || !retained.active || lane > 2U || (index != SIZE_MAX && index >= retained.count)) return 0;
    entry = index == SIZE_MAX ? &retained.latest : &retained.windows[index];
    if (!(entry->mask & (1U << lane))) return 0;
    return decode(&entry->lanes[lane], arena + entry->offset + entry->lanes[lane].payload_offset, out, capacity);
}

int shakti_retention_budget(size_t bytes)
{
    if (retained.fault || bytes < 65536U || bytes > SHAKTI_RETENTION_HISTORY_BYTES) return 0;
    while ((retained.bytes_used > bytes || retained.count == SHAKTI_RETENTION_WINDOWS) && retained.stride <= UINT64_MAX / 2U) {
        size_t before = retained.bytes_used;
        thin(retained.stride * 2U);
        if (retained.bytes_used == before) break;
    }
    if (retained.bytes_used > bytes) return 0;
    retained.history_budget = bytes;
    return snapshot();
}

int shakti_retention_transcribe(uint64_t start64, uint64_t end64, const char *text,
    uint64_t epoch, unsigned int event_frame)
{
    shakti_transcript_segment_t *segment;
    size_t bytes;
    FILE *file;
    int ok, was_blocked;
    if (!text || retained.fault || !retained.active || start64 > end64 || start64 < retained.first_position64 ||
        retained.latest.position64 == UINT64_MAX || end64 > retained.latest.position64 + 1U ||
        retained.transcript_count == SHAKTI_TRANSCRIPT_SEGMENTS) return 0;
    if (retained.transcript_count && start64 <= retained.transcripts[retained.transcript_count - 1U].end64) return 0;
    bytes = strlen(text);
    if (bytes >= SHAKTI_TRANSCRIPT_BYTES - retained.transcript_bytes) return 0;
    segment = &retained.transcripts[retained.transcript_count++];
    segment->epoch = epoch; segment->event_frame = event_frame; segment->start64 = start64; segment->end64 = end64;
    segment->offset = retained.transcript_bytes; segment->bytes = bytes;
    memcpy(transcript_text + segment->offset, text, bytes + 1U); retained.transcript_bytes += bytes + 1U;
    file = fopen(SHAKTI_RETENTION_PATH, "ab"); if (!file) return hold("cannot save actual transcript");
    ok = write_transcript(file, segment); if (!finish(file)) ok = 0;
    if (!ok) return hold("transcript write incomplete");
    was_blocked = retained.blocked; thin(retained.stride);
    if (was_blocked) { incoming = retained.latest; memcpy(WORK, LATEST, incoming.bytes); install(); }
    return snapshot();
}

const char *shakti_retention_transcript(size_t index, size_t *bytes)
{
    if (bytes) *bytes = 0U;
    if (retained.fault || index >= retained.transcript_count) return NULL;
    if (bytes) *bytes = retained.transcripts[index].bytes;
    return transcript_text + retained.transcripts[index].offset;
}

int shakti_retention_audio_pending(void)
{
    size_t i;
    if (retained.fault) return 1;
    for (i = 0U; i < retained.count; ++i)
        if ((retained.windows[i].mask & 6U) && !transcribed(&retained.windows[i])) return 1;
    return retained.active && (retained.latest.mask & 6U) && !transcribed(&retained.latest);
}

int shakti_retention_release(void)
{
    if (retained.fault) return 0;
    memset(&retained, 0, sizeof(retained)); retained.stride = 1U; retained.history_budget = SHAKTI_RETENTION_HISTORY_BYTES;
    return snapshot();
}
