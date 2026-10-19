#include "shakti_senses.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

int shakti_senses_prepare(shakti_runtime_t *runtime, const char *source_id,
    uint64_t window, const shakti_rgb_image_t *image, const shakti_pcm_wave_t *wave,
    shakti_senses_storage_t *storage)
{
    shakti_media_frame_t frames[3];
    shakti_media_state_t probe;
    size_t lane, i, recorded;
    unsigned int mask;
    if (!runtime || !source_id || !source_id[0] || strlen(source_id) >= SHAKTI_MEDIA_ID_BYTES ||
        !storage || (!image && !wave) || window > (UINT64_MAX - 1U) / 2U) return 0;
    if (image && (!image->rgb || !image->width || !image->height ||
        (uint64_t)image->width * image->height > SIZE_MAX / 3U ||
        image->bytes != (size_t)image->width * image->height * 3U)) return 0;
    for (i = 0U; i < SHAKTI_MEDIA_GROUPS; ++i) {
        const shakti_media_group_t *group = &runtime->media.pending[i];
        if (group->in_use) for (lane = 0U; lane < 3U; ++lane)
            if ((group->queued_mask & (1U << lane)) && group->parts[lane].position64 / 2U == window &&
                !strcmp(group->parts[lane].source_id, source_id)) return 0;
    }
    mask = image ? (wave ? SHAKTI_MEDIA_MOVIE : SHAKTI_MEDIA_SIGHT) : SHAKTI_MEDIA_AUDIO_PAIR;
    memset(frames, 0, sizeof(frames));
    probe = runtime->media;
    /* Prove every buffer lease before reshaping/writing any audio input. */
    for (lane = 0U; lane < 3U; ++lane) if (mask & (1U << lane)) {
        shakti_media_frame_t *frame = &frames[lane];
        memcpy(frame->source_id, source_id, strlen(source_id) + 1U);
        frame->position64 = window * 2U + (lane == 2U ? 1U : 0U);
        frame->lane = (unsigned int)lane; frame->required_mask = mask;
        frame->retention_policy = SHAKTI_MEDIA_MOMENT;
        if (!lane) {
            frame->format = SHAKTI_FORMAT_RGB8;
            frame->width = image->width; frame->height = image->height;
            frame->channels = 3U; frame->sample_bytes = 1U; frame->bytes = image->bytes;
            frame->input = image->rgb; frame->reference = storage->visual[0];
            frame->binary = storage->visual[1]; frame->rebuilt = storage->visual[2];
            frame->reference_capacity = frame->binary_capacity = frame->rebuilt_capacity = storage->visual_capacity;
        } else {
            frame->format = SHAKTI_FORMAT_PCM_S16LE;
            if (!shakti_ears_slice(wave, frame->position64, NULL, 0U, &frame->bytes, &recorded)) return 0;
            frame->channels = wave->channels; frame->sample_bytes = 2U; frame->sample_rate = wave->rate;
            frame->input = storage->audio[lane - 1U][0];
            frame->reference = storage->audio[lane - 1U][1]; frame->binary = storage->audio[lane - 1U][2];
            frame->rebuilt = storage->audio[lane - 1U][3];
            frame->reference_capacity = frame->binary_capacity = frame->rebuilt_capacity = storage->audio_capacity;
        }
        if (!shakti_media_queue(&probe, frame)) return 0;
    }
    for (lane = 0U; lane < 3U; ++lane) if (mask & (1U << lane)) {
        shakti_media_frame_t *frame = &frames[lane];
        if (lane && !shakti_ears_slice(wave, frame->position64, storage->audio[lane - 1U][0],
            storage->audio_capacity, &frame->bytes, &recorded)) goto refuse;
        frame->expected_hash = shakti_media_hash(frame->input, frame->bytes);
        if (!shakti_app_queue_frame(runtime, frame)) goto refuse;
    }
    if (shakti_media_prepare(&runtime->media) > 0) return 1;
refuse:
    (void)shakti_media_cancel(&runtime->media, source_id, window);
    return 0;
}

int shakti_senses_describe(const shakti_sensory_group_t *group,
    const shakti_ocr_result_t *ocr, const char *thought, char *out, size_t capacity)
{
    const shakti_sensory_frame_t *first, *visual;
    unsigned int mask, solid = 0U;
    size_t lane, i;
    int n;
    char sight[SHAKTI_OCR_TEXT_BYTES + 256U], sound[256];
    if (out && capacity) out[0] = 0;
    if (!group || !out || !capacity || !(mask = group->checked_mask) ||
        (mask != SHAKTI_MEDIA_SIGHT && mask != SHAKTI_MEDIA_AUDIO_PAIR && mask != SHAKTI_MEDIA_MOVIE)) return 0;
    first = &group->parts[(mask & 1U) ? 0U : 1U]; visual = &group->parts[0];
    for (lane = 0U; lane < 3U; ++lane) if (mask & (1U << lane)) {
        const shakti_sensory_frame_t *part = &group->parts[lane];
        if (!part->data || !part->bytes || !memchr(first->source_id, 0, sizeof(first->source_id)) ||
            !memchr(part->source_id, 0, sizeof(part->source_id)) || strcmp(first->source_id, part->source_id) ||
            (lane && (part->format != SHAKTI_FORMAT_PCM_S16LE || part->sample_bytes != 2U)) ||
            part->position64 / 2U != first->position64 / 2U ||
            shakti_media_hash(part->data, part->bytes) != part->validated_hash) return 0;
    }
    if (mask & 1U) {
        if (visual->format != SHAKTI_FORMAT_RGB8 || visual->channels != 3U || visual->sample_bytes != 1U ||
            !visual->width || !visual->height || (uint64_t)visual->width * visual->height > SIZE_MAX / 3U ||
            visual->bytes != (size_t)visual->width * visual->height * 3U) return 0;
        solid = 1U;
        for (i = 3U; i < visual->bytes; ++i) if (visual->data[i] != visual->data[i % 3U]) { solid = 0U; break; }
        if (ocr) {
            if (ocr->source_hash != visual->validated_hash || ocr->width != visual->width ||
                ocr->height != visual->height || !memchr(ocr->text, 0, sizeof(ocr->text))) return 0;
            n = snprintf(sight, sizeof(sight), "visual=OCR_DOCUMENT RGB8 %ux%u hash=%016" PRIx64 "\ntext=%s",
                visual->width, visual->height, visual->validated_hash, ocr->text);
        } else if (solid)
            n = snprintf(sight, sizeof(sight), "visual=SOLID_RGB8 %ux%u rgb=%u,%u,%u hash=%016" PRIx64,
                visual->width, visual->height, visual->data[0], visual->data[1], visual->data[2], visual->validated_hash);
        else n = snprintf(sight, sizeof(sight), "visual=UNINTERPRETED_RGB8 %ux%u hash=%016" PRIx64,
                visual->width, visual->height, visual->validated_hash);
    } else n = snprintf(sight, sizeof(sight), "visual=ABSENT");
    if (n < 0 || (size_t)n >= sizeof(sight)) return 0;
    if (mask & 6U)
        n = snprintf(sound, sizeof(sound), "audio=PCM_S16LE rate=%u channels=%u positions=%" PRIu64 ",%" PRIu64
            " hashes=%016" PRIx64 ",%016" PRIx64 "\ntranscript=NOT_SUPPLIED",
            group->parts[1].sample_rate, group->parts[1].channels,
            group->parts[1].position64, group->parts[2].position64,
            group->parts[1].validated_hash, group->parts[2].validated_hash);
    else n = snprintf(sound, sizeof(sound), "audio=ABSENT");
    if (n < 0 || (size_t)n >= sizeof(sound)) return 0;
    n = snprintf(out, capacity, "SHAKTI_SENSE_V1\nsource=%s\nwindow=%" PRIu64 "\n%s\n%s\nthought=%s",
        first->source_id, first->position64 / 2U, sight, sound, thought ? thought : "NOT_SUPPLIED");
    if (n < 0 || (size_t)n >= capacity) { out[0] = 0; return 0; }
    return 1;
}
