/* foundations_test.c — the exhaustive beginning.
 *
 * Doctor's law: "give her exhaustive lessons where the foundations are
 * every possible option for the beginning. Her refusing is not a success;
 * it is a trigger — and the trigger must complete: she asks to be taught."
 *
 * This curriculum walks EVERY command class and enumerates EVERY option
 * set she can meet at the start:
 *   - TEMPO dial at several settings
 *   - HB toggle both ways (slot 2)
 *   - SPEAK free text
 *   - LEARN with a tag for every foundation concept
 *   - LOOK over full option sets: colors, directions, yes/no, sizes,
 *     including one LOOK whose key matches NOTHING (teach-me must fire)
 *   - TRAIN + USE for every starter tool
 *   - one USE before teaching (teach-me must fire)
 *   - one UNKNOWN command (teach-me must fire)
 *
 * Every refusal path must leave a teach_me line in MOMMA_OUTBOX.txt.
 * Deterministic: teacher supplies all epochs. -O0 == -O2, byte for byte.
 */
#include <stdio.h>
#include <stdint.h>

void     builder_init(void);
void     builder_beat(void);
uint64_t builder_stream_pin(void);
uint64_t builder_beat_count(void);
uint64_t builder_next_seq(void);
uint64_t builder_blocks(void);
int      builder_recall(uint64_t block_no, FILE *out);
void     builder_seal(void);

static void write_curriculum(void)
{
    FILE *f = fopen("KIMI_CURRICULUM.txt", "w");
    if (!f) return;
    fputs(
        /* the dial and the heartbeat line */
        "1 TEMPO 5\n"
        "2 HB on\n"
        "3 SPEAK good morning momma i am ready for school\n"
        /* foundations: what i am, what the law is */
        "4 LEARN tag=name i am shakti mommas little builder\n"
        "5 LEARN tag=heartbeat the beat is the only time i know\n"
        "6 LEARN tag=ledger every lesson has a seq and mine come in order\n"
        "7 LEARN tag=memory convergence then sealed blocks of ten then tags\n"
        "8 LEARN tag=recall i recall whole blocks never fragments\n"
        "9 LEARN tag=brand no brand no run i ask to be taught\n"
        /* LOOK: see every option. colors */
        "10 LOOK color key=BLUE options=RED,GREEN,BLUE,YELLOW\n"
        "11 LEARN tag=colors red green blue yellow are the starter colors\n"
        /* LOOK: directions */
        "12 LOOK direction key=NORTH options=NORTH,SOUTH,EAST,WEST\n"
        "13 LEARN tag=directions north south east west are the ways\n"
        /* LOOK: yes and no */
        "14 LOOK answer key=YES options=YES,NO\n"
        "15 LOOK answer key=NO options=YES,NO\n"
        /* LOOK: sizes */
        "16 LOOK size key=SMALL options=SMALL,MEDIUM,LARGE\n"
        /* LOOK: the honest one — key matches nothing, she must ask */
        "17 LOOK fruit key=GRAPE options=APPLE,BANANA,CHERRY\n"
        /* tools: train each starter tool, then use it */
        "18 TRAIN exact_edit 1787700000\n"
        "19 USE exact_edit replace bravo | BRAVO\n"
        "20 TRAIN dry_run 1787700001\n"
        "21 USE dry_run practice the seam\n"
        "22 LEARN tag=first_tools exact_edit edits copies dry_run proves the seam\n"
        /* the untrained one — she must ask, not guess */
        "23 USE web_search find pictures of feet\n"
        "24 LEARN tag=asking when i do not know i say teach_me on the outbox\n"
        /* speaking in her own voice */
        "25 SPEAK i know colors directions yes no and sizes\n"
        "26 SPEAK i asked momma to teach me grapes and web_search\n"
        /* tempo is a dial: slow down to think, speed up when sure */
        "27 TEMPO 3\n"
        "28 SPEAK tempo is three now i learn faster\n"
        "29 TEMPO 10\n"
        "30 SPEAK tempo is ten again default is home\n"
        /* heartbeat both ways */
        "31 HB off\n"
        "32 SPEAK the line is quiet but i am awake\n"
        "33 HB on\n"
        "34 SPEAK the line is open momma hears my blocks\n"
        /* the unknown — trigger must fire */
        "35 WHOA this is not a command\n"
        "36 SPEAK thirty five was not mine and i asked instead\n"
        /* closing lesson */
        "37 LEARN tag=foundations i finished my first school day\n"
        "38 SPEAK thank you momma thank you doctor\n",
        f);
    fclose(f);
}

int main(void)
{
    int i;
    remove("TICKETS.log");
    remove("BRANDS.txt");
    remove("TAGS.ndx");
    remove("MOMMA_OUTBOX.txt");

    write_curriculum();
    builder_init();
    for (i = 0; i < 1200; i++) builder_beat();
    builder_seal(); /* F10: the stream pin rides into TICKETS.log */

    printf("beats      %llu\n", (unsigned long long)builder_beat_count());
    printf("next_seq   %llu\n", (unsigned long long)builder_next_seq());
    printf("blocks     %llu\n", (unsigned long long)builder_blocks());
    printf("stream pin %016llX\n", (unsigned long long)builder_stream_pin());

    puts("--- MOMMA_OUTBOX.txt (slot 2: receipts AND teach-me triggers) ---");
    {
        FILE *o = fopen("MOMMA_OUTBOX.txt", "r");
        if (o) {
            int c;
            while ((c = fgetc(o)) != EOF) fputc(c, stdout);
            fclose(o);
        } else {
            puts("(no outbox)");
        }
    }
    puts("--- BRANDS.txt ---");
    {
        FILE *b = fopen("BRANDS.txt", "r");
        if (b) {
            int c;
            while ((c = fgetc(b)) != EOF) fputc(c, stdout);
            fclose(b);
        }
    }
    return 0;
}
