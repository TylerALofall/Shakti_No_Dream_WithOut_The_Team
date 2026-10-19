#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include "shakti_senses.h"
#include "shakti_handwriting.h"
#include "shakti_memory.h"
#include "shakti_loop.h"
#include "shakti_time.h"

#define IMAGE_CAP (1920U * 1080U * 3U)
#define FILE_CAP (IMAGE_CAP + 8192U)
#define WAVE_CAP (1024U * 1024U)
#define ATLAS_CAP (1024U * 1024U)
#define AUDIO_CAP 96008U
#define NEED(c,m) do { if (!(c)) { fprintf(stderr,"BENCH senses: %s\n",m); return 1; } } while (0)

static shakti_runtime_t runtime;
static unsigned char input_file[FILE_CAP], rgb[IMAGE_CAP], atlas_file[ATLAS_CAP], atlas_rgb[ATLAS_CAP];
static unsigned char wave_file[WAVE_CAP], visual[2][3][IMAGE_CAP], audio[2][2][4][AUDIO_CAP];
static unsigned char sheet[768U * 384U * 3U];
static unsigned char sample_check[AUDIO_CAP], unknown_cell[128U * 128U * 3U];
static unsigned char rebuilt_pcm[WAVE_CAP + 2U * AUDIO_CAP];
static shakti_ocr_result_t ocr, other_ocr;
static shakti_memory_item_t fetched;
static char capsule[SHAKTI_MEMORY_CONTENT_BYTES];
static const char alphabet[] = " 0123456789+=ABC";
static const unsigned char palette[8][3] = {
    {0,0,0},{255,0,0},{0,255,0},{255,255,0},
    {0,0,255},{255,0,255},{0,255,255},{255,255,255}
};
static const char *const color_names[8] = {"black","red","green","yellow","blue","magenta","cyan","white"};

static int create(const char *path, const char *text)
{
    FILE *f = fopen(path,"wb");
    int ok;
    if (!f) return 0;
    ok = fputs(text,f) >= 0;
    if (fclose(f)) ok = 0;
    return ok;
}

static int read_whole(const char *path, unsigned char *out, size_t capacity, size_t *bytes)
{
    FILE *f = fopen(path,"rb");
    size_t n;
    int extra, bad;
    if (!f) return 0;
    n = fread(out,1U,capacity,f); extra = fgetc(f); bad = ferror(f);
    if (fclose(f)) bad = 1;
    if (extra != EOF || bad) return 0;
    *bytes = n; return 1;
}

static void put_le(unsigned char *p, uint32_t n, size_t size)
{
    while (size--) { *p++ = (unsigned char)n; n >>= 8; }
}

/* Self-contained PNG output, RGB8 with uncompressed DEFLATE blocks.
 * No metadata, embedded instructions, external assets or library calls. */
static uint32_t crc_byte(uint32_t crc, unsigned int byte)
{
    unsigned int i;
    crc ^= byte;
    for (i=0U;i<8U;++i) crc = (crc >> 1) ^ ((crc & 1U) ? UINT32_C(0xedb88320) : 0U);
    return crc;
}
static int be32(FILE *f, uint32_t n)
{
    return fputc((int)(n >> 24),f)!=EOF && fputc((int)((n >> 16)&255U),f)!=EOF &&
        fputc((int)((n >> 8)&255U),f)!=EOF && fputc((int)(n&255U),f)!=EOF;
}
static int chunk(FILE *f, const char *type, const unsigned char *data, size_t bytes)
{
    uint32_t crc=UINT32_MAX;
    size_t i;
    if (bytes>UINT32_MAX || !be32(f,(uint32_t)bytes) || fwrite(type,1U,4U,f)!=4U) return 0;
    for (i=0U;i<4U;++i) crc=crc_byte(crc,(unsigned char)type[i]);
    for (i=0U;i<bytes;++i) { if (fputc(data[i],f)==EOF) return 0; crc=crc_byte(crc,data[i]); }
    return be32(f,~crc);
}
static int idat_byte(FILE *f, unsigned int byte, uint32_t *crc)
{
    if (fputc((int)byte,f)==EOF) return 0;
    *crc=crc_byte(*crc,byte); return 1;
}
static int png(const char *path, const unsigned char *pixels, unsigned int width, unsigned int height)
{
    static const unsigned char signature[8]={137,80,78,71,13,10,26,10};
    unsigned char ihdr[13]={0};
    FILE *f;
    uint32_t crc=UINT32_MAX, a=1U,b=0U;
    size_t stride=(size_t)width*3U+1U, total=stride*height, offset=0U, i;
    int ok=1;
    if (!width || !height || total>UINT32_MAX-65536U) return 0;
    f=fopen(path,"wb"); if (!f) return 0;
    ihdr[0]=(unsigned char)(width>>24);ihdr[1]=(unsigned char)(width>>16);
    ihdr[2]=(unsigned char)(width>>8);ihdr[3]=(unsigned char)width;
    ihdr[4]=(unsigned char)(height>>24);ihdr[5]=(unsigned char)(height>>16);
    ihdr[6]=(unsigned char)(height>>8);ihdr[7]=(unsigned char)height;ihdr[8]=8U;ihdr[9]=2U;
    if (fwrite(signature,1U,8U,f)!=8U || !chunk(f,"IHDR",ihdr,sizeof(ihdr)) ||
        !be32(f,(uint32_t)(2U+total+5U*((total+65534U)/65535U)+4U)) ||
        fwrite("IDAT",1U,4U,f)!=4U) ok=0;
    for(i=0U;i<4U;++i) crc=crc_byte(crc,(unsigned char)"IDAT"[i]);
    if(ok) ok=idat_byte(f,0x78U,&crc)&&idat_byte(f,0x01U,&crc);
    while(ok && offset<total) {
        size_t n=total-offset;unsigned int len,inv;
        if(n>65535U)n=65535U;
        len=(unsigned int)n;inv=len^65535U;
        ok=idat_byte(f,offset+n==total?1U:0U,&crc)&&idat_byte(f,len&255U,&crc)&&
            idat_byte(f,len>>8,&crc)&&idat_byte(f,inv&255U,&crc)&&idat_byte(f,inv>>8,&crc);
        for(i=0U;ok&&i<n;++i,++offset) {
            size_t row=offset/stride,col=offset%stride;
            unsigned int byte=col?pixels[row*(stride-1U)+col-1U]:0U;
            a=(a+byte)%65521U;b=(b+a)%65521U;ok=idat_byte(f,byte,&crc);
        }
    }
    for(i=0U;ok&&i<4U;++i) ok=idat_byte(f,((b<<16)|a)>>(24U-8U*(unsigned int)i)&255U,&crc);
    if(ok)ok=be32(f,~crc)&&chunk(f,"IEND",NULL,0U)&&!fflush(f);
    if(fclose(f))ok=0;
    if(!ok)(void)remove(path);
    return ok;
}

static int ppm(const char *path, const unsigned char *pixels, unsigned int width, unsigned int height)
{
    FILE *f=fopen(path,"wb");
    size_t bytes=(size_t)width*height*3U;
    int ok;
    if(!f)return 0;
    ok=fprintf(f,"P6\n%u %u\n255\n",width,height)>0&&fwrite(pixels,1U,bytes,f)==bytes;
    if(fclose(f))ok=0;
    return ok;
}

static int wav(const char *path, const unsigned char *pcm, size_t bytes, unsigned int rate, unsigned int channels)
{
    unsigned char header[44]={0};
    FILE *f;int ok;
    if(bytes>UINT32_MAX-36U)return 0;
    memcpy(header,"RIFF",4U);put_le(header+4U,(uint32_t)bytes+36U,4U);memcpy(header+8U,"WAVEfmt ",8U);
    put_le(header+16U,16U,4U);put_le(header+20U,1U,2U);put_le(header+22U,channels,2U);
    put_le(header+24U,rate,4U);put_le(header+28U,rate*channels*2U,4U);put_le(header+32U,channels*2U,2U);
    put_le(header+34U,16U,2U);memcpy(header+36U,"data",4U);put_le(header+40U,(uint32_t)bytes,4U);
    f=fopen(path,"wb");if(!f)return 0;
    ok=fwrite(header,1U,sizeof(header),f)==sizeof(header)&&fwrite(pcm,1U,bytes,f)==bytes;
    if(fclose(f))ok=0;
    return ok;
}

static void stamp(unsigned char *pixels,unsigned int width,unsigned int x,unsigned int y,unsigned char character)
{
    char rows[8][9];
    unsigned int sx,sy,dx,dy;
    if(!shakti_handwriting_render_char(character,rows))return;
    for(sy=0U;sy<8U;++sy)for(sx=0U;sx<8U;++sx)
        for(dy=0U;dy<4U;++dy)for(dx=0U;dx<3U;++dx) {
            size_t at=((size_t)(y+sy*4U+dy)*width+x+sx*3U+dx)*3U;
            unsigned char value=rows[sy][sx]=='#'?0U:255U;
            pixels[at]=pixels[at+1U]=pixels[at+2U]=value;
        }
}

static int fixture(const char *directory)
{
    size_t i;
    const char *lines[3]={"ABC","2+2=4","123"};
    unsigned int row;
    NEED(!mkdir(directory,0700)&&!chdir(directory),"new fixture input directory");
    memset(rgb,255U,384U*128U*3U);
    for(row=0U;row<3U;++row)for(i=0U;lines[row][i];++i)
        stamp(rgb,384U,(unsigned int)i*24U,row*32U,(unsigned char)lines[row][i]);
    NEED(ppm("document.ppm",rgb,384U,128U)&&png("document.png",rgb,384U,128U),"fixture document pixels");
    memset(atlas_rgb,255U,sizeof(alphabet)*24U*32U*3U);
    for(i=0U;i<strlen(alphabet);++i)stamp(atlas_rgb,(unsigned int)strlen(alphabet)*24U,(unsigned int)i*24U,0U,(unsigned char)alphabet[i]);
    NEED(ppm("atlas.ppm",atlas_rgb,(unsigned int)strlen(alphabet)*24U,32U),"explicit full-resolution 24x32 glyph atlas");
    for(i=0U;i<44100U;++i)put_le(wave_file+i*2U,(uint16_t)(i*7919U+i/3U),2U);
    NEED(wav("sound.wav",wave_file,88200U,44100U,1U),"synthetic PCM source");
    NEED(create("Fixture.txt","SYNTHETIC INPUTS ONLY\nDocument font cells are 24x32, rendered using existing handwriting fixture glyphs.\nOCR compares the full 24x32 cells without rescaling the input.\nAlphabet:  0123456789+=ABC\nExpected text, for RUN comparison only: ABC / 2+2=4 / 123\nAudio is synthetic 44100 Hz PCM16, not a voice or speech-recognition fixture.\n"),"fixture declaration");
    puts("PASS fixture inputs created; inspect before running.");
    return 0;
}

static int make_runtime(const char *directory)
{
    NEED(!mkdir(directory,0700)&&!chdir(directory),"new output directory; never use production data");
    NEED(!mkdir("data",0700)&&!mkdir("data/eden",0700)&&!mkdir("data/learned",0700)&&
        !mkdir("data/control",0700)&&!mkdir("data/school",0700),"synthetic host subdirectories");
    NEED(create(SHAKTI_EDEN_FACTS_PATH,"fixture identity|synthetic|identity|RUN-ONLY\n")&&
        create(SHAKTI_EDEN_THESAURUS_PATH,"")&&create(SHAKTI_EVIDENCE_PATH,"")&&create(SHAKTI_SCHOOL_PATH,"")&&
        create(SHAKTI_LEARNED_STREAM_PATH,"")&&create(SHAKTI_GOAL_PATH,"Native senses RUN.")&&
        create(SHAKTI_NOTEBOOK_PATH,"Synthetic host; real supplied file bytes.")&&create(SHAKTI_MENU_PATH,"[Core]\nvalidate\n")&&
        create(SHAKTI_EDEN_MANIFEST_PATH,"<manifest schema=\"SHAKTI_EDEN_MANIFEST_V1\">\n<status>OPEN</status>\n<artifact_root>artifacts</artifact_root>\n<level order=\"1\">\n<id>fixture</id>\n<title>Synthetic host</title>\n<requires>none</requires>\n</level>\n</manifest>\n"),"explicit host fixture contents");
    NEED(shakti_app_initialize(&runtime),"native initialization");
    shakti_tick_clock_seed(&runtime.clock,(time_t)2140000000L);
    NEED(shakti_memory_policy(&runtime.memory,20U,64UL),"explicit RUN policy: 20 working beats, 64 idle short-term beats");
    return 0;
}

static void storage(unsigned int bank,shakti_senses_storage_t *out)
{
    unsigned int i,j;
    memset(out,0,sizeof(*out));
    for(i=0U;i<3U;++i)out->visual[i]=visual[bank][i];
    for(i=0U;i<2U;++i)for(j=0U;j<4U;++j)out->audio[i][j]=audio[bank][i][j];
    out->visual_capacity=IMAGE_CAP;out->audio_capacity=AUDIO_CAP;
}

static int snapshot(FILE *trace,FILE *planes,uint64_t window,unsigned int mask)
{
    unsigned int lane,plane;
    for(lane=0U;lane<3U;++lane)if(mask&(1U<<lane)) {
        const shakti_media_frame_t *part=NULL;
        size_t i,stride;
        for(i=0U;i<SHAKTI_MEDIA_GROUPS;++i) {
            const shakti_media_group_t *g=&runtime.media.pending[i];
            if(g->in_use&&g->prepared&&(g->required_mask&(1U<<lane))&&g->parts[lane].position64/2U==window)
                part=&g->parts[lane];
        }
        NEED(part,"complete pending prepared slice");
        stride=shakti_binary_capacity(part->bytes)/8U;
        NEED(fprintf(trace,"source=%s window=%" PRIu64 " pos64=%" PRIu64 " lane=%u format=%s bytes=%lu stage=DECOMPOSED_AND_REBUILT_UNPUBLISHED hash=%016" PRIx64 "\n",
            part->source_id,window,part->position64,lane,lane?"PCM_S16LE":"RGB8",(unsigned long)part->bytes,part->validated_hash)>0,"slice receipt");
        for(plane=0U;plane<8U;++plane) {
            long offset=ftell(planes);
            NEED(offset>=0L&&fprintf(trace,"  plane=%u offset=%ld length=%lu file=Planes.bin\n",plane,offset,(unsigned long)stride)>0,
                "tagged packed-plane offset");
            NEED(fwrite(part->binary+(size_t)plane*stride,1U,stride,planes)==stride,"whole bit plane");
        }
        /* Each plane byte packs eight consecutive source bytes, LSB first.
         * This is byte grouping, not an 8x8 spatial vision grid. */
        for(i=0U;i<part->bytes&&i<24U;++i) {
            unsigned int value=0U;
            for(plane=0U;plane<8U;++plane)
                value|=((part->binary[(size_t)plane*stride+i/8U]>>(i%8U))&1U)<<plane;
            NEED(value==part->input[i]&&value==part->rebuilt[i],"independent plane-index reconstruction");
            NEED(fprintf(trace,"  byte=%lu source=%u planes_to_byte=%u rebuilt=%u\n",
                (unsigned long)i,part->input[i],value,part->rebuilt[i])>0,"byte detail");
        }
    }
    return 0;
}

static int contracts(const shakti_pcm_wave_t *wave,const shakti_rgb_image_t *atlas,
    const char *letters,unsigned int cw,unsigned int ch)
{
    static const unsigned char gray[]={'P','5','\n','1',' ','1','\n','2','5','5','\n',32};
    static const unsigned char gray_cr[]={'P','5','\n','1',' ','1','\n','2','5','5','\r',10};
    static const unsigned char gray_crlf[]={'P','5','\n','1',' ','1','\n','2','5','5','\r','\n',10};
    static shakti_media_state_t state;
    static unsigned char visual_probe[4][8],audio_probe[2][4][8],bad_wave[44];
    shakti_rgb_image_t probe;
    shakti_media_frame_t frame;
    unsigned int pass,lane,i;
    size_t bytes,recorded,total=0U,actual=0U;
    NEED(shakti_eyes_pnm(gray,sizeof(gray),unknown_cell,sizeof(unknown_cell),&probe)&&
        probe.bytes==3U&&unknown_cell[0]==32U&&unknown_cell[1]==32U&&unknown_cell[2]==32U,
        "gray8 reshape preserves intensity and first whitespace-valued pixel");
    NEED(!shakti_eyes_pnm(gray,sizeof(gray)-1U,unknown_cell,sizeof(unknown_cell),&probe)&&unknown_cell[0]==32U,
        "truncated image refuses before output mutation");
    NEED(shakti_eyes_pnm(gray_cr,sizeof(gray_cr),unknown_cell,sizeof(unknown_cell),&probe)&&unknown_cell[0]==10U&&
        shakti_eyes_pnm(gray_crlf,sizeof(gray_crlf),unknown_cell,sizeof(unknown_cell),&probe)&&unknown_cell[0]==10U,
        "CR or CRLF header does not consume a newline-valued first pixel");
    if(wave) {
        for(i=0U;i<64U;++i) {
            NEED(shakti_ears_slice(wave,i,sample_check,sizeof(sample_check),&bytes,&recorded),"fractional audio slice");
            NEED(!recorded||!memcmp(sample_check,wave->pcm+actual*wave->channels*2U,recorded*wave->channels*2U),"no audio sample lost at fractional boundaries");
            total+=bytes;actual+=recorded;
        }
        NEED(total==(size_t)wave->rate*wave->channels*2U&&actual==(wave->frames<wave->rate?wave->frames:wave->rate),
            "all 64 slices cover one logical source second, preserving final partial samples");
    }
    memcpy(bad_wave,"RIFF",4U);put_le(bad_wave+4U,36U,4U);memcpy(bad_wave+8U,"WAVEfmt ",8U);
    put_le(bad_wave+16U,16U,4U);put_le(bad_wave+20U,3U,2U);
    { shakti_pcm_wave_t refused; NEED(!shakti_ears_wave(bad_wave,sizeof(bad_wave),&refused),"unsupported WAV encoding refuses"); }
    if(atlas&&(size_t)cw*ch>strlen(letters)) {
        size_t attempt,limit=strlen(letters);
        shakti_rgb_image_t unknown={cw,ch,unknown_cell,(size_t)cw*ch*3U};
        for(attempt=0U;attempt<=limit;++attempt) {
            memset(unknown_cell,255U,unknown.bytes);
            memset(unknown_cell+attempt*3U,0,3U);
            if(!shakti_eyes_ocr(&unknown,atlas,letters,cw,ch,&other_ocr))break;
        }
        NEED(attempt<=limit&&!other_ocr.count&&!other_ocr.text[0],"unknown glyph refuses the whole OCR result");
    }
    for(pass=0U;pass<2U;++pass) {
        shakti_media_init(&state);
        for(lane=0U;lane<3U;++lane) {
            unsigned char (*buffers)[8]=lane?audio_probe[lane-1U]:visual_probe;
            for(i=0U;i<8U;++i)buffers[0][i]=(unsigned char)(i*17U+lane);
            memset(&frame,0,sizeof(frame));memcpy(frame.source_id,"probe",6U);
            frame.lane=lane;frame.position64=lane==2U?1U:0U;frame.required_mask=7U;
            frame.retention_policy=SHAKTI_MEDIA_MOMENT;
            frame.format=lane?SHAKTI_FORMAT_PCM_S16LE:SHAKTI_FORMAT_RGB8;
            frame.width=lane?0U:2U;frame.height=lane?0U:1U;frame.channels=lane?1U:3U;
            frame.sample_bytes=lane?2U:1U;frame.sample_rate=lane?64U:0U;frame.bytes=lane?2U:6U;
            frame.input=buffers[0];frame.reference=buffers[1];frame.binary=buffers[2];frame.rebuilt=buffers[3];
            frame.reference_capacity=frame.binary_capacity=frame.rebuilt_capacity=8U;
            NEED(shakti_media_queue(&state,&frame),"probe source-window lane");
        }
        NEED(shakti_media_prepare(&state)==1&&!state.published&&!shakti_media_perception(&state),"compute ahead is not publication");
        NEED(shakti_media_step(&state,0U)==1&&!shakti_media_perception(&state),"prepared movie waits for complete two-slot heartbeat window");
        if(!pass) {
            audio_probe[1][2][0]^=1U;
            NEED(shakti_media_step(&state,1U)==-1&&!state.published&&!shakti_media_perception(&state),
                "changed prepared binary holds the entire window");
        } else {
            NEED(shakti_media_step(&state,1U)==2&&state.published==1U&&
                shakti_media_perception(&state)->checked_mask==7U,"all channels publish together after gate");
            NEED(shakti_media_step(&state,2U)==1&&state.published==1U,"leased publication cannot be credited twice");
            shakti_media_release(&state);
        }
    }
    puts("PASS refusal, PCM remainder, unknown OCR, prepare-before-gate, corruption hold and single-publication contracts.");
    return 0;
}

static int reviews(void *context,unsigned int question,const char *prompt,char *answer,size_t capacity)
{
    const shakti_runtime_t *state=context;int n;
    if(!prompt||!state)return 0;
    if(question==13U)n=snprintf(answer,capacity,"sense-document");
    else n=snprintf(answer,capacity,"SYNTHETIC RUN ANSWER review=%lu question=%u. Tests transport and consolidation, not autonomous reflection.",
        state->loop.reflection_count+1UL,question);
    return n>0&&(size_t)n<capacity;
}

static int run(int argc,char **argv)
{
    shakti_rgb_image_t image,atlas,rebuilt;
    shakti_pcm_wave_t wave;
    shakti_senses_storage_t banks[2];
    shakti_convergence_view_t view;
    size_t bytes,i,recorded,audio_bytes,rebuilt_pcm_bytes=0U;
    unsigned int cw,ch,window,mask,rate,windows=1U;
    uint64_t image_file_hash,wave_file_hash=0U,atlas_file_hash=0U;
    FILE *trace,*planes;
    char *end;
    unsigned long parsed;
    shakti_tick_t tick;
    int have_wave=strcmp(argv[4],"-")!=0,have_atlas=strcmp(argv[5],"-")!=0;
    NEED(argc==9,"--run NEW_DIRECTORY ABS_IMAGE_PNM ABS_PCM_WAV_OR_MINUS ABS_ATLAS_OR_MINUS ALPHABET CELL_WIDTH CELL_HEIGHT");
    errno=0;parsed=strtoul(argv[7],&end,10);NEED(!errno&&!*end&&parsed&&parsed<=128U,"cell width");cw=(unsigned int)parsed;
    errno=0;parsed=strtoul(argv[8],&end,10);NEED(!errno&&!*end&&parsed&&parsed<=128U,"cell height");ch=(unsigned int)parsed;
    NEED(read_whole(argv[3],input_file,sizeof(input_file),&bytes)&&
        shakti_eyes_pnm(input_file,bytes,rgb,sizeof(rgb),&image),"classify/reshape complete P5/P6 file");
    image_file_hash=shakti_media_hash(input_file,bytes);
    if(have_wave)NEED(read_whole(argv[4],wave_file,sizeof(wave_file),&bytes)&&shakti_ears_wave(wave_file,bytes,&wave),"classify PCM16 WAV");
    if(have_wave) {
        uint64_t n=((uint64_t)wave.frames*32U+wave.rate-1U)/wave.rate;
        NEED(n<=1024U,"complete item exceeds 1024-window diagnostic limit; no truncated RUN");
        windows=n?(unsigned int)n:1U;wave_file_hash=shakti_media_hash(wave_file,bytes);
    }
    if(have_atlas)NEED(read_whole(argv[5],atlas_file,sizeof(atlas_file),&bytes)&&
        shakti_eyes_pnm(atlas_file,bytes,atlas_rgb,sizeof(atlas_rgb),&atlas),"declared font atlas");
    if(have_atlas)atlas_file_hash=shakti_media_hash(atlas_file,bytes);
    NEED((uint64_t)windows*(image.bytes+(have_wave?2U*AUDIO_CAP:0U))<=64U*1024U*1024U,
        "complete plane trace exceeds fixed 64 MiB diagnostic budget; item refused");
    NEED(!contracts(have_wave?&wave:NULL,have_atlas?&atlas:NULL,argv[6],cw,ch),"bounded preflight contracts");
    NEED(!make_runtime(argv[2]),"create isolated native host");
    trace=fopen("Frames.txt","wb");NEED(trace,"frame receipt stream");
    planes=fopen("Planes.bin","wb");NEED(planes,"single packed-plane stream");
    NEED(fprintf(trace,"SENSES_ASSEMBLY_RUN_V1\nhost_epoch_anchor=%ld\nlogical_epoch_seed=2140000000 (synthetic)\ninput_format=RGB8 width=%u height=%u\nclocks=64-hearing:32-sight:1-heart\n",
        (long)time(NULL),image.width,image.height)>0,"explicit real/logical time distinction");
    NEED(fprintf(trace,"image_file_hash=%016" PRIx64 " wave_file_hash=%016" PRIx64 " atlas_file_hash=%016" PRIx64 " windows=%u\n",
        image_file_hash,wave_file_hash,atlas_file_hash,windows)>0,"input container identity");
    storage(0U,&banks[0]);storage(1U,&banks[1]);
    mask=have_wave?SHAKTI_MEDIA_MOVIE:SHAKTI_MEDIA_SIGHT;
    NEED(png("Original.png",image.rgb,image.width,image.height),"original RGB rendering");
    for(window=0U;window<windows;++window) {
        unsigned long before=runtime.loop.cycle_count;
        NEED(shakti_senses_prepare(&runtime,"source-1",window,&image,have_wave?&wave:NULL,&banks[window&1U]),"file item -> shared classified/sliced/tagged task");
        NEED(runtime.loop.cycle_count==before&&!shakti_media_perception(&runtime.media),"preparation does not advance heart or publish");
        NEED(!snapshot(trace,planes,window,mask),"inspect actual prepared planes");
        /* At x1/x4/x16 the counters and tagged source positions remain identical.
         * No sleep is used in this unpaced native qualification RUN. */
        rate=window%3U==0U?1U:(window%3U==1U?4U:16U);
        NEED(shakti_loop_set_heartbeat(&runtime.loop,rate),"shared pace control");
        for(i=0U;i<3U&&!shakti_media_perception(&runtime.media);++i)NEED(shakti_app_cycle(&runtime,NULL),"normal heart-owned cycle");
        NEED(shakti_app_convergence_view(&runtime,&view)&&view.sensory&&view.sensory->checked_mask==mask&&
            view.sensory->parts[0].data==banks[window&1U].visual[2]&&
            !memcmp(view.sensory->parts[0].data,image.rgb,image.bytes),"complete shared convergence pixels");
        rebuilt=image;rebuilt.rgb=view.sensory->parts[0].data;
        if(have_wave)for(i=0U;i<2U;++i) {
            const shakti_sensory_frame_t *part=&view.sensory->parts[i+1U];
            NEED(shakti_ears_slice(&wave,(uint64_t)window*2U+i,sample_check,AUDIO_CAP,&audio_bytes,&recorded)&&
                audio_bytes==part->bytes&&!memcmp(part->data,sample_check,audio_bytes),"sample-exact audio alignment");
            NEED(fprintf(trace,"window=%u lane=%lu recorded_sample_frames=%lu emitted_sample_frames=%lu EOF_padding_frames=%lu\n",
                window,(unsigned long)i+1UL,(unsigned long)recorded,(unsigned long)(audio_bytes/(wave.channels*2U)),
                (unsigned long)(audio_bytes/(wave.channels*2U)-recorded))>0,"explicit source-versus-EOF-padding receipt");
            NEED(audio_bytes<=sizeof(rebuilt_pcm)-rebuilt_pcm_bytes,"whole reconstructed PCM proof capacity");
            memcpy(rebuilt_pcm+rebuilt_pcm_bytes,part->data,audio_bytes);rebuilt_pcm_bytes+=audio_bytes;
        }
        NEED(fprintf(trace,"window=%u stage=HEARTBEAT_PUBLISHED rate=%u logical_slot=%u epoch=%ld:%u visual_hash=%016" PRIx64 "\n",
            window,rate,runtime.loop.heartbeat_slot,(long)view.tick.epoch_seconds,view.tick.frame,view.sensory->parts[0].validated_hash)>0,"publication receipt");
        if(have_atlas) {
            NEED(shakti_eyes_ocr(&rebuilt,&atlas,argv[6],cw,ch,&ocr),"exact supplied-font OCR from REBUILT shared pixels");
            other_ocr=ocr;other_ocr.source_hash^=1U;
            NEED(!shakti_senses_describe(view.sensory,&other_ocr,NULL,capsule,sizeof(capsule)),"OCR from a different frame cannot be attached");
            NEED(shakti_senses_describe(view.sensory,&ocr,NULL,capsule,sizeof(capsule)),"compact Shakti candidate tied to actual pixel hash");
            NEED(fprintf(trace,"window=%u stage=OCR lines=%lu characters=%lu text=[%s]\n",window,(unsigned long)ocr.lines,(unsigned long)ocr.count,ocr.text)>0,"OCR receipt");
        } else NEED(shakti_senses_describe(view.sensory,NULL,NULL,capsule,sizeof(capsule)),"explicit uninterpreted/solid descriptor");
        if(window==0U) {
            NEED(png("Rebuilt.png",rebuilt.rgb,rebuilt.width,rebuilt.height)&&create("Capsule.txt",capsule),"render actual rebuilt pixels and compact record");
            if(have_atlas) {
                FILE *f=fopen("Characters.txt","wb");NEED(f&&create("OCR.txt",ocr.text),"recognized text output");
                for(i=0U;i<ocr.count;++i) {
                    const shakti_ocr_character_t *p=&ocr.characters[i];
                    NEED(fprintf(f,"source=source-1 window=0 row=%u column=%u xy=%u,%u box=%ux%u char=%c\n",
                        p->row,p->column,p->x,p->y,p->width,p->height,p->character)>0,"character position");
                }
                NEED(!fclose(f),"character output");
                NEED(shakti_tick_next(&runtime.clock,&tick)&&shakti_memory_store(&runtime.memory,&tick,"sense-document","source-1 OCR document",capsule,0)&&
                    shakti_memory_reference(&runtime.memory,"sense-document","source-1",0U,have_wave?1U:0U),"whole compact capsule linked through existing memory API");
                NEED(shakti_memory_fetch(&runtime.memory,"sense-document",&fetched)==1&&!fetched.archived&&!fetched.reflection_count,"no fabricated library admission");
            }
        }
        shakti_media_release(&runtime.media);
    }
    NEED(!shakti_retention_view()->active,"moment mode does not automatically record a raw movie");
    NEED(!fclose(trace)&&!fclose(planes),"complete frame and plane streams");
    if(have_wave) {
        NEED(rebuilt_pcm_bytes>=wave.bytes&&!memcmp(rebuilt_pcm,wave.pcm,wave.bytes),"every original PCM source byte was reconstructed in order");
        for(i=wave.bytes;i<rebuilt_pcm_bytes;++i)NEED(!rebuilt_pcm[i],"only confirmed EOF padding follows source audio");
        NEED(wav("Rebuilt.wav",rebuilt_pcm,rebuilt_pcm_bytes,wave.rate,wave.channels),"playable whole reconstructed source PCM with declared final-slot padding");
    }
    if(have_atlas)NEED(shakti_memory_reference(&runtime.memory,"sense-document","source-1",0U,
        (uint64_t)windows*2U-(have_wave?1U:2U)),"capsule covers the complete source item");
    if(have_atlas) {
        unsigned long cycles;
        /* Deliberately supplied synthetic answers qualify the existing gate.
         * No production answer generator or learned reflection is asserted. */
        shakti_loop_set_reflection_provider(&runtime.loop,reviews,&runtime);
        for(cycles=0UL;cycles<10000UL;++cycles) {
            NEED(shakti_app_cycle(&runtime,NULL),"continuous native memory/reflection cycle");
            NEED(shakti_memory_fetch(&runtime.memory,"sense-document",&fetched)==1,"whole capsule remains available");
            if(fetched.archived)break;
        }
        NEED(cycles<10000UL&&fetched.reflection_count==5U&&runtime.loop.tool_calls==0UL,"five distinct explicit synthetic reviews; no invented tool calls");
        NEED(create("Memory.txt",fetched.text),"exact stored epoch-first capsule");
    }
    /* Independent full-size colors: eight known inputs, one reusable moment.
     * These labels are fixture metadata, never recognition or learned names. */
    for(window=0U;window<8U;++window) {
        shakti_rgb_image_t color={1920U,1080U,rgb,IMAGE_CAP};
        unsigned int x,y;
        uint64_t position=(uint64_t)windows+window;
        while(runtime.loop.heartbeat_slot&1U)NEED(shakti_app_cycle(&runtime,NULL),"align next empty source boundary");
        for(i=0U;i<IMAGE_CAP;++i)rgb[i]=palette[window][i%3U];
        NEED(shakti_senses_prepare(&runtime,"source-1",position,&color,NULL,&banks[0]),"full-HD color file-equivalent decoded input through same task adapter");
        for(i=0U;i<3U&&!shakti_media_perception(&runtime.media);++i)NEED(shakti_app_cycle(&runtime,NULL),"color heart gate");
        NEED(shakti_app_convergence_view(&runtime,&view)&&view.sensory&&
            !memcmp(view.sensory->parts[0].data,rgb,IMAGE_CAP),"all full-HD RGB bytes rebuilt");
        for(y=0U;y<192U;++y)for(x=0U;x<192U;++x) {
            size_t at=((size_t)((window/4U)*192U+y)*768U+(window%4U)*192U+x)*3U;
            memcpy(sheet+at,view.sensory->parts[0].data,3U);
        }
        printf("COLOR %s RGB=%u,%u,%u full-HD bytes=%u MATCH\n",color_names[window],
            palette[window][0],palette[window][1],palette[window][2],IMAGE_CAP);
        shakti_media_release(&runtime.media);
    }
    NEED(png("Colors-Rebuilt.png",sheet,768U,384U),"color sheet from validated shared outputs");
    shakti_memory_close(&runtime.memory);
    NEED(create("COMPLETE.txt","PASS native file intake, exact bit-plane reconstruction, source tags, heart gate, full shared convergence, supplied-font OCR when selected, compact candidate and existing five-review memory gate.\nUnpaced qualification; no claim of real-time throughput, general OCR, ASR, autonomous reflection, learning or three-clone phasing.\n"),"completion receipt");
    puts("PASS native senses assembly; inspect Original.png, Rebuilt.png, Frames.txt, OCR.txt when selected, Capsule.txt, Colors-Rebuilt.png.");
    return 0;
}

int main(int argc,char **argv)
{
    if(argc==3&&!strcmp(argv[1],"--fixture"))return fixture(argv[2]);
    if(argc==9&&!strcmp(argv[1],"--run"))return run(argc,argv);
    fputs("Usage:\n  shakti-senses --fixture NEW_INPUT_DIRECTORY\n  shakti-senses --run NEW_OUTPUT_DIRECTORY IMAGE_PNM WAV_OR_MINUS ATLAS_OR_MINUS ALPHABET CELL_WIDTH CELL_HEIGHT\nUse absolute input paths. --run creates a synthetic isolated host, never modifies production data.\n",stderr);
    return 2;
}
