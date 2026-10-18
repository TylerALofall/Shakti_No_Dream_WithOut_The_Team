#ifndef SHAKTI_LOG_H
#define SHAKTI_LOG_H
#include "shakti_types.h"
const char *shakti_channel_name(shakti_channel_t channel);
int shakti_log_append(const char *path, const char *prefix, const shakti_tick_t *tick, const char *section, unsigned int function_order, const char *phase, const char *channel, const char *subject, const char *property, const char *value);
/* 1 complete checked record, 0 EOF, -1 invalid record. Field pointers borrow
 * line until its next read. Comments and blank lines are skipped. */
int shakti_log_read_record(FILE *file, char *line, size_t capacity,
    char *fields[7], shakti_tick_t *tick);
int shakti_log_latest_epoch(const char *path, time_t *latest);
int shakti_log_validate_file(const char *path, unsigned long *valid_lines, unsigned long *invalid_lines);
#endif
