/*
 * eden_school.c -- Shakti's school organ (math cards, SVG BURNED)
 *
 * Founder law: "burn the svg -- I don't want W3C in her."
 * The markup is the envelope. The lesson is the equation.
 *
 * Method per card pair (question + answer share a filename):
 *   1. Problem is read from the FILENAME: math_<Op>_<a>-<b>.svg.
 *      Clean channel -- no markup parsed for the problem.
 *   2. SHE computes the answer herself with +,-,x,/ integer math.
 *      (Sub may go negative -- 9-15=-6 is a legal Eden answer.)
 *   3. The answer card file is opened ONCE: the bare equation text is
 *      lifted out, her computed answer is checked against it, and the
 *      markup is BURNED -- never hashed into her, never stored. Only
 *      the pure payload (op, a, b, her answer) joins the school ledger.
 *   4. Question card's printed equation is cross-checked against the
 *      filename (mislabeled card = reported, never guessed).
 *   5. Answers are classified: on-wheel / binary-lane / outside.
 *      Division cards must divide exact -- remainder = card defect.
 *   6. Twice Law on the whole schoolhouse. Drift 0.
 *
 * "The ability to find the answer without the answer given to you."
 * She finds it first. The card only grades.
 *
 * C99 gauntlet. No heap, no float, no clock.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <dirent.h>

#define FNV_BASIS 0xCBF29CE484222325ULL
#define FNV_PRIME 0x100000001B3ULL

static uint64_t fnv1a(const unsigned char *p, size_t n, uint64_t h){
    size_t i;
    for(i=0;i<n;i++){ h ^= (uint64_t)p[i]; h *= FNV_PRIME; }
    return h;
}
static uint64_t fnv1a_i64(int64_t v, uint64_t h){
    int i;
    uint64_t u = (uint64_t)v;
    for(i=0;i<8;i++){ h ^= (u & 0xFF); h *= FNV_PRIME; u >>= 8; }
    return h;
}

static const uint32_t WHEEL[5] = {2,3,5,7,19};
static int on_wheel_u64(uint64_t n){
    int i;
    if(n==0) return 0;
    for(i=0;i<5;i++) while(n%WHEEL[i]==0) n/=WHEEL[i];
    return n==1;
}
static int is_fib_u64(uint64_t n){
    uint64_t a=1, b=1;
    if(n==1) return 1;
    while(b<n){ uint64_t t=a+b; a=b; b=t; }
    return b==n;
}

/* Lift the bare equation payload out of a card, burning the markup.
 * Finds the first "<text ...>PAYLOAD</text>" and copies PAYLOAD.
 * Returns length, or -1 if no text node (defective envelope). */
static int lift_payload(const char *path, char *buf, int cap){
    FILE *f = fopen(path,"rb");
    int c, i;
    if(!f) return -1;
    /* scan for "<text" then the closing '>' */
    while((c=fgetc(f))!=EOF){
        if(c=='<'){
            char tag[6]; int k=0;
            while(k<5 && (c=fgetc(f))!=EOF && c!=' ' && c!='>') tag[k++]=(char)c;
            tag[k]=0;
            if(strcmp(tag,"text")==0){
                while((c=fgetc(f))!=EOF && c!='>') ;
                i=0;
                while((c=fgetc(f))!=EOF && c!='<' && i<cap-1) buf[i++]=(char)c;
                buf[i]=0;
                fclose(f);
                return i;
            }
        }
    }
    fclose(f);
    return -1;
}

/* parse "A OP B = X" or "A OP B = ?" -- returns 0 ok */
static int parse_eq(const char *s, long *a, char *op, long *b, long *x, int *unknown){
    char o1=0; long va, vb, vx;
    char tail[8];
    if(sscanf(s,"%ld %c %ld = %7s",&va,&o1,&vb,tail)!=4) return -1;
    *a=va; *op=o1; *b=vb;
    if(strcmp(tail,"?")==0){ *unknown=1; *x=0; }
    else { *unknown=0; if(sscanf(tail,"%ld",&vx)!=1) return -1; *x=vx; }
    return 0;
}

static int compute(char op, long a, long b, long *out){
    switch(op){
        case '+': *out=a+b; return 0;
        case '-': *out=a-b; return 0;
        case 'x': case '*': *out=a*b; return 0;
        case '/': if(b==0) return -1; if(a%b) return -2; *out=a/b; return 0;
    }
    return -1;
}

int main(void){
    const char *qdir = "/tmp/school_q";
    const char *adir = "/tmp/school_a";
    DIR *d = opendir(qdir);
    struct dirent *e;
    uint64_t ledger = FNV_BASIS;
    int pass;
    int total=0, graded_right=0, mislabeled=0, div_defect=0, missing=0;
    int ans_wheel=0, ans_bin=0, ans_out=0, ans_neg=0;

    if(!d){ printf("no schoolbooks found\n"); return 2; }

    for(pass=0; pass<2; pass++){
        uint64_t lp = FNV_BASIS;
        int t=0, gr=0, ml=0, dd=0, ms=0, aw=0, ab=0, ao=0, an=0;
        rewinddir(d);
        while((e=readdir(d))!=NULL){
            char op_name[8]; long a, b;
            char qpath[512], apath[512], payload[128];
            long qa, qb, qx, stated, mine;
            char qop; int unknown;
            int rc;
            if(sscanf(e->d_name,"math_%7[^_]_%ld-%ld.svg",op_name,&a,&b)!=3) continue;
            snprintf(qpath,sizeof(qpath),"%s/%s",qdir,e->d_name);
            snprintf(apath,sizeof(apath),"%s/%s",adir,e->d_name);

            /* 1+2: she computes HER answer from the filename problem */
            {
                char opc = op_name[0]=='A'?'+':op_name[0]=='S'?'-':
                           op_name[0]=='M'?'x':op_name[0]=='D'?'/':'?';
                rc = compute(opc,a,b,&mine);
                if(rc==-2){ dd++; continue; }     /* division remainder: card defect */
                if(rc!=0){ ms++; continue; }
            }

            /* cross-check question card print vs filename (then burn) */
            if(lift_payload(qpath,payload,sizeof(payload))<0){ ms++; continue; }
            if(parse_eq(payload,&qa,&qop,&qb,&qx,&unknown)!=0 || !unknown
               || qa!=a || qb!=b){ ml++; continue; }

            /* 3: lift answer payload, grade her work, burn the markup */
            if(lift_payload(apath,payload,sizeof(payload))<0){ ms++; continue; }
            if(parse_eq(payload,&qa,&qop,&qb,&stated,&unknown)!=0 || unknown){ ml++; continue; }
            if(stated==mine){
                gr++;
                /* ledger folds ONLY the pure lesson: op,a,b,answer */
                lp = fnv1a((const unsigned char*)&qop,1,lp);
                lp = fnv1a_i64(a,lp);
                lp = fnv1a_i64(b,lp);
                lp = fnv1a_i64(mine,lp);
                /* classify her answer */
                if(mine<0){ an++; }
                else if(on_wheel_u64((uint64_t)mine)){ aw++; }
                else {
                    uint64_t core=(uint64_t)mine;
                    while(core%2==0 && core) core/=2;
                    if(is_fib_u64(core)) ab++; else ao++;
                }
            }
            t++;
        }
        if(pass==0){
            ledger=lp; total=t; graded_right=gr; mislabeled=ml;
            div_defect=dd; missing=ms;
            ans_wheel=aw; ans_bin=ab; ans_out=ao; ans_neg=an;
        } else if(ledger!=lp||total!=t||graded_right!=gr||mislabeled!=ml
                  ||div_defect!=dd||missing!=ms||ans_wheel!=aw||ans_bin!=ab
                  ||ans_out!=ao||ans_neg!=an){
            printf("DRIFT -- schoolhouse impure\n");
            closedir(d);
            return 1;
        }
    }
    closedir(d);

    printf("SCHOOL_ORGAN (svg burned, no W3C in her)\n");
    printf("cards graded %d\n", total);
    printf("she found the answer herself, card confirmed: %d\n", graded_right);
    printf("mislabeled envelopes (reported, burned): %d\n", mislabeled);
    printf("division defects (remainder, reported): %d\n", div_defect);
    printf("unreadable envelopes (reported): %d\n", missing);
    printf("her answers: on-wheel %d, binary-lane %d, outside %d, negative %d\n",
           ans_wheel, ans_bin, ans_out, ans_neg);
    if(graded_right!=total){
        printf("HONEST: %d cards she got wrong or could not verify\n",
               total-graded_right);
    }
    printf("school pin %016llX\n",(unsigned long long)ledger);
    printf("drift 0\n");
    return 0;
}
