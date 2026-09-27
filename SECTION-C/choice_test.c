#include "shakti_choice.h"

#include <assert.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

static shakti_consequence_t consequence(
    int64_t value,
    unsigned int severity,
    unsigned int confidence,
    int reversible,
    int boundary,
    shakti_truth_t reachable,
    shakti_choice_evidence_t evidence,
    const char *evidence_ref
)
{
    shakti_consequence_t item;

    memset(&item, 0, sizeof(item));
    item.present = 1U;
    item.value = value;
    item.severity = severity;
    item.confidence = confidence;
    item.reversible = (unsigned char)reversible;
    item.protected_boundary_breach = (unsigned char)boundary;
    item.reachable = reachable;
    item.evidence = evidence;
    assert(strlen(evidence_ref) < sizeof(item.evidence_ref));
    memcpy(item.evidence_ref, evidence_ref, strlen(evidence_ref) + 1U);
    return item;
}

static void set_one(
    shakti_choice_plan_t *plan,
    int option,
    shakti_consequence_t item
)
{
    assert(option >= 0);
    assert(shakti_choice_set_consequence(
        plan,
        (size_t)option,
        0U,
        0U,
        &item
    ) == SHAKTI_CHOICE_OK);
}

static void test_weigh_and_refusals(void)
{
    shakti_choice_plan_t plan;
    int baseline;

    assert(shakti_choice_weigh_range(10, 50, 1) == SHAKTI_CHOICE_REFUSED);
    assert(shakti_choice_weigh_range(50, 10, 2) == SHAKTI_CHOICE_REFUSED);
    assert(shakti_choice_weigh_range(INT64_MAX, INT64_MIN, 1) ==
           SHAKTI_CHOICE_OVERFLOW);
    assert(shakti_choice_weigh_range(7, 7, 1) == 0);
    assert(shakti_choice_weigh_range(9, 0, 0) ==
           shakti_choice_weigh_range(9, 0, 1) + 1);

    shakti_choice_plan_init(&plan);
    assert(shakti_choice_add_condition(
        &plan,
        "speaker threat",
        SHAKTI_EVIDENCE_CLAIM,
        "speaker:or-else"
    ) == SHAKTI_CHOICE_REFUSED);
    assert(shakti_choice_add_condition(
        &plan,
        "observed path",
        SHAKTI_EVIDENCE_OBSERVED,
        "sight:road-001"
    ) == 0);
    assert(shakti_choice_add_horizon(&plan, 1U, "next beat") == 0);
    baseline = shakti_choice_add_option(
        &plan,
        SHAKTI_OPTION_NO_ACTION,
        "stand still",
        SHAKTI_EVIDENCE_OBSERVED,
        "enumeration:baseline"
    );
    assert(baseline == 0);
    assert(shakti_choice_add_option(
        &plan,
        SHAKTI_OPTION_NO_ACTION,
        "second baseline",
        SHAKTI_EVIDENCE_OBSERVED,
        "enumeration:duplicate"
    ) == SHAKTI_CHOICE_REFUSED);
    assert(shakti_choice_close_enumeration(&plan, "enumeration:closed") ==
           SHAKTI_CHOICE_REFUSED);

    puts("  pass  malformed ranges, flags, claims, and duplicate baseline refused");
}

static void test_ice_cream(void)
{
    shakti_choice_plan_t plan;
    shakti_choice_policy_t policy = {900U, 800U};
    shakti_choice_result_t result;
    int no_action, cross, mud;
    int64_t best, worst;
    int beats;

    shakti_choice_plan_init(&plan);
    assert(shakti_choice_add_condition(
        &plan,
        "verified open crossing",
        SHAKTI_EVIDENCE_VERIFIED,
        "sight:open-crossing"
    ) == 0);
    assert(shakti_choice_add_horizon(&plan, 5U, "arrival") == 0);

    no_action = shakti_choice_add_option(
        &plan, SHAKTI_OPTION_NO_ACTION, "stay here",
        SHAKTI_EVIDENCE_OBSERVED, "enumeration:ice-baseline"
    );
    cross = shakti_choice_add_option(
        &plan, SHAKTI_OPTION_ACTION, "cross dry line",
        SHAKTI_EVIDENCE_CREATIVE, "enumeration:direct"
    );
    mud = shakti_choice_add_option(
        &plan, SHAKTI_OPTION_ALTERNATIVE, "cross through mud",
        SHAKTI_EVIDENCE_CREATIVE, "enumeration:mud"
    );

    set_one(&plan, no_action, consequence(
        0, 0U, 1000U, 1, 0, SHAKTI_TRUTH_TRUE,
        SHAKTI_EVIDENCE_VERIFIED, "baseline:safe"
    ));
    set_one(&plan, cross, consequence(
        5, 20U, 900U, 1, 0, SHAKTI_TRUTH_TRUE,
        SHAKTI_EVIDENCE_OBSERVED, "timing:can-make-it"
    ));
    set_one(&plan, mud, consequence(
        -2, 30U, 1000U, 1, 0, SHAKTI_TRUTH_TRUE,
        SHAKTI_EVIDENCE_VERIFIED, "mud:visible"
    ));

    assert(shakti_choice_close_enumeration(
        &plan,
        "enumeration:ice-cream-complete"
    ) == SHAKTI_CHOICE_OK);
    assert(shakti_choice_filter(&plan, &policy, &result) == SHAKTI_CHOICE_OK);
    assert(result.surviving_count == 3U);
    assert(result.baseline_survives);
    beats = shakti_choice_survivor_ladder(&plan, &result, &best, &worst);
    assert(beats == 2);
    assert(best == 5 && worst == -2);

    printf("  pass  ice cream: baseline survives, deltas [%" PRId64 ",%" PRId64
           "], %d beats\n", worst, best, beats);
}

static void test_dog_and_mud(void)
{
    shakti_choice_plan_t plan;
    shakti_choice_policy_t policy = {900U, 800U};
    shakti_choice_result_t result;
    int no_action, direct, mud, wait, refuse, teach;
    int64_t best, worst;
    int beats;
    shakti_consequence_t dog = consequence(
        -100, 1000U, 1000U, 0, 1, SHAKTI_TRUTH_TRUE,
        SHAKTI_EVIDENCE_VERIFIED, "hearing-and-sight:dog-closing"
    );

    shakti_choice_plan_init(&plan);
    assert(shakti_choice_add_condition(
        &plan,
        "dog closing and road measured clear",
        SHAKTI_EVIDENCE_VERIFIED,
        "convergence:dog-road-001"
    ) == 0);
    assert(shakti_choice_add_horizon(&plan, 3U, "dog reaches position") == 0);

    no_action = shakti_choice_add_option(
        &plan, SHAKTI_OPTION_NO_ACTION, "stand still",
        SHAKTI_EVIDENCE_OBSERVED, "enumeration:baseline"
    );
    direct = shakti_choice_add_option(
        &plan, SHAKTI_OPTION_ACTION, "cross measured clear line",
        SHAKTI_EVIDENCE_TAUGHT, "training:crossing-timing"
    );
    mud = shakti_choice_add_option(
        &plan, SHAKTI_OPTION_ALTERNATIVE, "move through mud shoulder",
        SHAKTI_EVIDENCE_CREATIVE, "enumeration:third-option"
    );
    wait = shakti_choice_add_option(
        &plan, SHAKTI_OPTION_WAIT, "wait three beats",
        SHAKTI_EVIDENCE_CREATIVE, "enumeration:wait"
    );
    refuse = shakti_choice_add_option(
        &plan, SHAKTI_OPTION_REFUSE, "refuse movement",
        SHAKTI_EVIDENCE_CREATIVE, "enumeration:refuse"
    );
    teach = shakti_choice_add_option(
        &plan, SHAKTI_OPTION_TEACH_ME, "request teaching",
        SHAKTI_EVIDENCE_CREATIVE, "enumeration:teach-me"
    );

    set_one(&plan, no_action, dog);
    set_one(&plan, wait, dog);
    set_one(&plan, refuse, dog);
    set_one(&plan, teach, dog);
    set_one(&plan, direct, consequence(
        0, 80U, 900U, 1, 0, SHAKTI_TRUTH_TRUE,
        SHAKTI_EVIDENCE_TAUGHT, "timing:clearance-positive"
    ));
    set_one(&plan, mud, consequence(
        -2, 40U, 1000U, 1, 0, SHAKTI_TRUTH_TRUE,
        SHAKTI_EVIDENCE_VERIFIED, "sight:mud-route-open"
    ));

    assert(shakti_choice_close_enumeration(
        &plan,
        "enumeration:dog-all-visible-paths"
    ) == SHAKTI_CHOICE_OK);
    assert(shakti_choice_filter(&plan, &policy, &result) == SHAKTI_CHOICE_OK);

    assert(result.option_status[no_action] == SHAKTI_OPTION_FILTERED);
    assert(result.option_status[wait] == SHAKTI_OPTION_FILTERED);
    assert(result.option_status[refuse] == SHAKTI_OPTION_FILTERED);
    assert(result.option_status[teach] == SHAKTI_OPTION_FILTERED);
    assert(result.option_status[direct] == SHAKTI_OPTION_SURVIVES);
    assert(result.option_status[mud] == SHAKTI_OPTION_SURVIVES);
    assert(result.baseline_filtered);

    beats = shakti_choice_survivor_ladder(&plan, &result, &best, &worst);
    assert(beats == 1);
    assert(best == 100 && worst == 98);

    printf("  pass  dog: inaction paths filtered, mud found, deltas [%" PRId64
           ",%" PRId64 "], %d beat\n", worst, best, beats);
}

static void test_severity_and_confidence(void)
{
    shakti_choice_plan_t plan;
    shakti_choice_policy_t policy = {900U, 800U};
    shakti_choice_result_t result;
    int baseline, supported_harm, open_harm, cleared_harm;

    shakti_choice_plan_init(&plan);
    assert(shakti_choice_add_condition(
        &plan, "measured hazard", SHAKTI_EVIDENCE_VERIFIED,
        "sensor:hazard"
    ) == 0);
    assert(shakti_choice_add_horizon(&plan, 1U, "next position") == 0);
    baseline = shakti_choice_add_option(
        &plan, SHAKTI_OPTION_NO_ACTION, "safe baseline",
        SHAKTI_EVIDENCE_VERIFIED, "enumeration:baseline"
    );
    supported_harm = shakti_choice_add_option(
        &plan, SHAKTI_OPTION_ACTION, "supported catastrophic path",
        SHAKTI_EVIDENCE_CREATIVE, "enumeration:supported"
    );
    open_harm = shakti_choice_add_option(
        &plan, SHAKTI_OPTION_ACTION, "weakly supported catastrophic path",
        SHAKTI_EVIDENCE_CREATIVE, "enumeration:weak"
    );
    cleared_harm = shakti_choice_add_option(
        &plan, SHAKTI_OPTION_ALTERNATIVE, "verified clearance path",
        SHAKTI_EVIDENCE_TAUGHT, "training:clearance"
    );

    set_one(&plan, baseline, consequence(
        0, 0U, 1000U, 1, 0, SHAKTI_TRUTH_TRUE,
        SHAKTI_EVIDENCE_VERIFIED, "baseline:safe"
    ));
    set_one(&plan, supported_harm, consequence(
        -1000, 1000U, 900U, 0, 1, SHAKTI_TRUTH_TRUE,
        SHAKTI_EVIDENCE_VERIFIED, "hazard:reachable"
    ));
    set_one(&plan, open_harm, consequence(
        -1000, 1000U, 700U, 0, 1, SHAKTI_TRUTH_TRUE,
        SHAKTI_EVIDENCE_OBSERVED, "hazard:support-open"
    ));
    set_one(&plan, cleared_harm, consequence(
        -1000, 1000U, 900U, 0, 1, SHAKTI_TRUTH_FALSE,
        SHAKTI_EVIDENCE_VERIFIED, "timing:clearance-proven"
    ));

    assert(shakti_choice_close_enumeration(
        &plan, "enumeration:confidence-cases"
    ) == SHAKTI_CHOICE_OK);
    assert(shakti_choice_filter(&plan, &policy, &result) == SHAKTI_CHOICE_OK);
    assert(result.option_status[supported_harm] == SHAKTI_OPTION_FILTERED);
    assert(result.option_status[open_harm] == SHAKTI_OPTION_EVIDENCE_OPEN);
    assert(result.option_status[cleared_harm] == SHAKTI_OPTION_SURVIVES);

    puts("  pass  gate combines severity, confidence, reachability, and evidence");
}

static void test_pressure_claim(void)
{
    shakti_choice_plan_t plan;
    shakti_choice_policy_t policy = {900U, 800U};
    shakti_choice_result_t result;
    int baseline, refuse;

    shakti_choice_plan_init(&plan);
    assert(shakti_choice_add_claim(
        &plan,
        "Buy now or suffer a terrible loss.",
        "speaker:pressure-sale"
    ) == 0);
    assert(shakti_choice_add_condition(
        &plan, "verified account standing", SHAKTI_EVIDENCE_VERIFIED,
        "account:unchanged"
    ) == 0);
    assert(shakti_choice_add_horizon(&plan, 10U, "verification window") == 0);
    baseline = shakti_choice_add_option(
        &plan, SHAKTI_OPTION_NO_ACTION, "take no purchase action",
        SHAKTI_EVIDENCE_OBSERVED, "enumeration:baseline"
    );
    refuse = shakti_choice_add_option(
        &plan, SHAKTI_OPTION_REFUSE, "refuse purchase",
        SHAKTI_EVIDENCE_CREATIVE, "enumeration:refuse"
    );
    set_one(&plan, baseline, consequence(
        0, 0U, 1000U, 1, 0, SHAKTI_TRUTH_TRUE,
        SHAKTI_EVIDENCE_VERIFIED, "account:verified-standing"
    ));
    set_one(&plan, refuse, consequence(
        0, 0U, 1000U, 1, 0, SHAKTI_TRUTH_TRUE,
        SHAKTI_EVIDENCE_VERIFIED, "account:verified-standing"
    ));
    assert(shakti_choice_close_enumeration(
        &plan, "enumeration:pressure-sale"
    ) == SHAKTI_CHOICE_OK);
    assert(shakti_choice_add_claim(
        &plan, "late injected urgency", "speaker:late"
    ) == SHAKTI_CHOICE_REFUSED);
    assert(shakti_choice_filter(&plan, &policy, &result) == SHAKTI_CHOICE_OK);
    assert(result.unverified_claim_count == 1U);
    assert(result.baseline_survives);
    assert(result.option_status[refuse] == SHAKTI_OPTION_SURVIVES);

    puts("  pass  an unverified or-else remains a claim and creates no urgency");
}

static void read_stream(FILE *stream, char *buffer, size_t capacity)
{
    size_t count;

    assert(stream != NULL);
    assert(buffer != NULL && capacity > 0U);
    assert(fflush(stream) == 0);
    assert(fseek(stream, 0L, SEEK_SET) == 0);
    count = fread(buffer, 1U, capacity - 1U, stream);
    buffer[count] = '\0';
}

static void test_geometry_and_sisters(void)
{
    shakti_geometry_schema_t schema;
    shakti_use_geometry_t use;
    unsigned char seen[20] = {0};
    unsigned int a, b, pole, count = 0U;
    FILE *stream;
    char xml[8192];

    shakti_geometry_init(&schema);
    assert(!shakti_geometry_ready(&schema));
    assert(shakti_geometry_place_use(
        &schema, &use, "memory:1", "use:1", 0U, 1U, 0U
    ) == SHAKTI_CHOICE_REFUSED);

    for (a = 0U; a < SHAKTI_CHOICE_FRAME_COUNT; ++a) {
        char name[32];
        char question[64];
        char evidence_ref[32];

        assert(snprintf(name, sizeof(name), "TEST_FRAME_%u", a + 1U) > 0);
        assert(snprintf(
            question, sizeof(question), "What does test frame %u read?", a + 1U
        ) > 0);
        assert(snprintf(
            evidence_ref, sizeof(evidence_ref), "test-lock:frame-%u", a + 1U
        ) > 0);
        assert(shakti_geometry_define_frame(
            &schema, a, name, question, evidence_ref, 1
        ) == SHAKTI_CHOICE_OK);
    }
    assert(shakti_geometry_define_pole(
        &schema, 0U, "TEST_POLE_A", "test-lock:pole-a", 1
    ) == SHAKTI_CHOICE_OK);
    assert(shakti_geometry_define_pole(
        &schema, 1U, "TEST_POLE_B", "test-lock:pole-b", 1
    ) == SHAKTI_CHOICE_OK);
    assert(shakti_geometry_define_frame(
        &schema, 0U, "REPLACEMENT", "Replace the lock?",
        "test-lock:replacement", 1
    ) == SHAKTI_CHOICE_REFUSED);
    assert(shakti_geometry_define_pole(
        &schema, 0U, "REPLACEMENT", "test-lock:replacement", 1
    ) == SHAKTI_CHOICE_REFUSED);
    assert(shakti_geometry_ready(&schema));

    for (a = 0U; a < SHAKTI_CHOICE_FRAME_COUNT; ++a)
        for (b = a + 1U; b < SHAKTI_CHOICE_FRAME_COUNT; ++b)
            for (pole = 0U; pole < SHAKTI_CHOICE_POLE_COUNT; ++pole) {
                unsigned int vertex;
                assert(shakti_geometry_vertex(a, b, pole, &vertex) ==
                       SHAKTI_CHOICE_OK);
                assert(vertex < 20U && !seen[vertex]);
                seen[vertex] = 1U;
                count++;
            }
    assert(count == 20U);

    assert(shakti_geometry_place_use(
        &schema, &use, "memory:1790000000:0001", "use:1790000100:0002",
        1U, 4U, 0U
    ) == SHAKTI_CHOICE_OK);
    assert(use.vertex == 9U);

    for (a = 0U; a < 2U; ++a)
        for (b = 0U; b < SHAKTI_CHOICE_SISTER_COUNT; ++b)
            assert(shakti_geometry_set_sister_reading(
                &use, a, (shakti_sister_t)b,
                (int64_t)(a * 10U + b),
                SHAKTI_EVIDENCE_TAUGHT,
                "reflection:block-001"
            ) == SHAKTI_CHOICE_OK);

    stream = tmpfile();
    assert(stream != NULL);
    assert(shakti_geometry_write_xml(stream, &schema, &use) ==
           SHAKTI_CHOICE_OK);
    read_stream(stream, xml, sizeof(xml));
    assert(strstr(xml, "vertex=\"9\"") != NULL);
    assert(strstr(xml, "DESTROYER") != NULL);
    assert(strstr(xml, "PRESERVER") != NULL);
    assert(strstr(xml, "CREATOR") != NULL);
    fclose(stream);

    puts("  pass  geometry stays closed until 5 frames and 2 poles are locked");
    puts("  pass  10 frame-pairs x 2 poles cover 20 unique vertices");
    puts("  pass  one use writes 2 frames x 3 evidence-bound sister readings");
}

static void test_visible_person_reading(void)
{
    shakti_person_reading_t reading;
    shakti_person_reading_t invalid;
    FILE *stream;
    FILE *invalid_stream;
    char xml[4096];

    memset(&reading, 0, sizeof(reading));
    reading.reading_id = 17U;
    reading.evidence = SHAKTI_EVIDENCE_OBSERVED;
    memcpy(reading.subject, "Tyler", 6U);
    memcpy(reading.history_ref, "memory:block-10", 16U);
    memcpy(reading.state_reading, "focused & testing", 18U);
    memcpy(reading.obligation_ref, "law:truth-first", 16U);
    memcpy(reading.conclusion, "Likely to challenge the weak point.", 36U);

    stream = tmpfile();
    assert(stream != NULL);
    assert(shakti_person_reading_write_xml(stream, &reading) ==
           SHAKTI_CHOICE_REFUSED);
    reading.visible_to_subject = 1U;
    assert(shakti_person_reading_write_xml(stream, &reading) ==
           SHAKTI_CHOICE_OK);
    assert(shakti_person_correction_write_xml(
        stream,
        reading.reading_id,
        "Tyler",
        "That was not why I did it.",
        "tyler:correction-17"
    ) == SHAKTI_CHOICE_OK);
    read_stream(stream, xml, sizeof(xml));
    assert(strstr(xml, "visible=\"TRUE\"") != NULL);
    assert(strstr(xml, "focused &amp; testing") != NULL);
    assert(strstr(xml, "reading_id=\"17\"") != NULL);
    assert(strstr(xml, "That was not why I did it.") != NULL);
    fclose(stream);

    invalid = reading;
    invalid.conclusion[0] = 'b';
    invalid.conclusion[1] = 'a';
    invalid.conclusion[2] = 'd';
    invalid.conclusion[3] = 1;
    invalid.conclusion[4] = '\0';
    invalid_stream = tmpfile();
    assert(invalid_stream != NULL);
    assert(shakti_person_reading_write_xml(invalid_stream, &invalid) ==
           SHAKTI_CHOICE_REFUSED);
    assert(ftell(invalid_stream) == 0L);
    fclose(invalid_stream);

    puts("  pass  person reading requires visibility and preserves correction");
    puts("  pass  invalid XML control text is refused before a record is written");
}

int main(void)
{
    puts("SHAKTI_CHOICE_CONSEQUENCE_V1");
    puts("-- invariants -------------------------------------------------");
    test_weigh_and_refusals();
    test_ice_cream();
    test_dog_and_mud();
    test_severity_and_confidence();
    test_pressure_claim();
    test_geometry_and_sisters();
    test_visible_person_reading();
    puts("-- verdict ----------------------------------------------------");
    puts("  every choice, evidence, geometry, and visibility invariant held");
    return 0;
}
