#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include "shakti_prenatal_flow.h"
#include "shakti_prenatal_loader.h"
#include "shakti_config.h"
#include "shakti_log.h"
#include "shakti_loop.h"
#include "shakti_time.h"
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define FILE_CAP 65536U
#define AUDIO_BYTES 1000U
#define RECORD_BYTES 1216U
#define RECORD_HASH 96U

static unsigned char sight[4][SHAKTI_PRENATAL_FRAME_BYTES];
static unsigned char hearing[2][4][AUDIO_BYTES];
static unsigned char files[9][FILE_CAP];
static unsigned char profile_bytes[SHAKTI_PRENATAL_PROFILE_BYTES];
static unsigned char record[RECORD_BYTES];
static shakti_prenatal_wave_t waves[9];
static const char *const names[9] = {
    "black", "red", "green", "yellow", "blue", "magenta", "cyan", "white", "pulse"
};
static const unsigned char palette[8][3] = {
    {0,0,0}, {255,0,0}, {0,255,0}, {255,255,0},
    {0,0,255}, {255,0,255}, {0,255,255}, {255,255,255}
};
static struct {
    shakti_runtime_t *owner;
    shakti_prenatal_config_t config;
    shakti_prenatal_status_t status;
    shakti_media_frame_t frames[3];
    char assets[SHAKTI_PATH_CAPACITY], profile[SHAKTI_PATH_CAPACITY];
    unsigned int loaded, pending, published, warm, setting_rate;
    unsigned int stop_requested, replay_requested;
} flow;

static int busy(void)
{
    return flow.status.state != SHAKTI_PRENATAL_OFF &&
        flow.status.state != SHAKTI_PRENATAL_DELIVERED;
}

static int active(void)
{
    return flow.status.state == SHAKTI_PRENATAL_ARMED ||
        flow.status.state == SHAKTI_PRENATAL_RUNNING ||
        flow.status.state == SHAKTI_PRENATAL_PAUSING;
}

static int bench(const char *reason)
{
    fprintf(stderr, "BENCH prenatal workflow: %s\n", reason);
    return 0;
}

static void put(unsigned char *p, uint64_t value, size_t n)
{
    size_t i;
    for (i = 0U; i < n; ++i) { p[i] = (unsigned char)value; value >>= 8; }
}

static uint64_t get(const unsigned char *p, size_t n)
{
    uint64_t value = 0U;
    while (n) value = (value << 8) | p[--n];
    return value;
}

static int path_valid(const char *path)
{
    size_t i, n;
    if (!path || !(n = strlen(path)) || n >= SHAKTI_PATH_CAPACITY) return 0;
    for (i = 0U; i < n; ++i)
        if ((unsigned char)path[i] < 32U || path[i] == ':' || path[i] == '|') return 0;
    return 1;
}

static int read_whole(const char *path, unsigned char *out, size_t capacity, size_t *bytes)
{
    FILE *file = fopen(path, "rb");
    int extra, bad;
    size_t n;
    if (!file) return 0;
    n = fread(out, 1U, capacity, file); extra = fgetc(file); bad = ferror(file);
    if (fclose(file)) bad = 1;
    if (bad || extra != EOF) return 0;
    *bytes = n;
    return 1;
}

int shakti_prenatal_plan(uint64_t beat, shakti_prenatal_plan_t *plan)
{
    shakti_prenatal_plan_t candidate;
    if (!plan || !beat || beat > SHAKTI_PRENATAL_END_BEAT) return 0;
    memset(&candidate, 0, sizeof(candidate));
    if (beat >= SHAKTI_PRENATAL_LIGHT_BEAT) {
        candidate.phase = 2U;
        candidate.phase_beat = beat - SHAKTI_PRENATAL_LIGHT_BEAT;
    } else if (beat >= SHAKTI_PRENATAL_SOUND_BEAT) {
        candidate.phase = 1U;
        candidate.phase_beat = beat - SHAKTI_PRENATAL_SOUND_BEAT;
    } else candidate.phase_beat = beat - 1U;
    candidate.color = (unsigned int)((candidate.phase_beat / 2U) % 8U);
    candidate.voice_sample = (size_t)(candidate.phase_beat % 2U) * 16000U;
    *plan = candidate;
    return 1;
}

static int audit(const char *event)
{
    shakti_tick_t tick;
    char text[SHAKTI_TEXT_CAPACITY];
    int n;
    if (!flow.owner) return 0;
    n = snprintf(text, sizeof(text),
        "run=%" PRIu64 " completed=%" PRIu64 " window=%u corpus=%016" PRIx64,
        flow.status.run_serial, flow.status.completed_beats,
        flow.status.window, flow.status.corpus_hash);
    return n > 0 && (size_t)n < sizeof(text) &&
        shakti_tick_next(&flow.owner->clock, &tick) &&
        shakti_log_append(SHAKTI_LEARNED_STREAM_PATH, "LRN1", &tick,
            "shakti_prenatal_flow.c", 1U, "AWAKE", "internal",
            flow.status.source_id[0] ? flow.status.source_id : "prenatal.loader",
            event, text);
}

static int checkpoint(void)
{
    FILE *file;
    int bad, fd;
    memset(record, 0, sizeof(record));
    memcpy(record, "SPL20001", 8U);
    put(record + 8U, 2U, 4U); put(record + 12U, flow.status.state, 4U);
    put(record + 16U, flow.status.run_serial, 8U);
    put(record + 24U, flow.status.completed_beats, 8U);
    put(record + 32U, flow.status.acknowledged_windows, 8U);
    put(record + 40U, flow.status.replays, 8U);
    put(record + 48U, flow.status.corpus_hash, 8U);
    put(record + 56U, flow.status.requested_rate, 4U);
    put(record + 60U, flow.config.monitor_enabled, 4U);
    put(record + 64U, SHAKTI_PRENATAL_OPENING_BEATS, 4U);
    put(record + 68U, flow.config.width, 4U); put(record + 72U, flow.config.height, 4U);
    put(record + 80U, (uint64_t)flow.owner->clock.last_epoch_seconds, 8U);
    put(record + 88U, flow.owner->clock.next_frame, 4U);
    put(record + 104U, flow.status.synthetic_fixture, 4U);
    memcpy(record + 128U, flow.assets, strlen(flow.assets) + 1U);
    memcpy(record + 640U, flow.profile, strlen(flow.profile) + 1U);
    memcpy(record + RECORD_BYTES - 8U, "SPL2DONE", 8U);
    put(record + RECORD_HASH, shakti_media_hash(record, sizeof(record)), 8U);
    file = fopen(SHAKTI_PRENATAL_CHECKPOINT ".tmp", "wb");
    if (!file) return 0;
    fd = fileno(file);
    bad = fwrite(record, 1U, sizeof(record), file) != sizeof(record);
    if (fflush(file) || fd < 0 || fsync(fd)) bad = 1;
    if (fclose(file)) bad = 1;
    return !bad && rename(SHAKTI_PRENATAL_CHECKPOINT ".tmp", SHAKTI_PRENATAL_CHECKPOINT) == 0;
}

static void effective_rate(unsigned int rate)
{
    if (!flow.owner || flow.owner->loop.heartbeat_rate == rate) return;
    flow.setting_rate = 1U;
    (void)shakti_loop_set_heartbeat(&flow.owner->loop, rate);
    flow.setting_rate = 0U;
}

static void hold(const char *reason)
{
    if (flow.status.state != SHAKTI_PRENATAL_HELD) {
        size_t n = strlen(reason);
        if (n >= sizeof(flow.status.hold_reason)) n = sizeof(flow.status.hold_reason) - 1U;
        memcpy(flow.status.hold_reason, reason, n); flow.status.hold_reason[n] = 0;
        (void)bench(reason);
    }
    flow.status.state = SHAKTI_PRENATAL_HELD;
    flow.warm = 0U;
    effective_rate(flow.status.requested_rate ? flow.status.requested_rate : 1U);
}

static int32_t sample(const unsigned char *p)
{
    unsigned int v = (unsigned int)p[0] | ((unsigned int)p[1] << 8);
    return v < 32768U ? (int32_t)v : (int32_t)v - 65536;
}

static int load_assets(void)
{
    size_t bytes, i, on;
    uint64_t hash;
    int32_t quietest = 32768, pulse_peak = 0;
    if (!read_whole(flow.profile, profile_bytes, sizeof(profile_bytes), &bytes) ||
        !shakti_prenatal_config_parse((const char *)profile_bytes, bytes, &flow.config))
        return bench("complete local profile required");
    if ((uint64_t)flow.config.width * flow.config.height > SHAKTI_PRENATAL_FRAME_BYTES / 3U)
        return bench("full-size frame exceeds fixed loader capacity; no resizing");
    for (i = 0U; i < 32U; ++i)
        if (flow.config.density[i] != 4U) return bench("prenatal workflow requires 32 solid frames");
    hash = shakti_media_hash(profile_bytes, bytes);
    for (i = 0U; i < 9U; ++i) {
        char path[SHAKTI_PATH_CAPACITY];
        size_t k;
        int32_t peak = 0;
        int n = snprintf(path, sizeof(path), "%s/%s.wav", flow.assets, names[i]);
        if (n < 0 || (size_t)n >= sizeof(path) ||
            !shakti_prenatal_wave_load(path, files[i], sizeof(files[i]), &bytes, &waves[i]) ||
            (i < 8U ? waves[i].sample_count > 32000U : waves[i].sample_count != 16000U))
            return bench("whole supplied voice/pulse must fit its two-beat/one-beat bound");
        for (k = 0U; k < bytes; ++k) { hash ^= files[i][k]; hash *= UINT64_C(1099511628211); }
        for (k = 0U; k < waves[i].sample_count; ++k) {
            int32_t magnitude = sample(waves[i].samples + k * 2U);
            if (magnitude < 0) magnitude = -magnitude;
            if (magnitude > peak) peak = magnitude;
        }
        if (i < 8U && peak < quietest) quietest = peak;
        if (i == 8U) pulse_peak = peak;
    }
    on = (size_t)(((uint64_t)16000U * flow.config.pulse_on_numerator +
        flow.config.pulse_on_denominator / 2U) / flow.config.pulse_on_denominator);
    if (!pulse_peak || pulse_peak != quietest / (int32_t)flow.config.voice_to_pulse_ratio)
        return bench("prepared pulse gain differs from the profile and supplied voices");
    for (i = on * 2U; i < 32000U; ++i)
        if (waves[8].samples[i]) return bench("prepared pulse off region differs from profile");
    for (i = 0U; i < sizeof(palette); ++i) {
        hash ^= ((const unsigned char *)palette)[i]; hash *= UINT64_C(1099511628211);
    }
    flow.status.corpus_hash = hash;
    flow.status.monitor_enabled = flow.config.monitor_enabled;
    flow.loaded = 1U;
    return 1;
}

static int new_run(void)
{
    int n;
    if (flow.status.run_serial == UINT64_MAX) return 0;
    ++flow.status.run_serial;
    flow.status.completed_beats = 0U; flow.status.window = 0U;
    flow.status.acknowledged_windows = 0U;
    flow.pending = flow.published = 0U;
    flow.status.hold_reason[0] = 0;
    n = snprintf(flow.status.source_id, sizeof(flow.status.source_id), "p.%016" PRIx64 ".%016" PRIx64,
        (uint64_t)flow.owner->clock.last_epoch_seconds, flow.status.run_serial);
    return n > 0 && (size_t)n < sizeof(flow.status.source_id);
}

int shakti_prenatal_start(shakti_runtime_t *runtime, const char *assets, const char *profile)
{
    size_t i;
    const shakti_retention_state_t *retained = shakti_retention_view();
    if (!runtime || runtime->memory.fault || flow.owner != runtime || busy() || !retained || retained->active ||
        runtime->media.current_ready || runtime->media.active != SHAKTI_MEDIA_GROUPS ||
        !path_valid(assets) || !path_valid(profile)) return bench("start requires local paths and an idle source boundary");
    for (i = 0U; i < SHAKTI_MEDIA_GROUPS; ++i)
        if (runtime->media.pending[i].in_use) return bench("loading zone is already owned");
    memcpy(flow.assets, assets, strlen(assets) + 1U);
    memcpy(flow.profile, profile, strlen(profile) + 1U);
    flow.loaded = 0U;
    if (!load_assets()) return 0;
    flow.status.requested_rate = runtime->loop.heartbeat_rate;
    flow.status.synthetic_fixture = 0U;
    if (!new_run()) return bench("run identity exhausted");
    flow.status.state = SHAKTI_PRENATAL_ARMED;
    if (!audit("prenatal_armed") || !checkpoint()) { hold("cannot commit start receipt"); return 0; }
    return 1;
}

int shakti_prenatal_restore(shakti_runtime_t *runtime)
{
    FILE *file;
    size_t n;
    int extra, bad;
    uint64_t expected;
    unsigned int saved_state;
    memset(&flow, 0, sizeof(flow)); flow.owner = runtime;
    if (!runtime) return 0;
    flow.status.requested_rate = runtime->loop.heartbeat_rate;
    errno = 0; file = fopen(SHAKTI_PRENATAL_CHECKPOINT, "rb");
    if (!file) {
        if (errno != ENOENT) hold("cannot read loader checkpoint");
        return 1;
    }
    n = fread(record, 1U, sizeof(record), file); extra = fgetc(file); bad = ferror(file);
    if (fclose(file)) bad = 1;
    expected = get(record + RECORD_HASH, 8U); put(record + RECORD_HASH, 0U, 8U);
    if (bad || extra != EOF || n != sizeof(record) || memcmp(record, "SPL20001", 8U) ||
        memcmp(record + RECORD_BYTES - 8U, "SPL2DONE", 8U) || get(record + 8U, 4U) != 2U ||
        expected != shakti_media_hash(record, sizeof(record)) ||
        !memchr(record + 128U, 0, SHAKTI_PATH_CAPACITY) ||
        !memchr(record + 640U, 0, SHAKTI_PATH_CAPACITY) ||
        get(record + 12U, 4U) > SHAKTI_PRENATAL_DELIVERED ||
        get(record + 24U, 8U) > SHAKTI_PRENATAL_END_BEAT ||
        get(record + 104U, 4U) > 1U || get(record + 64U, 4U) != SHAKTI_PRENATAL_OPENING_BEATS) {
        hold("whole loader checkpoint failed validation"); return 1;
    }
#ifndef SHAKTI_PRENATAL_RUN_HOOKS
    if (get(record + 104U, 4U)) { hold("synthetic RUN checkpoint cannot train production"); return 1; }
#endif
    saved_state = (unsigned int)get(record + 12U, 4U);
    flow.status.run_serial = get(record + 16U, 8U);
    flow.status.replays = get(record + 40U, 8U);
    flow.status.synthetic_fixture = (unsigned int)get(record + 104U, 4U);
    flow.status.requested_rate = (unsigned int)get(record + 56U, 4U);
    if (!flow.status.requested_rate || flow.status.requested_rate > 64U ||
        (flow.status.requested_rate & (flow.status.requested_rate - 1U))) {
        hold("checkpoint heartbeat rate invalid"); return 1;
    }
    if (saved_state == SHAKTI_PRENATAL_OFF) return 1;
    memcpy(flow.assets, record + 128U, sizeof(flow.assets));
    memcpy(flow.profile, record + 640U, sizeof(flow.profile));
    expected = get(record + 48U, 8U);
    if (!path_valid(flow.assets) || !path_valid(flow.profile) || !load_assets() ||
        flow.status.corpus_hash != expected) {
        flow.loaded = 0U; hold("restart corpus changed or is incomplete"); return 1;
    }
    if (flow.status.replays == UINT64_MAX || !new_run()) {
        hold("ordered replay identity exhausted"); return 1;
    }
    ++flow.status.replays;
    flow.status.state = saved_state == SHAKTI_PRENATAL_PAUSED ? SHAKTI_PRENATAL_PAUSED :
        saved_state == SHAKTI_PRENATAL_HELD ? SHAKTI_PRENATAL_HELD : SHAKTI_PRENATAL_ARMED;
    effective_rate(flow.status.requested_rate);
    if (!audit("prenatal_restart_replay") || !checkpoint()) hold("cannot commit ordered restart");
    return 1;
}

unsigned int shakti_prenatal_rate_requested(shakti_loop_state_t *loop, unsigned int rate)
{
    if (flow.owner && loop == &flow.owner->loop && busy() && !flow.setting_rate) {
        flow.status.requested_rate = rate;
        return flow.warm ? 1U : rate;
    }
    return rate;
}

int shakti_prenatal_gate(const shakti_runtime_t *runtime, const shakti_media_frame_t *frame)
{
    size_t lane;
    if (!busy()) return 1;
    if (!runtime || runtime != flow.owner || !active() || !flow.pending) return 0;
    for (lane = 0U; lane < 3U; ++lane) if (frame == &flow.frames[lane]) return 1;
    return 0;
}

int shakti_prenatal_owns(const shakti_runtime_t *runtime, const shakti_media_group_t *group)
{
    size_t lane;
    if (runtime != flow.owner || !flow.loaded || !flow.pending || !group ||
        group->required_mask != flow.frames[1].required_mask) return 0;
    for (lane = 0U; lane < 3U; ++lane) if (group->required_mask & (1U << lane)) {
        const shakti_media_frame_t *a = &group->parts[lane], *b = &flow.frames[lane];
        if (strcmp(a->source_id, flow.status.source_id) || a->position64 != b->position64 ||
            a->input != b->input || a->reference != b->reference || a->binary != b->binary ||
            a->rebuilt != b->rebuilt || a->bytes != b->bytes || a->expected_hash != b->expected_hash ||
            a->width != b->width || a->height != b->height || a->channels != b->channels ||
            a->sample_bytes != b->sample_bytes || a->sample_rate != b->sample_rate ||
            a->context[0] || a->lane != b->lane || a->required_mask != b->required_mask)
            return 0;
    }
    return 1;
}

static void settle_request(void)
{
    if (flow.pending) return;
    if (flow.stop_requested) {
        flow.stop_requested = 0U; flow.replay_requested = 0U; flow.warm = 0U;
        flow.status.state = SHAKTI_PRENATAL_OFF; effective_rate(flow.status.requested_rate);
        if (!audit("prenatal_stopped") || !checkpoint()) hold("cannot commit stop receipt");
    } else if (flow.status.state == SHAKTI_PRENATAL_PAUSING) {
        flow.status.state = SHAKTI_PRENATAL_PAUSED; flow.warm = 0U;
        effective_rate(flow.status.requested_rate);
        if (!audit("prenatal_paused") || !checkpoint()) hold("cannot commit pause receipt");
    } else if (flow.replay_requested) {
        flow.replay_requested = 0U;
        if (flow.status.replays == UINT64_MAX || !new_run()) { hold("replay counter exhausted"); return; }
        ++flow.status.replays; flow.status.state = SHAKTI_PRENATAL_ARMED;
        if (!audit("prenatal_resume_replay") || !checkpoint()) hold("cannot commit replay receipt");
    }
}

void shakti_prenatal_before_cycle(shakti_runtime_t *runtime, unsigned int slot)
{
    shakti_prenatal_plan_t plan;
    size_t i;
    if (runtime != flow.owner || !busy()) return;
    if (flow.status.state == SHAKTI_PRENATAL_HELD && flow.pending) {
        if (flow.published && runtime->media.current_ready) {
            if (!shakti_prenatal_owns(runtime, shakti_media_current(&runtime->media))) {
                hold("held view was replaced before release"); return;
            }
            shakti_media_release(&runtime->media);
        }
        (void)shakti_media_cancel(&runtime->media, flow.status.source_id,
            flow.frames[1].position64 / 2U);
        flow.pending = flow.published = 0U;
    }
    if (flow.pending && flow.published) {
        if (runtime->media.current_ready) {
            if (!shakti_prenatal_owns(runtime, shakti_media_current(&runtime->media))) {
                hold("owned view was replaced before release"); return;
            }
            shakti_media_release(&runtime->media);
        }
        flow.pending = flow.published = 0U;
        ++flow.status.acknowledged_windows;
        ++flow.status.window;
        if (flow.status.window == 32U) {
            flow.status.window = 0U; ++flow.status.completed_beats;
            if (flow.status.completed_beats == SHAKTI_PRENATAL_END_BEAT) {
                flow.status.state = SHAKTI_PRENATAL_DELIVERED; flow.warm = 0U;
                effective_rate(flow.status.requested_rate);
                if (!audit("prenatal_inputs_delivered") || !checkpoint()) hold("cannot commit final delivery");
                return;
            }
            if (!audit("prenatal_beat_delivered") || !checkpoint()) { hold("cannot commit completed beat"); return; }
        }
    }
    if (flow.pending && !flow.published) {
        for (i = 0U; i < SHAKTI_MEDIA_GROUPS; ++i)
            if (runtime->media.pending[i].in_use && runtime->media.pending[i].held &&
                shakti_prenatal_owns(runtime, &runtime->media.pending[i])) {
                (void)shakti_media_cancel(&runtime->media, flow.status.source_id,
                    flow.frames[1].position64 / 2U);
                flow.pending = 0U; hold("source reconstruction held; ordered replay required");
                (void)checkpoint(); break;
            }
    }
    settle_request();
    if (!active()) return;
    if (!shakti_prenatal_plan(flow.status.completed_beats + 1U, &plan)) { hold("timeline exhausted"); return; }
    flow.status.phase = plan.phase; flow.status.color = plan.color;
    flow.warm = plan.phase != 0U && plan.phase_beat < SHAKTI_PRENATAL_OPENING_BEATS;
    effective_rate(flow.warm ? 1U : flow.status.requested_rate);
    if (flow.status.state == SHAKTI_PRENATAL_ARMED && slot == 0U)
        flow.status.state = SHAKTI_PRENATAL_RUNNING;
}

void shakti_prenatal_feed(shakti_runtime_t *runtime, unsigned int slot)
{
    shakti_prenatal_plan_t plan;
    unsigned int lane, first, mask, source_slot;
    size_t n = 0U;
    uint64_t position;
    if (runtime != flow.owner || flow.status.state != SHAKTI_PRENATAL_RUNNING ||
        flow.pending || (slot & 1U)) return;
    if (!shakti_prenatal_plan(flow.status.completed_beats + 1U, &plan)) { hold("invalid next beat"); return; }
    source_slot = flow.status.window * 2U;
    position = flow.status.completed_beats * 64U + source_slot;
    mask = plan.phase == 2U ? SHAKTI_MEDIA_MOVIE : SHAKTI_MEDIA_AUDIO_PAIR;
    first = plan.phase == 2U ? 0U : 1U;
    memset(flow.frames, 0, sizeof(flow.frames));
    if (first == 0U && !shakti_prenatal_pixels_rgb(&flow.config, palette[plan.color], 4U,
        sight[0], sizeof(sight[0]), &n)) { hold("whole solid sight frame failed"); return; }
    for (lane = first; lane < 3U; ++lane) {
        shakti_media_frame_t *frame = &flow.frames[lane];
        memcpy(frame->source_id, flow.status.source_id, strlen(flow.status.source_id) + 1U);
        frame->lane = lane; frame->required_mask = mask;
        frame->position64 = position + (lane == 2U ? 1U : 0U);
        if (!lane) {
            frame->width = flow.config.width; frame->height = flow.config.height;
            frame->channels = 3U; frame->sample_bytes = 1U; frame->bytes = n;
            frame->format = SHAKTI_FORMAT_RGB8;
            frame->input = sight[0]; frame->reference = sight[1];
            frame->binary = sight[2]; frame->rebuilt = sight[3];
            frame->reference_capacity = frame->binary_capacity = frame->rebuilt_capacity = sizeof(sight[0]);
        } else {
            size_t i, offset = ((size_t)source_slot + lane - 1U) * 250U;
            unsigned char *out = hearing[lane - 1U][0];
            memset(out, 0, AUDIO_BYTES);
            for (i = 0U; i < 250U; ++i) {
                size_t voice_sample = plan.voice_sample + offset + i;
                if (plan.phase && voice_sample < waves[plan.color].sample_count)
                    memcpy(out + i * 4U, waves[plan.color].samples + voice_sample * 2U, 2U);
                memcpy(out + i * 4U + 2U, waves[8].samples + (offset + i) * 2U, 2U);
            }
            frame->channels = 2U; frame->sample_bytes = 2U; frame->sample_rate = 16000U;
            frame->format = SHAKTI_FORMAT_PCM_S16LE;
            frame->bytes = AUDIO_BYTES; frame->input = out;
            frame->reference = hearing[lane - 1U][1]; frame->binary = hearing[lane - 1U][2];
            frame->rebuilt = hearing[lane - 1U][3];
            frame->reference_capacity = frame->binary_capacity = frame->rebuilt_capacity = AUDIO_BYTES;
        }
        frame->expected_hash = shakti_media_hash(frame->input, frame->bytes);
    }
    flow.pending = 1U;
    for (lane = first; lane < 3U; ++lane)
        if (!shakti_prenatal_deposit(runtime, &flow.frames[lane])) {
            (void)shakti_media_cancel(&runtime->media, flow.status.source_id, position / 2U);
            flow.pending = 0U; hold("complete source window intake refused"); (void)checkpoint(); return;
        }
}

int shakti_prenatal_published(shakti_runtime_t *runtime)
{
    if (!active() || !shakti_prenatal_owns(runtime, shakti_media_current(&runtime->media)) || flow.published)
        return 0;
    flow.published = 1U;
    if (!audit("prenatal_validated_window")) { hold("cannot record validated exposure"); return 0; }
    return 1;
}

void shakti_prenatal_cycle_result(shakti_runtime_t *runtime, int success)
{
    if (runtime == flow.owner && active() && (!success || runtime->memory.fault)) {
        hold("runtime cycle failed; ordered replay required"); (void)checkpoint();
    }
}

int shakti_prenatal_pause(shakti_runtime_t *runtime)
{
    if (runtime != flow.owner || !active()) return bench("no running loader to pause");
    flow.status.state = SHAKTI_PRENATAL_PAUSING;
    if (!audit("prenatal_pause_requested")) return 0;
    settle_request(); return 1;
}

int shakti_prenatal_resume(shakti_runtime_t *runtime)
{
    if (runtime != flow.owner || !flow.loaded ||
        (flow.status.state != SHAKTI_PRENATAL_PAUSED && flow.status.state != SHAKTI_PRENATAL_HELD))
        return bench("resume requires the same loaded corpus in paused/held state");
    flow.replay_requested = 1U; settle_request();
    return flow.replay_requested || flow.status.state == SHAKTI_PRENATAL_ARMED;
}

int shakti_prenatal_stop(shakti_runtime_t *runtime)
{
    if (runtime != flow.owner || flow.status.state == SHAKTI_PRENATAL_OFF) return 0;
    flow.stop_requested = 1U;
    if (active()) flow.status.state = SHAKTI_PRENATAL_PAUSING;
    settle_request(); return 1;
}

int shakti_prenatal_status(const shakti_runtime_t *runtime, shakti_prenatal_status_t *status)
{
    if (runtime != flow.owner || !status) return 0;
    *status = flow.status;
    status->effective_rate = runtime->loop.heartbeat_rate;
    status->reserved_bytes = sizeof(flow) + sizeof(sight) + sizeof(hearing) + sizeof(files) +
        sizeof(profile_bytes) + sizeof(record) + sizeof(waves);
    return 1;
}

void shakti_prenatal_print_status(const shakti_runtime_t *runtime)
{
    shakti_prenatal_status_t status;
    if (!shakti_prenatal_status(runtime, &status)) return;
    printf("Prenatal state=%u completed=%" PRIu64 "/%" PRIu64 " window=%u run=%" PRIu64
        " phase=%u pace=%u requested=%u monitor=%u bytes=%lu\n", status.state,
        status.completed_beats, SHAKTI_PRENATAL_END_BEAT, status.window, status.run_serial,
        status.phase, status.effective_rate, status.requested_rate, status.monitor_enabled,
        (unsigned long)status.reserved_bytes);
    if (status.hold_reason[0]) printf("Loader hold: %s\n", status.hold_reason);
}

int shakti_prenatal_command(shakti_runtime_t *runtime, const char *arguments)
{
    char text[SHAKTI_INPUT_CAPACITY], *bar, *profile, *end;
    size_t n;
    if (!arguments) return 0;
    if (!strcmp(arguments, "status")) { shakti_prenatal_print_status(runtime); return 1; }
    if (strncmp(arguments, "start ", 6U) || (n = strlen(arguments + 6U)) >= sizeof(text))
        return bench("use prenatal start ASSET_DIRECTORY | PROFILE_PATH");
    memcpy(text, arguments + 6U, n + 1U); bar = strchr(text, '|');
    if (!bar || strchr(bar + 1U, '|')) return bench("two local paths separated by one bar required");
    *bar = 0; end = bar;
    while (end > text && isspace((unsigned char)end[-1])) *--end = 0;
    profile = bar + 1U;
    while (isspace((unsigned char)*profile)) ++profile;
    end = profile + strlen(profile);
    while (end > profile && isspace((unsigned char)end[-1])) *--end = 0;
    return shakti_prenatal_start(runtime, text, profile);
}

#ifdef SHAKTI_PRENATAL_RUN_HOOKS
int shakti_prenatal_run_position(shakti_runtime_t *runtime, uint64_t completed_beats)
{
    if (runtime != flow.owner || flow.status.state != SHAKTI_PRENATAL_ARMED ||
        flow.pending || completed_beats >= SHAKTI_PRENATAL_END_BEAT) return 0;
    flow.status.synthetic_fixture = 1U;
    flow.status.completed_beats = completed_beats;
    return audit("RUN_ONLY_injected_boundary") && checkpoint();
}
#endif
