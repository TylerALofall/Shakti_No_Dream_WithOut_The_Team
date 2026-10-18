#ifndef SHAKTI_TIME_H
#define SHAKTI_TIME_H
#include <stddef.h>
#include "shakti_types.h"
void shakti_tick_clock_init(shakti_tick_clock_t *clock_state);
void shakti_tick_clock_seed(shakti_tick_clock_t *clock_state, time_t epoch);
int shakti_tick_clock_beat(shakti_tick_clock_t *clock_state);
int shakti_tick_next(shakti_tick_clock_t *clock_state, shakti_tick_t *tick);
int shakti_tick_format(const shakti_tick_t *tick, char *destination, size_t destination_size);
int shakti_event_id_format(const shakti_tick_t *tick, const char *section, unsigned int function_order, char *destination, size_t destination_size);
#endif
