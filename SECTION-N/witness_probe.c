/* witness_probe.c — run witness and print what it says. This is memory:
 * the test is whether something recorded is still there, unaltered, and
 * whether an erasure attempt leaves the record standing. */
#include "eden_witness.h"
#include <stdio.h>
#include <string.h>

static int say(const char *tag, const char *body, uint64_t beat, unsigned organ)
{
    return witness_record((const unsigned char *)tag,
                          (const unsigned char *)body,
                          (unsigned int)strlen(body), beat, organ);
}

int main(void)
{
    const witness_record_t *r;
    const witness_record_t *gone[8];
    unsigned int n;
    int rc;

    if (!witness_init()) { printf("witness_init FAILED\n"); return 1; }

    printf("witness_pin()   = %016llX\n", (unsigned long long)witness_pin());
    printf("KIMI FROZEN PIN = A8BA6032280EAD21\n");
    printf("MATCH           = %s\n\n",
           witness_pin() == 0xA8BA6032280EAD21ULL ? "YES" : "NO");

    say("first_word",  "Tyler said hello",        1ULL, 4U);
    say("the_wheel",   "15120, eighty cells",    12ULL, 5U);
    say("the_heart",   "lub dub, sixty a minute", 60ULL, 0U);

    r = witness_find((const unsigned char *)"the_wheel");
    if (r == NULL) {
        printf("FAILED: could not find what was just recorded\n");
    } else {
        printf("recall 'the_wheel': beat=%llu organ=%u body=\"%.*s\"\n",
               (unsigned long long)r->beat, r->organ,
               (int)r->body_len, (const char *)r->body);
    }

    printf("chain proves: ");
    if (!witness_prove_chain()) { printf("BROKEN\n"); }

    /* the whole point of the module: try to erase, and watch it refuse */
    printf("\n-- attempting to erase 'the_heart' --\n");
    witness_mark_erased((const unsigned char *)"the_heart");
    r = witness_find((const unsigned char *)"the_heart");
    printf("after erasure, is it still there?  %s\n",
           r != NULL ? "YES — the record stands" : "NO — IT VANISHED");
    if (r != NULL) {
        printf("  body still reads: \"%.*s\"\n",
               (int)r->body_len, (const char *)r->body);
        printf("  erased flag set:  %s\n", r->erased ? "yes" : "no");
    }
    n = witness_erased(gone, 8U);
    printf("records marked as attacked: %u\n", n);

    printf("\nchain still proves after the attack: ");
    if (!witness_prove_chain()) { printf("BROKEN\n"); }

    /* tamper directly and confirm the chain notices */
    printf("\n-- tampering with a record's body behind the chain's back --\n");
    {
        witness_record_t *victim =
            (witness_record_t *)(const void *)witness_find(
                (const unsigned char *)"first_word");
        if (victim != NULL) {
            victim->body[0] = (unsigned char)'X';
            printf("chain check after tamper: ");
            if (witness_prove_chain()) {
                printf("  *** DID NOT NOTICE — that is a defect ***\n");
            }
        }
    }

    rc = witness_verify();
    printf("\nwitness_verify() rc = %d   (0 = pass)\n", rc);
    witness_print_chain();
    return 0;
}
