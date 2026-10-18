/* Tool.Forms.h -- two fill-in forms -> explicit families and file mappings.
 * The six-slot README follows Tool.Assembler.c v4. FOUR is an explicit
 * alternative following Tool_Assembler.README_builder.c; never auto-switched.
 * Existing READMEs are copied byte-for-byte, not run through either fixer.
 */
#include <stddef.h>
typedef struct { const char *key; size_t offset, capacity; } TsField;
#define PF(k,m) {k,offsetof(SubmitPlan,m),sizeof ts.m}
#define FF(k,m) {k,offsetof(SubmitFamily,m),sizeof ts.family[0].m}
static const TsField ts_global_fields[] = {
    PF("MODE",mode), PF("BATCH",batch), PF("OWNER",owner), PF("REPOSITORY",repo),
    PF("BASE",base), PF("BRANCH",branch), PF("ROOT",root), PF("HASHER",hasher),
    PF("LOGGER",logger), PF("CHECK",check)
};
static const TsField ts_family_fields[] = {
    FF("VERSION",version), FF("DATE",date), FF("README_STYLE",style),
    FF("README_SOURCE",readme), FF("DESCRIPTION",description), FF("RELATED",related),
    FF("RUN",runs), FF("CHANGED",changed), FF("REQUIREMENTS",requirements),
    FF("EXAMPLE",example), FF("EXPECTED",expected), FF("REFERENCES",references)
};
#undef PF
#undef FF
static char *ts_trim(char *s)
{
    char *end;
    while (*s==' ' || *s=='\t' || *s=='\r') ++s;
    end=s+strlen(s);
    while (end>s && (end[-1]==' ' || end[-1]=='\t' || end[-1]=='\r')) --end;
    *end=0; return s;
}
static void ts_atom(const char *s)
{
    const unsigned char *p=(const unsigned char *)s;
    ts_need(*p!=0,"empty identifier");
    for (;*p;++p) ts_need(isalnum(*p)||*p=='-'||*p=='_'||*p=='.',"invalid identifier");
    ts_need(!strstr(s,"..") && *s!='.',"unsafe identifier");
    ts_need(!strstr(s,"CHANGE_ME"),"fill in every CHANGE_ME field first");
}
static void ts_date(const char *s, int unknown)
{
    unsigned y,m,d,i,days;
    static const unsigned md[]={31,28,31,30,31,30,31,31,30,31,30,31};
    if (unknown && !strcmp(s,"UNKNOWN")) return;
    ts_need(strlen(s)==10 && s[4]=='-' && s[7]=='-',"date must be YYYY-MM-DD");
    for (i=0;i<10;++i) if (i!=4 && i!=7) ts_need(isdigit((unsigned char)s[i]),"invalid date digit");
    y=(unsigned)atoi(s); m=(unsigned)atoi(s+5); d=(unsigned)atoi(s+8);
    ts_need(y>=1970 && y<=9999 && m>=1 && m<=12,"date out of range");
    days=md[m-1]+(m==2 && (!(y%400) || (!(y%4) && y%100)));
    ts_need(d>=1 && d<=days,"invalid calendar day");
}
static int ts_field_set(void *record, const TsField *fields, size_t count,
                        const char *key, const char *value)
{
    size_t i;
    for (i=0;i<count;++i) if (!strcmp(key,fields[i].key)) {
        char *dest=(char *)record+fields[i].offset;
        ts_need(!*dest && *value,"duplicate or empty form field");
        ts_copy(dest,fields[i].capacity,value); return 1;
    }
    return 0;
}
static void ts_settings(void)
{
    ts_atom(ts.batch); ts_atom(ts.owner); ts_atom(ts.repo);
    ts_path(ts.root); ts_path(ts.base); ts_path(ts.branch);
    ts_need(!strchr(ts.batch,'/') && !strstr(ts.root,"CHANGE_ME"),"explicit destination root required");
    ts_need(!strncmp(ts.branch,"submit/",7) && strlen(ts.branch)>7,"review branch must start submit/");
    ts_need(strcmp(ts.base,ts.branch)!=0 && !strchr(ts.branch,' ') && !strstr(ts.branch,".."),"invalid review branch");
    ts_need(!strcmp(ts.mode,"SINGLE") || !strcmp(ts.mode,"SET"),"MODE must be SINGLE or SET");
}
/* Parse only the named form. FILE rows never execute or compile their sources. */
void ts_form(const char *path)
{
    static char form[65537];
    char *line,*next; size_t n,i; int fam=-1;
    memset(&ts,0,sizeof ts); ts_used=0;
    n=ts_read(path,form,sizeof form); ts_utf8(form,n); ts_sha256(form,n,ts.form_sha);
    for (line=form;line && *line;line=next) {
        char *key,*value,*equal;
        next=strchr(line,'\n'); if (next) *next++=0;
        key=ts_trim(line); if (!*key || *key=='#') continue;
        if (!strcmp(key,"END_FAMILY")) { ts_need(fam>=0,"unexpected END_FAMILY"); fam=-1; continue; }
        equal=strchr(key,'='); ts_need(equal!=NULL,"form line needs KEY=value");
        *equal=0; value=ts_trim(equal+1); key=ts_trim(key);
        if (!strcmp(key,"FAMILY")) {
            ts_need(fam<0 && ts.family_count<TS_FAMILIES,"nested/too many families");
            fam=ts.family_count++; ts_atom(value);
            ts_copy(ts.family[fam].name,sizeof ts.family[fam].name,value);
        } else if (!strcmp(key,"FILE")) {
            SubmitFile *f; char *p=strchr(value,'|'), *q;
            ts_need(fam>=0 && p && ts.count<TS_FILES,"FILE outside family or capacity");
            *p++=0; q=strchr(p,'|'); ts_need(q!=NULL,"FILE needs source|destination|declared date");
            *q++=0; ts_need(!strchr(q,'|'),"too many FILE fields");
            f=&ts.files[ts.count++]; f->family=fam;
            ts_copy(f->source,sizeof f->source,ts_trim(value));
            ts_copy(f->path,sizeof f->path,ts_trim(p)); ts_copy(f->date,sizeof f->date,ts_trim(q));
            ts_path(f->source); ts_path(f->path); ts_date(f->date,1);
            ts_need(strcmp(f->path,"README.md")!=0,"use README_SOURCE for the family README");
        } else if (fam>=0) {
            ts_need(ts_field_set(&ts.family[fam],ts_family_fields,12,key,value),"unknown family field");
        } else ts_need(ts_field_set(&ts,ts_global_fields,10,key,value),"unknown global field");
    }
    ts_need(fam<0 && ts.family_count && ts.count,"incomplete or empty form");
    for (i=0;i<10;++i) ts_need(*((char *)&ts+ts_global_fields[i].offset)!=0,"missing global field");
    ts_settings();
    ts_need(strcmp(ts.mode,"SINGLE") || ts.family_count==1,"SINGLE requires exactly one family");
    for (fam=0;fam<ts.family_count;++fam) {
        SubmitFamily *f=&ts.family[fam]; int j;
        for (i=0;i<12;++i) ts_need(*((char *)f+ts_family_fields[i].offset)!=0,"missing family field");
        ts_date(f->date,0); ts_atom(f->version);
        ts_need(!strcmp(f->style,"SIX") || !strcmp(f->style,"FOUR"),"README_STYLE must be SIX or FOUR");
        for (j=0;j<fam;++j) ts_need(strcmp(f->name,ts.family[j].name)!=0,"duplicate family");
    }
}
static const char *ts_relative(const SubmitFile *f)
{
    return f->path+strlen(ts.root)+1+strlen(ts.family[f->family].name)+1;
}
/* No guessed evidence: RUN and CHANGED are exactly the user's form text. */
static void ts_readme(int family)
{
    SubmitFamily *f=&ts.family[family]; int i,k=0,total=0;
    ts_reset(); ts_printf("# %s\n\n",f->name);
    ts_add("SUBMISSION SCOPE: Explicitly listed files only; unlisted repository files are not inventoried.\n\n");
    if (!strcmp(f->style,"SIX")) {
        ts_printf("VERSION: %s\nRELEASE DATE (declared): %s\nDESCRIPTION: %s\n",f->version,f->date,f->description);
        ts_printf("EXAMPLE INPUT: %s\nEXPECTED OUTPUT: %s\n\nFILES:\n",f->example,f->expected);
        for (i=0;i<ts.count;++i) if (ts.files[i].family==family) ts_printf("  %s\n",ts_relative(&ts.files[i]));
        ts_add("FORMAT:");
        for (i=0;i<ts.count;++i) if (ts.files[i].family==family) {
            const char *ext=strrchr(ts_relative(&ts.files[i]),'.'); int j,seen=0;
            if (!ext) continue;
            for (j=0;j<i;++j) if (ts.files[j].family==family) {
                const char *prev=strrchr(ts_relative(&ts.files[j]),'.');
                if (prev && !strcmp(prev,ext)) seen=1;
            }
            if (!seen) ts_printf("%s%s",k++?", ":" ",ext);
        }
        ts_add("\nNAMES MEAN: family dir First-Name; source First.Name.c; binary shakti-<name> "
               "\xE2\x80\x94 a file that breaks the law is drift, flag it.\n");
        ts_printf("\nRELATED TO: %s\n\nRUN: %s\n\nCHANGED: %s\n",f->related,f->runs,f->changed);
    } else {
        ts_add("FILE SETS (in order):\n");
        for (i=0;i<ts.count;++i) if (ts.files[i].family==family) {
            const char *p=ts_relative(&ts.files[i]); size_t n=strlen(p);
            if (n>2 && !strcmp(p+n-2,".c")) ++total;
        }
        for (i=0;i<ts.count;++i) if (ts.files[i].family==family) {
            const char *p=ts_relative(&ts.files[i]); size_t n=strlen(p);
            if (n>2 && !strcmp(p+n-2,".c")) ts_printf("%d of %d - %.*s\n",++k,total,(int)n-2,p);
        }
        if (!total) ts_add("(none)\n");
        ts_add("\nSCRIPT FILES (in order):\n"); k=0;
        for (i=0;i<ts.count;++i) if (ts.files[i].family==family) ts_printf("%d. %s\n",++k,ts_relative(&ts.files[i]));
        ts_printf("\n## I. INTRODUCTION\n\n%s\n\n## II. REQUIREMENTS\n\n%s\n",f->description,f->requirements);
        ts_printf("\n## III. RUNNING NOTES\n\nVersion: %s\nDeclared release: %s\n",f->version,f->date);
        ts_printf("Example input: %s\nExpected output: %s\nRUN: %s\nCHANGED: %s\n",f->example,f->expected,f->runs,f->changed);
        ts_printf("\n## IV. REFERENCES\n\n%s\n",f->references);
    }
}
