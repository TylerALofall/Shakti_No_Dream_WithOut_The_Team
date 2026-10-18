/* Tool.Git.h -- GitHub read-only query contract and strict response gates.
 * Reads the selected paths AND every ancestor from ONE commit object.
 * Missing aliases are errors, not missing files. Non-text, truncated blobs,
 * symlinks, executable entries and directories at file paths are blocked.
 */
static void ts_wrap_query(void)
{
    static char query[TS_QUERY];
    ts_copy(query,sizeof query,ts_output); ts_reset();
    ts_add("{\"query\":"); ts_quote(query); ts_add("}\n");
}
static void ts_repo_query(void)
{
    ts_printf("repo:repository(owner:\"%s\",name:\"%s\") { id nameWithOwner ",ts.owner,ts.repo);
}
void ts_query(int readback)
{
    int i;
    ts_reset(); ts_add("query { "); ts_repo_query();
    if (readback) ts_printf("snapshot:object(oid:\"%s\") { __typename oid ... on Commit { ",ts.commit_oid);
    else ts_printf("base:ref(qualifiedName:\"refs/heads/%s\") { name prefix target { __typename oid ... on Commit { ",ts.base);
    for (i=0;i<ts.count;++i)
        ts_printf("f%d:file(path:\"%s\") { path mode type object { __typename oid ... on Blob { byteSize isBinary isTruncated text } } } ",i,ts.files[i].path);
    for (i=0;i<ts.dir_count;++i)
        ts_printf("d%d:file(path:\"%s\") { path mode type } ",i,ts.dirs[i]);
    if (readback) {
        ts_add("} } ");
        ts_printf("published:ref(qualifiedName:\"refs/heads/%s\") { name prefix target { oid } } ",ts.branch);
    } else {
        ts_add("} } } ");
        ts_printf("taken:ref(qualifiedName:\"refs/heads/%s\") { id } ",ts.branch);
    }
    ts_add("} }"); ts_wrap_query();
    ts_save_output(readback?"04-readback.txt":"01-check.txt");
}
static int ts_response(const char *suffix, char hash[65])
{
    int root=ts_load_json(suffix,hash);
    ts_need(ts_optional(root,"errors")<0,"GitHub returned errors; no permission inferred");
    return ts_get(root,"data");
}
static void ts_repository(int node)
{
    char expected[194];
    snprintf(expected,sizeof expected,"%s/%s",ts.owner,ts.repo);
    ts_expect(ts_get(node,"nameWithOwner"),expected);
}
static void ts_oid_copy(char *dst, size_t cap, int node)
{
    const char *s=ts_string(node); size_t i,n=strlen(s);
    ts_need(n==40 || n==64,"invalid Git object ID length");
    for (i=0;i<n;++i) ts_need(isxdigit((unsigned char)s[i]),"invalid Git object ID");
    ts_copy(dst,cap,s);
}
static void ts_branch_identity(int node, const char *name)
{
    ts_expect(ts_get(node,"name"),name); ts_expect(ts_get(node,"prefix"),"refs/heads/");
}
/* Returns 0 same, 1 new, 2 changed. Anything uncertain is a hard stop. */
static int ts_compare_entry(int entry, int i)
{
    SubmitFile *f=&ts.files[i]; int object,text,flag,bytes;
    if (ts_null(entry)) { ts.old_sha[i][0]=0; return 1; }
    ts_expect(ts_get(entry,"path"),f->path);
    ts_expect(ts_get(entry,"type"),"blob");
    ts_need(ts_number(ts_get(entry,"mode"))==33188,"destination is not a regular 100644 file");
    object=ts_get(entry,"object"); ts_expect(ts_get(object,"__typename"),"Blob");
    flag=ts_get(object,"isBinary"); ts_need(ts_json[flag].type=='f',"remote file is binary or encoding is unknown");
    flag=ts_get(object,"isTruncated"); ts_need(ts_json[flag].type=='f',"remote file is truncated");
    bytes=ts_number(ts_get(object,"byteSize")); text=ts_get(object,"text");
    ts_need((size_t)bytes==strlen(ts_string(text)),"remote byteSize differs from returned text");
    ts_sha256(ts_string(text),(size_t)bytes,ts.old_sha[i]);
    return strcmp(ts.old_sha[i],f->sha)?2:0;
}
int ts_remote(int readback)
{
    int data,repo,commit,i,changes=0; char key[32];
    data=ts_response(readback?"04-response.txt":"01-response.txt",readback?NULL:ts.response_sha);
    repo=ts_get(data,"repo"); ts_repository(repo);
    JC(ts.repository_id,repo,"id");
    if (readback) {
        int published=ts_get(repo,"published");
        commit=ts_get(repo,"snapshot");
        ts_expect(ts_get(commit,"oid"),ts.commit_oid);
        ts_branch_identity(published,ts.branch);
        ts_expect(ts_get(ts_get(published,"target"),"oid"),ts.commit_oid);
    } else {
        int base=ts_get(repo,"base"); ts_branch_identity(base,ts.base);
        ts_need(ts_null(ts_get(repo,"taken")),"review branch already exists; choose a fresh branch and batch");
        commit=ts_get(base,"target");
        ts_oid_copy(ts.base_oid,sizeof ts.base_oid,ts_get(commit,"oid"));
    }
    ts_expect(ts_get(commit,"__typename"),"Commit");
    for (i=0;i<ts.dir_count;++i) {
        int entry; snprintf(key,sizeof key,"d%d",i); entry=ts_get(commit,key);
        if (ts_null(entry)) { ts_need(!readback,"readback parent directory missing"); continue; }
        ts_expect(ts_get(entry,"path"),ts.dirs[i]); ts_expect(ts_get(entry,"type"),"tree");
        ts_need(ts_number(ts_get(entry,"mode"))==16384,"unsafe parent: symlink/submodule/file");
    }
    for (i=0;i<ts.count;++i) {
        int state; snprintf(key,sizeof key,"f%d",i);
        state=ts_compare_entry(ts_get(commit,key),i);
        if (readback) ts_need(state==0,"readback content missing or SHA-256 differs");
        else { ts.state[i]=state; changes+=state!=0; }
    }
    return changes;
}
void ts_receipt(void)
{
    int i;
    ts_reset(); ts_printf("SHAKTI SUBMISSION RECEIPT: %s\n",ts.batch);
    ts_add("STATUS: returned text bytes match frozen SHA-256 for every listed file.\n");
    ts_printf("Repository: %s/%s\nReview branch: %s\nCommit: %s\n",ts.owner,ts.repo,ts.branch,ts.commit_oid);
    ts_printf("GitHub committedDate: %s\nBase commit: %s\n",ts.committed,ts.base_oid);
    ts_printf("Snapshot SHA-256: %s\nApproval SHA-256: %s\n",ts.plan_sha,ts.approval_sha);
    ts_add("Transport: user-operated iCurlHTTP; response origin is not independently authenticated by this offline program.\n");
    ts_add("No merge, deletion, force update or supervisor acceptance was performed.\n\n");
    for (i=0;i<ts.count;++i) ts_printf("%s\n  SHA-256 %s\n",ts.files[i].path,ts.files[i].sha);
    ts_save_output("receipt.txt");
}
