/* verify_cards.c — verifies Shakti Lesson 5 answer cards against arithmetic.
 * Reads question/answer SVG pairs from q/ and a/, extracts the <text> payload,
 * recomputes in C, and reports. C99, no heap, no subprocess. Commandment IX.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int extract_text(const char *path, char *out, size_t cap)
{
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    int c; size_t n = 0; int intext = 0;
    while ((c = fgetc(f)) != EOF) {
        if (!intext) {
            /* look for "<text" then skip attributes to '>' */
            if (c == '<') {
                char word[5]; int i = 0;
                while (i < 4 && (c = fgetc(f)) != EOF) word[i++] = (char)c;
                word[i] = 0;
                if (i == 4 && strncmp(word, "text", 4) == 0) {
                    while ((c = fgetc(f)) != EOF && c != '>') { /* attributes */ }
                    if (c == '>') intext = 1;
                }
            }
        } else {
            if (c == '<') break;
            if (n + 1 < cap) out[n++] = (char)c;
        }
    }
    fclose(f);
    out[n] = 0;
    return 1;
}

#define VERIFY_CARDS_MAIN main
int VERIFY_CARDS_MAIN(void)
{
    static const struct { const char *name; char sym; int mode; } OPS[4] = {
        {"Add", '+', 0}, {"Sub", '-', 1}, {"Mul", '*', 2}, {"Div", '/', 3}
    };
    int checked = 0, errors = 0;
    char qp[256], ap[256], txt[128];

    for (int o = 0; o < 4; o++) {
        for (int x = 1; x <= 20; x++) {
            for (int y = 1; y <= 20; y++) {
                int a = x, d = y, expect;
                if (o == 3) { a = x * y; expect = x; }
                else if (o == 0) expect = a + d;
                else if (o == 1) expect = a - d;
                else expect = a * d;

                snprintf(qp, sizeof qp, "q/math_%s_%d-%d.svg", OPS[o].name, a, d);
                snprintf(ap, sizeof ap, "a/math_%s_%d-%d.svg", OPS[o].name, a, d);
                FILE *t = fopen(qp, "r");
                if (!t) { printf("MISSING QUESTION %s\n", qp); errors++; continue; }
                fclose(t);
                if (!extract_text(ap, txt, sizeof txt)) {
                    printf("MISSING ANSWER %s\n", ap); errors++; continue;
                }
                /* txt is like "7 + 3 = 10" — take after last '=' */
                char *eq = strrchr(txt, '=');
                if (!eq) { printf("NO ANSWER TEXT %s: %s\n", ap, txt); errors++; continue; }
                int got = atoi(eq + 1);
                if (got != expect) {
                    printf("WRONG %s: card says %d, arithmetic says %d\n", ap, got, expect);
                    errors++;
                }
                checked++;
            }
        }
    }
    printf("verify_cards: %d answer cards recomputed in C, %d errors\n", checked, errors);
    printf(errors == 0 ? "verify_cards: PASS — no card bears false witness\n"
                       : "verify_cards: FAIL\n");
    return errors != 0;
}
