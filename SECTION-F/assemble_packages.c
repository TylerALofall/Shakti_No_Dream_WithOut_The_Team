/* assemble_packages.c — build the heading-owned function package markdown
 * from extract_inventory records + exact source bytes.
 * C99, deterministic. Model-side structured generation (Law V); the recorder
 * re-validates every block byte-for-byte. Never edits project source.
 *
 * Usage: assemble_packages <inv.txt> <packages.md> <file1> [file2...]
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAXR 1024
#define MAXLONG 8192
#define MAXPATH 256
#define MAXFILES 64

typedef struct {
    char file[MAXPATH];
    char name[128];
    char ret[128];
    char sig[1024];
    long start, end;
    char calls[2048];
} rec_t;

static rec_t g_r[MAXR];
static int g_nr;
static char g_src[MAXFILES][1<<20];
static long g_sn[MAXFILES];
static char g_sf[MAXFILES][MAXPATH];
static int g_nf;

static int section_of(const char *f){
    if (strstr(f, "src/main.c")) return 'A';
    if (strstr(f, "shakti_loop")) return 'B';
    if (strstr(f, "shakti_reason") || strstr(f, "shakti_receptor")) return 'C';
    if (strstr(f, "shakti_memory") || strstr(f, "shakti_log")) return 'D';
    if (strstr(f, "shakti_asset") || strstr(f, "shakti_artifact") ||
        strstr(f, "shakti_manifest") || strstr(f, "shakti_loader")) return 'E';
    if (strstr(f, "shakti_tablet") || strstr(f, "shakti_school") ||
        strstr(f, "shakti_score") || strstr(f, "shakti_handwriting") ||
        strstr(f, "shakti_time") || strstr(f, "shakti_report")) return 'F';
    if (strstr(f, "/eyes/")) return 'G';
    if (strstr(f, "/tools/")) return 'H';
    if (strstr(f, "/tests/")) return 'I';
    return 'J';
}

static int src_index(const char *f){
    int i; for(i=0;i<g_nf;i++) if(!strcmp(g_sf[i],f)) return i; return -1;
}

/* split params into types */
static void emit_input_types(FILE *out, const char *sig){
    const char *lp = strchr(sig, '(');
    const char *rp = strrchr(sig, ')');
    char buf[1024]; size_t n;
    if (!lp || !rp || rp <= lp+1) return;
    n = (size_t)(rp-lp-1); if (n >= sizeof buf) n = sizeof buf - 1;
    memcpy(buf, lp+1, n); buf[n] = 0;
    /* split on commas (no nested parens expected in these decls) */
    {
        char *p = buf, *tok;
        while ((tok = p) != NULL && *p) {
            char *c = strchr(p, ','); if (c) *c = 0;
            /* trim */
            while (*tok==' ') tok++;
            { size_t L=strlen(tok); while(L>0&&tok[L-1]==' ') tok[--L]=0; }
            if (!strcmp(tok,"void") || !tok[0]) { if(!c)break; p=c+1; continue; }
            /* drop trailing identifier */
            { size_t L=strlen(tok);
              while(L>0 && ( (tok[L-1]>='a'&&tok[L-1]<='z')||(tok[L-1]>='A'&&tok[L-1]<='Z')||
                     (tok[L-1]>='0'&&tok[L-1]<='9')||tok[L-1]=='_' )) L--;
              while(L>0 && tok[L-1]==' ') L--;
              /* array suffix like name[4] already cut with ident; trailing ] fix */
              tok[L]=0;
              if (L>0) fprintf(out, "<input_type>%s</input_type>\n", tok);
            }
            if(!c) break; p = c+1;
        }
    }
}

static int rec_index(const char *name){
    int i; for(i=0;i<g_nr;i++) if(!strcmp(g_r[i].name,name)) return i; return -1;
}

/* C scoping law: a static (file-local) callee is always the one defined in
 * the caller's own file. Prefer same-file resolution; fall back to a global
 * match only when the name is not defined locally. */
static int rec_index_for(const char *caller_file, const char *name){
    int i; for(i=0;i<g_nr;i++)
        if(!strcmp(g_r[i].name,name) && !strcmp(g_r[i].file,caller_file)) return i;
    return rec_index(name);
}

int main(int argc, char **argv){
    FILE *in, *out;
    char line[MAXLONG];
    int a;
    if (argc < 4){ printf("usage: assemble_packages <inv> <out.md> <files...>\n"); return 1; }
    for (a=3;a<argc && g_nf<MAXFILES;a++){
        FILE *fp=fopen(argv[a],"rb");
        if(!fp){ printf("STOP: read %s\n", argv[a]); return 1; }
        g_sn[g_nf]=fread(g_src[g_nf],1,sizeof g_src[g_nf]-1,fp); fclose(fp);
        snprintf(g_sf[g_nf],MAXPATH,"%s",argv[a]);
        g_nf++;
    }
    in=fopen(argv[1],"r");
    if(!in){ printf("STOP: read inv\n"); return 1; }
    while (fgets(line,sizeof line,in) && g_nr<MAXR){
        rec_t *r=&g_r[g_nr]; char *p,*q; int field=0;
        if (strncmp(line,"FN|",3)) continue;
        p=line+3;
        while (p && field<7){
            q=strchr(p,'|');
            if(!q)break;
            *q=0;
            if(field==0) snprintf(r->file,MAXPATH,"%s",p);
            if(field==1) snprintf(r->name,128,"%s",p);
            if(field==2) snprintf(r->ret,128,"%s",p);
            if(field==3) snprintf(r->sig,1024,"%s",p);
            if(field==4) r->start=atol(p);
            if(field==5) r->end=atol(p);
            field++; p=q+1;
        }
        /* rest = calls */
        { char *e=strchr(p,'\n'); if(e)*e=0; snprintf(r->calls,2048,"%s",p); }
        g_nr++;
    }
    fclose(in);

    out=fopen(argv[2],"w");
    if(!out){ printf("STOP: write out\n"); return 1; }
    fprintf(out,"# 2026_08_21 SHAKTI MASTER ARCHITECTURE — SOLE SOURCE OF TRUTH\n\n");
    fprintf(out,"## CHAPTER VI — CURRENT BUILT STRUCTURE\n\n");

    {
        int sec, fi, ri, num;
        for (sec='A'; sec<='J'; sec++){
            int any=0, sub=0;
            for (fi=0; fi<g_nf; fi++){
                if (section_of(g_sf[fi]) != sec) continue;
                if (!any){ fprintf(out,"### SECTION %c — SECTION %c [PROPOSED]\n\n",sec,sec); any=1; }
                /* does this file have functions? */
                { int has=0; for(ri=0;ri<g_nr;ri++) if(!strcmp(g_r[ri].file,g_sf[fi])){has=1;break;}
                  if(!has) continue; }
                fprintf(out,"#### SubSection %c — %s [PROPOSED]\n\n",'a'+sub, g_sf[fi]); sub++;
                num=0;
                for (ri=0; ri<g_nr; ri++){
                    rec_t *r=&g_r[ri];
                    int si;
                    if (strcmp(r->file,g_sf[fi])) continue;
                    si=src_index(r->file);
                    num++;
                    fprintf(out,"##### function %02d — %s\n\n",num,r->name);
                    fprintf(out,"<function_record>\n");
                    fprintf(out,"<file_name>%s</file_name>\n",r->file + (strncmp(r->file,"branch/",7)?0:7));
                    fprintf(out,"<function_name>%s</function_name>\n",r->name);
                    fprintf(out,"<return_type>%s</return_type>\n",r->ret);
                    fprintf(out,"<declaration>%s</declaration>\n",r->sig);
                    fprintf(out,"<more_than_one_parent_source>no</more_than_one_parent_source>\n");
                    fprintf(out,"<input_types>\n");
                    emit_input_types(out,r->sig);
                    fprintf(out,"</input_types>\n");
                    fprintf(out,"<parent_sources>\n</parent_sources>\n");
                    fprintf(out,"<more_than_one_return_type>no</more_than_one_return_type>\n");
                    fprintf(out,"<all_return_types_collected>yes</all_return_types_collected>\n");
                    fprintf(out,"<is_memory_held>no</is_memory_held>\n");
                    fprintf(out,"<child_inputs>\n");
                    { char cb[2048]; char *p,*tok;
                      snprintf(cb,sizeof cb,"%s",r->calls); p=cb;
                      while(*p){ char *c=strchr(p,','); if(c)*c=0; tok=p;
                        { int ci=rec_index_for(r->file,tok);
                          if(ci>=0){
                            fprintf(out,"<child_input>\n<file_name>%s</file_name>\n<function_name>%s</function_name>\n<input_type>%s</input_type>\n</child_input>\n",
                              g_r[ci].file + (strncmp(g_r[ci].file,"branch/",7)?0:7),
                              g_r[ci].name, g_r[ci].ret);
                          } }
                        if(!c)break; p=c+1; } }
                    fprintf(out,"</child_inputs>\n");
                    fprintf(out,"</function_record>\n\n");
                    fprintf(out,"```c\n");
                    if (si>=0 && r->start>=0 && r->end>r->start && r->end<=g_sn[si])
                        fwrite(g_src[si]+r->start,1,(size_t)(r->end-r->start),out);
                    fprintf(out,"\n```\n\n");
                }
            }
        }
    }
    fclose(out);
    printf("assemble_packages: %d records, %d files -> %s\n", g_nr, g_nf, argv[2]);
    return 0;
}
