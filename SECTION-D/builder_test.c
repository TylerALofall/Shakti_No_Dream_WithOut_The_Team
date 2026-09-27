/* builder_test.c — the harness for mommas_little_builder.c
 *
 * A fixed 12-command curriculum, 400 beats, then the pins of record.
 * Deterministic: the teacher supplies every epoch; the organ reads no
 * clock. -O0 must equal -O2, byte for byte.
 *
 * The curriculum walks every law:
 *   TEMPO dial, HB toggle (slot 2), SPEAK, LEARN with tags,
 *   a LOOK with a red button and a blue button (she must see both),
 *   USE before training (refused: not taught), TRAIN (the brand lands),
 *   USE after training (the actuator runs), and one honest UNKNOWN.
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
        "1 TEMPO 5\n"
        "2 HB on\n"
        "3 SPEAK good morning momma\n"
        "4 LEARN tag=colors blue is the color of the sky\n"
        "5 LOOK button key=BLUE options=RED,BLUE\n"
        "6 USE exact_edit replace bravo | BRAVO\n"
        "7 TRAIN exact_edit 1787700000\n"
        "8 USE exact_edit replace bravo | BRAVO\n"
        "9 LEARN tag=first_tool exact_edit changes copies never originals\n"
        "10 SPEAK i learned my first tool\n"
        "11 WHOA this is not a command\n"
        "12 SPEAK eleven was not mine and i knew it\n",
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
    for (i = 0; i < 400; i++) builder_beat();
    builder_seal(); /* F10: the stream pin rides into TICKETS.log */

    printf("beats      %llu\n", (unsigned long long)builder_beat_count());
    printf("next_seq   %llu\n", (unsigned long long)builder_next_seq());
    printf("blocks     %llu\n", (unsigned long long)builder_blocks());
    printf("stream pin %016llX\n", (unsigned long long)builder_stream_pin());

    puts("--- block 1 recall (whole block or nothing) ---");
    if (!builder_recall(1, stdout)) puts("(no block)");
    return 0;
}
