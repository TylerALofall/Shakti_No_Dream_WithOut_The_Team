/* Tool.Plan.h -- form -> frozen text snapshot -> later request stages.
 * All source files are read twice before publication. Subsequent stages use
 * frozen bytes, not files that may have changed in the editor meanwhile.
 */
static void ts_pool_put(SubmitFile *f, const char *bytes, size_t n)
{
    ts_need(n<=TS_FILE_BYTES && n+1<=TS_POOL_BYTES-ts_used,"file/payload capacity exceeded");
    ts_utf8(bytes,n); f->offset=ts_used; f->bytes=n;
    memcpy(ts_pool+ts_used,bytes,n); ts_pool[ts_used+n]=0; ts_used+=n+1;
    ts_sha256(bytes,n,f->sha);
}
static int ts_case_compare(const char *a, const char *b)
{
    while (*a && *b && tolower((unsigned char)*a)==tolower((unsigned char)*b)) { ++a; ++b; }
    return tolower((unsigned char)*a)-tolower((unsigned char)*b);
}
static void ts_directories(void)
{
    int i,j; ts.dir_count=0;
    for (i=0;i<ts.count;++i) {
        char path[TS_PATH]; size_t k;
        ts_path(ts.files[i].path);
        for (j=0;j<i;++j) {
            size_t a=strlen(ts.files[j].path),b=strlen(ts.files[i].path);
            ts_need(ts_case_compare(ts.files[i].path,ts.files[j].path),"duplicate/case-colliding destination");
            ts_need(!(a<b && !strncmp(ts.files[i].path,ts.files[j].path,a) && ts.files[i].path[a]=='/'),"file also used as parent directory");
            ts_need(!(b<a && !strncmp(ts.files[j].path,ts.files[i].path,b) && ts.files[j].path[b]=='/'),"file also used as parent directory");
        }
        ts_copy(path,sizeof path,ts.files[i].path);
        for (k=0;path[k];++k) if (path[k]=='/') {
            path[k]=0;
            for (j=0;j<ts.dir_count;++j) if (!strcmp(path,ts.dirs[j])) break;
            if (j==ts.dir_count) {
                ts_need(j<TS_DIRS,"ancestor-directory capacity");
                ts_copy(ts.dirs[ts.dir_count++],TS_PATH,path);
            }
            path[k]='/';
        }
    }
}
void ts_collect(void)
{
    int i,j,original=ts.count; char path[TS_PATH],hash[65];
    for (i=0;i<original;++i) {
        SubmitFile *f=&ts.files[i];
        int n=snprintf(path,sizeof path,"%s/%s/%s",ts.root,ts.family[f->family].name,f->path);
        ts_need(n>0 && (size_t)n<sizeof path,"destination path capacity");
        ts_copy(f->path,sizeof f->path,path);
    }
    for (i=0;i<ts.family_count;++i) {
        SubmitFile *f; int n;
        ts_need(ts.count<TS_FILES,"no capacity for family README"); f=&ts.files[ts.count++];
        f->family=i; ts_copy(f->date,sizeof f->date,ts.family[i].date);
        ts_copy(f->source,sizeof f->source,!strcmp(ts.family[i].readme,"GENERATE")?"@GENERATED":ts.family[i].readme);
        n=snprintf(f->path,sizeof f->path,"%s/%s/README.md",ts.root,ts.family[i].name);
        ts_need(n>0 && (size_t)n<sizeof f->path,"README destination capacity");
    }
    for (i=1;i<ts.count;++i) {
        SubmitFile f=ts.files[i];
        for (j=i;j>0 && strcmp(ts.files[j-1].path,f.path)>0;--j) ts.files[j]=ts.files[j-1];
        ts.files[j]=f;
    }
    ts_directories();
    for (i=0;i<ts.count;++i) {
        SubmitFile *f=&ts.files[i]; size_t n;
        if (!strcmp(f->source,"@GENERATED")) {
            ts_readme(f->family); ts_pool_put(f,ts_output,ts_length);
        } else {
            n=ts_read(f->source,ts_input,TS_FILE_BYTES+1); ts_pool_put(f,ts_input,n);
        }
    }
    ts_run_check();
    for (i=0;i<ts.count;++i) if (strcmp(ts.files[i].source,"@GENERATED")) {
        size_t n=ts_read(ts.files[i].source,ts_input,TS_FILE_BYTES+1);
        ts_sha256(ts_input,n,hash);
        if (strcmp(hash,ts.files[i].sha)) {
            fprintf(stderr,"Changed source: %s\nFirst: %s\nSecond: %s\n",ts.files[i].source,ts.files[i].sha,hash);
            ts_die("source changed between inventory passes");
        }
    }
    printf("Inventory pass 1 and pass 2 agree: %d payload files, including READMEs.\n",ts.count);
}
static void ts_pair(const char *key, const char *value)
{
    ts_quote(key); ts_add(":"); ts_quote(value); ts_add(",\n");
}
void ts_plan_write(void)
{
    int i; char suffix[80],path[TS_PATH];
    ts_name(path,"plan.txt"); ts_absent(path);
    ts_name(path,"inventory.txt"); ts_absent(path);
    ts_name(path,"01-check.txt"); ts_absent(path);
    for (i=0;i<ts.count;++i) if (!strcmp(ts.files[i].source,"@GENERATED")) {
        snprintf(suffix,sizeof suffix,"family-%d.README.txt",ts.files[i].family+1);
        ts_name(path,suffix); ts_absent(path);
    }
    ts_reset(); ts_add("{\n"); ts_pair("format","ShaktiSubmit/1");
    ts_pair("mode",ts.mode); ts_pair("batch",ts.batch); ts_pair("owner",ts.owner);
    ts_pair("repository",ts.repo); ts_pair("base",ts.base); ts_pair("branch",ts.branch);
    ts_pair("root",ts.root); ts_pair("form_sha256",ts.form_sha);
    ts_pair("hasher",ts.hasher); ts_pair("logger",ts.logger); ts_pair("check",ts.check);
    ts_add("\"files\":[\n");
    for (i=0;i<ts.count;++i) {
        SubmitFile *f=&ts.files[i]; if (i) ts_add(",\n"); ts_add("{");
        ts_pair("source",f->source); ts_pair("family",ts.family[f->family].name);
        ts_pair("path",f->path); ts_pair("declared_source_date",f->date);
        ts_pair("sha256",f->sha); ts_add("\"content\":"); ts_quote(ts_pool+f->offset); ts_add("}");
    }
    ts_add("\n]}\n"); ts_sha256(ts_output,ts_length,ts.plan_sha); ts_save_output("plan.txt");
    for (i=0;i<ts.count;++i) if (!strcmp(ts.files[i].source,"@GENERATED")) {
        snprintf(suffix,sizeof suffix,"family-%d.README.txt",ts.files[i].family+1);
        ts_name(path,suffix); ts_save(path,ts_pool+ts.files[i].offset,ts.files[i].bytes);
    }
}
static int ts_load_json(const char *suffix, char hash[65])
{
    char path[TS_PATH]; size_t n;
    ts_name(path,suffix); n=ts_read(path,ts_input,TS_BUFFER+1);
    if (hash) ts_sha256(ts_input,n,hash);
    ts_need(strlen(ts_input)==n,"NUL in saved JSON");
    return ts_json_parse();
}
#define JC(dst,obj,key) ts_copy(dst,sizeof(dst),ts_string(ts_get(obj,key)))
void ts_plan_load(const char *batch)
{
    int root,list,p; char old[65],expected[64];
    memset(&ts,0,sizeof ts); ts_used=0;
    ts_copy(ts.batch,sizeof ts.batch,batch); ts_atom(batch); ts_copy(expected,sizeof expected,batch);
    root=ts_load_json("plan.txt",ts.plan_sha); ts_expect(ts_get(root,"format"),"ShaktiSubmit/1");
    JC(ts.mode,root,"mode"); JC(ts.batch,root,"batch"); JC(ts.owner,root,"owner");
    JC(ts.repo,root,"repository"); JC(ts.base,root,"base"); JC(ts.branch,root,"branch");
    JC(ts.root,root,"root"); JC(ts.form_sha,root,"form_sha256");
    JC(ts.hasher,root,"hasher"); JC(ts.logger,root,"logger"); JC(ts.check,root,"check");
    ts_need(!strcmp(expected,ts.batch),"snapshot batch identity mismatch"); ts_settings();
    list=ts_get(root,"files"); ts_need(ts_json[list].type=='[',"snapshot file array required");
    for (p=ts_json[list].child;p>=0 && p<ts_json[list].next;p=ts_json[p].next) {
        SubmitFile *f; const char *content,*family; int j;
        ts_need(ts.count<TS_FILES,"snapshot file count capacity"); f=&ts.files[ts.count++];
        JC(f->source,p,"source"); JC(f->path,p,"path"); JC(f->date,p,"declared_source_date");
        family=ts_string(ts_get(p,"family"));
        for (j=0;j<ts.family_count;++j) if (!strcmp(family,ts.family[j].name)) break;
        if (j==ts.family_count) {
            ts_need(j<TS_FAMILIES,"snapshot family capacity");
            ts_atom(family); ts_copy(ts.family[ts.family_count++].name,96,family);
        }
        f->family=j; ts_date(f->date,1);
        ts_need(!strncmp(f->path,ts.root,strlen(ts.root)) && f->path[strlen(ts.root)]=='/',"snapshot path outside declared root");
        JC(old,p,"sha256"); content=ts_string(ts_get(p,"content"));
        ts_pool_put(f,content,strlen(content)); ts_need(!strcmp(old,f->sha),"snapshot content hash mismatch");
    }
    ts_need(ts.count>0,"empty snapshot"); ts_directories(); ts_run_check();
}
void ts_inventory(void)
{
    int i;
    ts_reset(); ts_printf("SHAKTI SUBMISSION %s\nSTATUS: PREPARED LOCALLY; NOT UPLOADED\n",ts.batch);
    ts_printf("Repository: %s/%s\nReview branch: %s\nBase read from: %s\n",ts.owner,ts.repo,ts.branch,ts.base);
    ts_printf("Form SHA-256: %s\nSnapshot SHA-256: %s\n",ts.form_sha,ts.plan_sha);
    ts_add("Dates below are user-declared, not filesystem timestamps. UNKNOWN remains unknown.\n");
    ts_add("Original files read twice; unchanged. RUN text is supplied evidence, not a new run certification.\n\n");
    for (i=0;i<ts.count;++i) {
        SubmitFile *f=&ts.files[i];
        ts_printf("%d. %s\n   Source: %s\n   Declared date: %s\n   Bytes: %lu\n   SHA-256: %s\n",i+1,f->path,f->source,f->date,(unsigned long)f->bytes,f->sha);
    }
    ts_save_output("inventory.txt");
}
