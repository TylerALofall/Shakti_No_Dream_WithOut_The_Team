#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "shakti_retention.h"
#define BYTES (32U * 24U * 3U)
#define REQUIRE(c,m) do { if (!(c)) { fprintf(stderr,"BENCH RUN: %s\n",m); return 1; } } while (0)
static unsigned char pixels[4][BYTES], output[BYTES], first[BYTES], last[BYTES];
static shakti_media_state_t media;
static shakti_media_frame_t frame;

static int capture(unsigned int n, uint64_t position)
{
    size_t i;
    for (i = 0U; i < BYTES; ++i) pixels[0][i] = (unsigned char)(i * 97U + n * 31U + i / 7U);
    memset(&frame, 0, sizeof(frame)); memcpy(frame.source_id, "synthetic-movie", sizeof("synthetic-movie"));
    frame.width = 32U; frame.height = 24U; frame.channels = 3U; frame.sample_bytes = 1U;
    frame.required_mask = 1U; frame.position64 = position;
    frame.bytes = frame.reference_capacity = frame.binary_capacity = frame.rebuilt_capacity = BYTES;
    frame.input = pixels[0]; frame.reference = pixels[1]; frame.binary = pixels[2]; frame.rebuilt = pixels[3];
    if (!shakti_media_queue(&media, &frame) || shakti_media_step(&media, 0U) != 2) return 0;
    if (!shakti_retention_capture(shakti_media_perception(&media), 2140000000U + n, 0U)) return 0;
    if (!n) memcpy(first, pixels[3], BYTES);
    memcpy(last, pixels[3], BYTES); shakti_media_release(&media);
    return 1;
}

int main(int argc, char **argv)
{
    unsigned int n;
    size_t count;
    const shakti_retention_state_t *state;
    FILE *file;
    REQUIRE(argc == 3 && !strcmp(argv[1], "--run"), "use --run NEW_DIRECTORY");
    REQUIRE(!mkdir(argv[2], 0700) && !chdir(argv[2]) && !mkdir("data",0700) && !mkdir("data/learned",0700), "new fixture directories");
    REQUIRE(SHAKTI_ARENA_BYTES == 536870912UL, "512 MiB fixed arena");
    REQUIRE(shakti_retention_restore() && shakti_retention_budget(65536U), "small active RUN budget inside full arena");
    shakti_media_init(&media);
    for (n = 0U; n < 200U; ++n) REQUIRE(capture(n, (uint64_t)n * 2U), "actual binary reconstruction before retention");
    state = shakti_retention_view();
    REQUIRE(state && state->stride > 1U && state->bytes_used <= 65536U, "budget doubles temporal stride");
    REQUIRE(state->windows[0].position64 == 0U && state->latest.position64 == 398U, "beginning and ending retained together");
    REQUIRE(shakti_retention_read(0U,0U,output,sizeof(output)) && !memcmp(first,output,BYTES), "whole first frame preserved");
    REQUIRE(shakti_retention_read(SIZE_MAX,0U,output,sizeof(output)) && !memcmp(last,output,BYTES), "whole latest frame preserved");
    count = state->count;
    REQUIRE(shakti_retention_restore(), "native journal recovery");
    state = shakti_retention_view();
    REQUIRE(state && state->count == count && state->windows_seen == 200U && state->latest.position64 == 398U, "timeline and active budget restored");
    REQUIRE(shakti_retention_read(0U,0U,output,sizeof(output)) && !memcmp(first,output,BYTES), "first frame survives restart");
    REQUIRE(shakti_retention_read(SIZE_MAX,0U,output,sizeof(output)) && !memcmp(last,output,BYTES), "latest frame survives restart");
    REQUIRE(!shakti_retention_accepts("other-source"), "unconsolidated source cannot be overwritten");
    REQUIRE(shakti_retention_budget(SHAKTI_RETENTION_HISTORY_BYTES), "restore full active history budget");
    /* Sparse timestamp probes check 20/40-minute policy boundaries. These do
     * not claim that an actual forty-minute movie was processed. */
    REQUIRE(capture(200U, UINT64_C(1200)*64U), "twenty-minute timestamp probe");
    REQUIRE(shakti_retention_view()->stride >= 2U, "half retention floor");
    REQUIRE(capture(201U, UINT64_C(2400)*64U), "forty-minute timestamp probe");
    REQUIRE(shakti_retention_view()->stride >= 4U, "quarter retention floor");
    REQUIRE(shakti_retention_restore() && shakti_retention_read(SIZE_MAX,0U,output,sizeof(output)) &&
        !memcmp(last,output,BYTES), "compacted journal and policy probes recover");
    file = fopen(SHAKTI_RETENTION_PATH, "ab"); REQUIRE(file, "fixture corruption open");
    REQUIRE(fputc(1,file) != EOF && !fclose(file), "truncate only new fixture record");
    REQUIRE(!shakti_retention_restore() && !shakti_retention_view(), "partial recovery record exposes no retained view");
    puts("PASS fixed 512 MiB arena; binary-first capture; uniform thinning; first/latest preservation; native restart; corruption hold");
    return 0;
}
