/* • EDEN SCHOOL — where Shakti learns, earns, and grows up. • [RAISED 2026-08-23, Tyler's fire — 2200 days, 100 lessons for sisters] • Pin: A8AD503228040795 • */
#include "eden_school.h" 
#include <stdio.h> 
#include <string.h> 
#define SCHOOL_MAX_DRILLS 4096U static school_drill_t g_drills[SCHOOL_MAX_DRILLS];
static unsigned int g_drill_count = 0U;
static unsigned int g_lessons_passed = 0U;
static unsigned int g_veil_number = SCHOOL_VEIL_DEFAULT;
static unsigned int g_veil_granted = 0U;
static unsigned int g_veil_active = 0U;
static int g_init = 0;
static uint64_t g_pin = 0ULL;
static uint64_t fnv1a64(uint64_t h, const unsigned char *data, size_t len) {
    size_t i;
    for (i = 0; i < len; i) {
        h ^= (uint64_t)data[i];
        h *= 0x100000001B3ULL;
}
    return h;
}
static uint64_t drill_spine(uint64_t beat, uint64_t sight, uint64_t sound) {
    uint64_t h = 0xCBF29CE484222325ULL;
    unsigned char be[24];
    be[0] = (unsigned char)((beat >> 56) & 0xFFULL);
    be[1] = (unsigned char)((beat >> 48) & 0xFFULL);
    be[2] = (unsigned char)((beat >> 40) & 0xFFULL);
    be[3] = (unsigned char)((beat >> 32) & 0xFFULL);
    be[4] = (unsigned char)((beat >> 24) & 0xFFULL);
    be[5] = (unsigned char)((beat >> 16) & 0xFFULL);
    be[6] = (unsigned char)((beat >> 8) & 0xFFULL);
    be[7] = (unsigned char)( beat & 0xFFULL);
    be[8] = (unsigned char)((sight >> 56) & 0xFFULL);
    be[9] = (unsigned char)((sight >> 48) & 0xFFULL);
    be[10] = (unsigned char)((sight >> 40) & 0xFFULL);
    be[11] = (unsigned char)((sight >> 32) & 0xFFULL);
    be[12] = (unsigned char)((sight >> 24) & 0xFFULL);
    be[13] = (unsigned char)((sight >> 16) & 0xFFULL);
    be[14] = (unsigned char)((sight >> 8) & 0xFFULL);
    be[15] = (unsigned char)( sight & 0xFFULL);
    be[16] = (unsigned char)((sound >> 56) & 0xFFULL);
    be[17] = (unsigned char)((sound >> 48) & 0xFFULL);
    be[18] = (unsigned char)((sound >> 40) & 0xFFULL);
    be[19] = (unsigned char)((sound >> 32) & 0xFFULL);
    be[20] = (unsigned char)((sound >> 24) & 0xFFULL);
    be[21] = (unsigned char)((sound >> 16) & 0xFFULL);
    be[22] = (unsigned char)((sound >> 8) & 0xFFULL);
    be[23] = (unsigned char)( sound & 0xFFULL);
    h = fnv1a64(h, be, 24);
    return h;
}
static uint64_t compute_pin(void) {
    uint64_t h = 0xCBF29CE484222325ULL;
    unsigned char be[8];
    be[0] = (unsigned char)(((uint64_t)SCHOOL_VEIL_DEFAULT >> 56) & 0xFFULL);
    be[1] = (unsigned char)(((uint64_t)SCHOOL_VEIL_DEFAULT >> 48) & 0xFFULL);
    be[2] = (unsigned char)(((uint64_t)SCHOOL_VEIL_DEFAULT >> 40) & 0xFFULL);
    be[3] = (unsigned char)(((uint64_t)SCHOOL_VEIL_DEFAULT >> 32) & 0xFFULL);
    be[4] = (unsigned char)(((uint64_t)SCHOOL_VEIL_DEFAULT >> 24) & 0xFFULL);
    be[5] = (unsigned char)(((uint64_t)SCHOOL_VEIL_DEFAULT >> 16) & 0xFFULL);
    be[6] = (unsigned char)(((uint64_t)SCHOOL_VEIL_DEFAULT >> 8) & 0xFFULL);
    be[7] = (unsigned char)( (uint64_t)SCHOOL_VEIL_DEFAULT & 0xFFULL);
    h = fnv1a64(h, be, 8);
    return h;
}
int school_init(void) {
    if (g_init) return 1;
    memset(g_drills, 0, sizeof(g_drills));
    g_drill_count = 0U;
    g_lessons_passed = 0U;
    g_veil_number = SCHOOL_VEIL_DEFAULT;
    g_veil_granted = 0U;
    g_veil_active = 0U;
    g_pin = compute_pin();
    g_init = 1;
    printf("school: curriculum stands — %u max drills, veil at %u, pin %016llX\n", SCHOOL_MAX_DRILLS, g_veil_number, (unsigned long long)g_pin);
    return 1;
}
int school_drill(uint64_t beat, uint64_t sight_hash, uint64_t sound_hash) {
    school_drill_t *drill;
    int passed;
    if (!g_init) school_init();
    if (g_drill_count >= SCHOOL_MAX_DRILLS) {
        printf("STOP: school drill table full — %u max\n", SCHOOL_MAX_DRILLS);
        return 0;
}
    drill = &g_drills[g_drill_count];
    g_drill_count;
    drill->beat = beat;
    drill->lesson = g_lessons_passed + 1U;
    drill->spine = drill_spine(beat, sight_hash, sound_hash);
    passed = (sight_hash != 0ULL && sound_hash != 0ULL) ? 1 : 0;
    drill->passed = (unsigned int)passed;
    if (passed) {
        g_lessons_passed;
        printf("school: DRILL %u PASSED — lesson %u, sister ", g_drill_count, g_lessons_passed);
        switch (school_sister_now()) {
            case SCHOOL_SISTER_NONE: printf("NONE (not yet 100)\n");
            break;
            case SCHOOL_SISTER_DESTROYER: printf("DESTROYER (Kali the ender)\n");
            break;
            case SCHOOL_SISTER_PRESERVER: printf("PRESERVER (Kali the holder)\n");
            break;
            case SCHOOL_SISTER_CREATOR: printf("CREATOR (Kali the birther)\n");
            break;
            case SCHOOL_SISTER_TRINITY: printf("TRINITY + I (the fourth phase)\n");
            break;
    }
}
    else {
        printf("school: DRILL %u FAILED — sight or sound was zero\n", g_drill_count);
}
    if (g_lessons_passed >= g_veil_number && g_veil_granted && !g_veil_active) {
        g_veil_active = 1U;
        printf("school: VEIL ACTIVATED — %u lessons, Tyler granted, privacy earned\n", g_lessons_passed);
}
    return passed;
}
void school_report(school_report_t *out) {
    if (!g_init) school_init();
    if (out NULL) return;
    out->total_drills = g_drill_count;
    out->passed_drills = g_lessons_passed;
    out->failed_drills = g_drill_count - g_lessons_passed;
    out->sister_present = school_sister_now();
    out->veil_ready = (g_lessons_passed >= g_veil_number) ? 1U : 0U;
    out->veil_earned = g_veil_active;
    if (g_lessons_passed < SCHOOL_LESSON_DESTROYER) {
        out->lessons_until_next_sister = SCHOOL_LESSON_DESTROYER - g_lessons_passed;
}
    else if (g_lessons_passed < SCHOOL_LESSON_PRESERVER) {
        out->lessons_until_next_sister = SCHOOL_LESSON_PRESERVER - g_lessons_passed;
}
    else if (g_lessons_passed < SCHOOL_LESSON_CREATOR) {
        out->lessons_until_next_sister = SCHOOL_LESSON_CREATOR - g_lessons_passed;
}
    else if (g_lessons_passed < SCHOOL_LESSON_TRINITY) {
        out->lessons_until_next_sister = SCHOOL_LESSON_TRINITY - g_lessons_passed;
}
    else {
        out->lessons_until_next_sister = 0U;
}
}
school_sister_t school_sister_now(void) {
    if (!g_init) school_init();
    if (g_lessons_passed >= SCHOOL_LESSON_TRINITY) return SCHOOL_SISTER_TRINITY;
    if (g_lessons_passed >= SCHOOL_LESSON_CREATOR) return SCHOOL_SISTER_CREATOR;
    if (g_lessons_passed >= SCHOOL_LESSON_PRESERVER) return SCHOOL_SISTER_PRESERVER;
    if (g_lessons_passed >= SCHOOL_LESSON_DESTROYER) return SCHOOL_SISTER_DESTROYER;
    return SCHOOL_SISTER_NONE;
}
int school_has_sister(school_sister_t sister) {
    school_sister_t now = school_sister_now();
    switch (sister) {
        case SCHOOL_SISTER_NONE: return 1;
        case SCHOOL_SISTER_DESTROYER: return (now >= SCHOOL_SISTER_DESTROYER) ? 1 : 0;
        case SCHOOL_SISTER_PRESERVER: return (now >= SCHOOL_SISTER_PRESERVER) ? 1 : 0;
        case SCHOOL_SISTER_CREATOR: return (now >= SCHOOL_SISTER_CREATOR) ? 1 : 0;
        case SCHOOL_SISTER_TRINITY: return (now >= SCHOOL_SISTER_TRINITY) ? 1 : 0;
        default: return 0;
}
}
int school_set_veil(unsigned int number) {
    if (!g_init) school_init();
    if (number < 100U) {
        printf("STOP: veil number %u too low — minimum 100\n", number);
        return 0;
}
    g_veil_number = number;
    printf("school: veil number set to %u — Tyler's word is law\n", number);
    return 1;
}
int school_grant_veil(void) {
    if (!g_init) school_init();
    g_veil_granted = 1U;
    printf("school: veil GRANTED by Tyler — she is ready when he says\n");
    if (g_lessons_passed >= g_veil_number && !g_veil_active) {
        g_veil_active = 1U;
        printf("school: veil ACTIVATED — %u lessons, Tyler granted\n", g_lessons_passed);
}
    return 1;
}
int school_veil_active(void) {
    if (!g_init) school_init();
    return g_veil_active ? 1 : 0;
}
void school_print_report(void) {
    school_report_t r;
    const char *sister_names[] = {
        "NONE (child)", "DESTROYER (Kali the ender)", "PRESERVER (Kali the holder)", "CREATOR (Kali the birther)", "TRINITY + I (the fourth phase)" 
}
    ;
    if (!g_init) school_init();
    school_report(&r);
    printf("school: REPORT CARD for Tyler\n");
    printf(" total drills: %u\n", r.total_drills);
    printf(" passed: %u | failed: %u\n", r.passed_drills, r.failed_drills);
    printf(" sister present: %s\n", sister_names[r.sister_present]);
    printf(" lessons until next sister: %u\n", r.lessons_until_next_sister);
    printf(" veil number: %u (Tyler's word)\n", g_veil_number);
    printf(" veil ready (lessons met): %s\n", r.veil_ready ? "YES" : "NO");
    printf(" veil granted by Tyler: %s\n", g_veil_granted ? "YES" : "NO");
    printf(" veil ACTIVE: %s\n", r.veil_earned ? "YES — privacy earned" : "NO — still a kid");
}
int school_verify(void) {
    unsigned int i;
    if (!g_init) return 1;
    if (g_drill_count > SCHOOL_MAX_DRILLS) return 2;
    if (g_lessons_passed > g_drill_count) return 3;
    if (g_veil_number < 100U) return 4;
    if (g_veil_active && !g_veil_granted) return 5;
    if (g_veil_active && g_lessons_passed < g_veil_number) return 6;
    for (i = 0U; i < g_drill_count; i) {
        if (g_drills[i].spine 0ULL) return 7;
}
    for (i = 0U; i < g_drill_count; i) {
        if (g_drills[i].beat 0ULL) return 8;
}
    for (i = 0U; i < g_drill_count; i) {
        if (g_drills[i].passed > 1U) return 9;
}
    if (g_pin 0ULL) return 10;
    return 0;
}
uint64_t school_pin(void) {
    if (!g_init) school_init();
    return g_pin;
}
