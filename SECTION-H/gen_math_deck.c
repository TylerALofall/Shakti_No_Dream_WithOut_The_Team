/* gen_math_deck.c — builds MATCH decks from the sealed lesson5 tables.
 * Each problem becomes a Q-card ("12 + 3 = ?") and an A-card ("15"),
 * both classed by their problem id — under CLASS rule, a question
 * matches its own answer. She learns equation -> answer, not orphans.
 * C99, no heap. Usage: gen_math_deck <pairs_wanted> <outfile>
 */
#include <stdio.h>
#include <string.h>

static const char *TAB[] = {
    "school/tables/lesson5_add_table.txt",
    "school/tables/lesson5_sub_table.txt",
    "school/tables/lesson5_mul_table.txt",
    "school/tables/lesson5_div_table.txt"
};

#define GEN_MATH_DECK_MAIN main
int GEN_MATH_DECK_MAIN(int argc, char **argv)
{
    if (argc != 3) { printf("usage: gen_math_deck <pairs> <out>\n"); return 1; }
    int want = 0;
    for (const char *p = argv[1]; *p; p++) want = want * 10 + (*p - '0');
    FILE *out = fopen(argv[2], "w");
    if (!out) { printf("STOP: no out\n"); return 1; }
    int per = want / 4, made = 0;
    for (int t = 0; t < 4; t++) {
        FILE *f = fopen(TAB[t], "r");
        if (!f) { printf("STOP: missing %s\n", TAB[t]); return 1; }
        char line[256]; int n = 0;
        while (fgets(line, sizeof line, f) && n < per) {
            char card[64], op[8]; int l, r, a;
            if (sscanf(line, "%63[^,],%d,%7[^,],%d,%d", card, &l, op, &r, &a) != 5) continue;
            fprintf(out, "%d %s %d = ?|founder_voice:%s|%s\n", l, op, r, card, card);
            fprintf(out, "%d|founder_voice:%d|%s\n", a, a, card);
            n++; made++;
        }
        fclose(f);
    }
    fclose(out);
    printf("gen_math_deck: %d pairs written to %s\n", made, argv[2]);
    return 0;
}
