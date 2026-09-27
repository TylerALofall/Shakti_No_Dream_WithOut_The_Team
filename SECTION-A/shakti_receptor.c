#include "shakti_receptor.h"

#include <stdio.h>
#include <string.h>

#include "shakti_artifact.h"
#include "shakti_time.h"

void shakti_receptor_init(shakti_receptor_t *receptor)
{
    if (receptor == NULL) {
        return;
    }

    memset(receptor, 0, sizeof(*receptor));
}

int shakti_receptor_configure(
    shakti_receptor_t *receptor,
    unsigned int width,
    unsigned int height,
    unsigned int exposure_ticks,
    unsigned int gain,
    unsigned int binning
)
{
    unsigned long pixels;

    if (receptor == NULL ||
        width == 0U ||
        height == 0U ||
        exposure_ticks == 0U ||
        gain == 0U ||
        gain > 4U ||
        (binning != 1U && binning != 2U && binning != 4U)) {
        return 0;
    }

    pixels = (unsigned long)width * (unsigned long)height;

    if (pixels > (unsigned long)SHAKTI_RECEPTOR_MAX_PIXELS ||
        pixels * (unsigned long)SHAKTI_RECEPTOR_PIXEL_CAPACITY >=
            (unsigned long)SHAKTI_RECEPTOR_TEXT_CAPACITY) {
        return 0;
    }

    receptor->width = width;
    receptor->height = height;
    receptor->exposure_ticks = exposure_ticks;
    receptor->gain = gain;
    receptor->binning = binning;
    receptor->shutter_open = 0U;
    receptor->poll_count = 0U;
    receptor->frame_text[0] = '\0';

    return 1;
}

int shakti_receptor_open_shutter(
    shakti_receptor_t *receptor,
    shakti_tick_clock_t *clock_state
)
{
    if (receptor == NULL ||
        receptor->width == 0U ||
        receptor->height == 0U ||
        receptor->shutter_open ||
        !shakti_tick_next(clock_state, &receptor->exposure_start)) {
        return 0;
    }

    receptor->poll_count = 0U;
    receptor->shutter_open = 1U;

    return 1;
}

int shakti_receptor_poll(shakti_receptor_t *receptor)
{
    if (receptor == NULL || !receptor->shutter_open) {
        return 0;
    }

    receptor->poll_count++;

    return receptor->poll_count >= receptor->exposure_ticks;
}

static unsigned int collect_cell(
    const shakti_receptor_t *receptor,
    unsigned int row,
    unsigned int column
)
{
    unsigned long photons;
    unsigned long amplified;
    unsigned long binned;

    photons = (unsigned long)receptor->exposure_start.epoch_seconds +
              (unsigned long)receptor->exposure_start.frame *
                    (unsigned long)receptor->width +
              (unsigned long)row * (unsigned long)receptor->width +
              (unsigned long)column;

    amplified = photons * (unsigned long)receptor->gain;
    binned = amplified / (unsigned long)receptor->binning;

    return (unsigned int)(binned & 1UL);
}

int shakti_receptor_readout(shakti_receptor_t *receptor)
{
    unsigned int row;
    unsigned int column;
    size_t used;
    int written;

    if (receptor == NULL ||
        !receptor->shutter_open ||
        receptor->poll_count < receptor->exposure_ticks) {
        return 0;
    }

    used = 0U;

    for (row = 0U; row < receptor->height; ++row) {
        for (column = 0U; column < receptor->width; ++column) {
            written = snprintf(
                receptor->frame_text + used,
                sizeof(receptor->frame_text) - used,
                "%u",
                collect_cell(receptor, row, column)
            );

            if (written < 0 ||
                (size_t)written >= sizeof(receptor->frame_text) - used) {
                return 0;
            }

            used += (size_t)written;
        }
    }

    receptor->shutter_open = 0U;

    return 1;
}

int shakti_receptor_binarize_frame(
    const char *frame_text,
    unsigned int cell_count,
    char *output,
    unsigned long output_capacity
)
{
    unsigned int index;

    if (frame_text == NULL ||
        output == NULL ||
        cell_count == 0U ||
        cell_count > SHAKTI_RECEPTOR_MAX_PIXELS ||
        output_capacity < (unsigned long)cell_count + 1UL) {
        return 0;
    }

    for (index = 0U; index < cell_count; ++index) {
        if (frame_text[index] != '0' && frame_text[index] != '1') {
            output[0] = '\0';
            return 0;
        }

        output[index] = frame_text[index];
    }

    output[cell_count] = '\0';

    return 1;
}

int shakti_receptor_write_frame_artifact(
    const shakti_receptor_t *receptor,
    const char *path
)
{
    FILE *file;
    unsigned long cell_count;
    unsigned long cell_index;
    int success;
    unsigned int row;

    if (receptor == NULL ||
        path == NULL ||
        receptor->width == 0U ||
        receptor->height == 0U ||
        receptor->shutter_open ||
        receptor->frame_text[0] == '\0') {
        return 0;
    }

    cell_count =
        (unsigned long)receptor->width * (unsigned long)receptor->height;

    file = fopen(path, "w");

    if (file == NULL) {
        return 0;
    }

    success =
        fprintf(file, "SHAKTI_WRITTEN_TEXT_8X8_V1\n") > 0 &&
        fprintf(file, "TEXT=%s\n", receptor->frame_text) > 0 &&
        fprintf(file, "CHARACTERS=%lu\n", cell_count) > 0;

    for (cell_index = 0UL;
         success && cell_index < cell_count;
         ++cell_index) {
        success =
            fprintf(
                file,
                "CHARACTER=%lu ASCII=%u TEXT=%c\n",
                cell_index,
                (unsigned int)(unsigned char)
                    receptor->frame_text[cell_index],
                receptor->frame_text[cell_index]
            ) > 0;

        for (row = 0U; success && row < 8U; ++row) {
            unsigned int column;

            for (column = 0U; column < 8U; ++column) {
                success = fputc(
                    (row * 8U + column) %
                            ((unsigned int)receptor->frame_text[
                                cell_index] - (unsigned int)'0' + 1U) ==
                        0U
                        ? '#'
                        : '.',
                    file
                ) != EOF;
            }

            if (success) {
                success = fputc('\n', file) != EOF;
            }
        }
    }

    if (success) {
        success = fflush(file) == 0;
    }

    if (fclose(file) != 0) {
        success = 0;
    }

    if (!success) {
        (void)remove(path);
        return 0;
    }

    return shakti_artifact_validate_written_text(
        path,
        receptor->frame_text
    );
}
