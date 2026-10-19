#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "shakti_app.h"
#include "shakti_memory.h"
#include "shakti_time.h"

#define REQUIRE(c, m) do { if (!(c)) { fprintf(stderr, "BENCH RUN: %s\n", m); return 1; } } while (0)
static shakti_runtime_t runtime;
static shakti_memory_state_t restored;
static shakti_memory_item_t capsule;
static char answers[13][512];
static const char *answer_pointers[13];

static int text_file(const char *path, const char *text)
{
    FILE *file = fopen(path, "wb");
    int ok;
    if (!file) return 0;
    ok = fputs(text, file) >= 0;
    return fclose(file) == 0 && ok;
}

static long file_size(const char *path)
{
    FILE *file = fopen(path, "rb");
    long size;
    if (!file) return -1L;
    if (fseek(file, 0L, SEEK_END)) { fclose(file); return -1L; }
    size = ftell(file); if (fclose(file)) return -1L;
    return size;
}

static int restore(void)
{
    shakti_memory_init(&restored);
    return shakti_memory_load_always(&restored, SHAKTI_GOAL_PATH,
        SHAKTI_NOTEBOOK_PATH, SHAKTI_MENU_PATH, SHAKTI_LEARNED_STREAM_PATH);
}

int main(int argc, char **argv)
{
    unsigned int i, review;
    shakti_tick_t tick = { 2140000000L, 0U };
    long bytes;
    REQUIRE(argc == 3 && !strcmp(argv[1], "--run"), "use --run NEW_DIRECTORY");
    REQUIRE(!mkdir(argv[2], 0700) && !chdir(argv[2]), "new isolated fixture directory");
    REQUIRE(!mkdir("data", 0700) && !mkdir("data/eden", 0700) && !mkdir("data/learned", 0700) &&
        !mkdir("data/control", 0700) && !mkdir("data/school", 0700), "fixture subdirectories");
    REQUIRE(text_file(SHAKTI_GOAL_PATH, "synthetic memory RUN") &&
        text_file(SHAKTI_NOTEBOOK_PATH, "fixture only") && text_file(SHAKTI_MENU_PATH, "[Core]\nvalidate\n"), "resident fixtures");
    REQUIRE(restore(), "empty memory initialization");
    REQUIRE(!shakti_memory_policy(&restored, 19U, 600UL) && !shakti_memory_policy(&restored, 91U, 600UL), "working bounds enforced");
    REQUIRE(shakti_memory_policy(&restored, 20U, 600UL), "configure logical time");
    REQUIRE(shakti_memory_store(&restored, &tick, "episode-1", "rare car", "Unique blue car at the bend.", 0), "whole working admission");
    REQUIRE(restored.working_count == 1U && !restored.short_term_count && file_size(SHAKTI_CONTAINER_PATH) == 0L, "not prematurely in library");
    REQUIRE(shakti_memory_organize(&restored, "episode-1", "objects and exceptions", "transport/rare-cars"), "chosen organization persisted");
    REQUIRE(shakti_memory_step(&restored, 1279U) && shakti_memory_activate(&restored, "episode-1"), "working before twenty beats");
    REQUIRE(shakti_memory_step(&restored, 1280U) && !restored.working_count && restored.short_term_count == 1U, "whole demotion at twenty beats despite activation");
    REQUIRE(shakti_memory_step(&restored, 50000U) && restored.short_term_count == 1U, "unreviewed capsule held past idle decay");
    for (i = 0U; i < 13U; ++i) answer_pointers[i] = answers[i];
    for (review = 1U; review <= 5U; ++review) {
        char id[64];
        tick.epoch_seconds += 1;
        for (i = 0U; i < 13U; ++i)
            (void)snprintf(answers[i], sizeof(answers[i]), "Synthetic review %u, answer %u; observed whole-container lifecycle.", review, i + 1U);
        memcpy(answers[12], "episode-1", sizeof("episode-1"));
        (void)snprintf(id, sizeof(id), "review-%u", review);
        if (review == 1U) {
            answers[3][0] = '\0';
            REQUIRE(!shakti_memory_commit_reflection(&restored, &tick, id, "car", answer_pointers), "incomplete reflection refused");
            memcpy(answers[3], "Synthetic result observed.", sizeof("Synthetic result observed."));
        }
        REQUIRE(shakti_memory_commit_reflection(&restored, &tick, id, "car", answer_pointers), "thirteen-answer receipt linked");
        REQUIRE(shakti_memory_commit_reflection(&restored, &tick, id, "car", answer_pointers), "idempotent same receipt");
        REQUIRE(restored.short_term[0].reflection_count == review, "duplicate id does not count twice");
        if (review < 5U) REQUIRE(file_size(SHAKTI_CONTAINER_PATH) == 0L, "four reflections cannot promote");
    }
    REQUIRE(restored.short_term[0].archived && !shakti_memory_pending_reviews(&restored), "fifth distinct reflection promotes whole capsule");
    REQUIRE(strstr(restored.short_term[0].text, "13. Cross reference") && strstr(restored.short_term[0].text, "Synthetic review 5"), "actual answered questions in capsule");
    bytes = file_size(SHAKTI_CONTAINER_PATH);
    REQUIRE(bytes > 0L && restore(), "restart restores complete short-term capsule");
    REQUIRE(restored.short_term_count == 1U && restored.short_term[0].reflection_count == 5U &&
        !strcmp(restored.short_term[0].method, "objects and exceptions") &&
        !strcmp(restored.short_term[0].location, "transport/rare-cars"), "organization and review identities survive");
    REQUIRE(shakti_memory_step(&restored, restored.current_slot + 38400U) && !restored.short_term_count, "unused short term fades");
    REQUIRE(shakti_memory_request(&restored, "RARE"), "case-insensitive trigger recall queued");
    REQUIRE(shakti_memory_step(&restored, restored.current_slot + 1U) && restored.working_count == 1U, "whole long-term capsule recalled");
    REQUIRE(shakti_memory_fetch(&restored, "episode-1", &capsule) == 1 && capsule.archived && capsule.reflection_count == 5U,
        "whole capsule retained after RAM expiry");
    REQUIRE(file_size(SHAKTI_CONTAINER_PATH) == bytes, "recall and restart never duplicate library record");
    shakti_memory_close(&restored);
    REQUIRE(text_file(SHAKTI_MEMORY_CHECKPOINT_PATH, "interrupted checkpoint"), "damage only fixture checkpoint");
    REQUIRE(!restore() && !restored.working_count && !restored.short_term_count, "damaged checkpoint admits no fragments");
    /* No production answers or learning are installed by this fixture. */
    (void)runtime;
    puts("PASS static capsules; 20-beat demotion; five distinct complete reflections; decay; whole recall; restart integrity");
    return 0;
}
