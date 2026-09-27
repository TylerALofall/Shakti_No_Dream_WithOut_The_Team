#include "shakti_time.h"

#include <stdio.h>
#include <time.h>

void shakti_tick_clock_init(shakti_tick_clock_t *clock_state)
{
    if (clock_state == NULL) {
        return;
    }

    clock_state->last_epoch_seconds = (time_t)0;
    clock_state->next_frame = 0U;
}

int shakti_tick_next(shakti_tick_clock_t *clock_state, shakti_tick_t *tick)
{
    time_t now;

    if (clock_state == NULL || tick == NULL) {
        return 0;
    }

    now = time(NULL);

    if (now == (time_t)-1) {
        return 0;
    }

    if (clock_state->last_epoch_seconds == (time_t)0 ||
        now > clock_state->last_epoch_seconds) {
        clock_state->last_epoch_seconds = now;
        clock_state->next_frame = 0U;
    } else if (clock_state->next_frame >= SHAKTI_FRAME_LIMIT) {
        do {
            now = time(NULL);
        } while (now != (time_t)-1 &&
                 now <= clock_state->last_epoch_seconds);

        if (now == (time_t)-1) {
            return 0;
        }

        clock_state->last_epoch_seconds = now;
        clock_state->next_frame = 0U;
    }

    tick->epoch_seconds = clock_state->last_epoch_seconds;
    tick->frame = clock_state->next_frame;
    clock_state->next_frame++;

    return 1;
}

int shakti_tick_format(
    const shakti_tick_t *tick,
    char *destination,
    size_t destination_size
)
{
    int written;

    if (tick == NULL || destination == NULL || destination_size == 0U) {
        return 0;
    }

    written = snprintf(
        destination,
        destination_size,
        "%ld:%04u",
        (long)tick->epoch_seconds,
        tick->frame
    );

    return written >= 0 && (size_t)written < destination_size;
}

int shakti_event_id_format(
    const shakti_tick_t *tick,
    const char *section,
    unsigned int function_order,
    char *destination,
    size_t destination_size
)
{
    char tick_text[SHAKTI_TICK_CAPACITY];
    int written;

    if (section == NULL ||
        !shakti_tick_format(tick, tick_text, sizeof(tick_text))) {
        return 0;
    }

    written = snprintf(
        destination,
        destination_size,
        "[%s]-[%s]-[%02u]",
        tick_text,
        section,
        function_order
    );

    return written >= 0 && (size_t)written < destination_size;
}
