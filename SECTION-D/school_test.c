/* school_test.c — the first school day.
 *
 * A starter corpus of 64 K-12 words, each bound to a SELF-MADE shape
 * token (the kind her eye organ's center-surround waves produce — her
 * own compression of the picture, never a label handed down).
 *
 * The full corpus is thousands with pictures (WORD_SCHOOL.md §3) and
 * arrives in iterations; the organ does not change as the corpus grows.
 *
 * Expectations: every word starts as noise, every word fails early
 * (recognize-fail / spell-fail with teach_me on the outbox), every word
 * ends KNOWN — three spaced spelling passes across three sealed blocks.
 * -O0 == -O2, byte for byte.
 */
#include <stdio.h>
#include <stdint.h>

void     school_init(void);
size_t   school_load(void);
void     school_beat(void);
uint64_t school_stream_pin(void);
uint64_t school_beat_count(void);
uint64_t school_blocks(void);
uint64_t school_known(void);
uint64_t school_teach_count(void);
size_t   school_word_count(void);
void     school_seal(void);

static void write_wordlist(void)
{
    FILE *f = fopen("WORDLIST.txt", "w");
    if (!f) return;
    fputs(
        "cat four-legs-meow-whiskers\n"
        "dog four-legs-tail-wags\n"
        "mom tall-warms-me-holds-me\n"
        "dad tall-lifts-me-deep-voice\n"
        "ball round-bounces-rolls\n"
        "sun round-yellow-up-warm\n"
        "moon round-pale-night\n"
        "star pointy-five-twinkles\n"
        "fish fins-swims-water\n"
        "bird wings-flies-sings\n"
        "tree tall-brown-green-top\n"
        "car four-wheels-goes-fast\n"
        "book opens-pages-words\n"
        "milk white-drink-cup\n"
        "cake round-sweet-candles\n"
        "egg oval-white-cracks\n"
        "hat head-cover-brim\n"
        "shoe foot-cover-laces\n"
        "hand five-fingers-grabs\n"
        "eye round-sees-blinks\n"
        "nose middle-face-smells\n"
        "ear side-head-hears\n"
        "mouth opens-talks-eats\n"
        "run legs-fast-chase\n"
        "jump up-down-legs-spring\n"
        "sit bend-rest-down-still\n"
        "stand up-legs-straight\n"
        "walk legs-slow-steps\n"
        "red color-warm-stop\n"
        "blue color-sky-cool\n"
        "green color-grass-go\n"
        "one single-alone-first\n"
        "two pair-again-second\n"
        "three triple-third\n"
        "four legs-table-count\n"
        "five hand-fingers-count\n"
        "apple round-red-crunch\n"
        "banana long-yellow-peel\n"
        "water clear-drink-rain\n"
        "bread soft-slices-toast\n"
        "cheese yellow-holes-melts\n"
        "house roof-door-windows\n"
        "door opens-closes-knob\n"
        "window glass-see-through\n"
        "bed soft-sleep-pillow\n"
        "cup holds-drink-handle\n"
        "spoon scoops-small-metal\n"
        "frog green-hops-croaks\n"
        "duck waddles-quacks-swims\n"
        "cow big-moos-milk\n"
        "pig pink-round-oinks\n"
        "horse big-rides-neighs\n"
        "sheep fluffy-white-baas\n"
        "chicken feathers-pecks-eggs\n"
        "rain falls-wet-drops\n"
        "snow white-cold-falls\n"
        "cloud white-floats-shapes\n"
        "wind invisible-pushes-leaves\n"
        "fire hot-orange-burns\n"
        "rock hard-gray-heavy\n"
        "sand tiny-grains-beach\n"
        "leaf green-flat-falls\n"
        "flower petals-smells-pretty\n"
        "grass green-ground-soft\n"
        "bug tiny-crawls-legs\n"
        "ant tiny-lines-carries\n"
        "bee yellow-buzzes-stings\n",
        f);
    fclose(f);
}

int main(void)
{
    uint64_t i;
    remove("SCHOOL.log");
    remove("MASTERY.txt");
    remove("MOMMA_OUTBOX.txt");

    write_wordlist();
    school_init();
    printf("corpus     %llu words\n", (unsigned long long)school_load());

    for (i = 0; i < 6000 && school_known() < school_word_count(); i++)
        school_beat();
    school_seal(); /* F10: the stream pin rides into SCHOOL.log */

    printf("beats      %llu\n", (unsigned long long)school_beat_count());
    printf("known      %llu of %llu\n",
           (unsigned long long)school_known(),
           (unsigned long long)school_word_count());
    printf("blocks     %llu\n", (unsigned long long)school_blocks());
    printf("teach_me   %llu (every one a question asked, none a failure hidden)\n",
           (unsigned long long)school_teach_count());
    printf("stream pin %016llX\n", (unsigned long long)school_stream_pin());

    puts("--- MASTERY.txt (first 12 lines) ---");
    {
        FILE *m = fopen("MASTERY.txt", "r");
        int c, lines = 0;
        if (m) {
            while (lines < 12 && (c = fgetc(m)) != EOF) {
                fputc(c, stdout);
                if (c == '\n') lines++;
            }
            fclose(m);
        }
    }
    return 0;
}
