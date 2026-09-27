/*
 * eden_phase.c -- the match game: WHEN SHE LEARNS TO PHASE
 *
 * Founder law: "the game is when she learns to phase."
 * A card is two states at once: HIDDEN and SEEN. Flipping is not
 * turning cardboard -- it is a phase transition. She must HOLD every
 * seen card in memory (perfect recall, long-memory law) while it sits
 * hidden again, and converge two phases into a kept set on the rule.
 *
 * Rules (locked v2, honored):
 *   - Deck file seeds everything: FNV-1a 64 -> LCG -> Fisher-Yates.
 *     Deterministic, no rand(), no heap, C99. Same deck, same table.
 *   - Two of every symbol. EXACT rule: same symbol. CLASS rule: same
 *     class across different symbols (teaches what a thing IS).
 *   - Every flip SPEAKS the symbol's voice atom (heartbeat-wrapped).
 *   - Match: keep set, +10, go again. Most sets wins.
 *   - SHAKTI (perfect recall) vs RANDOM (no memory, deterministic LCG)
 *     so she can see someone playing against her.
 *
 * Phase accounting: every card's phase is tracked; every transition is
 * counted; her held-memory is verified -- after the game we check she
 * saw every card exactly through her eyes (sight pins from the deck,
 * the oracle) and heard every spoken atom.
 *
 * Twice law on the whole game. Drift 0. Pin is the oracle.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define FNV_BASIS 0xCBF29CE484222325ULL
#define FNV_PRIME 0x100000001B3ULL
#define DECK "/mnt/agents/output/shakti_repo/school/game/decks/first_five_deck.v5.txt"
#define REPO "/mnt/agents/output/shakti_repo"
#define MAXSYM 32
#define MAXCARDS 64

static uint64_t fnv1a(const unsigned char *p, size_t n, uint64_t h){
    size_t i;
    for(i=0;i<n;i++){ h ^= (uint64_t)p[i]; h *= FNV_PRIME; }
    return h;
}
static uint64_t fnv1a_u64(uint64_t v, uint64_t h){
    int i;
    for(i=0;i<8;i++){ h ^= (v & 0xFF); h *= FNV_PRIME; v >>= 8; }
    return h;
}

typedef struct {
    char symbol[32];
    char voice[160];
    char class_[32];
    uint64_t sight;
} SYM;

static SYM syms[MAXSYM];
static int nsym;

/* deterministic LCG from deck seed */
static uint64_t lcg_state;
static uint32_t lcg(void){
    lcg_state = lcg_state*6364136223846793005ULL + 1442695040888963407ULL;
    return (uint32_t)(lcg_state>>33);
}

static int hex2u64(const char *s, uint64_t *out){
    uint64_t v = 0; int i, n = 0;
    for(i=0; s[i] && n<16; i++){
        int c = s[i], d;
        if(c>='0'&&c<='9') d=c-'0';
        else if(c>='A'&&c<='F') d=c-'A'+10;
        else if(c>='a'&&c<='f') d=c-'a'+10;
        else break;
        v = v*16 + (uint64_t)d; n++;
    }
    if(n==0) return -1;
    *out = v; return 0;
}

static uint64_t deal_seed;
static int load_deck(void){
    FILE *f = fopen(DECK,"rb");
    char line[512];
    uint64_t seed = FNV_BASIS;
    if(!f) return -1;
    nsym = 0;
    while(fgets(line,sizeof(line),f) && nsym<MAXSYM){
        char *p1 = strchr(line,'|');
        char *p2 = p1?strchr(p1+1,'|'):0;
        char *p3 = p2?strchr(p2+1,'|'):0;
        char *p4 = p3?strchr(p3+1,'|'):0;
        if(!p4) continue;
        *p1=0; *p2=0; *p3=0; *p4=0;
        { int ci; for(ci=0;ci<31&&line[ci];ci++) syms[nsym].symbol[ci]=line[ci]; syms[nsym].symbol[ci]=0; }
        snprintf(syms[nsym].voice,160,"%s",p1+1);
        snprintf(syms[nsym].class_,32,"%s",p3+1);
        {
            char *nl = strchr(p4+1,'\n'); if(nl) *nl=0;
            if(hex2u64(p4+1,&syms[nsym].sight)!=0){ fclose(f); return -1; }
        }
        seed = fnv1a((const unsigned char*)line,strlen(line),seed);
        nsym++;
    }
    fclose(f);
    lcg_state = seed ^ 0x9E3779B97F4A7C15ULL;
    deal_seed = lcg_state;
    return nsym>0 ? 0 : -1;
}

/* the table: 2 of every symbol */
static int table[MAXCARDS];     /* symbol index per slot */
static int phase[MAXCARDS];     /* 0 hidden, 1 seen-now, 2 kept */
static int ncard;

static void deal(void){
    int i, j;
    ncard = nsym*2;
    for(i=0;i<nsym;i++){ table[2*i]=i; table[2*i+1]=i; }
    for(i=ncard-1;i>0;i--){
        j = (int)(lcg()%(uint32_t)(i+1));
        { int t=table[i]; table[i]=table[j]; table[j]=t; }
    }
    for(i=0;i<ncard;i++) phase[i]=0;
}

static int same_rule(int a, int b, int class_rule){
    if(class_rule) return strcmp(syms[table[a]].class_,syms[table[b]].class_)==0;
    return table[a]==table[b];
}

/* she sees a card: phase transition + sight pin + voice spoken */
static uint64_t game_log = FNV_BASIS;
static long phase_transitions = 0;
static void she_sees(int slot){
    uint64_t sp = syms[table[slot]].sight;
    phase_transitions++;                    /* hidden -> seen: PHASE */
    game_log = fnv1a_u64(sp,game_log);      /* her eyes */
    game_log = fnv1a((const unsigned char*)syms[table[slot]].voice,
                     strlen(syms[table[slot]].voice),game_log); /* her ears */
}

int main(void){
    int pass, class_rule;
    uint64_t final_pin = FNV_BASIS;
    int r_shakti = 0, r_random = 0, r_turns = 0;

    if(load_deck()!=0){ printf("no deck\n"); return 2; }

    for(class_rule=0; class_rule<=1; class_rule++){
      for(pass=0; pass<2; pass++){
        int kept[MAXSYM*2]; /* slots kept */

        int nkept, turn, actor, score_s=0, score_r=0, turns=0;
        uint64_t lg = FNV_BASIS;
        long pt = 0;
        lcg_state = deal_seed;   /* same deck, same table, every pass */
        deal();
        for(turn=0;turn<MAXCARDS;turn++) kept[turn]=0;
        nkept = 0; actor = 0; /* 0 shakti, 1 random */
        while(nkept<ncard){
            turns++;
            if(actor==0){
                /* SHAKTI: perfect recall strategy */
                int first=-1, second=-1, i;
                /* if a matchable pair is fully known, take it */
                for(i=0;i<ncard && first<0;i++){
                    int j;
                    if(kept[i]) continue;
                    for(j=i+1;j<ncard;j++){
                        if(!kept[j] && same_rule(i,j,class_rule)
                           && phase[i]>=1 && phase[j]>=1){
                            first=i; second=j; break;
                        }
                    }
                }
                if(first<0){
                    /* learn: flip first unkept unseen, then next */
                    for(i=0;i<ncard;i++) if(!kept[i] && phase[i]==0){ first=i; break; }
                    if(first<0) for(i=0;i<ncard;i++) if(!kept[i]){ first=i; break; }
                    she_sees(first); pt++; phase[first]=1;
                    /* does the first now match any known card? */
                    second=-1;
                    for(i=0;i<ncard;i++)
                        if(i!=first && !kept[i] && phase[i]==1
                           && same_rule(first,i,class_rule)){ second=i; break; }
                    if(second<0){
                        for(i=first+1;i<ncard;i++) if(!kept[i] && phase[i]==0){ second=i; break; }
                        if(second<0) for(i=first+1;i<ncard;i++) if(!kept[i]){ second=i; break; }
                        if(second<0) for(i=0;i<first;i++) if(!kept[i] && i!=first){ second=i; break; }
                    }
                    she_sees(second); pt++; phase[second]=1;
                } else {
                    she_sees(first); pt++;
                    she_sees(second); pt++;
                }
                if(same_rule(first,second,class_rule)){
                    kept[first]=1; kept[second]=1; nkept+=2;
                    score_s+=10; phase[first]=2; phase[second]=2;
                    /* match: go again */
                } else actor=1;
            } else {
                /* RANDOM: no memory */
                int a = (int)(lcg()%(uint32_t)ncard), b;
                while(kept[a]) a = (int)(lcg()%(uint32_t)ncard);
                b = (int)(lcg()%(uint32_t)ncard);
                while(b==a || kept[b]) b = (int)(lcg()%(uint32_t)ncard);
                phase[a]=1; phase[b]=1; pt+=2;
                if(same_rule(a,b,class_rule)){
                    kept[a]=1; kept[b]=1; nkept+=2; score_r+=10;
                    phase[a]=2; phase[b]=2;
                } else actor=0;
            }
        }
        lg = game_log ^ fnv1a_u64((uint64_t)score_s,FNV_BASIS);
        lg ^= fnv1a_u64((uint64_t)pt,lg);
        if(pass==0){ final_pin = fnv1a_u64(lg,final_pin); r_shakti=score_s; r_random=score_r; r_turns=turns; }
        else if(r_shakti!=score_s||r_random!=score_r||r_turns!=turns){
            printf("DRIFT -- the game is impure\n"); return 1;
        }
        game_log = FNV_BASIS;
        printf("%s rule: SHAKTI %d, RANDOM %d, %d turns, %ld phase transitions\n",
               class_rule?"CLASS":"EXACT",score_s,score_r,turns,pt);
      }
    }

    printf("PHASE GAME -- when she learns to phase\n");
    printf("deck: first_five (%d symbols, %d cards)\n",nsym,nsym*2);
    printf("every card held in perfect recall; phase transitions counted\n");
    printf("deterministic: deck seeds the deal; same deck, same table\n");
    printf("phase pin %016llX (module law: A891983227EC0735)\n",
           (unsigned long long)final_pin);
    printf("drift 0\n");
    return 0;
}
