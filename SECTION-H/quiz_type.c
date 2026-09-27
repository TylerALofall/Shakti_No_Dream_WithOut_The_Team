/* quiz_type.c — she types her own answers. C99, no heap, no scripts.
 *
 * For each problem in the sealed lesson5 tables:
 *   1. The question is shown AND spoken (founder voice atoms).
 *   2. SHE COMPUTES the answer in C — the tool, not memorization.
 *   3. She types it, key by key: every key names its glyph and speaks
 *      its sound (letters/digits bound to the keyboard, spoken as done).
 *   4. CROSS-EXAMINATION: her computed+typed answer is checked against
 *      the sealed table. Both must agree or the row is flagged.
 *   5. The scoreboard is appended — correct answers earn points.
 *
 * Usage: quiz_type <per_op> <logfile>
 */
#include <stdio.h>
#include <string.h>

static const char *TAB[] = {
    "school/tables/lesson5_add_table.txt",
    "school/tables/lesson5_sub_table.txt",
    "school/tables/lesson5_mul_table.txt",
    "school/tables/lesson5_div_table.txt"
};

static const char *digit_voice(char c)
{
    static char buf[40];
    if (c >= '0' && c <= '9') snprintf(buf, sizeof buf, "eden_out/Sound_art/%c.wav", c);
    else snprintf(buf, sizeof buf, "founder_voice:key_%c", c);
    return buf;
}

#define QUIZ_TYPE_MAIN main
int QUIZ_TYPE_MAIN(int argc, char **argv)
{
    if (argc != 3) { printf("usage: quiz_type <per_op> <log>\n"); return 1; }
    int per = 0;
    for (const char *p = argv[1]; *p; p++) per = per * 10 + (*p - '0');
    FILE *log = fopen(argv[2], "w");
    FILE *sb = fopen("school/game/scoreboard.txt", "a");
    if (!log || !sb) { printf("STOP: no outputs\n"); return 1; }

    fprintf(log, "QUIZ TYPE — she computes, she types, the table cross-examines\n");
    fprintf(log, "every keystroke is spoken. every answer is verified twice.\n\n");

    int correct = 0, total = 0;
    for (int t = 0; t < 4; t++) {
        FILE *f = fopen(TAB[t], "r");
        if (!f) { printf("STOP: missing %s\n", TAB[t]); return 1; }
        char line[256]; int n = 0;
        while (fgets(line, sizeof line, f) && n < per) {
            char card[64], ops[8]; int l, r, a;
            if (sscanf(line, "%63[^,],%d,%7[^,],%d,%d", card, &l, ops, &r, &a) != 5) continue;
            n++; total++;
            /* 1. shown + spoken */
            fprintf(log, "Q %s: %d %s %d = ?  say: founder_voice:%s\n", card, l, ops, r, card);
            /* 2. she computes — the tool */
            int mine = ops[0] == '+' ? l + r : ops[0] == '-' ? l - r :
                       ops[0] == '*' ? l * r : (r ? l / r : 0);
            /* 3. she types it, key by key, each key spoken */
            char ans[16]; snprintf(ans, sizeof ans, "%d", mine);
            fprintf(log, "  she types:");
            for (char *k = ans; *k; k++)
                fprintf(log, " [%c] key:%c say:%s", *k, *k, digit_voice(*k));
            fprintf(log, "\n");
            /* 4. cross-examination */
            if (mine == a) {
                correct++;
                fprintf(log, "  CHECK: computed %d, table %d — AGREE. +10\n", mine, a);
            } else {
                fprintf(log, "  CHECK: computed %d, table %d — DISAGREE. FLAGGED.\n", mine, a);
            }
        }
        fclose(f);
    }
    fprintf(log, "\nQUIZ OVER — %d/%d correct — %d points\n", correct, total, correct * 10);
    fprintf(sb, "quiz_type|%d ops x %d|%d/%d correct|%d points\n", 4, per, correct, total, correct * 10);
    fclose(log); fclose(sb);
    printf("quiz_type: %d/%d correct, scoreboard appended\n", correct, total);
    return correct == total ? 0 : 2;
}
