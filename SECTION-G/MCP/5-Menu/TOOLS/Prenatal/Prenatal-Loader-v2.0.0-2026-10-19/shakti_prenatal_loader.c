#include "shakti_prenatal_loader.h"

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

static int bench(const char *reason)
{
    fprintf(stderr, "BENCH prenatal loader: %s\n", reason);
    return 0;
}

static int overlap(const void *a, size_t na, const void *b, size_t nb)
{
    uintptr_t aa = (uintptr_t)a, bb = (uintptr_t)b;
    if (na > UINTPTR_MAX - aa || nb > UINTPTR_MAX - bb) return 1;
    return aa < bb + nb && bb < aa + na;
}

static uint32_t le32(const unsigned char *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
        ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static unsigned int le16(const unsigned char *p)
{
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8);
}

static int32_t signed16(const unsigned char *p)
{
    unsigned int value = le16(p);
    return value < 32768U ? (int32_t)value : (int32_t)value - 65536;
}

static void put16(unsigned char *p, int32_t sample)
{
    uint16_t value = (uint16_t)sample;
    p[0] = (unsigned char)value;
    p[1] = (unsigned char)(value >> 8);
}

static int config_valid(const shakti_prenatal_config_t *c)
{
    unsigned int i;
    if (!c || !c->width || !c->height ||
        !c->pulse_on_numerator || !c->pulse_on_denominator ||
        c->pulse_on_denominator > 1000000U ||
        c->pulse_on_numerator > c->pulse_on_denominator ||
        c->voice_to_pulse_ratio < 2U || c->voice_to_pulse_ratio > 32768U ||
        c->monitor_enabled > 1U) return 0;
    for (i = 0U; i < 32U; ++i) if (c->density[i] > 4U) return 0;
    return 1;
}

void shakti_prenatal_defaults(shakti_prenatal_config_t *c)
{
    unsigned int i;
    if (!c) return;
    memset(c, 0, sizeof(*c));
    c->width = 1920U; c->height = 1080U;
    c->pulse_on_numerator = 2U; c->pulse_on_denominator = 3U;
    c->voice_to_pulse_ratio = 4U;
    for (i = 0U; i < 32U; ++i) c->density[i] = 4U;
}

static void space(const char **p)
{
    while (**p && isspace((unsigned char)**p)) ++*p;
}

static int take(const char **p, const char *literal)
{
    size_t n = strlen(literal);
    if (strncmp(*p, literal, n)) return 0;
    *p += n;
    return 1;
}

static int number(const char **p, unsigned int *out)
{
    unsigned int n = 0U;
    if (**p < '0' || **p > '9') return 0;
    do {
        unsigned int digit = (unsigned int)(**p - '0');
        if (n > (UINT_MAX - digit) / 10U) return 0;
        n = n * 10U + digit;
        ++*p;
    } while (**p >= '0' && **p <= '9');
    *out = n;
    return 1;
}

int shakti_prenatal_config_parse(const char *xml, size_t bytes,
    shakti_prenatal_config_t *config)
{
    static const char *names[7] = {"width", "height", "pulse_on_numerator",
        "pulse_on_denominator", "voice_to_pulse_ratio", "monitor_enabled", "density"};
    char text[SHAKTI_PRENATAL_PROFILE_BYTES];
    shakti_prenatal_config_t candidate;
    unsigned int *fields[6], seen = 0U, i;
    const char *p;
    if (!xml || !config || bytes >= sizeof(text) || memchr(xml, 0, bytes))
        return bench("profile bytes/capacity");
    memcpy(text, xml, bytes); text[bytes] = 0; p = text;
    memset(&candidate, 0, sizeof(candidate));
    fields[0] = &candidate.width; fields[1] = &candidate.height;
    fields[2] = &candidate.pulse_on_numerator;
    fields[3] = &candidate.pulse_on_denominator;
    fields[4] = &candidate.voice_to_pulse_ratio;
    fields[5] = &candidate.monitor_enabled;
    space(&p);
    if (!take(&p, "<prenatal_loader>")) return bench("profile root");
    for (;;) {
        unsigned int key;
        char tag[64];
        space(&p);
        if (take(&p, "</prenatal_loader>")) break;
        for (key = 0U; key < 7U; ++key) {
            int n = snprintf(tag, sizeof(tag), "<%s>", names[key]);
            if (n < 0 || (size_t)n >= sizeof(tag)) return bench("profile tag capacity");
            if (take(&p, tag)) break;
        }
        if (key == 7U || (seen & (1U << key))) return bench("unknown/duplicate profile field");
        space(&p);
        if (key == 6U) {
            for (i = 0U; i < 32U; ++i) {
                if (!number(&p, &candidate.density[i])) return bench("32 density values required");
                if (i < 31U && !isspace((unsigned char)*p)) return bench("density separator");
                space(&p);
            }
        } else if (!number(&p, fields[key])) return bench("unsigned profile integer");
        space(&p);
        {
            int n = snprintf(tag, sizeof(tag), "</%s>", names[key]);
            if (n < 0 || (size_t)n >= sizeof(tag) || !take(&p, tag))
                return bench("profile closing tag");
        }
        seen |= 1U << key;
    }
    space(&p);
    if (*p || seen != 127U || !config_valid(&candidate)) return bench("incomplete/invalid profile");
    *config = candidate;
    return 1;
}

int shakti_prenatal_wave_parse(const unsigned char *file, size_t bytes,
    shakti_prenatal_wave_t *wave)
{
    size_t offset = 12U, data_size = 0U;
    const unsigned char *data = NULL;
    int format = 0;
    shakti_prenatal_wave_t candidate;
    if (!file || !wave || bytes < 12U || memcmp(file, "RIFF", 4U) ||
        memcmp(file + 8U, "WAVE", 4U) ||
        (uint64_t)le32(file + 4U) + 8U != bytes) return bench("complete RIFF/WAVE required");
    while (offset < bytes) {
        size_t n, padded;
        const unsigned char *chunk;
        if (bytes - offset < 8U) return bench("incomplete WAV chunk header");
        n = (size_t)le32(file + offset + 4U);
        if (n > bytes - offset - 8U) return bench("WAV chunk exceeds input");
        padded = n + (n & 1U);
        if (padded < n || padded > bytes - offset - 8U) return bench("WAV chunk padding");
        chunk = file + offset + 8U;
        if (!memcmp(file + offset, "fmt ", 4U)) {
            if (format || n < 16U || le16(chunk) != 1U || le16(chunk + 2U) != 1U ||
                le32(chunk + 4U) != SHAKTI_PRENATAL_RATE ||
                le32(chunk + 8U) != 32000U || le16(chunk + 12U) != 2U ||
                le16(chunk + 14U) != 16U) return bench("16 kHz signed 16-bit mono PCM required");
            format = 1;
        } else if (!memcmp(file + offset, "data", 4U)) {
            if (data || !n || (n & 1U)) return bench("single complete WAV sample chunk required");
            data = chunk; data_size = n;
        }
        offset += 8U + padded;
    }
    if (!format || !data) return bench("missing WAV format or samples");
    candidate.samples = data; candidate.sample_count = data_size / 2U;
    candidate.sample_rate = SHAKTI_PRENATAL_RATE;
    *wave = candidate;
    return 1;
}

int shakti_prenatal_wave_load(const char *path, unsigned char *storage,
    size_t capacity, size_t *loaded_bytes, shakti_prenatal_wave_t *wave)
{
    FILE *file;
    size_t n;
    int extra, bad;
    if (!path || !storage || !capacity || !loaded_bytes || !wave)
        return bench("wave load arguments");
    *loaded_bytes = 0U;
    memset(wave, 0, sizeof(*wave));
    file = fopen(path, "rb");
    if (!file) return bench("cannot open supplied WAV");
    n = fread(storage, 1U, capacity, file);
    extra = fgetc(file); bad = ferror(file);
    if (fclose(file)) bad = 1;
    if (bad || extra != EOF) return bench("whole WAV read/capacity");
    if (!shakti_prenatal_wave_parse(storage, n, wave)) return 0;
    *loaded_bytes = n;
    return 1;
}

static int wave_valid(const shakti_prenatal_wave_t *wave)
{
    return wave && wave->samples && wave->sample_count &&
        wave->sample_count <= SIZE_MAX / 2U && wave->sample_rate == SHAKTI_PRENATAL_RATE;
}

int shakti_prenatal_pulse(const shakti_prenatal_wave_t *source,
    const shakti_prenatal_wave_t *voice, const shakti_prenatal_config_t *config,
    unsigned char *input, size_t capacity, size_t *active_samples)
{
    size_t i, on, edge;
    int32_t voice_peak = 0, pulse_peak = 0, target;
    if (!wave_valid(source) || !wave_valid(voice) || !config_valid(config) ||
        source->sample_count < SHAKTI_PRENATAL_RATE || !input || !active_samples ||
        capacity < SHAKTI_PRENATAL_PULSE_BYTES ||
        overlap(input, SHAKTI_PRENATAL_PULSE_BYTES, source->samples, source->sample_count * 2U) ||
        overlap(input, SHAKTI_PRENATAL_PULSE_BYTES, voice->samples, voice->sample_count * 2U))
        return bench("pulse arguments/capacity/ownership");
    *active_samples = 0U;
    on = (size_t)(((uint64_t)SHAKTI_PRENATAL_RATE * config->pulse_on_numerator +
        config->pulse_on_denominator / 2U) / config->pulse_on_denominator);
    if (on < 2U || on > SHAKTI_PRENATAL_RATE) return bench("pulse sample fraction");
    for (i = 0U; i < voice->sample_count; ++i) {
        int32_t v = signed16(voice->samples + i * 2U);
        if (v < 0) v = -v;
        if (v > voice_peak) voice_peak = v;
    }
    target = voice_peak / (int32_t)config->voice_to_pulse_ratio;
    if (!target) return bench("reference voice has no usable amplitude");
    edge = 1500U;
    if (edge > on / 2U) edge = on / 2U;
    memset(input, 0, SHAKTI_PRENATAL_PULSE_BYTES);
    for (i = 0U; i < on; ++i) {
        size_t source_index = (size_t)((uint64_t)i * (SHAKTI_PRENATAL_RATE - 1U) / (on - 1U));
        size_t envelope = i + 1U;
        int32_t sample, magnitude;
        if (envelope > on - i) envelope = on - i;
        if (envelope > edge) envelope = edge;
        sample = (int32_t)((int64_t)signed16(source->samples + source_index * 2U) *
            (int64_t)envelope / (int64_t)edge);
        put16(input + i * 2U, sample);
        magnitude = sample < 0 ? -sample : sample;
        if (magnitude > pulse_peak) pulse_peak = magnitude;
    }
    if (!pulse_peak) return bench("source pulse has no usable amplitude");
    for (i = 0U; i < on; ++i) {
        int32_t sample = (int32_t)((int64_t)signed16(input + i * 2U) * target / pulse_peak);
        put16(input + i * 2U, sample);
    }
    *active_samples = on;
    return 1;
}

int shakti_prenatal_audio(const shakti_prenatal_wave_t *wave,
    size_t sample_offset, unsigned char *input, size_t capacity,
    size_t *recorded_samples)
{
    size_t n = 0U;
    if (!wave_valid(wave) || !input || !recorded_samples ||
        capacity < SHAKTI_PRENATAL_AUDIO_BYTES ||
        overlap(input, SHAKTI_PRENATAL_AUDIO_BYTES, wave->samples, wave->sample_count * 2U))
        return bench("audio input arguments/capacity/ownership");
    if (sample_offset < wave->sample_count) {
        n = wave->sample_count - sample_offset;
        if (n > 250U) n = 250U;
    }
    memset(input, 0, SHAKTI_PRENATAL_AUDIO_BYTES);
    if (n) memcpy(input, wave->samples + sample_offset * 2U, n * 2U);
    *recorded_samples = n;
    return 1;
}

int shakti_prenatal_pixels_rgb(const shakti_prenatal_config_t *config,
    const unsigned char rgb[3], unsigned int density, unsigned char *input,
    size_t capacity, size_t *bytes)
{
    static const unsigned int rank[4] = {0U, 2U, 1U, 3U};
    uint64_t required;
    unsigned int x, y;
    size_t offset = 0U;
    unsigned char color[3];
    if (!config_valid(config) || !rgb || density > 4U || !input || !bytes)
        return bench("sight input arguments");
    required = (uint64_t)config->width * config->height;
    if (required > SIZE_MAX / 3U || capacity < (size_t)required * 3U)
        return bench("whole native sight frame exceeds input capacity");
    memcpy(color, rgb, sizeof(color));
    for (y = 0U; y < config->height; ++y) for (x = 0U; x < config->width; ++x) {
        unsigned int active = rank[((x & 3U) + 4U - (y & 3U)) & 3U] < density;
        input[offset++] = active ? color[0] : 0U;
        input[offset++] = active ? color[1] : 0U;
        input[offset++] = active ? color[2] : 0U;
    }
    *bytes = offset;
    return 1;
}

int shakti_prenatal_pixels(const shakti_prenatal_config_t *config,
    unsigned int rgb_bits, unsigned int density, unsigned char *input,
    size_t capacity, size_t *bytes)
{
    unsigned char rgb[3];
    if (rgb_bits > 7U) return bench("RGB corner bits");
    rgb[0] = rgb_bits & 1U ? 255U : 0U;
    rgb[1] = rgb_bits & 2U ? 255U : 0U;
    rgb[2] = rgb_bits & 4U ? 255U : 0U;
    return shakti_prenatal_pixels_rgb(config, rgb, density, input, capacity, bytes);
}

int shakti_prenatal_density(const shakti_prenatal_config_t *config,
    unsigned int hearing_slot, unsigned int *density)
{
    if (!config_valid(config) || hearing_slot >= 64U || !density)
        return bench("density phase arguments");
    *density = config->density[hearing_slot / 2U];
    return 1;
}

int shakti_prenatal_deposit(shakti_runtime_t *runtime,
    const shakti_media_frame_t *frame)
{
    if (!runtime || !frame || frame->context[0]) return bench("empty prenatal context required");
    return shakti_app_queue_frame(runtime, frame);
}
