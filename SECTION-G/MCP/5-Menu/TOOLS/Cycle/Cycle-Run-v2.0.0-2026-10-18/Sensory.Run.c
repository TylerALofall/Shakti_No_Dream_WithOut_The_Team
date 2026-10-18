#include <stdio.h>
#include <string.h>
#include "shakti_media.h"

#define RGB_BYTES (1920U * 1080U * 3U)
#define PCM_BYTES 1500U
#define REQUIRE(c, m) do { if (!(c)) { fprintf(stderr, "BENCH RUN: %s\n", m); return 1; } } while (0)
static unsigned char pixels[4][RGB_BYTES];
static unsigned char audio[2][4][1504U];
static shakti_media_state_t media;
static shakti_media_frame_t frame;

/* These deliberately broken decoders exist only in separately linked RUN
 * binaries. The production module has no decoder bypass or fault switch. */
#if defined(SHAKTI_RUN_NOOP) || defined(SHAKTI_RUN_PARTIAL)
int shakti_binary_rebuild(const unsigned char *binary, size_t bytes,
    unsigned char *out, size_t capacity)
{
#ifdef SHAKTI_RUN_PARTIAL
    size_t i, bit, stride = shakti_binary_capacity(bytes) / 8U;
    if (capacity < bytes) return 0;
    for (i = 0U; i < bytes / 2U; ++i) {
        unsigned char value = 0U;
        for (bit = 0U; bit < 8U; ++bit)
            value |= (unsigned char)(((binary[bit * stride + i / 8U] >> (i % 8U)) & 1U) << bit);
        out[i] = value;
    }
#else
    (void)binary; (void)bytes; (void)out; (void)capacity;
#endif
    return 1;
}
#endif

#if !defined(SHAKTI_RUN_NOOP) && !defined(SHAKTI_RUN_PARTIAL)
static const unsigned char *eyes(const shakti_sensory_group_t *group)
{
    return group->parts[0].data;
}

static const unsigned char *mind(const shakti_sensory_group_t *group)
{
    return group->parts[0].data;
}
#endif

static int deposit(unsigned int window)
{
    unsigned int lane;
    memset(&frame, 0, sizeof(frame));
    memcpy(frame.source_id, "opaque-0001", sizeof("opaque-0001"));
    frame.required_mask = SHAKTI_MEDIA_MOVIE;
    frame.sample_bytes = 1U;
    frame.channels = 3U;
    frame.width = 1920U; frame.height = 1080U;
    frame.position64 = (uint64_t)window * 2U;
    frame.bytes = RGB_BYTES;
    frame.input = pixels[0]; frame.reference = pixels[1];
    frame.binary = pixels[2]; frame.rebuilt = pixels[3];
    frame.reference_capacity = frame.binary_capacity = frame.rebuilt_capacity = RGB_BYTES;
    if (!shakti_media_queue(&media, &frame)) return 0;
    for (lane = 1U; lane <= 2U; ++lane) {
        frame.lane = lane; frame.width = frame.height = 0U;
        frame.channels = 1U; frame.sample_bytes = 2U; frame.sample_rate = 48000U;
        frame.position64 = (uint64_t)window * 2U + lane - 1U;
        frame.bytes = PCM_BYTES;
        frame.input = audio[lane - 1U][0]; frame.reference = audio[lane - 1U][1];
        frame.binary = audio[lane - 1U][2]; frame.rebuilt = audio[lane - 1U][3];
        frame.reference_capacity = frame.binary_capacity = frame.rebuilt_capacity = 1504U;
        if (!shakti_media_queue(&media, &frame)) return 0;
    }
    return 1;
}

int main(void)
{
    static const unsigned char known[8] = { 0U,1U,2U,3U,4U,5U,6U,7U };
    static const unsigned char expected[8] = { 170U,204U,240U,0U,0U,0U,0U,0U };
    static unsigned char planes[8];
    size_t i;
    unsigned int window;
    shakti_media_init(&media);
    REQUIRE(shakti_binary_encode(known, sizeof(known), planes, sizeof(planes)) &&
        !memcmp(planes, expected, sizeof(expected)), "independent known binary-plane layout");
    for (window = 0U; window < 32U; ++window) {
        for (i = 0U; i < RGB_BYTES; ++i) pixels[0][i] = i % 3U == 2U ? 255U : 0U;
        for (i = 0U; i < PCM_BYTES; ++i) {
            audio[0][0][i] = (unsigned char)(i + window);
            audio[1][0][i] = (unsigned char)(i * 3U + window);
        }
        REQUIRE(deposit(window), "actual RGB and PCM deposit");
        REQUIRE(shakti_media_take(&media) == 1, "take complete window");
        for (i = 0U; i < RGB_BYTES; ++i)
            REQUIRE(pixels[1][i] != pixels[3][i], "every output byte starts different");
        memset(pixels[0], 127, RGB_BYTES);
#if defined(SHAKTI_RUN_NOOP) || defined(SHAKTI_RUN_PARTIAL)
        REQUIRE(shakti_media_step(&media, window * 2U) == -1, "broken decoder held");
        REQUIRE(shakti_media_perception(&media) == NULL && media.published == 0U,
            "broken decoder never publishes");
        puts("PASS deliberate broken decoder refused; no sensory publication");
        return 0;
#else
        const shakti_sensory_group_t *shared;
        REQUIRE(shakti_media_step(&media, window * 2U) == 1, "await synchronized audio");
        REQUIRE(shakti_media_perception(&media) == NULL, "no early perception");
        REQUIRE(shakti_media_step(&media, window * 2U + 1U) == 2, "complete synchronized rebuild");
        shared = shakti_media_perception(&media);
        REQUIRE(shared && shared->checked_mask == 7U, "all three lanes validated");
        REQUIRE(eyes(shared) == mind(shared) && mind(shared) == pixels[3],
            "eyes and mind share exact rebuilt pointer");
        REQUIRE(shared->parts[1].data == audio[0][3] && shared->parts[2].data == audio[1][3],
            "rebuilt audio uses the shared buffers");
        REQUIRE(shared->parts[0].context[0] == '\0', "no text color hint");
        for (i = 0U; i < RGB_BYTES; ++i)
            REQUIRE(mind(shared)[i] == (i % 3U == 2U ? 255U : 0U), "actual full-size blue pixels");
        REQUIRE(memcmp(audio[0][0], shared->parts[1].data, PCM_BYTES) == 0 &&
            memcmp(audio[1][0], shared->parts[2].data, PCM_BYTES) == 0, "every PCM byte rebuilt");
        shakti_media_release(&media);
        REQUIRE(shakti_media_perception(&media) == NULL, "released view unavailable");
#endif
    }
    REQUIRE(media.published == 32U && media.validated_parts == 96U, "32 sight frames plus 64 clips");
    puts("PASS 32 full-HD frames; 64 PCM clips; all bytes rebuilt; identical shared pointers");
    return 0;
}
