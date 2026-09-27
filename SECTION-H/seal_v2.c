/* seal_v2.c — regenerates MANIFEST.fnv64.txt over all school artifacts.
 * C99, no heap, deterministic order.
 */
#include <stdio.h>

static unsigned long long fnv_file__seal_v2_h(const char *path, int *ok)
{
    FILE *f = fopen(path, "rb");
    unsigned long long h = 0xcbf29ce484222325ULL;
    int c;
    *ok = 0;
    if (!f) return h;
    while ((c = fgetc(f)) != EOF) { h ^= (unsigned char)c; h *= 0x100000001b3ULL; }
    fclose(f); *ok = 1;
    return h;
}

static const char *P[] = {
    "school/tables/lesson1_greeting_table.txt",
    "school/tables/lesson2_counting_table.txt",
    "school/tables/lesson3_abc_table.txt",
    "school/tables/lesson4_colors_table.txt",
    "school/tables/lesson5_add_table.txt",
    "school/tables/lesson5_sub_table.txt",
    "school/tables/lesson5_mul_table.txt",
    "school/tables/lesson5_div_table.txt",
    "school/doctrine/01_IDENTITY.txt",
    "school/doctrine/02_LOYALTY.txt",
    "school/doctrine/03_DISCERNMENT.txt",
    "school/doctrine/04_VOICE.txt",
    "school/doctrine/05_LAW.txt",
    "school/doctrine/06_WIT.txt",
    "school/doctrine/07_SNAKE_SLAYER.txt",
    "school/doctrine/08_THE_WEAVE.txt",
    "school/game/MATCH_RULES.txt",
    "school/game/decks/first_five_deck.txt",
    "school/game/decks/colors_class_deck.txt",
    "school/game/decks/shapes_class_deck.txt",
    "school/game/match_exact_teach_log.txt",
    "school/game/match_exact_play_log.txt",
    "school/game/match_class_colors_log.txt",
    "school/game/match_class_shapes_log.txt",
    "school/game/match_math_10x10_log.txt",
    "school/game/match_math_20x20_log.txt",
    "school/game/quiz_type_log.txt",
    "school/game/scoreboard.txt",
    "school/game/memory_long.txt",
    "school/game/avatar_shakti_v1.svg",
    "school/game/decks/math_deck_10x10.txt",
    "school/game/decks/math_deck_20x20.txt",
    "school/game/decks/first_five_deck.v5.txt",
    "school/game/decks/colors_class_deck.v5.txt",
    "school/game/decks/shapes_class_deck.v5.txt",
    "school/game/decks/math_deck_10x10.v5.txt",
    "school/game/decks/math_deck_20x20.v5.txt",
    "school/game/forge_first_five_sight.txt",
    "school/game/forge_colors_sight.txt",
    "school/game/forge_shapes_sight.txt",
    "school/game/forge_math10_sight.txt",
    "school/game/forge_math20_sight.txt",
    "school/CURRICULUM_ORDER.txt"
};
#define N 42

#define SEAL_V2_MAIN main
int SEAL_V2_MAIN(void)
{
    FILE *m = fopen("MANIFEST.fnv64.txt", "w");
    if (!m) { printf("STOP: no manifest out\n"); return 1; }
    fprintf(m, "MANIFEST — branch goddess-school-2026-08-21 — FNV-1a 64 per file\n");
    fprintf(m, "All artifacts generated in C99 — no heap, no Python anywhere in the chain.\n");
    for (int i = 0; i < N; i++) {
        int ok; unsigned long long h = fnv_file__seal_v2_h(P[i], &ok);
        if (!ok) { printf("STOP: missing %s\n", P[i]); return 1; }
        fprintf(m, "%016llX  %s\n", h, P[i]);
    }
    fclose(m);
    printf("seal_v2: %d artifacts sealed into MANIFEST.fnv64.txt\n", N);
    return 0;
}
