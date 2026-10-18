/* SECTION-A -- Tool.Support.c v1.0
 * MODIFIED: 2026-10-03
 * QA SPECIALIST: GPT-6 Astra Pro
 * LOCATION: OpenAI Linux working container; phone run pending
 * SUPERVISOR QA: Kimi K3 (not performed; no acceptance asserted)
 * SUPERVISOR QA LOCATION: Not supplied; independent review pending
 * SUPERVISOR NOTES:
 * *****
 *
 * *****
 * INSTRUCTIONS: Compile with Tool.Submit.c, never as a separate program.
 * This unit owns the bounded buffers, codec, form, snapshot and Git schema.
 * Implementation headers below are source, not external dependencies.
 * File writes require exclusive-create mode "x" and pass a runtime probe.
 * This stdio mode is a runtime extension to C99, not silently emulated.
 * Only local generated outputs are written. Original inputs stay read-only.
 */
#include "Tool.Submit.h"
SubmitPlan ts;
char ts_pool[TS_POOL_BYTES+1], ts_input[TS_BUFFER+1], ts_output[TS_BUFFER+1];
size_t ts_used, ts_length;
SubmitJson ts_json[TS_TOKENS];
void ts_die(const char *message)
{
    fprintf(stderr,"BENCH: %s\nNothing was sent by this program.\n",message);
    exit(2);
}
void ts_need(int condition, const char *message)
{
    if (!condition) ts_die(message);
}
void ts_copy(char *to, size_t capacity, const char *from)
{
    size_t n=strlen(from);
    ts_need(n<capacity,"field exceeds its fixed capacity");
    memcpy(to,from,n+1);
}
void ts_reset(void)
{
    ts_length=0; ts_output[0]=0;
}
void ts_add(const char *text)
{
    size_t n=strlen(text);
    ts_need(n<=TS_BUFFER-ts_length,"output exceeds fixed capacity");
    memcpy(ts_output+ts_length,text,n+1); ts_length+=n;
}
void ts_printf(const char *format, ...)
{
    va_list ap; int n;
    va_start(ap,format);
    n=vsnprintf(ts_output+ts_length,TS_BUFFER+1-ts_length,format,ap);
    va_end(ap);
    ts_need(n>=0 && (size_t)n<=TS_BUFFER-ts_length,"formatted output capacity");
    ts_length+=(size_t)n;
}
void ts_quote(const char *text)
{
    const unsigned char *p=(const unsigned char *)text;
    ts_add("\"");
    for (;*p;++p) {
        if (*p=='"' || *p=='\\') ts_printf("\\%c",*p);
        else if (*p<32) ts_printf("\\u%04x",*p);
        else ts_printf("%c",*p);
    }
    ts_add("\"");
}
/* Paths are explicit relative names; never expand globs or parent traversal. */
void ts_path(const char *path)
{
    const char *p=path, *start=path;
    ts_need(*p && strlen(p)<TS_PATH && *p!='/',"empty/absolute/oversized path");
    for (;;++p) {
        unsigned c=(unsigned char)*p;
        if (!c || c=='/') {
            size_t n=(size_t)(p-start);
            ts_need(n && start[0]!='.' && start[0]!=' ' && p[-1]!=' ',"unsafe path component");
            ts_need(!(n>=5 && !strncmp(start,".git",4)),"reserved path");
            if (!c) break;
            start=p+1;
        } else ts_need(isalnum(c) || c=='_' || c=='-' || c=='.' || c==' ',"unsupported path character");
    }
}
size_t ts_read(const char *path, char *buffer, size_t capacity)
{
    FILE *f; size_t n; int extra, bad;
    ts_path(path); f=fopen(path,"rb");
    if (!f) { fprintf(stderr,"Cannot read: %s\n",path); ts_die("file unavailable or binary read mode unsupported"); }
    n=fread(buffer,1,capacity-1,f); extra=fgetc(f); bad=ferror(f);
    ts_need(fclose(f)==0 && !bad,"file read/close failed");
    ts_need(extra==EOF,"input exceeds its fixed capacity");
    buffer[n]=0; return n;
}
static void ts_absent(const char *path)
{
    FILE *f;
    errno=0; f=fopen(path,"rb");
    if (f) { fclose(f); fprintf(stderr,"Existing output: %s\n",path); ts_die("will not overwrite; use a new batch name"); }
    ts_need(errno==ENOENT,"cannot establish that output is absent");
}
/* A reserved final name prevents another cooperating writer from claiming it.
 * Rename replaces OUR empty reservation, never an admitted existing file.
 * Failure leaves a visible reservation/.tmp; never silently retries a write.
 * Private app workspace and one operator are required; not a hostile-FS API.
 */
void ts_save(const char *path, const char *bytes, size_t n)
{
    char temp[TS_PATH], before[65], after[65]; FILE *f; size_t got; int w;
    ts_path(path); ts_absent(path);
    w=snprintf(temp,sizeof temp,"%s.tmp",path);
    ts_need(w>0 && (size_t)w<sizeof temp,"temporary path capacity");
    ts_absent(temp); ts_sha256(bytes,n,before);
    f=fopen(temp,"wbx"); ts_need(f!=NULL,"exclusive-create temporary file unavailable");
    ts_need(fwrite(bytes,1,n,f)==n,"short temporary write");
    ts_need(fclose(f)==0,"temporary close failed");
    f=fopen(path,"wbx"); ts_need(f!=NULL,"destination reservation failed; no replacement");
    ts_need(fclose(f)==0,"reservation close failed");
    ts_need(rename(temp,path)==0,"publish failed; reservation retained");
    got=ts_read(path,ts_input,TS_BUFFER+1); ts_sha256(ts_input,got,after);
    ts_need(got==n && !strcmp(before,after),"write/read-back bytes disagree");
    printf("Created: %s\n",path);
}
void ts_name(char path[TS_PATH], const char *suffix)
{
    int n=snprintf(path,TS_PATH,"%s.%s",ts.batch,suffix);
    ts_need(n>0 && n<TS_PATH,"output filename capacity");
}
void ts_save_output(const char *suffix)
{
    char path[TS_PATH]; ts_name(path,suffix); ts_save(path,ts_output,ts_length);
}
/* Disposable all-byte sample; proves mode handling before real preparation. */
void ts_probe(void)
{
    char a[80], b[80], sample[257], readback[300];
    FILE *f,*second; size_t n; unsigned i,attempt;
    ts_need(CHAR_BIT==8 && sizeof(uint32_t)==4,"8-bit bytes and uint32_t required");
    ts_hash_checks();
    for (attempt=0;attempt<100;++attempt) {
        snprintf(a,sizeof a,"Submit_Probe_%u.txt.tmp",attempt);
        errno=0; f=fopen(a,"rb");
        if (f) { fclose(f); continue; }
        ts_need(errno==ENOENT,"probe cannot inspect local filenames");
        break;
    }
    ts_need(attempt<100,"probe name capacity; no files overwritten");
    snprintf(b,sizeof b,"Submit_Probe_%u_Final.txt",attempt); ts_absent(b);
    for (i=0;i<256;++i) sample[i]=(char)i;
    f=fopen(a,"wbx"); ts_need(f!=NULL,"phone lacks exclusive binary create (wbx); stop here");
    ts_need(fwrite(sample,1,256,f)==256 && fclose(f)==0,"probe write failed");
    second=fopen(a,"wbx");
    if (second) { fclose(second); ts_die("runtime ignored exclusive create; only disposable probe affected"); }
    n=ts_read(a,readback,sizeof readback);
    ts_need(n==256 && !memcmp(sample,readback,256),"runtime changed file bytes");
    f=fopen(b,"wbx"); ts_need(f!=NULL && fclose(f)==0,"probe reservation failed");
    ts_need(rename(a,b)==0,"runtime cannot publish over its own reserved name");
    n=ts_read(b,readback,sizeof readback);
    ts_need(n==256 && !memcmp(sample,readback,256),"rename round-trip changed bytes");
    ts_need(remove(b)==0,"probe cleanup failed");
    puts("PASS: SHA-256, base64, all 256 byte values, exclusive create and rename.");
}
#include "Tool.Hash.h"
#include "Tool.Json.h"
#include "Tool.Forms.h"
#include "Tool.Plan.h"
#include "Tool.Git.h"
/* Optional checks are linked C functions. Configuration cannot execute code. */
static SubmitCheck registered_check;
static char registered_name[32];
void ts_register_check(const char *name, SubmitCheck check)
{
    ts_need(check!=NULL && strcmp(name,"none"),"invalid custom check registration");
    ts_copy(registered_name,sizeof registered_name,name); registered_check=check;
}
void ts_run_check(void)
{
    ts_need(!strcmp(ts.hasher,"sha256"),"selected hasher is NOT CONNECTED");
    ts_need(!strcmp(ts.logger,"inventory"),"selected logger is NOT CONNECTED");
    if (!strcmp(ts.check,"none")) return;
    ts_need(registered_check && !strcmp(ts.check,registered_name),"required C check is NOT CONNECTED");
    ts_need(registered_check(&ts,ts_pool,ts_used)==0,"registered C check refused submission");
}
#include "Tool.Requests.h"
