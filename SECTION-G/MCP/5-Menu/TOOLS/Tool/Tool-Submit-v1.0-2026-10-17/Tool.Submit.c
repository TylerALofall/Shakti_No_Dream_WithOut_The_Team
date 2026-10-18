/* SECTION-A -- Tool.Submit.c v1.0
 * MODIFIED: 2026-10-03
 * QA SPECIALIST: GPT-6 Astra Pro
 * LOCATION: OpenAI Linux working container; C Code Develop 5.2.0 pending
 * SUPERVISOR QA: Kimi K3 (not performed; independent review pending)
 * SUPERVISOR QA LOCATION: Not supplied
 * SUPERVISOR NOTES:
 * *****
 *
 * *****
 * INSTRUCTIONS:
 * 1. Compile this file WITH Tool.Support.c and the supplied .h files.
 *    cc -std=c99 -pedantic -Wall -Wextra -Werror -O2
 *       Tool.Submit.c Tool.Support.c -o shakti-submit
 * 2. In C Code Develop, turn legacy interpreter and predefined stdin off.
 *    Do not add the assembler or payload .c files to this build.
 * 3. Run with no arguments; choose 1 before any other operation.
 * 4. No network calls occur. iCurlHTTP sends only the request you select.
 * 5. New batch names are mandatory; no output is silently overwritten.
 *    Unknown paths, missing hooks, malformed responses and overflows BENCH.
 * Optional desktop syntax: shakti-submit ACTION ARGUMENT
 * Actions: check, demo, prepare FORM, approve BATCH, commit BATCH,
 *          readback BATCH, verify BATCH, show FILE.
 */
#include "Tool.Submit.h"
/* Read one decision. EOF, excess input and an empty answer never mean yes. */
static void answer(const char *prompt, char *text, size_t cap)
{
    size_t n;
    fputs(prompt,stdout); fflush(stdout);
    ts_need(fgets(text,(int)cap,stdin)!=NULL,"no keyboard input; turn predefined stdin off");
    n=strlen(text);
    ts_need(n && text[n-1]=='\n',"incomplete or oversized answer");
    text[--n]=0;
    if (n && text[n-1]=='\r') text[--n]=0;
    ts_need(n>0,"empty answer cancels the operation");
}
/* A toy source is a TEXT payload, not another compiled main function. */
void ts_demo(void)
{
    const char *source="#include <stdio.h>\nint main(void)\n{\n    puts(\"Demo only\");\n    return 0;\n}\n";
    const char *sample="Example input: none\nExpected output: Demo only\nNot run evidence.\n";
    const char *form=
        "# OFFLINE EXAMPLE. Do not send its requests to GitHub.\n"
        "MODE=SINGLE\nBATCH=demo_001\nOWNER=ExampleOwner\nREPOSITORY=OfflineExample\n"
        "BASE=main\nBRANCH=submit/demo-001\nROOT=Example_Tools\n"
        "HASHER=sha256\nLOGGER=inventory\nCHECK=none\n\n"
        "FAMILY=Demo-Example\nVERSION=1.0\nDATE=2026-10-03\n"
        "README_STYLE=SIX\nREADME_SOURCE=GENERATE\n"
        "DESCRIPTION=Disposable submission example; not a live project.\n"
        "RELATED=Local preparer reads these files; iCurlHTTP transports approved request text.\n"
        "RUN=(empty - no proven runs recorded)\nCHANGED=Initial demonstration only.\n"
        "REQUIREMENTS=C99 for the example source.\nEXAMPLE=No input.\nEXPECTED=Demo only\n"
        "REFERENCES=example.txt contains the illustrative input and expected output.\n"
        "FILE=Demo.Source.c.txt|Demo.Example.c|2026-10-03\n"
        "FILE=Demo.Sample.txt|example.txt|2026-10-03\nEND_FAMILY\n";
    ts_save("Demo.Source.c.txt",source,strlen(source));
    ts_save("Demo.Sample.txt",sample,strlen(sample));
    ts_save("Demo.Single.txt",form,strlen(form));
    puts("Demo files created. Next run: Prepare, using Demo.Single.txt.\nDo not send demo requests to GitHub.");
}
static void prepare(const char *form)
{
    ts_form(form); ts_collect(); ts_plan_write(); ts_inventory(); ts_query(0);
    puts("PREPARED LOCALLY. No upload request exists yet.");
    printf("Next: send %s.01-check.txt as a read-only GraphQL query in iCurlHTTP.\n",ts.batch);
    printf("Save only the JSON response body as %s.01-response.txt, then choose Approve.\n",ts.batch);
}
static void approve(const char *batch)
{
    char text[160],expected[160]; int i,changes;
    ts_plan_load(batch); changes=ts_remote(0);
    printf("Repository: %s/%s\nNew review branch: %s\nChecked base: %s\n",ts.owner,ts.repo,ts.branch,ts.base_oid);
    for (i=0;i<ts.count;++i) {
        printf("\n%d. %s\n%s\n",i+1,ts.files[i].path,
               ts.state[i]==0?"UNCHANGED":ts.state[i]==1?"NEW":"CONFLICT - different content exists");
        if (ts.state[i]==2) {
            printf("Existing SHA-256: %s\nPrepared SHA-256: %s\n",ts.old_sha[i],ts.files[i].sha);
            printf("Replacement is on the new review branch only. Main is untouched.\n");
            snprintf(expected,sizeof expected,"REPLACE %d",i+1);
            printf("To approve this exact file, type %s. Anything else cancels.\n",expected);
            answer("> ",text,sizeof text);
            ts_need(!strcmp(text,expected),"replacement not approved; change the form destination or cancel");
        }
    }
    if (!changes) {
        puts("ALL UNCHANGED. No branch or commit is needed."); return;
    }
    snprintf(expected,sizeof expected,"PREPARE %s",ts.batch);
    printf("\n%d changed/new files. Type %s to prepare the branch request, not to send it.\n",changes,expected);
    answer("> ",text,sizeof text); ts_need(!strcmp(text,expected),"submission not approved");
    ts_approval_write(); ts_branch_request();
    printf("Next: send %s.02-branch.txt. This creates ONLY the new review branch.\n",ts.batch);
    printf("Save the JSON body as %s.02-response.txt, then choose Commit request.\n",ts.batch);
}
/* The assembler may call this entry directly in a reviewed linked build. */
int shakti_submit_action(const char *action, const char *argument)
{
    ts_probe();
    if (!strcmp(action,"check")) {
        puts("LOCAL COMPATIBILITY PASS. No credentials or GitHub are needed.");
    } else if (!strcmp(action,"demo")) ts_demo();
    else if (!strcmp(action,"prepare")) prepare(argument);
    else if (!strcmp(action,"approve")) approve(argument);
    else if (!strcmp(action,"commit") || !strcmp(action,"readback") || !strcmp(action,"verify")) {
        ts_plan_load(argument); ts_approval_load(); ts_branch_response();
        if (!strcmp(action,"commit")) {
            ts_upload_request();
            printf("Next: send %s.03-upload.txt. This commits the reviewed file set.\n",ts.batch);
            printf("Save the JSON body as %s.03-response.txt, then choose Read-back request.\n",ts.batch);
        } else {
            ts_upload_response();
            if (!strcmp(action,"readback")) {
                ts_query(1);
                printf("Next: send %s.04-readback.txt. Save its JSON body as %s.04-response.txt.\n",ts.batch,ts.batch);
                puts("Then choose Verify returned files.");
            } else {
                (void)ts_remote(1); ts_receipt();
                puts("VERIFIED RETURNED BYTES. See the receipt. No merge was performed.");
            }
        }
    } else if (!strcmp(action,"show")) {
        size_t n=ts_read(argument,ts_input,TS_BUFFER+1);
        puts("BEGIN FILE (copy the file content only)");
        ts_need(fwrite(ts_input,1,n,stdout)==n,"console write failed");
        puts("\nEND FILE");
    } else ts_die("unknown action");
    return 0;
}
#ifndef SHAKTI_SUBMIT_NO_MAIN
int main(int argc, char **argv)
{
    static const char *actions[]={"check","demo","prepare","approve","commit","readback","verify","show"};
    char choice[8],argument[TS_PATH]=""; unsigned index;
    puts("SHAKTI TOOL SUBMISSION 1.0 -- OFFLINE PREPARER");
    if (argc==2 || argc==3) return shakti_submit_action(argv[1],argc==3?argv[2]:"");
    ts_need(argc==1,"unexpected arguments; clear Invoke main() args");
    puts("1. Check this phone\n2. Create an offline example\n3. Prepare a form\n4. Review GitHub check / approve\n5. Prepare commit request\n6. Prepare read-back request\n7. Verify returned files\n8. Show a saved file\n0. Stop");
    answer("Choose a number, then Return: ",choice,sizeof choice);
    if (!strcmp(choice,"0")) return 0;
    ts_need(strlen(choice)==1 && choice[0]>='1' && choice[0]<='8',"choose one number 0 through 8");
    index=(unsigned)(choice[0]-'1');
    if (index>=2) answer(index==2?"Form filename: ":index==7?"Filename: ":"Batch name: ",argument,sizeof argument);
    return shakti_submit_action(actions[index],argument);
}
#endif
