#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "shakti_app.h"
#include "shakti_loop.h"
#include "shakti_memory.h"
#include "shakti_time.h"
#include "shakti_log.h"
#include <time.h>

#define REQUIRE(c, message) do { if (!(c)) { fprintf(stderr, "BENCH RUN: %s\n", message); return 0; } } while (0)
static shakti_runtime_t runtime;
static shakti_memory_state_t memory;
static char payload[SHAKTI_MEMORY_BYTES + 1U];

static int create_text(const char *path, const char *text)
{
    int fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0600);
    size_t left = strlen(text);
    const char *cursor = text;
    if (fd < 0) return 0;
    while (left) {
        ssize_t n = write(fd, cursor, left);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) { close(fd); return 0; }
        cursor += (size_t)n;
        left -= (size_t)n;
    }
    return close(fd) == 0;
}

static int seed_fixture(const char *directory)
{
    static const char manifest[] =
        "<manifest schema=\"SHAKTI_EDEN_MANIFEST_V1\">\n"
        "<status>OPEN</status>\n<artifact_root>artifacts</artifact_root>\n"
        "<level order=\"1\">\n<id>fixture</id>\n<title>Synthetic only</title>\n"
        "<requires>none</requires>\n</level>\n</manifest>\n";
    REQUIRE(mkdir(directory, 0700) == 0, "fixture directory must be new");
    REQUIRE(chdir(directory) == 0, "enter fixture directory");
    REQUIRE(mkdir("data", 0700) == 0 && mkdir("data/eden", 0700) == 0 &&
        mkdir("data/learned", 0700) == 0 && mkdir("data/school", 0700) == 0 &&
        mkdir("data/control", 0700) == 0, "fixture directories");
    REQUIRE(create_text(SHAKTI_EDEN_FACTS_PATH, "fixture identity|synthetic|identity|RUN-ONLY\n"), "synthetic fact");
    REQUIRE(create_text(SHAKTI_EDEN_THESAURUS_PATH, ""), "synthetic thesaurus");
    REQUIRE(create_text(SHAKTI_EVIDENCE_PATH, ""), "empty learned evidence");
    REQUIRE(create_text(SHAKTI_SCHOOL_PATH, ""), "empty school record");
    REQUIRE(create_text(SHAKTI_LEARNED_STREAM_PATH, ""), "empty epoch stream");
    REQUIRE(create_text(SHAKTI_GOAL_PATH, "Exercise the synthetic runtime only."), "fixture goal");
    REQUIRE(create_text(SHAKTI_NOTEBOOK_PATH, "No personal data or production lessons."), "fixture notebook");
    REQUIRE(create_text(SHAKTI_MENU_PATH, "[Core]\nvalidate and status\n"), "fixture menu");
    REQUIRE(create_text(SHAKTI_EDEN_MANIFEST_PATH, manifest), "fixture manifest");
    return 1;
}

/* Synthetic fixture answers exercise the C adapter contract; they are never
 * installed as Shakti's production reasoning or claimed to prove cognition. */
static int fixture_reflection(void *context, unsigned int question,
    const char *prompt, char *answer, size_t capacity)
{
    const shakti_runtime_t *state = context;
    int n;
    if (prompt == NULL || state == NULL) return 0;
    n = snprintf(answer, capacity, "RUN-only question %u; observed tool calls=%lu",
        question, state->loop.tool_calls);
    return n > 0 && (size_t)n < capacity;
}

static int core_run(void)
{
    unsigned int i, rate;
    unsigned long before;
    size_t samples;
    const int16_t *audio;
    REQUIRE(shakti_app_initialize(&runtime), "initialize complete C runtime");
    shakti_tick_clock_seed(&runtime.clock, (time_t)2140000000L);
    for (i = 0U; i < 256U; ++i) REQUIRE(shakti_app_cycle(&runtime, NULL), "quiet cycle");
    REQUIRE(runtime.loop.cycle_count == 256UL && runtime.loop.hearing_frames == 256U &&
        runtime.loop.vision_frames == 128U && runtime.loop.heartbeat_count == 4U, "32:64 slots across four beats");
    REQUIRE(runtime.loop.tool_calls == 0UL && runtime.loop.reflection_count == 0UL, "quiet cycles never count as tool calls");
    REQUIRE(runtime.memory.eden == &runtime.reason && runtime.reason.fact_count == 1U, "Eden remains resident");
    for (rate = 1U; rate <= 64U; rate *= 2U) {
        unsigned long delay = 0UL;
        unsigned long pcm_samples = 0UL, pulses = 0UL;
        REQUIRE(shakti_loop_set_heartbeat(&runtime.loop, rate), "doubling rate accepted");
        for (i = 0U; i < 64U * rate; ++i) {
            delay += shakti_loop_delay_us(&runtime.loop);
            REQUIRE(shakti_app_cycle(&runtime, NULL), "heartbeat at selected pace");
            audio = shakti_loop_heartbeat_audio(&runtime.loop, &samples);
            pcm_samples += (unsigned long)samples;
            if (samples && audio[0]) ++pulses;
        }
        REQUIRE(delay == 1000000UL, "integer pacing retains exact aggregate second");
        REQUIRE(pcm_samples == 48000UL && pulses == rate,
            "fixed 48 kHz heartbeat PCM follows every selected beat rate without a backlog");
    }
    REQUIRE(!shakti_loop_set_heartbeat(&runtime.loop, 0U) &&
        !shakti_loop_set_heartbeat(&runtime.loop, 3U) &&
        !shakti_loop_set_heartbeat(&runtime.loop, 65U), "invalid pace refused");
    REQUIRE(shakti_loop_set_heartbeat(&runtime.loop, 1U), "resting pace restored");
    REQUIRE(shakti_app_cycle(&runtime, "/interrupt/"), "interrupt is a cycle");
    before = runtime.loop.cycle_count;
    REQUIRE(shakti_app_cycle(&runtime, "/shakti_run/ validate"), "held tool request");
    for (i = 0U; i < 64U; ++i) REQUIRE(shakti_app_cycle(&runtime, NULL), "interrupted quiet cycle");
    REQUIRE(runtime.loop.cycle_count == before + 65UL && runtime.loop.tool_calls == 0UL, "interrupt holds tools, preserves cycle");
    REQUIRE(shakti_app_cycle(&runtime, "/resume/"), "resume tools");
    REQUIRE(shakti_app_cycle(&runtime, "/shakti_run/ unknown"), "unknown tool refused");
    REQUIRE(runtime.loop.tool_calls == 0UL, "unknown tool is not a dispatched call");
    for (i = 0U; i < 10U; ++i) REQUIRE(shakti_app_cycle(&runtime, "/shakti_run/ validate"), "actual tool call");
    REQUIRE(runtime.loop.tool_calls == 10UL && runtime.loop.reflection_count == 0UL &&
        runtime.loop.reflection_due, "ten calls request actual reflection, no fabricated answers");
    before = runtime.loop.cycle_count;
    REQUIRE(shakti_app_cycle(&runtime, "/shakti_run/ validate"), "eleventh tool held");
    for (i = 0U; i < 64U; ++i) REQUIRE(shakti_app_cycle(&runtime, NULL), "pending reflection keeps cycling");
    REQUIRE(runtime.loop.cycle_count == before + 65UL && runtime.loop.tool_calls == 10UL &&
        runtime.loop.reflection_count == 0UL, "no reply or tool dispatch required to continue");
    for (i = 1U; i <= 12U; ++i) {
        char answer[64];
        REQUIRE(shakti_loop_reflection_question(i) != NULL, "question available to reasoning adapter");
        snprintf(answer, sizeof(answer), "Synthetic RUN observation for question %u", i);
        REQUIRE(shakti_loop_answer_reflection(&runtime, i, answer), "actual fixture answer accepted");
    }
    REQUIRE(!shakti_loop_reflection_question(0U) && !shakti_loop_reflection_question(14U),
        "question bounds enforced");
    REQUIRE(!shakti_loop_answer_reflection(&runtime, 13U, "   "), "blank answer refused");
    memset(payload, 'a', SHAKTI_REFLECTION_ANSWER_BYTES);
    payload[SHAKTI_REFLECTION_ANSWER_BYTES] = '\0';
    REQUIRE(!shakti_loop_answer_reflection(&runtime, 13U, payload), "over-capacity answer refused whole");
    REQUIRE(shakti_app_cycle(&runtime, NULL) && runtime.loop.reflection_due, "twelve answers cannot complete reflection");
    REQUIRE(shakti_app_cycle(&runtime, "/reflection/answer/ 13 Synthetic run cross-reference"),
        "actual thirteenth answer through nonblocking control");
    REQUIRE(runtime.loop.reflection_count == 1UL && !runtime.loop.reflection_due &&
        runtime.loop.turns_since_reflection == 0U, "all thirteen answers commit one whole reflection");
    shakti_loop_set_reflection_provider(&runtime.loop, fixture_reflection, &runtime);
    for (i = 0U; i < 10U; ++i) REQUIRE(shakti_app_cycle(&runtime, "/shakti_run/ validate"), "second actual batch");
    for (i = 0U; i < 12U; ++i) REQUIRE(shakti_app_cycle(&runtime, NULL), "compiled adapter answers one per slot");
    REQUIRE(runtime.loop.tool_calls == 20UL && runtime.loop.reflection_count == 2UL &&
        !runtime.loop.reflection_due, "automatic callback completes actual synthetic reflection without input");
    REQUIRE(shakti_app_cycle(&runtime, "/shakti_run/ remember run-lesson | fixture permanent | training | Complete synthetic lesson"),
        "Shakti can organize a whole training memory");
    REQUIRE(runtime.memory.working[0].permanent &&
        strcmp(runtime.memory.working[0].text, "Complete synthetic lesson") == 0, "training lesson preserved whole");
    REQUIRE(shakti_app_cycle(&runtime, "/shakti_run/ ask fixture identity"), "existing reasoner focus");
    REQUIRE(shakti_app_cycle(&runtime, NULL) && shakti_app_cycle(&runtime, NULL) &&
        strcmp(runtime.loop.current_thought, "synthetic") == 0, "quiet internal reasoning from resident Eden");
    REQUIRE(!shakti_loop_defer_reflection(&runtime.loop), "required reflection cannot be deferred");
    REQUIRE(shakti_app_cycle(&runtime, "/message_tyler/ optional fixture message"), "optional outbound message");
    REQUIRE(!runtime.loop.waiting_for_tyler, "outbound never creates a reply prerequisite");
    REQUIRE(shakti_app_cycle(&runtime, "/quit/"), "old quit closes tool gate only");
    for (i = 0U; i < 128U; ++i) REQUIRE(shakti_app_cycle(&runtime, NULL), "cycle survives former quit");
    REQUIRE(runtime.loop.tool_calls == 22UL && runtime.loop.reflection_count == 2UL, "idle and messages do not schedule reflection");
    audio = shakti_loop_heartbeat_audio(&runtime.loop, &samples);
    REQUIRE(audio != NULL && samples == 750U, "PCM heartbeat frame exposed to native host");
    printf("RUN core PASS cycles=%lu hearing=%" PRIu64 " vision=%" PRIu64 " tools=%lu reflections=%lu\n",
        runtime.loop.cycle_count, runtime.loop.hearing_frames, runtime.loop.vision_frames,
        runtime.loop.tool_calls, runtime.loop.reflection_count);
    shakti_memory_close(&runtime.memory);
    return 1;
}

static int memory_run(void)
{
    shakti_tick_t tick = { (time_t)1700000100L, 1U };
    unsigned int i;
    shakti_memory_init(&memory);
    memcpy(memory.container_path, "whole-memory.bin", sizeof("whole-memory.bin"));
    REQUIRE(shakti_memory_load_always(&memory, SHAKTI_GOAL_PATH, SHAKTI_NOTEBOOK_PATH,
        SHAKTI_MENU_PATH, SHAKTI_LEARNED_STREAM_PATH), "open separate synthetic memory store");
    shakti_memory_bind_eden(&memory, &runtime.reason);
    memset(payload, 'x', SHAKTI_MEMORY_BYTES - 1U);
    memcpy(payload, "alpha ", 6U);
    payload[SHAKTI_MEMORY_BYTES - 1U] = '\0';
    REQUIRE(shakti_memory_store(&memory, &tick, "large-whole", "alpha lesson", payload, 1), "capacity-sized whole container");
    REQUIRE(memory.working[0].size == SHAKTI_MEMORY_BYTES - 1U &&
        memcmp(memory.working[0].text, payload, SHAKTI_MEMORY_BYTES) == 0, "no memory truncation");
    memset(memory.working, 0, sizeof(memory.working));
    memset(memory.short_term, 0, sizeof(memory.short_term));
    memory.working_count = 0U;
    memory.short_term_count = 0U;
    REQUIRE(shakti_memory_request(&memory, "ALPHA"), "case-independent trigger request");
    for (i = 1U; memory.recall_file && i < 8U; ++i) REQUIRE(shakti_memory_step(&memory, i), "bounded whole recall step");
    REQUIRE(!memory.recall_file && memory.recalled_count == 1UL && memory.working_count == 1U &&
        memcmp(memory.working[0].text, payload, SHAKTI_MEMORY_BYTES) == 0, "long term to RAM to working, intact");
    memory.recent_decay_beats = 1UL;
    ++tick.frame;
    REQUIRE(shakti_memory_store(&memory, &tick, "recent", "recent", "unused recent event", 0), "recent container");
    REQUIRE(shakti_memory_activate(&memory, "large-whole"), "focus permanent lesson");
    REQUIRE(shakti_memory_step(&memory, 100U), "apply configured decay");
    REQUIRE(memory.short_term_count == 1U && memory.short_term[0].permanent && memory.eden == &runtime.reason,
        "unused recent event expires in RAM; lesson and Eden remain");
    REQUIRE(memory.working_count == 1U && strcmp(memory.working[0].id, "large-whole") == 0,
        "expired memory leaves no working copies");
    REQUIRE(shakti_memory_request(&memory, "recent"), "expired RAM event remains in long-term library");
    for (i = 101U; memory.recall_file && i < 110U; ++i) REQUIRE(shakti_memory_step(&memory, i), "recall retained recent container");
    REQUIRE(memory.recalled_count == 1UL && strcmp(memory.working[0].text, "unused recent event") == 0,
        "RAM decay never deletes long-term container");
    memset(memory.working, 0, sizeof(memory.working));
    memset(memory.short_term, 0, sizeof(memory.short_term));
    memory.working_count = memory.short_term_count = 0U;
    ++tick.frame;
    REQUIRE(!shakti_memory_store(&memory, &tick, "recent", "recent", "conflicting replacement", 0),
        "cold-store id conflict cannot overwrite whole memory");
    shakti_memory_close(&memory);
    shakti_memory_init(&memory);
    memcpy(memory.container_path, "whole-memory.bin", sizeof("whole-memory.bin"));
    REQUIRE(shakti_memory_load_always(&memory, SHAKTI_GOAL_PATH, SHAKTI_NOTEBOOK_PATH,
        SHAKTI_MENU_PATH, SHAKTI_LEARNED_STREAM_PATH), "restart audits whole records");
    REQUIRE(memory.serial == 2U && memory.newest_epoch == tick.epoch_seconds &&
        memory.working_count == 1U && memory.working[0].permanent &&
        memcmp(memory.working[0].text, payload, SHAKTI_MEMORY_BYTES) == 0,
        "restart restores identity watermark and whole permanent lesson in RAM");
    memset(payload, 'z', SHAKTI_MEMORY_BYTES);
    payload[SHAKTI_MEMORY_BYTES] = '\0';
    REQUIRE(!shakti_memory_store(&memory, &tick, "oversize", "bad", payload, 0), "oversize refuses whole item");
    REQUIRE(memory.fault, "capacity refusal is visible");
    shakti_memory_close(&memory);
    puts("RUN whole-memory PASS");
    return 1;
}

#define RGB_BYTES (1920U * 1080U * 3U)
static unsigned char rgb_input[RGB_BYTES], rgb_reference[RGB_BYTES];
static unsigned char rgb_binary[RGB_BYTES], rgb_rebuilt[RGB_BYTES];
static unsigned char pcm_input[2][1500], pcm_reference[2][1500];
static unsigned char pcm_binary[2][1504], pcm_rebuilt[2][1500];
static shakti_media_frame_t frames[3];

static void prepare_frames(uint64_t position, unsigned int mask, unsigned int sample_rate)
{
    unsigned int lane;
    memset(frames, 0, sizeof(frames));
    for (lane = 0U; lane < 3U; ++lane) {
        shakti_media_frame_t *f = &frames[lane];
        memcpy(f->source_id, "fixture-movie", sizeof("fixture-movie"));
        memcpy(f->context, "Synthetic RGB and PCM at one source position", sizeof("Synthetic RGB and PCM at one source position"));
        f->position64 = position + (lane == 2U ? 1U : 0U);
        f->lane = lane;
        f->required_mask = mask;
        f->sample_bytes = lane ? 2U : 1U;
        f->channels = lane ? 1U : 3U;
        if (lane == 0U) {
            f->width = 1920U; f->height = 1080U;
            f->input = rgb_input; f->reference = rgb_reference;
            f->binary = rgb_binary; f->rebuilt = rgb_rebuilt;
            f->bytes = f->reference_capacity = f->binary_capacity = f->rebuilt_capacity = RGB_BYTES;
        } else {
            uint64_t part = f->position64 % 64U;
            size_t size = (size_t)(((uint64_t)sample_rate * (part + 1U)) / 64U -
                ((uint64_t)sample_rate * part) / 64U) * 2U;
            f->sample_rate = sample_rate;
            f->input = pcm_input[lane - 1U]; f->reference = pcm_reference[lane - 1U];
            f->binary = pcm_binary[lane - 1U]; f->rebuilt = pcm_rebuilt[lane - 1U];
            f->bytes = size; f->reference_capacity = f->rebuilt_capacity = 1500U;
            f->binary_capacity = 1504U;
        }
    }
}

static int align_even(void)
{
    if (runtime.loop.cycle_count & 1UL) return shakti_app_cycle(&runtime, NULL);
    return 1;
}

static int media_run(void)
{
    static const unsigned char tiny[5] = { 0U, 1U, 127U, 128U, 255U };
    static const unsigned char known[8] = { 22U, 20U, 20U, 20U, 20U, 20U, 20U, 24U };
    unsigned char encoded[8], decoded[5];
    shakti_convergence_view_t view;
    uint64_t published;
    unsigned int i, lane;
    size_t j;
    struct timespec begin, end;
    double elapsed;
    REQUIRE(shakti_binary_encode(tiny, sizeof(tiny), encoded, sizeof(encoded)) &&
        memcmp(encoded, known, sizeof(known)) == 0, "independent known binary-plane layout");
    REQUIRE(shakti_binary_rebuild(known, sizeof(tiny), decoded, sizeof(decoded)) &&
        memcmp(decoded, tiny, sizeof(tiny)) == 0, "known binary planes rebuild exact bytes");
    REQUIRE(!shakti_binary_encode(tiny, sizeof(tiny), encoded, 7U) &&
        !shakti_binary_encode(encoded, sizeof(encoded), encoded, sizeof(encoded)) &&
        shakti_binary_capacity(SIZE_MAX) == 0U, "binary bounds and alias refused");
    for (j = 0U; j < RGB_BYTES; ++j) rgb_input[j] = (unsigned char)(j & 255U);
    for (lane = 0U; lane < 2U; ++lane)
        for (j = 0U; j < 1500U; ++j) pcm_input[lane][j] = (unsigned char)((j * 7U + lane) & 255U);
    shakti_media_init(&runtime.media);
    prepare_frames(0U, SHAKTI_MEDIA_MOVIE, 48000U);
    REQUIRE(shakti_app_queue_frame(&runtime, &frames[0]) &&
        shakti_app_queue_frame(&runtime, &frames[1]), "loading zone accepts full-size sight and first microclip");
    for (i = 0U; i < 4U; ++i) REQUIRE(shakti_app_cycle(&runtime, NULL), "missing clip never stalls heartbeat");
    REQUIRE(!shakti_media_current(&runtime.media) && runtime.media.validated_parts == 0U, "incomplete source window waits whole");
    memcpy(frames[2].source_id, "other-source", sizeof("other-source"));
    REQUIRE(shakti_app_queue_frame(&runtime, &frames[2]), "other source remains a separate waiting window");
    REQUIRE(shakti_app_cycle(&runtime, NULL) && !shakti_media_current(&runtime.media), "different source cannot fill missing audio");
    REQUIRE(shakti_media_cancel(&runtime.media, "other-source", 0U), "explicit cancellation releases wrong-source lease");
    memcpy(frames[2].source_id, "fixture-movie", sizeof("fixture-movie"));
    REQUIRE(shakti_app_queue_frame(&runtime, &frames[2]) && align_even(), "matching second clip completes queued group");
    REQUIRE(shakti_media_take(&runtime.media) == 1 &&
        memcmp(rgb_reference, rgb_input, RGB_BYTES) == 0 &&
        memcmp(rgb_rebuilt, rgb_input, RGB_BYTES) == 0, "taking immediately creates independent full reference and processing copies");
    rgb_input[7] ^= 1U;
    REQUIRE(shakti_app_cycle(&runtime, NULL) && shakti_app_cycle(&runtime, NULL), "two hearing slots converge one sight frame");
    REQUIRE(shakti_app_convergence_view(&runtime, &view) && view.sensory != NULL &&
        view.sensory->checked_mask == SHAKTI_MEDIA_MOVIE && view.eden == &runtime.reason &&
        view.working_count != 0U && strcmp(view.thought, "synthetic") == 0, "senses Eden organized memory and reasoning share convergence view");
    REQUIRE(memcmp(rgb_reference, rgb_rebuilt, RGB_BYTES) == 0 && rgb_reference[7] != rgb_input[7] &&
        memcmp(pcm_reference, pcm_rebuilt, sizeof(pcm_reference)) == 0, "full RGB and both PCM clips rebuilt byte-for-byte from independent held copies");
    rgb_input[7] ^= 1U;
    REQUIRE(!shakti_app_queue_frame(&runtime, &frames[0]), "current view buffers remain leased until release");
    shakti_media_release(&runtime.media);
    published = runtime.media.published;
    prepare_frames(2U, SHAKTI_MEDIA_SIGHT, 48000U);
    frames[0].expected_hash = shakti_media_hash(rgb_input, RGB_BYTES) ^ UINT64_C(1);
    REQUIRE(shakti_app_queue_frame(&runtime, &frames[0]) && align_even() &&
        shakti_app_cycle(&runtime, NULL), "bad expected hash handled within live loop");
    REQUIRE(runtime.media.published == published && !shakti_media_current(&runtime.media),
        "bad hash admits no source window");
    REQUIRE(shakti_media_cancel(&runtime.media, "fixture-movie", 1U), "held invalid window remains explicitly recoverable");
    prepare_frames(4U, SHAKTI_MEDIA_SIGHT, 48000U);
    REQUIRE(shakti_app_queue_frame(&runtime, &frames[0]) && align_even() &&
        shakti_media_take(&runtime.media) == 1, "binary corruption fixture taken");
    rgb_binary[17] ^= 1U;
    REQUIRE(shakti_app_cycle(&runtime, NULL) && runtime.media.published == published &&
        !shakti_media_current(&runtime.media), "one corrupt bit cannot pass full reconstruction comparison");
    REQUIRE(shakti_media_cancel(&runtime.media, "fixture-movie", 2U), "cancel corrupt window");
    prepare_frames(6U, SHAKTI_MEDIA_SIGHT, 48000U);
    frames[0].reference = rgb_input;
    REQUIRE(!shakti_app_queue_frame(&runtime, &frames[0]), "input/reference aliases refused before any write");
    prepare_frames(14U, SHAKTI_MEDIA_AUDIO_PAIR, 44100U);
    REQUIRE(frames[1].bytes == 1378U && frames[2].bytes == 1380U, "fractional-rate microclips retain source samples");
    REQUIRE(shakti_app_queue_frame(&runtime, &frames[1]) && shakti_app_queue_frame(&runtime, &frames[2]) &&
        align_even() && shakti_app_cycle(&runtime, NULL) && shakti_app_cycle(&runtime, NULL),
        "44100 Hz source window converges its correctly sized pair");
    REQUIRE(shakti_media_current(&runtime.media) != NULL, "audio-only whole pair published");
    shakti_media_release(&runtime.media);
    REQUIRE(clock_gettime(CLOCK_MONOTONIC, &begin) == 0, "begin native throughput observation");
    for (i = 0U; i < 32U; ++i) {
        prepare_frames((uint64_t)(i + 16U) * 2U, SHAKTI_MEDIA_MOVIE, 48000U);
        for (lane = 0U; lane < 3U; ++lane)
            REQUIRE(shakti_app_queue_frame(&runtime, &frames[lane]), "full-size paced source window queued");
        REQUIRE(align_even() && shakti_app_cycle(&runtime, NULL) && shakti_app_cycle(&runtime, NULL),
            "one movie window per two hearing slots");
        REQUIRE(shakti_media_current(&runtime.media) != NULL &&
            memcmp(rgb_input, rgb_rebuilt, RGB_BYTES) == 0, "all full-size pixel/channel values retained");
        shakti_media_release(&runtime.media);
    }
    REQUIRE(clock_gettime(CLOCK_MONOTONIC, &end) == 0, "finish throughput observation");
    elapsed = (double)(end.tv_sec - begin.tv_sec) + (double)(end.tv_nsec - begin.tv_nsec) / 1000000000.0;
    printf("RUN full-size convergence PASS rgb=1920x1080x3 windows=32 audio_clips=64 elapsed=%.6f seconds\n", elapsed);
    printf("RUN validation holds PASS rejected=%" PRIu64 " published=%" PRIu64 "\n",
        runtime.media.rejected, runtime.media.published);
    shakti_memory_close(&runtime.memory);
    return 1;
}

static int damaged_memory_run(void)
{
    shakti_tick_t tick = { (time_t)1700000300L, 1U };
    int byte;
    FILE *file = fopen("damaged-memory.bin", "wb");
    REQUIRE(file != NULL && fwrite("SMC1", 1U, 4U, file) == 4U && fclose(file) == 0,
        "create incomplete synthetic record");
    shakti_memory_init(&memory);
    memcpy(memory.container_path, "damaged-memory.bin", sizeof("damaged-memory.bin"));
    REQUIRE(!shakti_memory_load_always(&memory, SHAKTI_GOAL_PATH, SHAKTI_NOTEBOOK_PATH,
        SHAKTI_MENU_PATH, SHAKTI_LEARNED_STREAM_PATH) && memory.fault && !memory.long_term_ready,
        "incomplete durable container blocks startup admission and writes");
    REQUIRE(memory.working_count == 0U && memory.short_term_count == 0U, "no partial container admitted");
    puts("RUN incomplete-container hold PASS");
    shakti_memory_init(&memory);
    memcpy(memory.container_path, "checksum-memory.bin", sizeof("checksum-memory.bin"));
    REQUIRE(shakti_memory_load_always(&memory, SHAKTI_GOAL_PATH, SHAKTI_NOTEBOOK_PATH,
        SHAKTI_MENU_PATH, SHAKTI_LEARNED_STREAM_PATH) &&
        shakti_memory_store(&memory, &tick, "checksum", "fixture", "exact bytes", 0),
        "create complete checksum fixture");
    file = fopen("checksum-memory.bin", "r+b");
    REQUIRE(file != NULL && fseek(file, 55L, SEEK_SET) == 0, "locate first fixture text byte");
    byte = fgetc(file);
    REQUIRE(byte != EOF && fseek(file, -1L, SEEK_CUR) == 0 && fputc(byte ^ 1, file) != EOF &&
        fclose(file) == 0, "flip one byte only in disposable stored memory");
    shakti_memory_init(&memory);
    memcpy(memory.container_path, "checksum-memory.bin", sizeof("checksum-memory.bin"));
    REQUIRE(!shakti_memory_load_always(&memory, SHAKTI_GOAL_PATH, SHAKTI_NOTEBOOK_PATH,
        SHAKTI_MENU_PATH, SHAKTI_LEARNED_STREAM_PATH) && memory.fault &&
        memory.working_count == 0U && memory.short_term_count == 0U,
        "complete-looking container with bad checksum is never admitted");
    puts("RUN damaged-checksum hold PASS");
    return 1;
}

static int restart_run(void)
{
    static const char multiline[] = "Actual synthetic observation\nwith a second line\tand a tab.";
    time_t latest = (time_t)-1;
    unsigned int i;
    uint64_t records;
    unsigned long count;
    shakti_tick_t committed_tick;
    char id[SHAKTI_MEMORY_ID_BYTES];
    REQUIRE(shakti_app_initialize(&runtime), "restart restores logs and whole lessons");
    REQUIRE(runtime.loop.tool_calls == 22UL && runtime.loop.reflection_count == 2UL &&
        runtime.loop.turns_since_reflection == 2U && runtime.memory.short_term_count == 1U,
        "actual tool count and permanent training survive restart");
    shakti_loop_set_reflection_provider(&runtime.loop, NULL, NULL);
    for (i = 0U; i < 8U; ++i) REQUIRE(shakti_app_cycle(&runtime, "/shakti_run/ validate"), "finish actual pending batch");
    REQUIRE(runtime.loop.reflection_due && runtime.loop.tool_calls == 30UL, "third batch waits for actual answers");
    REQUIRE(shakti_loop_answer_reflection(&runtime, 1U, multiline), "actual multiline answer recorded without text loss");
    shakti_memory_close(&runtime.memory);
    REQUIRE(shakti_app_initialize(&runtime) && runtime.loop.reflection_due &&
        runtime.loop.tool_calls == 30UL && runtime.loop.reflection_count == 2UL &&
        runtime.loop.reflection_answered_mask == 1U &&
        strcmp(runtime.loop.reflection_answers[0], multiline) == 0, "restart cannot bypass pending reflection or lose answer bytes");
    count = runtime.loop.cycle_count;
    REQUIRE(shakti_app_cycle(&runtime, "/shakti_run/ validate") &&
        runtime.loop.tool_calls == 30UL && runtime.loop.cycle_count == count + 1UL, "restored gate holds tools while heartbeat runs");
    shakti_loop_set_reflection_provider(&runtime.loop, fixture_reflection, &runtime);
    for (i = 0U; i < 12U; ++i) REQUIRE(shakti_app_cycle(&runtime, NULL), "finish actual pending reflection after restart");
    REQUIRE(runtime.loop.reflection_count == 3UL && !runtime.loop.reflection_due,
        "third batch commits once after restored answers");
    records = runtime.memory.serial;
    memcpy(id, runtime.loop.reflection_id, strlen(runtime.loop.reflection_id) + 1U);
    committed_tick = runtime.loop.reflection_tick;
    /* Model interruption after durable container commit but before receipt:
     * truncate only this disposable fixture's final complete-record receipt. */
    {
        FILE *file = fopen(SHAKTI_REFLECTION_PATH, "rb");
        char line[SHAKTI_LINE_CAPACITY], *fields[7];
        shakti_tick_t tick;
        long cut = -1L, position;
        int result;
        REQUIRE(file != NULL, "open fixture reflection receipt");
        for (;;) {
            position = ftell(file);
            result = shakti_log_read_record(file, line, sizeof(line), fields, &tick);
            if (result <= 0) break;
            if (strcmp(fields[5], "complete_13_answers") == 0 && strcmp(fields[4], id) == 0)
                cut = position;
        }
        REQUIRE(result == 0 && fclose(file) == 0 && cut >= 0L, "locate exact fixture receipt boundary");
        REQUIRE(truncate(SHAKTI_REFLECTION_PATH, (off_t)cut) == 0, "remove only final fixture receipt");
    }
    shakti_memory_close(&runtime.memory);
    REQUIRE(shakti_app_initialize(&runtime) && runtime.loop.reflection_due, "interrupted receipt restores pending batch");
    for (i = 0U; i < 4U && runtime.loop.reflection_due; ++i)
        REQUIRE(shakti_app_cycle(&runtime, NULL), "retry receipt against already committed whole container");
    REQUIRE(!runtime.loop.reflection_due && runtime.loop.reflection_count == 3UL &&
        runtime.memory.serial == records && strcmp(runtime.loop.reflection_id, id) == 0 &&
        runtime.loop.reflection_tick.epoch_seconds == committed_tick.epoch_seconds &&
        runtime.loop.reflection_tick.frame == committed_tick.frame,
        "restart reuses identical committed reflection without another memory copy");
    shakti_tick_clock_seed(&runtime.clock, (time_t)2140001000L);
    REQUIRE(shakti_app_cycle(&runtime, NULL), "future logical epoch fixture");
    REQUIRE(shakti_log_latest_epoch(SHAKTI_LEARNED_STREAM_PATH, &latest) &&
        latest == (time_t)2140001000L, "checked epoch records restore logical clock watermark");
    shakti_memory_close(&runtime.memory);
    REQUIRE(shakti_app_initialize(&runtime) && runtime.clock.last_epoch_seconds > latest,
        "logical time cannot move backward across restart");
    REQUIRE(create_text("invalid-epoch.log", "LRN1\tbad\n") &&
        !shakti_log_latest_epoch("invalid-epoch.log", &latest) &&
        latest == (time_t)2140001000L, "bad log cannot advance clock watermark");
    shakti_memory_close(&runtime.memory);
    puts("RUN restart PASS pending_answers=preserved duplicate_reflections=0 logical_epoch=monotonic");
    return 1;
}

int main(int argc, char **argv)
{
    if (argc != 3 || (strcmp(argv[1], "--run") != 0 && strcmp(argv[1], "--seed") != 0)) {
        fputs("Use: Cycle.Run --run|--seed NEW_FIXTURE_DIRECTORY\n", stderr);
        return 2;
    }
    if (!seed_fixture(argv[2])) return 1;
    if (strcmp(argv[1], "--seed") == 0) { puts("RUN fixture seeded; synthetic data only."); return 0; }
    return core_run() && memory_run() && media_run() && damaged_memory_run() && restart_run() ? 0 : 1;
}
