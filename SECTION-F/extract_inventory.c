/* extract_inventory.c — first-pass heading-owned package builder for the
 * shakti-guide skill. C99, no heap beyond static arenas, deterministic.
 *
 * NOT a lexer for the recorder: this is the model-side structured extraction
 * (Law V). The recorder independently validates every emitted code block
 * character-for-character against the named file; a wrong block stops the
 * run with evidence. This tool never edits project source.
 *
 * Usage: extract_inventory <out_prefix> <file1> [file2 ...]
 * Reads each C file, finds top-level function definitions (brace-depth 0),
 * extracts exact blocks, normalizes declarations, collects intra-project
 * calls as child_inputs, and writes one package skeleton per file to
 * stdout-collected records: <out_prefix>.index then per-file fragments are
 * assembled by the caller script into the package markdown.
 */
#include <stdio.h>
#include <string.h>

#define MAXF 512
#define MAXNAME 128
#define MAXSIG 1024
#define MAXBODY (1<<20)
#define MAXCALLS 512
#define MAXFILES 64
#define MAXPATH 256

typedef struct {
    char file[MAXPATH];
    char name[MAXNAME];
    char ret[MAXNAME];
    char sig[MAXSIG];      /* normalized declaration, no body */
    char params[MAXSIG];   /* raw param text */
    char calls[MAXCALLS][MAXNAME]; /* callee names found in body */
    int ncalls;
    long start, end;       /* byte span incl. declaration and body */
} fn_t;

static fn_t g_fn[MAXF * 4];
static int g_nfn;

static char g_buf[MAXBODY];
static long g_len;

static int is_ident0(int c){return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||c=='_';}
static int is_ident(int c){return is_ident0(c)||(c>='0'&&c<='9');}

static int kw(const char *s){
    static const char *K[]={"if","for","while","switch","return","sizeof",
        "typedef","struct","enum","union","do","else","case","default",0};
    int i; for(i=0;K[i];i++) if(!strcmp(s,K[i])) return 1; return 0;
}

/* normalize a declaration: collapse whitespace to single spaces */
static void norm(char *out, size_t cap, const char *in, long n){
    size_t o=0; long i; int sp=0;
    for(i=0;i<n && o+1<cap;i++){
        int c=in[i];
        if(c==' '||c=='\t'||c=='\r'||c=='\n'||c=='\f'||c=='\v'){sp=1;continue;}
        if(sp&&o>0){out[o++]=' ';}
        sp=0; out[o++]=(char)c;
    }
    out[o]=0;
}

/* find function definitions in g_buf; append to g_fn */
static void scan_file(const char *path){
    long i=0, depth=0, segstart=0;
    int instr=0, inchr=0, esc=0, lc=0, bc=0;
    while(i<g_len){
        int c=g_buf[i];
        if(lc){ if(c=='\n') lc=0; i++; continue; }
        if(bc){ if(c=='*'&&i+1<g_len&&g_buf[i+1]=='/'){bc=0;i+=2;continue;} i++; continue; }
        if(esc){ esc=0; i++; continue; }
        if(instr){ if(c=='\\')esc=1; else if(c=='"')instr=0; i++; continue; }
        if(inchr){ if(c=='\\')esc=1; else if(c=='\'')inchr=0; i++; continue; }
        if(c=='/'&&i+1<g_len&&g_buf[i+1]=='/'){lc=1;i+=2;continue;}
        if(c=='/'&&i+1<g_len&&g_buf[i+1]=='*'){bc=1;i+=2;continue;}
        if(c=='"'){instr=1;i++;continue;}
        if(c=='\''){inchr=1;i++;continue;}
        if(c=='#'){ /* preprocessor line: skip to eol, reset segment */
            while(i<g_len&&g_buf[i]!='\n')i++;
            segstart=i+1; i++; continue; }
        if(c=='{'){
            if(depth==0){
                /* candidate: text from segstart..i must end with ')' */
                long e=i-1; while(e>=segstart&&(g_buf[e]==' '||g_buf[e]=='\t'||g_buf[e]=='\n'||g_buf[e]=='\r'))e--;
                if(e>=segstart&&g_buf[e]==')'){
                    /* find matching '(' for params */
                    long p=e, d=0;
                    while(p>segstart){ if(g_buf[p]==')')d++; else if(g_buf[p]=='('){d--; if(d==0)break;} p--; }
                    /* name = identifier before p */
                    long ne=p-1; while(ne>=segstart&&!is_ident(g_buf[ne]))ne--;
                    long ns=ne; while(ns>segstart&&is_ident(g_buf[ns-1]))ns--;
                    char name[MAXNAME]; long nl=ne-ns+1;
                    if(nl>0&&nl<MAXNAME&&is_ident0(g_buf[ns])){
                        long k; for(k=0;k<nl;k++)name[k]=g_buf[ns+k]; name[nl]=0;
                        /* header start: back over qualifiers/type to depth-0 line start */
                        long hs=ns-1; while(hs>=segstart&&(g_buf[hs]==' '||g_buf[e]!='\0')){ if(g_buf[hs]=='\n'||g_buf[hs]==';'||g_buf[hs]=='}')break; hs--; }
                        hs++;
                        /* skip leading blank space */
                        while(hs<ns&&(g_buf[hs]==' '||g_buf[hs]=='\t'||g_buf[hs]=='\n'||g_buf[hs]=='\r'))hs++;
                        if(!kw(name)&&hs<ns){
                            /* reject control statements disguised: header must contain a type-ish token before name */
                            char head[MAXSIG]; long hl=ns-hs; if(hl>=MAXSIG)hl=MAXSIG-1;
                            for(k=0;k<hl;k++)head[k]=g_buf[hs+k]; head[hl]=0;
                            /* must not contain '=', and first token must be ident */
                            if(!strchr(head,'=')&&is_ident0(head[0])&&strchr(head,' ')!=NULL){
                                fn_t *f=&g_fn[g_nfn];
                                snprintf(f->file,MAXPATH,"%s",path);
                                snprintf(f->name,MAXNAME,"%s",name);
                                /* ret = header without trailing space */
                                { long rl=strlen(head); while(rl>0&&head[rl-1]==' ')rl--; head[rl]=0;
                                  snprintf(f->ret,MAXNAME,"%s",head); }
                                /* params raw */
                                { long pl=e-p-1; if(pl>=MAXSIG)pl=MAXSIG-1;
                                  for(k=0;k<pl;k++)f->params[k]=g_buf[p+1+k]; f->params[pl]=0; }
                                /* full sig */
                                { char t[MAXSIG*2];
                                  snprintf(t,sizeof t,"%s %s(%s)",f->ret,f->name,f->params);
                                  norm(f->sig,MAXSIG,t,strlen(t)); }
                                /* body span: from hs to matching '}' */
                                { long j=i+1, dd=1, s2=0,c2=0,e2=0,l2=0,b2=0;
                                  while(j<g_len&&dd>0){
                                    int ch=g_buf[j];
                                    if(l2){if(ch=='\n')l2=0;j++;continue;}
                                    if(b2){if(ch=='*'&&j+1<g_len&&g_buf[j+1]=='/'){b2=0;j+=2;continue;}j++;continue;}
                                    if(e2){e2=0;j++;continue;}
                                    if(s2){if(ch=='\\')e2=1;else if(ch=='"')s2=0;j++;continue;}
                                    if(c2){if(ch=='\\')e2=1;else if(ch=='\'')c2=0;j++;continue;}
                                    if(ch=='/'&&j+1<g_len&&g_buf[j+1]=='/'){l2=1;j+=2;continue;}
                                    if(ch=='/'&&j+1<g_len&&g_buf[j+1]=='*'){b2=1;j+=2;continue;}
                                    if(ch=='"'){s2=1;j++;continue;}
                                    if(ch=='\''){c2=1;j++;continue;}
                                    if(ch=='{')dd++;
                                    if(ch=='}')dd--;
                                    j++;
                                  }
                                  f->start=hs; f->end=j; /* [start,end) exact bytes */
                                }
                                g_nfn++;
                            }
                        }
                    }
                }
            }
            depth++; i++; continue;
        }
        if(c=='}'){ depth--; i++; if(depth==0) segstart=i; continue; }
        if(c==';'&&depth==0){ segstart=i+1; }
        i++;
    }
}

static int fn_index(const char *name){
    int i; for(i=0;i<g_nfn;i++) if(!strcmp(g_fn[i].name,name)) return i; return -1;
}

/* collect calls within one function's exact span */
static void collect_calls(fn_t *f){
    long i=f->start;
    int instr=0,inchr=0,esc=0,lc=0,bc=0;
    while(i<f->end){
        int c=g_buf[i];
        if(lc){if(c=='\n')lc=0;i++;continue;}
        if(bc){if(c=='*'&&i+1<f->end&&g_buf[i+1]=='/'){bc=0;i+=2;continue;}i++;continue;}
        if(esc){esc=0;i++;continue;}
        if(instr){if(c=='\\')esc=1;else if(c=='"')instr=0;i++;continue;}
        if(inchr){if(c=='\\')esc=1;else if(c=='\'')inchr=0;i++;continue;}
        if(c=='/'&&i+1<f->end&&g_buf[i+1]=='/'){lc=1;i+=2;continue;}
        if(c=='/'&&i+1<f->end&&g_buf[i+1]=='*'){bc=1;i+=2;continue;}
        if(c=='"'){instr=1;i++;continue;}
        if(c=='\''){inchr=1;i++;continue;}
        if(is_ident0(c)){
            long s=i; while(i<f->end&&is_ident(g_buf[i]))i++;
            long n=i-s; if(n>0&&n<MAXNAME){
                char id[MAXNAME]; long k; for(k=0;k<n;k++)id[k]=g_buf[s+k]; id[n]=0;
                long j=i; while(j<f->end&&(g_buf[j]==' '||g_buf[j]=='\t'||g_buf[j]=='\n'||g_buf[j]=='\r'))j++;
                if(j<f->end&&g_buf[j]=='('&&!kw(id)&&strcmp(id,f->name)){
                    int ci=fn_index(id);
                    if(ci>=0 && f->ncalls<MAXCALLS){
                        int dup=0,q;
                        for(q=0;q<f->ncalls;q++)if(!strcmp(f->calls[q],id))dup=1;
                        if(!dup){ snprintf(f->calls[f->ncalls++],MAXNAME,"%s",id); }
                    }
                }
            }
            continue;
        }
        i++;
    }
}

int main(int argc, char **argv){
    int a;
    if(argc<3){ printf("usage: extract_inventory <out> <file...>\n"); return 1; }
    /* pass 1: definitions */
    for(a=2;a<argc;a++){
        FILE *fp=fopen(argv[a],"rb");
        if(!fp){ printf("STOP: cannot read %s\n",argv[a]); return 1; }
        g_len=fread(g_buf,1,MAXBODY-1,fp); fclose(fp); g_buf[g_len]=0;
        scan_file(argv[a]);
    }
    /* pass 2: calls per function (re-read each file for its spans) */
    {
        /* group by file: re-scan buffers per file */
        int fi;
        for(a=2;a<argc;a++){
            FILE *fp=fopen(argv[a],"rb"); if(!fp)continue;
            g_len=fread(g_buf,1,MAXBODY-1,fp); fclose(fp); g_buf[g_len]=0;
            for(fi=0;fi<g_nfn;fi++) if(!strcmp(g_fn[fi].file,argv[a])) collect_calls(&g_fn[fi]);
        }
    }
    /* emit machine-readable records: file|name|ret|sig|start|end|calls */
    {
        FILE *out=fopen(argv[1],"w");
        int i,j;
        if(!out){ printf("STOP: cannot write %s\n",argv[1]); return 1; }
        for(i=0;i<g_nfn;i++){
            fprintf(out,"FN|%s|%s|%s|%s|%ld|%ld|",
                g_fn[i].file,g_fn[i].name,g_fn[i].ret,g_fn[i].sig,g_fn[i].start,g_fn[i].end);
            for(j=0;j<g_fn[i].ncalls;j++) fprintf(out,"%s%s",j?",":"",g_fn[i].calls[j]);
            fprintf(out,"\n");
        }
        fclose(out);
    }
    printf("extract_inventory: %d functions from %d files -> %s\n", g_nfn, argc-2, argv[1]);
    return 0;
}
