/* Native input-boundary qualification. The RUN driver, not the loader,
 * invokes the existing workflow. Use only a new disposable fixture directory.
 * Link the actual pinned Section-A sources; no intake/decoder stand-ins. */
#define _POSIX_C_SOURCE 200809L
#define main shakti_original_console_main
#include "main.c"
#undef main
#include "shakti_prenatal_loader.h"
#include <sys/stat.h>

#define RUN_HD_BYTES (1920U * 1080U * 3U)
static shakti_runtime_t run_runtime;
static unsigned char run_sight[4][RUN_HD_BYTES + 8U];
static unsigned char run_audio[2][4][504U];
static unsigned char run_files[9][65536U];
static unsigned char run_heart_file[3U * 1024U * 1024U];
static unsigned char run_pulse[SHAKTI_PRENATAL_PULSE_BYTES];
static shakti_prenatal_wave_t run_waves[9];
static shakti_prenatal_config_t run_config;
static const char *const run_names[9] = {"black", "red", "green", "yellow",
    "blue", "magenta", "cyan", "white", "pulse"};

static void run_require(int ok, const char *what)
{
    if (!ok) { fprintf(stderr, "FAIL prenatal RUN: %s\n", what); exit(3); }
}

static size_t run_read(const char *path, unsigned char *out, size_t capacity)
{
    FILE *f = fopen(path, "rb");
    size_t n;
    int extra, bad;
    run_require(f != NULL, "read input");
    n = fread(out, 1U, capacity, f); extra = fgetc(f); bad = ferror(f);
    if (fclose(f)) bad = 1;
    run_require(!bad && extra == EOF, "whole input fits fixed storage");
    return n;
}

static void run_profile(const char *path)
{
    unsigned char xml[SHAKTI_PRENATAL_PROFILE_BYTES];
    size_t n = run_read(path, xml, sizeof(xml));
    run_require(shakti_prenatal_config_parse((const char *)xml, n, &run_config), "XML profile");
}

static void run_put32(unsigned char *p, uint32_t n)
{
    p[0] = (unsigned char)n; p[1] = (unsigned char)(n >> 8);
    p[2] = (unsigned char)(n >> 16); p[3] = (unsigned char)(n >> 24);
}

static void run_write_pulse(const char *path)
{
    unsigned char header[44] = {0};
    int fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0600), bad;
    FILE *f;
    run_require(fd >= 0, "create a new pulse file exclusively");
    f = fdopen(fd, "wb");
    run_require(f != NULL, "pulse file stream");
    memcpy(header, "RIFF", 4U); run_put32(header + 4U, 32036U);
    memcpy(header + 8U, "WAVEfmt ", 8U); run_put32(header + 16U, 16U);
    header[20] = 1U; header[22] = 1U; run_put32(header + 24U, 16000U);
    run_put32(header + 28U, 32000U); header[32] = 2U; header[34] = 16U;
    memcpy(header + 36U, "data", 4U); run_put32(header + 40U, 32000U);
    bad = fwrite(header, 1U, sizeof(header), f) != sizeof(header);
    bad |= fwrite(run_pulse, 1U, sizeof(run_pulse), f) != sizeof(run_pulse);
    bad |= fflush(f) != 0; bad |= fsync(fd) != 0; bad |= fclose(f) != 0;
    run_require(!bad, "complete pulse file write");
}

static int32_t run_peak(const shakti_prenatal_wave_t *wave)
{
    int32_t peak = 0;
    size_t i;
    for (i = 0U; i < wave->sample_count; ++i) {
        uint32_t n = (uint32_t)wave->samples[i * 2U] |
            ((uint32_t)wave->samples[i * 2U + 1U] << 8);
        int32_t v = n < 32768U ? (int32_t)n : (int32_t)n - 65536;
        if (v < 0) v = -v;
        if (v > peak) peak = v;
    }
    return peak;
}

static void run_pack(const char *directory)
{
    unsigned int i;
    puts("asset,samples,rate_hz,peak");
    for (i = 0U; i < 9U; ++i) {
        char path[1024];
        size_t n;
        int printed = snprintf(path, sizeof(path), "%s/%s.wav", directory, run_names[i]);
        run_require(printed > 0 && (size_t)printed < sizeof(path), "asset path capacity");
        run_require(shakti_prenatal_wave_load(path, run_files[i], sizeof(run_files[i]),
            &n, &run_waves[i]), "whole real WAV load");
        printf("%s,%lu,%u,%ld\n", run_names[i], (unsigned long)run_waves[i].sample_count,
            run_waves[i].sample_rate, (long)run_peak(&run_waves[i]));
    }
}

static void run_input_checks(void)
{
    shakti_prenatal_config_t c = run_config, before;
    unsigned int d, odd, strength, color;
    size_t n, off, total;
    unsigned char small[32U * 24U * 3U], bad_wav[64];
    shakti_prenatal_wave_t wave;
    const unsigned char mid[3] = {0U, 128U, 0U};
    for (d = 0U; d < 64U; d += 2U) {
        unsigned int even;
        run_require(shakti_prenatal_density(&c, d, &even) &&
            shakti_prenatal_density(&c, d + 1U, &odd) && even == odd, "two hearing slots share one sight density");
    }
    c.width = 32U; c.height = 24U;
    for (strength = 0U; strength <= 4U; ++strength) {
        size_t lit = 0U, p;
        run_require(shakti_prenatal_pixels(&c, 7U, strength, small, sizeof(small), &n), "density frame");
        for (p = 0U; p < n; p += 3U) if (small[p]) ++lit;
        run_require(lit == 32U * 24U * strength / 4U, "exact quarter pixel density");
    }
    for (color = 0U; color < 8U; ++color) {
        size_t p;
        run_require(shakti_prenatal_pixels(&c, color, 4U, small, sizeof(small), &n), "eight RGB corners");
        for (p = 0U; p < n; p += 3U) {
            run_require(small[p] == ((color & 1U) ? 255U : 0U) &&
                small[p + 1U] == ((color & 2U) ? 255U : 0U) &&
                small[p + 2U] == ((color & 4U) ? 255U : 0U), "exact native RGB bytes");
        }
    }
    run_require(shakti_prenatal_pixels_rgb(&c, mid, 4U, small, sizeof(small), &n) &&
        small[0] == 0U && small[1] == 128U && small[2] == 0U, "hex palette brightness retained");
    memset(small, 0x5a, sizeof(small));
    run_require(!shakti_prenatal_pixels(&c, 7U, 4U, small, sizeof(small) - 1U, &n) &&
        small[0] == 0x5aU, "over-capacity whole frame refused before writes");
    before = c;
    run_require(!shakti_prenatal_config_parse("<prenatal_loader><width>1</width></prenatal_loader>",
        strlen("<prenatal_loader><width>1</width></prenatal_loader>"), &c) &&
        !memcmp(&before, &c, sizeof(c)), "invalid XML retains prior complete config");
    run_require(!shakti_prenatal_wave_parse(run_files[1], 12U, &wave), "truncated WAV refused");
    memcpy(bad_wav, run_files[1], sizeof(bad_wav)); bad_wav[20] = 3U;
    run_put32(bad_wav + 4U, sizeof(bad_wav) - 8U);
    run_require(!shakti_prenatal_wave_parse(bad_wav, sizeof(bad_wav), &wave), "unsupported WAV refused");
    run_require(!shakti_prenatal_audio(&run_waves[1], 0U, (unsigned char *)run_waves[1].samples,
        500U, &n), "borrowed voice input alias refused");
    for (color = 0U; color < 8U; ++color) {
        total = 0U;
        for (off = 0U; off < run_waves[color].sample_count; off += 250U) {
            size_t k;
            run_require(shakti_prenatal_audio(&run_waves[color], off, run_audio[0][0],
                sizeof(run_audio[0][0]), &n), "voice continuation chunk");
            run_require(!memcmp(run_audio[0][0], run_waves[color].samples + off * 2U, n * 2U),
                "every original voice sample preserved");
            for (k = n * 2U; k < 500U; ++k) run_require(!run_audio[0][0][k], "final audio padding is silence");
            total += n;
        }
        run_require(total == run_waves[color].sample_count, "long voice retains whole tail across beats");
    }
    {
        shakti_prenatal_wave_t prepared;
        c = run_config; c.voice_to_pulse_ratio = 100U;
        run_require(shakti_prenatal_pulse(&run_waves[8], &run_waves[5], &c,
            run_pulse, sizeof(run_pulse), &n), "quieter gain is configurable");
        prepared.samples = run_pulse; prepared.sample_count = 16000U; prepared.sample_rate = 16000U;
        run_require(run_peak(&prepared) == run_peak(&run_waves[5]) / 100, "actual 100:1 peak setting");
        c.pulse_on_numerator = 1597U; c.pulse_on_denominator = 2584U;
        run_require(shakti_prenatal_pulse(&run_waves[8], &run_waves[5], &c,
            run_pulse, sizeof(run_pulse), &n) && n == 9889U, "declared rational phi approximation");
    }
    puts("INPUT PASS: exact density/RGB, shared 2:1 profile, XML bounds, whole WAVs and complete voice tails");
}

static shakti_media_frame_t run_frame(unsigned int lane, unsigned int mask,
    uint64_t position, const shakti_prenatal_config_t *c)
{
    shakti_media_frame_t f;
    memset(&f, 0, sizeof(f));
    strcpy(f.source_id, "prenatal.input"); f.position64 = position;
    f.lane = lane; f.required_mask = mask;
    if (!lane) {
        f.width = c->width; f.height = c->height; f.channels = 3U; f.sample_bytes = 1U;
        f.bytes = (size_t)c->width * c->height * 3U;
        f.input = run_sight[0]; f.reference = run_sight[1];
        f.binary = run_sight[2]; f.rebuilt = run_sight[3];
        f.reference_capacity = f.binary_capacity = f.rebuilt_capacity = sizeof(run_sight[0]);
    } else {
        f.channels = 1U; f.sample_bytes = 2U; f.sample_rate = 16000U; f.bytes = 500U;
        f.input = run_audio[lane - 1U][0]; f.reference = run_audio[lane - 1U][1];
        f.binary = run_audio[lane - 1U][2]; f.rebuilt = run_audio[lane - 1U][3];
        f.reference_capacity = f.binary_capacity = f.rebuilt_capacity = sizeof(run_audio[0][0]);
    }
    return f;
}

static void run_deposit(const shakti_media_frame_t *f)
{
    uint64_t checked = run_runtime.media.validated_parts, published = run_runtime.media.published;
    run_require(shakti_prenatal_deposit(&run_runtime, f), "actual MCP intake admits sample");
    run_require(run_runtime.media.validated_parts == checked && run_runtime.media.published == published,
        "deposit itself advances no binary or convergence stage");
}

static void run_windows(const shakti_prenatal_wave_t *voice,
    const shakti_prenatal_config_t *c, unsigned int beats, int sight)
{
    unsigned int p, windows = beats * 32U, mask = sight ? 7U : 6U;
    uint64_t memory_before = shakti_media_hash((const unsigned char *)&run_runtime.memory, sizeof(run_runtime.memory));
    shakti_media_init(&run_runtime.media);
    for (p = 0U; p < windows; ++p) {
        shakti_media_frame_t parts[3];
        shakti_convergence_view_t view;
        unsigned int lane, first = sight ? 0U : 1U, density;
        size_t n;
        if (!(p % 32U)) run_require(shakti_tick_clock_beat(&run_runtime.clock), "external RUN beat epoch");
        if (sight) {
            run_require(shakti_prenatal_density(c, (p % 32U) * 2U, &density), "requested density");
            run_require(shakti_prenatal_pixels(c, 4U, density, run_sight[0], sizeof(run_sight[0]), &n), "requested full sight input");
            parts[0] = run_frame(0U, mask, (uint64_t)p * 2U, c);
            run_deposit(&parts[0]);
        }
        for (lane = 1U; lane < 3U; ++lane) {
            size_t sample = sight ? ((size_t)p * 2U + lane - 1U) * 250U :
                (((size_t)p * 2U + lane - 1U) % 64U) * 250U;
            run_require(shakti_prenatal_audio(voice, sample, run_audio[lane - 1U][0], sizeof(run_audio[0][0]), &n), "requested audio input");
            parts[lane] = run_frame(lane, mask, (uint64_t)p * 2U + lane - 1U, c);
            run_deposit(&parts[lane]);
        }
        run_require(shakti_media_take(&run_runtime.media) == 1, "workflow takes ordinary queued window");
        for (lane = first; lane < 3U; ++lane) {
            size_t i;
            for (i = 0U; i < parts[lane].bytes; ++i)
                run_require(parts[lane].rebuilt[i] != parts[lane].reference[i], "all rebuilt bytes initially poisoned");
            memset((unsigned char *)parts[lane].input, 0x39, parts[lane].bytes);
        }
        run_require(shakti_media_step(&run_runtime.media, (p % 32U) * 2U) == 1 &&
            shakti_media_step(&run_runtime.media, (p % 32U) * 2U + 1U) == 2, "real binary reconstruction and publication");
        run_require(shakti_app_convergence_view(&run_runtime, &view) && view.sensory &&
            view.sensory == shakti_media_perception(&run_runtime.media), "real app and senses share one view");
        for (lane = first; lane < 3U; ++lane) {
            run_require(view.sensory->parts[lane].data == parts[lane].rebuilt &&
                !memcmp(parts[lane].rebuilt, parts[lane].reference, parts[lane].bytes) &&
                !view.sensory->parts[lane].context[0], "same rebuilt pointers, exact bytes, empty context");
        }
        if (!p) {
            shakti_media_frame_t reuse = parts[first];
            reuse.position64 += 128U;
            run_require(!shakti_prenatal_deposit(&run_runtime, &reuse), "leased buffers refuse premature reuse");
        }
        shakti_media_release(&run_runtime.media); /* RUN owns this external workflow action. */
        run_require(!shakti_media_perception(&run_runtime.media), "release ends the borrowed view");
    }
    run_require(run_runtime.media.published == windows &&
        run_runtime.media.validated_parts == (uint64_t)windows * (sight ? 3U : 2U), "all requested windows actually processed");
    run_require(memory_before == shakti_media_hash((const unsigned char *)&run_runtime.memory, sizeof(run_runtime.memory)),
        "loader and queue fixture leave memory byte-identical");
    printf("BINARY PASS: sight=%u audio=%u windows=%u beats=%u\n", sight ? windows : 0U,
        windows * 2U, windows, beats);
}

static void run_rejections(void)
{
    static unsigned char pool[5][4][24];
    shakti_media_frame_t f;
    unsigned int i;
    uint64_t submitted;
    shakti_media_init(&run_runtime.media);
    memset(&f, 0, sizeof(f)); strcpy(f.source_id, "prenatal.input");
    f.required_mask = 1U; f.width = 4U; f.height = 2U; f.channels = 3U; f.sample_bytes = 1U;
    f.bytes = f.reference_capacity = f.binary_capacity = f.rebuilt_capacity = 24U;
    for (i = 0U; i < 5U; ++i) {
        f.position64 = 200U + i * 2U; f.input = pool[i][0]; f.reference = pool[i][1];
        f.binary = pool[i][2]; f.rebuilt = pool[i][3];
        run_require(shakti_prenatal_deposit(&run_runtime, &f) == (i < 4U), "bounded loading-zone pressure");
    }
    for (i = 0U; i < 4U; ++i)
        run_require(shakti_media_cancel(&run_runtime.media, "prenatal.input", 100U + i), "RUN cancels owned pressure probes");
    f.position64 = 0U; f.input = pool[0][0]; f.reference = pool[0][1]; f.binary = pool[0][2]; f.rebuilt = pool[0][3];
    strcpy(f.context, "named-color"); submitted = run_runtime.media.submitted;
    run_require(!shakti_prenatal_deposit(&run_runtime, &f) && run_runtime.media.submitted == submitted,
        "prenatal context label refused before intake");
    f.context[0] = 0; run_deposit(&f);
    run_require(shakti_media_take(&run_runtime.media) == 1, "take corrupt-binary probe");
    f.binary[0] ^= 1U;
    run_require(shakti_media_step(&run_runtime.media, 0U) == -1 && !shakti_media_perception(&run_runtime.media),
        "wrong binary refuses whole window before convergence");
    run_require(shakti_media_cancel(&run_runtime.media, "prenatal.input", 0U), "RUN cancels held probe");
    puts("HOLD PASS: queue pressure, context labels, leased reuse and damaged binary");
}

int main(int argc, char **argv)
{
    size_t n, active, i;
    shakti_prenatal_wave_t source, reference;
    if (argc == 6 && !strcmp(argv[1], "--prepare")) {
        run_profile(argv[2]);
        run_require(shakti_prenatal_wave_load(argv[3], run_heart_file, sizeof(run_heart_file), &n, &source), "source heartbeat WAV");
        run_require(shakti_prenatal_wave_load(argv[4], run_files[0], sizeof(run_files[0]), &n, &reference), "reference voice WAV");
        run_require(shakti_prenatal_pulse(&source, &reference, &run_config, run_pulse, sizeof(run_pulse), &active), "prepare one pulse atom");
        run_write_pulse(argv[5]);
        printf("PULSE PREPARED: samples=16000 active=%lu off=%lu ratio=%u/%u voice_peak_ratio=%u monitor=%u\n",
            (unsigned long)active, (unsigned long)(16000U - active), run_config.pulse_on_numerator,
            run_config.pulse_on_denominator, run_config.voice_to_pulse_ratio, run_config.monitor_enabled);
        return 0;
    }
    if (argc != 5 || strcmp(argv[1], "--run")) {
        fputs("usage: prenatal-run --prepare PROFILE SOURCE_HEART REFERENCE_VOICE NEW_PULSE\n"
              "       prenatal-run --run ASSETS PROFILE NEW_FIXTURE\n", stderr); return 2;
    }
    run_profile(argv[3]); run_pack(argv[2]); run_input_checks();
    run_require(run_waves[8].sample_count == 16000U, "one reusable pulse atom");
    active = (size_t)(((uint64_t)16000U * run_config.pulse_on_numerator + run_config.pulse_on_denominator / 2U) /
        run_config.pulse_on_denominator);
    for (i = active * 2U; i < 32000U; ++i) run_require(!run_waves[8].samples[i], "pulse off region is exact zero");
    {
        int32_t quietest = run_peak(&run_waves[0]);
        for (i = 1U; i < 8U; ++i) {
            int32_t peak = run_peak(&run_waves[i]);
            if (peak < quietest) quietest = peak;
        }
        run_require(run_peak(&run_waves[8]) == quietest / (int32_t)run_config.voice_to_pulse_ratio,
            "pulse referenced to the quietest whole color voice");
    }
    run_require(mkdir(argv[4], 0700) == 0 && chdir(argv[4]) == 0 && mkdir("data", 0700) == 0 &&
        mkdir("data/learned", 0700) == 0, "new isolated fixture");
    memset(&run_runtime, 0, sizeof(run_runtime));
    shakti_tick_clock_seed(&run_runtime.clock, (time_t)2200000000L);
    run_windows(&run_waves[4], &run_config, 1U, 1);
    run_windows(&run_waves[8], &run_config, 1U, 0);
    {
        shakti_prenatal_config_t small = run_config;
        small.width = 32U; small.height = 24U;
        run_windows(&run_waves[0], &small, 2U, 1);
    }
    run_rejections();
    {
        static unsigned char complete[128U * 1024U];
        unsigned long valid = 0UL, invalid = 0UL;
        size_t bytes;
        FILE *audit;
        int fd, bad;
        run_require(shakti_log_validate_file(SHAKTI_LEARNED_STREAM_PATH, &valid, &invalid) &&
            valid == 357UL && invalid == 0UL, "357 complete checked native intake receipts");
        bytes = run_read(SHAKTI_LEARNED_STREAM_PATH, complete, sizeof(complete));
        fd = open("audit.pending", O_WRONLY | O_CREAT | O_EXCL, 0600);
        run_require(fd >= 0, "exclusive audit export");
        audit = fdopen(fd, "wb"); run_require(audit != NULL, "audit stream");
        bad = fwrite(complete, 1U, bytes, audit) != bytes;
        bad |= fflush(audit) != 0; bad |= fsync(fd) != 0; bad |= fclose(audit) != 0;
        run_require(!bad && rename("audit.pending", "audit.complete.txt") == 0, "atomic complete receipt export");
        printf("AUDIT PASS: valid=%lu invalid=%lu\n", valid, invalid);
        printf("AUDIT BYTES: %lu fnv1a64=%016" PRIx64 "\n", (unsigned long)bytes,
            shakti_media_hash(complete, bytes));
    }
    puts("PASS: input-only loader; actual native intake/binary/shared-view checks; no speaker opened");
    puts("BOUNDARY: synthetic external driver; separate simultaneous heartbeat port/native HTML bridge remain integration work");
    return 0;
}
