/* Tool.Requests.h -- explicit approval -> branch / commit request bodies.
 * No HTTP client. Mutation text is emitted only after local policy gates.
 * A new branch is separate from the atomic file-set commit; a failed commit
 * can leave that empty review branch. No rollback deletion is generated.
 */
void ts_approval_write(void)
{
    int i;
    ts_reset(); ts_add("{"); ts_pair("format","ShaktiApproval/1");
    ts_pair("plan_sha256",ts.plan_sha); ts_pair("response_sha256",ts.response_sha);
    ts_pair("base_oid",ts.base_oid); ts_pair("branch",ts.branch);
    ts_add("\"states\":[");
    for (i=0;i<ts.count;++i) ts_printf("%s%d",i?",":"",ts.state[i]);
    ts_add("]}\n"); ts_sha256(ts_output,ts_length,ts.approval_sha);
    ts_save_output("approval.txt");
}
void ts_approval_load(void)
{
    int root,a,p,i=0;
    (void)ts_remote(0);
    root=ts_load_json("approval.txt",ts.approval_sha);
    ts_expect(ts_get(root,"format"),"ShaktiApproval/1");
    ts_expect(ts_get(root,"plan_sha256"),ts.plan_sha);
    ts_expect(ts_get(root,"response_sha256"),ts.response_sha);
    ts_expect(ts_get(root,"base_oid"),ts.base_oid); ts_expect(ts_get(root,"branch"),ts.branch);
    a=ts_get(root,"states"); ts_need(ts_json[a].type=='[',"approval states required");
    for (p=ts_json[a].child;p>=0 && p<ts_json[a].next;p=ts_json[p].next) {
        ts_need(i<ts.count && ts_number(p)==ts.state[i],"approval does not match current check"); ++i;
    }
    ts_need(i==ts.count,"incomplete approval record");
}
static void ts_mutation_start(const char *query)
{
    ts_reset(); ts_add("{\"query\":"); ts_quote(query);
    ts_add(",\"variables\":{\"input\":{");
    ts_pair("clientMutationId",ts.approval_sha);
}
void ts_branch_request(void)
{
    char name[160]; snprintf(name,sizeof name,"refs/heads/%s",ts.branch);
    ts_mutation_start("mutation($input:CreateRefInput!) { createRef(input:$input) { clientMutationId ref { name prefix repository { nameWithOwner } target { oid } } } }");
    ts_pair("repositoryId",ts.repository_id); ts_pair("name",name);
    ts_add("\"oid\":"); ts_quote(ts.base_oid); ts_add("}}}\n");
    ts_save_output("02-branch.txt");
}
void ts_branch_response(void)
{
    int d=ts_response("02-response.txt",NULL),result,ref;
    result=ts_get(d,"createRef"); ts_expect(ts_get(result,"clientMutationId"),ts.approval_sha);
    ref=ts_get(result,"ref"); ts_branch_identity(ref,ts.branch);
    ts_repository(ts_get(ref,"repository"));
    ts_expect(ts_get(ts_get(ref,"target"),"oid"),ts.base_oid);
}
void ts_upload_request(void)
{
    int i,n=0; char repository[194];
    snprintf(repository,sizeof repository,"%s/%s",ts.owner,ts.repo);
    ts_mutation_start("mutation($input:CreateCommitOnBranchInput!) { createCommitOnBranch(input:$input) { clientMutationId commit { oid committedDate repository { nameWithOwner } parents(first:2) { totalCount nodes { oid } } } ref { name prefix target { oid } } } }");
    ts_pair("expectedHeadOid",ts.base_oid);
    ts_add("\"branch\":{"); ts_pair("repositoryNameWithOwner",repository);
    ts_add("\"branchName\":"); ts_quote(ts.branch); ts_add("},\n\"message\":{\"headline\":");
    ts_quote(ts.batch); ts_add(",\"body\":"); ts_quote(ts.plan_sha); ts_add("},\n\"fileChanges\":{\"additions\":[");
    for (i=0;i<ts.count;++i) if (ts.state[i]) {
        SubmitFile *f=&ts.files[i]; if (n++) ts_add(",");
        ts_add("{\"path\":"); ts_quote(f->path); ts_add(",\"contents\":\"");
        ts_base64((const unsigned char *)ts_pool+f->offset,f->bytes); ts_add("\"}");
    }
    ts_need(n>0,"nothing changed; no commit is needed");
    ts_add("]}}}}\n"); ts_save_output("03-upload.txt");
}
void ts_upload_response(void)
{
    int d=ts_response("03-response.txt",NULL),result,commit,parents,nodes,ref,p;
    result=ts_get(d,"createCommitOnBranch");
    ts_expect(ts_get(result,"clientMutationId"),ts.approval_sha);
    commit=ts_get(result,"commit"); ts_repository(ts_get(commit,"repository"));
    ts_oid_copy(ts.commit_oid,sizeof ts.commit_oid,ts_get(commit,"oid"));
    ts_need(strcmp(ts.commit_oid,ts.base_oid)!=0,"commit did not advance");
    JC(ts.committed,commit,"committedDate");
    ts_need(strlen(ts.committed)>=20,"missing server commit date");
    parents=ts_get(commit,"parents"); ts_need(ts_number(ts_get(parents,"totalCount"))==1,"unexpected commit parent count");
    nodes=ts_get(parents,"nodes"); ts_need(ts_json[nodes].type=='[',"parent nodes array required");
    p=ts_json[nodes].child;
    ts_need(p>=0 && ts_json[p].next==ts_json[nodes].next,"unexpected parent list");
    ts_expect(ts_get(p,"oid"),ts.base_oid);
    ref=ts_get(result,"ref"); ts_branch_identity(ref,ts.branch);
    ts_expect(ts_get(ts_get(ref,"target"),"oid"),ts.commit_oid);
}
