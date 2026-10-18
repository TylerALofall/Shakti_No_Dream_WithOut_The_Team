#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "shakti_app.h"
#include "shakti_memory.h"
#include "shakti_loop.h"
#include "shakti_time.h"
#define REQUIRE(c,m) do { if (!(c)) { fprintf(stderr,"BENCH RUN: %s\n",m); return 1; } } while (0)
#define RGB_BYTES (1920U*1080U*3U)
static shakti_runtime_t runtime;
static shakti_memory_item_t capsule;
static unsigned char pixels[4][RGB_BYTES], audio[2][4][1504U];
static shakti_media_frame_t frame;

static int create(const char *path, const char *text)
{
    FILE *file = fopen(path,"wb");
    int ok;
    if (!file) return 0;
    ok = fputs(text,file) >= 0; return fclose(file) == 0 && ok;
}

/* A clearly synthetic compiled callback exercises the provider contract.
 * It is not production reasoning, speech recognition, or proof of learning. */
static int reflection(void *context, unsigned int question, const char *prompt, char *answer, size_t capacity)
{
    const shakti_runtime_t *state = context;
    int n;
    if (!prompt || !state) return 0;
    if (question == 13U) n = snprintf(answer,capacity,"movie-capsule");
    else if (question == 12U) n = snprintf(answer,capacity,"blue fixture timeline");
    else n = snprintf(answer,capacity,"Synthetic review %lu question %u; actual calls=%lu; pending=%lu.",
        state->loop.reflection_count + 1UL,question,state->loop.tool_calls,
        (unsigned long)shakti_memory_pending_reviews(&state->memory));
    return n > 0 && (size_t)n < capacity;
}

static int deposit(unsigned int window)
{
    unsigned int lane;
    size_t i;
    memset(&frame,0,sizeof(frame)); memcpy(frame.source_id,"opaque-movie-1",sizeof("opaque-movie-1"));
    frame.required_mask = 7U; frame.sample_bytes = 1U; frame.channels = 3U;
    frame.width = 1920U; frame.height = 1080U; frame.position64 = (uint64_t)window*2U;
    frame.bytes = frame.reference_capacity = frame.binary_capacity = frame.rebuilt_capacity = RGB_BYTES;
    frame.input = pixels[0]; frame.reference = pixels[1]; frame.binary = pixels[2]; frame.rebuilt = pixels[3];
    if (!shakti_app_queue_frame(&runtime,&frame)) return 0;
    for (lane=1U;lane<=2U;++lane) {
        for (i=0U;i<1500U;++i) audio[lane-1U][0][i]=(unsigned char)(i*31U+window*47U+lane*19U+i/7U);
        frame.lane=lane;frame.width=frame.height=0U;frame.channels=1U;frame.sample_bytes=2U;frame.sample_rate=48000U;
        frame.position64=(uint64_t)window*2U+lane-1U;frame.bytes=1500U;
        frame.reference_capacity=frame.binary_capacity=frame.rebuilt_capacity=1504U;
        frame.input=audio[lane-1U][0];frame.reference=audio[lane-1U][1];frame.binary=audio[lane-1U][2];frame.rebuilt=audio[lane-1U][3];
        if (!shakti_app_queue_frame(&runtime,&frame)) return 0;
    }
    return 1;
}

int main(int argc,char **argv)
{
    unsigned int i,window,rate;
    uint64_t end64;
    unsigned long cycles;
    int restarted=0;
    shakti_tick_t tick;
    shakti_convergence_view_t view;
    REQUIRE(argc==3 && !strcmp(argv[1],"--run"),"use --run NEW_DIRECTORY");
    REQUIRE(!mkdir(argv[2],0700)&&!chdir(argv[2]),"new fixture directory");
    REQUIRE(!mkdir("data",0700)&&!mkdir("data/eden",0700)&&!mkdir("data/learned",0700)&&
        !mkdir("data/control",0700)&&!mkdir("data/school",0700),"fixture subdirectories");
    REQUIRE(create(SHAKTI_EDEN_FACTS_PATH,"fixture identity|synthetic|identity|RUN-ONLY\n")&&
        create(SHAKTI_EDEN_THESAURUS_PATH,"")&&create(SHAKTI_EVIDENCE_PATH,"")&&create(SHAKTI_SCHOOL_PATH,"")&&
        create(SHAKTI_LEARNED_STREAM_PATH,"")&&create(SHAKTI_GOAL_PATH,"Native integration fixture.")&&
        create(SHAKTI_NOTEBOOK_PATH,"Synthetic data only.")&&create(SHAKTI_MENU_PATH,"[Core]\nvalidate\n")&&
        create(SHAKTI_EDEN_MANIFEST_PATH,"<manifest schema=\"SHAKTI_EDEN_MANIFEST_V1\">\n<status>OPEN</status>\n<artifact_root>artifacts</artifact_root>\n<level order=\"1\">\n<id>fixture</id>\n<title>Synthetic only</title>\n<requires>none</requires>\n</level>\n</manifest>\n"),"fixture contents");
    REQUIRE(shakti_app_initialize(&runtime),"initialize native runtime");
    shakti_tick_clock_seed(&runtime.clock,(time_t)2140000000L);
    REQUIRE(shakti_retention_budget(65536U),"small RUN budget, full static arena reserved");
    for (i=0U;i<RGB_BYTES;++i) pixels[0][i]=i%3U==2U?255U:0U;
    for (window=0U;window<32U;++window) {
        REQUIRE(deposit(window),"MCP loading-zone deposit");
        REQUIRE(shakti_app_cycle(&runtime,NULL)&&shakti_app_cycle(&runtime,NULL),"synchronized native cycles");
        REQUIRE(shakti_app_convergence_view(&runtime,&view)&&view.sensory&&
            view.sensory->parts[0].data==pixels[3]&&view.eden==&runtime.reason,"rebuilt shared pixels and Eden at convergence");
        REQUIRE(view.retained_media&&view.retained_media->windows_seen==window+1U,"native short-term capture");
        shakti_media_release(&runtime.media);
        if (view.retained_media->blocked) break;
    }
    REQUIRE(window<32U && shakti_retention_audio_pending(),"pinned speech applies intake backpressure");
    end64=shakti_retention_view()->latest.position64+1U;
    REQUIRE(shakti_retention_read(0U,0U,pixels[0],RGB_BYTES),"beginning retained while waiting for transcript");
    REQUIRE(shakti_app_transcribe(&runtime,0U,end64,"RUN-only supplied transcript; this fixture does not perform ASR."),"actual transcript-input contract");
    REQUIRE(!shakti_retention_audio_pending()&&shakti_retention_accepts("opaque-movie-1"),"text releases pinned audio pressure");
    REQUIRE(shakti_tick_next(&runtime.clock,&tick)&&shakti_memory_policy(&runtime.memory,20U,600UL)&&
        shakti_memory_store(&runtime.memory,&tick,"movie-capsule","blue fixture timeline","Observed synthetic blue frames and a synchronized fixture audio timeline.",0)&&
        shakti_memory_reference(&runtime.memory,"movie-capsule","opaque-movie-1",0U,end64)&&
        shakti_memory_organize(&runtime.memory,"movie-capsule","shared concepts and exceptions","movie/fixture"),"whole semantic capsule with source coverage");
    REQUIRE(!shakti_app_release_artifact(&runtime,"movie-capsule"),"cannot release video before five actual review completions");
    shakti_loop_set_reflection_provider(&runtime.loop,reflection,&runtime);
    for (cycles=0UL;cycles<10000UL;++cycles) {
        REQUIRE(shakti_app_cycle(&runtime,NULL),"quiet heartbeat and timed review");
        if (!restarted && runtime.loop.reflection_count==2UL && runtime.loop.reflection_due && runtime.loop.reflection_answered_mask) {
            unsigned int mask=runtime.loop.reflection_answered_mask;
            shakti_memory_close(&runtime.memory);
            REQUIRE(shakti_app_initialize(&runtime)&&runtime.loop.reflection_due&&
                runtime.loop.reflection_answered_mask==mask&&runtime.loop.reflection_count==2UL,"restart preserves pending timed answers without tool-count fiction");
            REQUIRE(shakti_retention_transcript(0U,NULL)&&!shakti_retention_audio_pending(),"real transcript bytes survive native journal restart");
            shakti_loop_set_reflection_provider(&runtime.loop,reflection,&runtime);restarted=1;
        }
        REQUIRE(shakti_memory_fetch(&runtime.memory,"movie-capsule",&capsule)==1,"whole pending capsule always retrievable");
        if (capsule.archived) break;
    }
    REQUIRE(restarted&&cycles<10000UL&&capsule.reflection_count==5U&&runtime.loop.tool_calls==0UL,
        "five distinct timed reflections consolidate with zero invented tool calls");
    REQUIRE(shakti_app_release_artifact(&runtime,"movie-capsule"),"reviewed whole-source capsule permits media release");
    REQUIRE(shakti_memory_fetch(&runtime.memory,"movie-capsule",&capsule)==1&&capsule.archived&&
        !strcmp(capsule.media_source_id,"opaque-movie-1"),"epoch capsule remains after raw media release");
    for (rate=1U;rate<=64U;rate*=2U) {
        unsigned long delay=0UL,samples=0UL;
        REQUIRE(shakti_loop_set_heartbeat(&runtime.loop,rate),"set shared pace");
        for (i=0U;i<64U*rate;++i) {
            delay+=shakti_loop_delay_us(&runtime.loop);
            REQUIRE(shakti_app_cycle(&runtime,NULL),"quiet paced cycle");
            samples+=(unsigned long)runtime.loop.heartbeat_samples;
        }
        REQUIRE(delay==1000000UL&&samples==48000UL,"exact logical pacing and 48kHz audio at each rate");
    }
    shakti_loop_set_reflection_provider(&runtime.loop,NULL,NULL);
    for(i=0U;i<10U;++i) REQUIRE(shakti_app_cycle(&runtime,"/shakti_run/ validate"),"actual dispatched tool");
    REQUIRE(runtime.loop.tool_calls==10UL&&runtime.loop.reflection_due,"ten actual calls still require reflection");
    REQUIRE(shakti_app_cycle(&runtime,"/shakti_run/ validate")&&runtime.loop.tool_calls==10UL,"pending answers hold eleventh call");
    shakti_loop_set_reflection_provider(&runtime.loop,reflection,&runtime);
    for(i=0U;i<16U;++i) REQUIRE(shakti_app_cycle(&runtime,NULL),"thirteen actual fixture answers");
    REQUIRE(runtime.loop.reflection_count==6UL&&!runtime.loop.reflection_due,"tool review after timed reviews completes");
    shakti_memory_close(&runtime.memory);
    REQUIRE(shakti_app_initialize(&runtime)&&runtime.loop.reflection_count==6UL&&runtime.loop.tool_calls==10UL&&
        !runtime.loop.turns_since_reflection,"restart restores mixed timed/tool reflection chronology");
    shakti_memory_close(&runtime.memory);
    puts("PASS full-HD MCP convergence; pinned PCM/transcript recovery; five timed reviews; checked release; shared pacing; tool gates; restart");
    return 0;
}
