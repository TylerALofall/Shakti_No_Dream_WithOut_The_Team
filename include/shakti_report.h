#ifndef SHAKTI_REPORT_H
#define SHAKTI_REPORT_H
#include "shakti_types.h"
typedef struct {
    unsigned long stones;
    unsigned long verified_stones;
    unsigned long incomplete_stones;
    unsigned long required_text_missing;
    unsigned long required_written_text_missing;
    unsigned long required_sound_missing;
    unsigned long required_visual_missing;
    unsigned long optional_foundation_sound_missing;
    unsigned long planned_tablets;
    unsigned long staged_tablets;
    unsigned long ready_tablets;
    unsigned long complete_tablets;
    unsigned long incomplete_tablets;
    unsigned long passed_checks;
    unsigned long required_checks;
    unsigned int confidence_percent;
    unsigned char runnable;
    unsigned char eden_lock_ready;
} shakti_report_t;
void shakti_report_init(shakti_report_t *report);
int shakti_report_read_ledger(const char *ledger_path, shakti_report_t *report);
int shakti_report_write(const char *report_path, const shakti_report_t *report, const shakti_school_state_t *school);
void shakti_report_print(const shakti_report_t *report, const shakti_school_state_t *school);
int shakti_report_run(const shakti_manifest_t *manifest, const char *manifest_path, const char *ledger_path, const char *report_path, const shakti_school_state_t *school, shakti_report_t *report);
#endif
