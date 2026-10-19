#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
/* Explicit native synthetic RUN. No subprocesses, devices, allocations or
 * production decoder replacement. Boundary injection exists only when the
 * separate RUN build enables SHAKTI_PRENATAL_RUN_HOOKS. */
#define main shakti_console_main_for_run
#include "main.c"
#undef main
#include <sys/stat.h>
#include "shakti_prenatal_loader.h"

#define NEED(c,m) do { if (!(c)) { fprintf(stderr,"BENCH RUN: %s at %u\n",m,(unsigned int)__LINE__); return 0; } } while (0)
static shakti_runtime_t state;
static shakti_memory_item_t saved;
static unsigned char originals[9][65536U];
static shakti_prenatal_wave_t original[9];
static const char *const labels[9] = {
    "black","red","green","yellow","blue","magenta","cyan","white","pulse"
};
static const char story[] = "I played baseball. A baseball bat hits the ball. The cat is not the target.";
static unsigned long windows_checked, pixels_checked;

#ifdef SHAKTI_PRENATAL_RUN_HOOKS
static int write_text(const char *path, const char *text)
{
    FILE *file = fopen(path,"wb");
    int ok;
    if (!file) return 0;
    ok = fputs(text,file) >= 0;
    return fclose(file) == 0 && ok;
}
#endif

static int preserved(void)
{
    char expected[sizeof(story)+80U];
    int n=snprintf(expected,sizeof(expected),"2140000000:0\nid=context-capsule\n%s\n",story);
    if(n<=0 || (size_t)n>=sizeof(expected)) return 0;
    return shakti_memory_fetch(&state.memory,"context-capsule",&saved) == 1 &&
        !strcmp(saved.text,expected) && !strcmp(saved.tags,"baseball ball bat context") &&
        !strcmp(saved.method,"whole experience") && !strcmp(saved.location,"playground/baseball");
}

static int load_originals(const char *assets)
{
    unsigned int i;
    for (i = 0U; i < 9U; ++i) {
        char path[SHAKTI_PATH_CAPACITY];
        size_t bytes;
        int n = snprintf(path,sizeof(path),"%s/%s.wav",assets,labels[i]);
        NEED(n > 0 && (size_t)n < sizeof(path) &&
            shakti_prenatal_wave_load(path,originals[i],sizeof(originals[i]),&bytes,&original[i]),
            "independent original WAV input");
    }
    return 1;
}

static int verify_view(void)
{
    shakti_convergence_view_t view;
    shakti_prenatal_plan_t plan;
    const shakti_media_group_t *held = shakti_media_current(&state.media);
    const shakti_sensory_group_t *group;
    uint64_t position, beat;
    unsigned int lane, color, mask;
    size_t i;
    NEED(shakti_app_convergence_view(&state,&view) && view.sensory && held,
        "real runtime convergence publishes a validated view");
    group = view.sensory;
    position = group->parts[1].position64;
    beat = position / 64U + 1U;
    NEED(shakti_prenatal_plan(beat,&plan),"independent source position lookup");
    color = plan.color;
    mask = plan.phase == 2U ? 7U : 6U;
    NEED(group->checked_mask == mask && shakti_prenatal_owns(&state,held) &&
        view.eden == &state.reason && view.working == state.memory.working &&
        view.short_term == state.memory.short_term,"shared sensory and memory ownership");
    NEED(view.retained_media && !view.retained_media->active &&
        view.retained_media->windows_seen == 0U,"replayable corpus does not duplicate into movie arena");
    if (mask == 7U) {
        const shakti_sensory_frame_t *frame = &group->parts[0];
        NEED(frame->bytes == SHAKTI_PRENATAL_FRAME_BYTES && frame->width == 1920U &&
            frame->height == 1080U && frame->channels == 3U && frame->sample_bytes == 1U &&
            frame->position64 == position,"whole full-HD RGB frame geometry");
        for (i = 0U; i < frame->bytes; ++i) {
            unsigned char expected = color & (1U << (i % 3U)) ? 255U : 0U;
            NEED(frame->data[i] == expected,"every rebuilt RGB pixel matches the solid color");
        }
        ++pixels_checked;
    } else NEED(!group->parts[0].data && !group->parts[0].bytes,"no light before its own phase");
    for (lane = 1U; lane < 3U; ++lane) {
        const shakti_sensory_frame_t *frame = &group->parts[lane];
        size_t offset = (size_t)(frame->position64 % 64U) * 250U;
        NEED(frame->position64 == position + lane - 1U && frame->sample_rate == 16000U &&
            frame->sample_bytes == 2U && frame->channels == 2U && frame->bytes == 1000U,
            "two consecutive hearing microclips each carry voice and heartbeat");
        for (i = 0U; i < 250U; ++i) {
            size_t at = plan.voice_sample + offset + i;
            unsigned char low = 0U, high = 0U;
            if (plan.phase && at < original[color].sample_count) {
                low = original[color].samples[at*2U]; high = original[color].samples[at*2U+1U];
            }
            NEED(frame->data[i*4U] == low && frame->data[i*4U+1U] == high,
                "recorded voice exactly preserved including its second-beat tail");
            NEED(!memcmp(frame->data+i*4U+2U,original[8].samples+(offset+i)*2U,2U),
                "prepared quiet heartbeat preserved simultaneously without mixing");
        }
    }
    for (lane = 0U; lane < 3U; ++lane) if (mask & (1U << lane)) {
        const shakti_sensory_frame_t *frame = &group->parts[lane];
        NEED(!frame->context[0] && frame->source_id[0] == 'p' && frame->source_id[1] == '.' &&
            frame->data == held->parts[lane].rebuilt &&
            frame->data != held->parts[lane].input && frame->data != held->parts[lane].reference &&
            frame->validated_hash == shakti_media_hash(frame->data,frame->bytes),
            "actual rebuilt shared bytes; opaque identity; no word-label context");
    }
    ++windows_checked;
    return 1;
}

static int cycles(unsigned int count)
{
    unsigned int i;
    for (i = 0U; i < count; ++i) NEED(shakti_app_cycle(&state,NULL),"continuous quiet cycle");
    return 1;
}

static int views(unsigned int count)
{
    unsigned int seen = 0U, guard;
    uint64_t before = state.media.published;
    for (guard = 0U; seen < count && guard < count*2U+64U; ++guard) {
        NEED(shakti_app_cycle(&state,NULL),"real scheduled source processing");
        if (state.media.published != before) {
            NEED(state.media.published == before+1U && verify_view(),"one complete published source window");
            before = state.media.published; ++seen;
        }
    }
    NEED(seen == count,"every requested window actually reconstructed and validated");
    return 1;
}

static int enter_fixture(const char *directory)
{
    char marker[64];
    FILE *file;
    NEED(!chdir(directory),"existing synthetic fixture directory");
    file = fopen("RUN_ONLY.marker","rb");
    NEED(file && fgets(marker,sizeof(marker),file),"owned synthetic fixture marker");
    NEED(!fclose(file) && !strcmp(marker,"Prenatal.Flow.Run.v2 synthetic only\n"),"exact RUN marker");
    NEED(shakti_app_initialize(&state) && preserved(),"whole learned capsule and organization survive actual restart");
    return 1;
}

static int restart_run(const char *directory, const char *assets, int reject)
{
    shakti_prenatal_status_t status;
    NEED(enter_fixture(directory) && shakti_prenatal_status(&state,&status),"restart loader state");
    if (reject) {
        NEED(status.state == SHAKTI_PRENATAL_HELD && status.completed_beats == 0U &&
            strstr(status.hold_reason,"synthetic RUN checkpoint") && !state.media.submitted,
            "production refuses a synthetic boundary checkpoint without consuming it");
        NEED(cycles(64U) && preserved(),"production refusal keeps heartbeat and memories alive");
        puts("PASS production build rejects RUN-only checkpoint; continuous loop and whole memory retained");
    } else {
        NEED(status.state == SHAKTI_PRENATAL_ARMED && status.completed_beats == 0U &&
            status.replays > 0U,"physical restart re-arms heartbeat-first without crediting an injected prefix");
        NEED(load_originals(assets) && views(32U),"fresh process first beat contains heart and no voice/light");
        NEED(preserved(),"fresh process keeps complete learned context");
        puts("PASS fresh-process ordered replay; original capsule, context and organization retained");
    }
    shakti_memory_close(&state.memory);
    return 1;
}

#ifdef SHAKTI_PRENATAL_RUN_HOOKS
static int stop_loader(void)
{
    shakti_prenatal_status_t status;
    unsigned int i;
    NEED(shakti_prenatal_stop(&state),"stop training intake only");
    for (i = 0U; i < 4U; ++i) {
        NEED(shakti_prenatal_status(&state,&status),"stop status");
        if (status.state == SHAKTI_PRENATAL_OFF) break;
        NEED(cycles(1U),"drain borrowed training window");
    }
    NEED(status.state == SHAKTI_PRENATAL_OFF && preserved(),"intake stopped; memories unmodified");
    while (state.loop.cycle_count % 64U) NEED(cycles(1U),"align by real quiet cycles without a second clock");
    return 1;
}

static int start_at(const char *assets, const char *profile, uint64_t completed)
{
    NEED(shakti_prenatal_start(&state,assets,profile) &&
        shakti_prenatal_run_position(&state,completed),"explicit synthetic prefix; no production advancement API");
    return 1;
}

static int make_fixture(const char *directory)
{
    shakti_tick_t tick;
    NEED(!mkdir(directory,0700) && !chdir(directory),"exclusively create a fresh synthetic fixture");
    NEED(!mkdir("data",0700) && !mkdir("data/eden",0700) && !mkdir("data/learned",0700) &&
        !mkdir("data/control",0700) && !mkdir("data/school",0700),"fixed fixture subdirectories");
    NEED(write_text("RUN_ONLY.marker","Prenatal.Flow.Run.v2 synthetic only\n") &&
        write_text(SHAKTI_EDEN_FACTS_PATH,"fixture identity|synthetic|identity|RUN-ONLY\n") &&
        write_text(SHAKTI_EDEN_THESAURUS_PATH,"") && write_text(SHAKTI_EVIDENCE_PATH,"") &&
        write_text(SHAKTI_SCHOOL_PATH,"") && write_text(SHAKTI_LEARNED_STREAM_PATH,"") &&
        write_text(SHAKTI_GOAL_PATH,"Native integration fixture.") &&
        write_text(SHAKTI_NOTEBOOK_PATH,"Synthetic data only.") &&
        write_text(SHAKTI_MENU_PATH,"[Core]\nvalidate\nprenatal status\n") &&
        write_text(SHAKTI_EDEN_MANIFEST_PATH,"<manifest schema=\"SHAKTI_EDEN_MANIFEST_V1\">\n<status>OPEN</status>\n<artifact_root>artifacts</artifact_root>\n<level order=\"1\">\n<id>fixture</id>\n<title>Synthetic only</title>\n<requires>none</requires>\n</level>\n</manifest>\n"),"explicit fixture data");
    NEED(shakti_app_initialize(&state),"initialize native runtime");
    shakti_tick_clock_seed(&state.clock,(time_t)2140000000L);
    NEED(shakti_tick_next(&state.clock,&tick) &&
        shakti_memory_store(&state.memory,&tick,"context-capsule","baseball ball bat context",story,1) &&
        shakti_memory_organize(&state.memory,"context-capsule","whole experience","playground/baseball"),
        "complete context container under learner-chosen organization");
    return 1;
}

static int profile_fixture(const char *path, unsigned int width, unsigned int density)
{
    FILE *file = fopen(path,"wb");
    unsigned int i;
    int ok;
    if (!file) return 0;
    ok = fprintf(file,"<prenatal_loader>\n<width>%u</width><height>1080</height>\n"
        "<pulse_on_numerator>2</pulse_on_numerator><pulse_on_denominator>3</pulse_on_denominator>\n"
        "<voice_to_pulse_ratio>4</voice_to_pulse_ratio><monitor_enabled>0</monitor_enabled>\n<density>",width) > 0;
    for (i = 0U; i < 32U; ++i) if (fprintf(file,"%u%s",density,i==31U?"":" ") < 0) ok = 0;
    if (fputs("</density>\n</prenatal_loader>\n",file) < 0) ok = 0;
    return fclose(file) == 0 && ok;
}

static int full_run(const char *directory, const char *assets, const char *profile)
{
    shakti_prenatal_status_t status, before;
    shakti_prenatal_plan_t plan;
    shakti_media_frame_t foreign;
    unsigned long cycle_count;
    uint64_t published, used, corpus;
    char command[SHAKTI_INPUT_CAPACITY];
    unsigned int i;
    NEED(make_fixture(directory) && load_originals(assets),"native fixture setup");
    NEED(!shakti_prenatal_plan(0U,&plan) && shakti_prenatal_plan(1U,&plan) && plan.phase==0U &&
        shakti_prenatal_plan(SHAKTI_PRENATAL_SOUND_BEAT-1U,&plan) && plan.phase==0U &&
        shakti_prenatal_plan(SHAKTI_PRENATAL_SOUND_BEAT,&plan) && plan.phase==1U && !plan.phase_beat &&
        shakti_prenatal_plan(SHAKTI_PRENATAL_LIGHT_BEAT-1U,&plan) && plan.phase==1U &&
        shakti_prenatal_plan(SHAKTI_PRENATAL_LIGHT_BEAT,&plan) && plan.phase==2U && !plan.phase_beat &&
        shakti_prenatal_plan(SHAKTI_PRENATAL_END_BEAT,&plan) && plan.phase==2U &&
        !shakti_prenatal_plan(SHAKTI_PRENATAL_END_BEAT+1U,&plan),"exact staged timeline boundaries");
    NEED((SHAKTI_PRENATAL_END_BEAT-SHAKTI_PRENATAL_LIGHT_BEAT+1U)/8U==982800U &&
        UINT64_C(982800)*32U==31449600U,"analytical capacity only; no imaginary delivered prefix");
    puts("PASS exact phase boundaries and balanced 982800 beats / 31449600 full frames per color");
    NEED(profile_fixture("oversized.xml",1921U,4U) &&
        !shakti_prenatal_start(&state,assets,"oversized.xml") &&
        profile_fixture("pattern.xml",1920U,3U) &&
        !shakti_prenatal_start(&state,assets,"pattern.xml") &&
        !shakti_prenatal_start(&state,"missing-assets",profile),"oversize, patterned and missing corpus refuse whole");
    NEED(snprintf(command,sizeof(command),"/shakti_run/ prenatal start %s | %s",assets,profile)>0 &&
        shakti_app_cycle(&state,command) && state.loop.tool_calls==1UL,"actual MCP start counted once");
    NEED(views(64U) && preserved(),"heartbeat-only input for two genuinely processed beats");
    memset(&foreign,0,sizeof(foreign)); memcpy(foreign.source_id,"foreign",8U);
    NEED(!shakti_app_queue_frame(&state,&foreign),"foreign intake cannot overwrite borrowed training buffers");
    NEED(shakti_app_cycle(&state,"/prenatal/pause/") && cycles(3U) &&
        shakti_prenatal_status(&state,&status) && status.state==SHAKTI_PRENATAL_PAUSED,
        "loader pause drains its window without stopping the loop");
    before=status; cycle_count=state.loop.cycle_count; used=state.memory.current_slot;
    state.loop.reflection_due=1U; shakti_loop_interrupt_tools(&state.loop);
    NEED(cycles(128U) && state.loop.cycle_count==cycle_count+128UL &&
        state.memory.current_slot==used+128U && preserved() &&
        shakti_prenatal_status(&state,&status) && status.completed_beats==before.completed_beats,
        "heartbeat, reasoning cycle and memory aging continue while intake and tools are paused");
    NEED(shakti_app_cycle(&state,"/prenatal/resume/") &&
        shakti_prenatal_status(&state,&status) && status.completed_beats==0U &&
        status.run_serial==before.run_serial+1U && state.loop.cycle_count>cycle_count &&
        views(1U) && preserved(),"live ordered replay; no runtime initialization or arena clearing");
    puts("PASS heartbeat-only input; MCP ownership; live pause/replay keeps clock, memory and context running");
    NEED(stop_loader() && shakti_loop_set_heartbeat(&state.loop,64U) &&
        start_at(assets,profile,SHAKTI_PRENATAL_SOUND_BEAT-1U) && views(512U),
        "sixteen real sound beats with every complete recorded word");
    NEED(state.loop.heartbeat_rate==1U && cycles(1U) && state.loop.heartbeat_rate==64U,
        "sound opening slows shared heart, then returns to requested rate");
    puts("PASS sound only: all eight original words and long tails; 64 paired-channel microclips per beat");
    NEED(stop_loader() && start_at(assets,profile,SHAKTI_PRENATAL_LIGHT_BEAT-1U) && views(1U) &&
        shakti_loop_set_heartbeat(&state.loop,32U) && state.loop.heartbeat_rate==1U &&
        views(511U) && cycles(1U) && state.loop.heartbeat_rate==32U,
        "512 actual full-HD frames; sixteen color beats and warm pace request retained");
    NEED(shakti_prenatal_status(&state,&status) && status.window==0U &&
        status.completed_beats==SHAKTI_PRENATAL_LIGHT_BEAT-1U+16U &&
        status.acknowledged_windows==512U && status.reserved_bytes < 26U*1024U*1024U &&
        status.requested_rate==32U && status.monitor_enabled==0U && preserved(),
        "acknowledgments reflect real windows; fixed private bank; movie arena untouched");
    puts("PASS full-size sight and simultaneous voice/heart: complete pixels, 32:64 ratio, private fixed banks");
    NEED(stop_loader() && start_at(assets,profile,SHAKTI_PRENATAL_LIGHT_BEAT-1U) && cycles(1U) &&
        state.media.active<SHAKTI_MEDIA_GROUPS,"real pending binary exposure");
    state.media.pending[state.media.active].parts[2].binary[0]^=1U;
    NEED(cycles(2U) && !shakti_media_perception(&state.media) &&
        shakti_prenatal_status(&state,&status) && status.state==SHAKTI_PRENATAL_HELD &&
        status.completed_beats==SHAKTI_PRENATAL_LIGHT_BEAT-1U && status.acknowledged_windows==0U,
        "damaged second audio is held with no sensory publication or training credit");
    cycle_count=state.loop.cycle_count;
    NEED(shakti_prenatal_resume(&state) && views(1U) && state.loop.cycle_count>cycle_count &&
        shakti_prenatal_status(&state,&status) && status.completed_beats==0U && preserved(),
        "validation hold replays heart-first in the same live memory instance");
    shakti_prenatal_cycle_result(&state,0);
    NEED(cycles(1U) && shakti_prenatal_status(&state,&status) &&
        status.state==SHAKTI_PRENATAL_HELD && status.acknowledged_windows==0U && !state.media.current_ready,
        "failed cycle discards publication without advancing acknowledged progress");
    puts("PASS binary corruption and failed-cycle refusal; live replay retains learned containers");
    NEED(stop_loader() && start_at(assets,profile,0U) && cycles(1U),"memory fault fixture with live intake");
    state.memory.fault=1U; /* Synthetic refusal only; never edit a real capsule. */
    NEED(cycles(1U) && shakti_prenatal_status(&state,&status) &&
        status.state==SHAKTI_PRENATAL_HELD && !status.acknowledged_windows,
        "memory integrity failure prevents training credit");
    state.memory.fault=0U;
    NEED(cycles(1U) && preserved(),"fault cleanup releases only the loader window");
    puts("PASS memory integrity fault holds training while heartbeat continues");
    NEED(stop_loader() && profile_fixture("mutable.xml",1920U,4U) &&
        start_at(assets,"mutable.xml",0U) && shakti_prenatal_status(&state,&status),"local checkpoint corpus fixture");
    corpus=status.corpus_hash;
    {
        FILE *file=fopen("mutable.xml","ab");
        NEED(file && fputs("\n",file)>=0 && !fclose(file),"synthetic profile mutation");
    }
    NEED(shakti_prenatal_restore(&state) && shakti_prenatal_status(&state,&status) &&
        status.state==SHAKTI_PRENATAL_HELD && status.corpus_hash!=corpus &&
        !shakti_prenatal_resume(&state) && preserved(),"changed corpus held and cannot silently resume");
    NEED(stop_loader() && start_at(assets,profile,0U),"fresh start after explicit corpus refusal");
    {
        FILE *file=fopen(SHAKTI_PRENATAL_CHECKPOINT,"r+b");
        NEED(file && fputc('X',file)!=EOF && !fclose(file),"synthetic checkpoint corruption");
    }
    NEED(shakti_prenatal_restore(&state) && shakti_prenatal_status(&state,&status) &&
        status.state==SHAKTI_PRENATAL_HELD && cycles(64U) && preserved(),"whole checkpoint corruption held; loop lives");
    puts("PASS changed corpus and corrupt checkpoint refusal; memories unaffected");
    NEED(stop_loader() && start_at(assets,profile,SHAKTI_PRENATAL_END_BEAT-1U),"explicit final-beat boundary fixture");
    published=state.media.published;
    NEED(views(32U) && cycles(1U) && shakti_prenatal_status(&state,&status) &&
        status.state==SHAKTI_PRENATAL_DELIVERED && status.completed_beats==SHAKTI_PRENATAL_END_BEAT &&
        status.acknowledged_windows==32U && state.media.published==published+32U,
        "only last fixture beat processed; prefix explicitly marked synthetic, no mastery assertion");
    cycle_count=state.loop.cycle_count;
    NEED(cycles(64U) && state.loop.cycle_count==cycle_count+64UL && preserved(),
        "delivery handoff never stops continuous loop or clears memories");
    NEED(shakti_prenatal_gate(&state,&foreign),"ordinary media admission restored after training delivery");
    NEED(state.loop.tool_calls==1UL,"no fake tool calls or fabricated self-reflection answers from exposures");
    for(i=0U;i<SHAKTI_REFLECTION_QUESTIONS;++i)
        NEED(!state.loop.reflection_answers[i][0],"no injected production reflection answers");
    shakti_memory_close(&state.memory);
    printf("PASS native windows=%lu full_HD_frames=%lu; synthetic boundary prefixes only; no learned-mastery claim\n",
        windows_checked,pixels_checked);
    return 1;
}
#endif

int main(int argc,char **argv)
{
    int ok=0;
    if(argc==5 && argv[3][0]=='/' && argv[4][0]=='/' && !strcmp(argv[1],"--run")) {
#ifdef SHAKTI_PRENATAL_RUN_HOOKS
        ok=full_run(argv[2],argv[3],argv[4]);
#else
        fputs("BENCH RUN: boundary fixtures require separate RUN-only build\n",stderr);
#endif
    } else if(argc==4 && argv[3][0]=='/' && !strcmp(argv[1],"--restart"))
        ok=restart_run(argv[2],argv[3],0);
    else if(argc==4 && argv[3][0]=='/' && !strcmp(argv[1],"--reject"))
        ok=restart_run(argv[2],argv[3],1);
    else fputs("Use --run NEW_DIRECTORY ABSOLUTE_ASSETS ABSOLUTE_PROFILE; or --restart/--reject RUN_DIRECTORY ABSOLUTE_ASSETS\n",stderr);
    return ok?0:1;
}
