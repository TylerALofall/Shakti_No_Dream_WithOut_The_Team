/*
 * Global.Hasher.c  ->  shakti-hasher
 *
 * The global hasher and logger. Walks a root directory POST-ORDER:
 * every subdirectory is fully hashed before its parent writes its log,
 * so no parent log is ever blind to what sits beneath it.
 *
 * Per directory:  00-hash_log.txt   (sha256 + name, sorted, one per file)
 * At the root:    00-master_hash_log.txt (one status line per directory,
 *                  plus MISSING lines for registry paths not found)
 *
 * Registry check: every file whose path appears in the registry must
 * hash to the registry's LATEST line for that path (append-only law:
 * the last line wins). Mismatch = FAIL. Registered path absent = MISSING.
 * Present but unregistered = UNREGISTERED (informational, not an error).
 *
 * Laws honored:
 *   zero heap    - all storage is fixed compile-time static
 *   deterministic- sorted listings, no timestamps, same tree = same bytes
 *   loud failure - cap overflows and io errors BENCH with exit 2
 *   no clobber   - writes ONLY the two log names, via .tmp then rename;
 *                  nothing else in the tree is ever opened for writing
 *
 * Usage:  shakti-hasher ROOT_DIR REGISTRY_FILE
 *         shakti-hasher --self-test
 * Exit:   0 all registered files match; 2 any FAIL/MISSING/bench.
 *
 * C99 strict: cc -std=c99 -pedantic -Wall -Wextra -Werror -O2
 */

#define _POSIX_C_SOURCE 200809L

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

/* ---------------- fixed storage ---------------- */

#define MAXP        1024                 /* one path                    */
#define MAXN        256                  /* one entry name              */
#define MAXF        4096                 /* file entries per directory  */
#define MAXSUB      512                  /* subdirs per directory       */
#define MAXD        1024                 /* directories per tree        */
#define MAXDEPTH    32                   /* recursion depth             */
#define MAXREG      8192                 /* registry lines              */
#define MAXREGN     768                  /* registry path length        */
#define MAXLINE     4096                 /* one line buffer             */
#define MAXOUT      (256U * 1024U)       /* one log file                */
#define MAXMASTER   (512U * 1024U)       /* master log                  */
#define RDCHUNK     65536                /* file read chunk             */

#define DIR_LOG    "00-hash_log.txt"
#define MASTER_LOG "00-master_hash_log.txt"

typedef struct { char name[MAXN]; int isdir; } ent_t;

typedef struct {
    char     path[MAXREGN];
    char     hex[65];
    int      matched;                    /* file found and checked      */
    int      ok;                         /* hash equals                 */
} reg_t;

static ent_t g_ent[MAXF];
static ent_t g_subs[MAXDEPTH][MAXSUB];   /* subdir lists per depth — recursion never shares a frame */
static reg_t g_reg[MAXREG];
static int   g_nreg = 0;

static char  g_out[MAXOUT];              /* one dir log under construction */
static size_t g_olen = 0;

static char  g_master[MAXMASTER];
static size_t g_mlen = 0;

static char  g_rd[RDCHUNK];

static unsigned long g_files = 0, g_regok = 0, g_fail = 0,
                     g_unreg = 0, g_missing = 0, g_dirs = 0;

/* ---------------- sha-256 (FIPS 180-4) ---------------- */

typedef struct {
    uint32_t h[8];
    uint64_t len;
    unsigned char buf[64];
    size_t nbuf;
} sha256_t;

static const uint32_t K[64] = {
    0x428a2f98U,0x71374491U,0xb5c0fbcfU,0xe9b5dba5U,0x3956c25bU,0x59f111f1U,
    0x923f82a4U,0xab1c5ed5U,0xd807aa98U,0x12835b01U,0x243185beU,0x550c7dc3U,
    0x72be5d74U,0x80deb1feU,0x9bdc06a7U,0xc19bf174U,0xe49b69c1U,0xefbe4786U,
    0x0fc19dc6U,0x240ca1ccU,0x2de92c6fU,0x4a7484aaU,0x5cb0a9dcU,0x76f988daU,
    0x983e5152U,0xa831c66dU,0xb00327c8U,0xbf597fc7U,0xc6e00bf3U,0xd5a79147U,
    0x06ca6351U,0x14292967U,0x27b70a85U,0x2e1b2138U,0x4d2c6dfcU,0x53380d13U,
    0x650a7354U,0x766a0abbU,0x81c2c92eU,0x92722c85U,0xa2bfe8a1U,0xa81a664bU,
    0xc24b8b70U,0xc76c51a3U,0xd192e819U,0xd6990624U,0xf40e3585U,0x106aa070U,
    0x19a4c116U,0x1e376c08U,0x2748774cU,0x34b0bcb5U,0x391c0cb3U,0x4ed8aa4aU,
    0x5b9cca4fU,0x682e6ff3U,0x748f82eeU,0x78a5636fU,0x84c87814U,0x8cc70208U,
    0x90befffaU,0xa4506cebU,0xbef9a3f7U,0xc67178f2U
};

static uint32_t rotr(uint32_t x, unsigned n) { return (x >> n) | (x << (32U - n)); }

static void sha_block(sha256_t *s, const unsigned char *p)
{
    uint32_t w[64], a, b, c, d, e, f, g, h;
    unsigned i;
    for (i = 0; i < 16; i++)
        w[i] = ((uint32_t)p[i*4] << 24) | ((uint32_t)p[i*4+1] << 16) |
               ((uint32_t)p[i*4+2] << 8) | (uint32_t)p[i*4+3];
    for (i = 16; i < 64; i++) {
        uint32_t s0 = rotr(w[i-15], 7) ^ rotr(w[i-15], 18) ^ (w[i-15] >> 3);
        uint32_t s1 = rotr(w[i-2], 17) ^ rotr(w[i-2], 19) ^ (w[i-2] >> 10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }
    a = s->h[0]; b = s->h[1]; c = s->h[2]; d = s->h[3];
    e = s->h[4]; f = s->h[5]; g = s->h[6]; h = s->h[7];
    for (i = 0; i < 64; i++) {
        uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
        uint32_t ch = (e & f) ^ (~e & g);
        uint32_t t1 = h + S1 + ch + K[i] + w[i];
        uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
        uint32_t mj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t t2 = S0 + mj;
        h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }
    s->h[0]+=a; s->h[1]+=b; s->h[2]+=c; s->h[3]+=d;
    s->h[4]+=e; s->h[5]+=f; s->h[6]+=g; s->h[7]+=h;
}

static void sha_init(sha256_t *s)
{
    static const uint32_t H0[8] = {
        0x6a09e667U,0xbb67ae85U,0x3c6ef372U,0xa54ff53aU,
        0x510e527fU,0x9b05688cU,0x1f83d9abU,0x5be0cd19U
    };
    memcpy(s->h, H0, sizeof H0);
    s->len = 0; s->nbuf = 0;
}

static void sha_update(sha256_t *s, const unsigned char *p, size_t n)
{
    s->len += (uint64_t)n;
    while (n) {
        size_t take = 64 - s->nbuf;
        if (take > n) take = n;
        memcpy(s->buf + s->nbuf, p, take);
        s->nbuf += take; p += take; n -= take;
        if (s->nbuf == 64) { sha_block(s, s->buf); s->nbuf = 0; }
    }
}

static void sha_final(sha256_t *s, unsigned char out[32])
{
    uint64_t bits = s->len * 8U;
    unsigned char pad = 0x80;
    unsigned char zero = 0x00;
    unsigned char lenb[8];
    unsigned i;
    sha_update(s, &pad, 1);
    while (s->nbuf != 56) sha_update(s, &zero, 1);
    for (i = 0; i < 8; i++) lenb[i] = (unsigned char)(bits >> (56 - i*8));
    sha_update(s, lenb, 8);
    for (i = 0; i < 8; i++) {
        out[i*4]   = (unsigned char)(s->h[i] >> 24);
        out[i*4+1] = (unsigned char)(s->h[i] >> 16);
        out[i*4+2] = (unsigned char)(s->h[i] >> 8);
        out[i*4+3] = (unsigned char)(s->h[i]);
    }
}

static sha256_t g_sha;                   /* one context, reused        */

/* self-test against FIPS 180-4 vectors; returns 0 pass, 1 fail */
static int sha_selftest(void)
{
    static const struct { const char *msg; const char *hex; } V[] = {
        { "",   "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855" },
        { "abc","ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad" },
        { "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
          "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1" },
        { "abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu",
          "cf5b16a778af8380036ce59e7b0492370b249b11e8f07a51afac45037afee9d1" }
    };
    static const char HEXD[] = "0123456789abcdef";
    unsigned char dg[32];
    char got[65];
    size_t v, i;

    for (v = 0; v < sizeof(V)/sizeof(V[0]); v++) {
        sha_init(&g_sha);
        sha_update(&g_sha, (const unsigned char *)V[v].msg, strlen(V[v].msg));
        sha_final(&g_sha, dg);
        for (i = 0; i < 32; i++) {
            got[i*2]   = HEXD[dg[i] >> 4];
            got[i*2+1] = HEXD[dg[i] & 15];
        }
        got[64] = '\0';
        if (strcmp(got, V[v].hex) != 0) {
            fprintf(stderr, "BENCH: sha-256 self-test vector %lu FAILED (%s)\n",
                    (unsigned long)v, got);
            return 1;
        }
    }
    /* million 'a' */
    {
        static const char M_HEX[] =
          "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0";
        char got[65];
        unsigned long i;
        sha_init(&g_sha);
        for (i = 0; i < 1000000UL; i++)
            sha_update(&g_sha, (const unsigned char *)"a", 1);
        sha_final(&g_sha, dg);
        for (i = 0; i < 32; i++) {
            got[i*2]   = HEXD[dg[i] >> 4];
            got[i*2+1] = HEXD[dg[i] & 15];
        }
        got[64] = '\0';
        if (strcmp(got, M_HEX) != 0) {
            fprintf(stderr, "BENCH: sha-256 self-test million-a FAILED (%s)\n", got);
            return 1;
        }
    }
    printf("SHA-256 self-test passed.\n");
    return 0;
}

/* ---------------- small helpers ---------------- */

static void bench(const char *why)
{
    fprintf(stderr, "BENCH: %s\n", why);
    exit(2);
}

static void out_add(const char *s)
{
    size_t n = strlen(s);
    if (g_olen + n >= MAXOUT) bench("dir log over MAXOUT bytes");
    memcpy(g_out + g_olen, s, n); g_olen += n; g_out[g_olen] = '\0';
}

static void mas_add(const char *s)
{
    size_t n = strlen(s);
    if (g_mlen + n >= MAXMASTER) bench("master log over MAXMASTER bytes");
    memcpy(g_master + g_mlen, s, n); g_mlen += n; g_master[g_mlen] = '\0';
}

static int join2(char *dst, size_t cap, const char *a, const char *b)
{
    int n = snprintf(dst, cap, "%s/%s", a, b);
    return (n < 0 || (size_t)n >= cap) ? -1 : 0;
}

static int ent_cmp(const void *pa, const void *pb)
{
    return strcmp(((const ent_t *)pa)->name, ((const ent_t *)pb)->name);
}

/* hash one file to lowercase hex; returns 0 ok */
static int hash_file(const char *path, char hex[65])
{
    static const char HEXD[] = "0123456789abcdef";
    unsigned char dg[32];
    FILE *fp = fopen(path, "rb");
    size_t n, i;
    if (!fp) return -1;
    sha_init(&g_sha);
    while ((n = fread(g_rd, 1, sizeof g_rd, fp)) > 0)
        sha_update(&g_sha, (const unsigned char *)g_rd, n);
    if (ferror(fp)) { fclose(fp); return -1; }
    fclose(fp);
    sha_final(&g_sha, dg);
    for (i = 0; i < 32; i++) {
        hex[i*2]   = HEXD[dg[i] >> 4];
        hex[i*2+1] = HEXD[dg[i] & 15];
    }
    hex[64] = '\0';
    return 0;
}

/* registry lookup: latest line for path wins (append-only law) */
static reg_t *reg_find(const char *relpath)
{
    int i, found = -1;
    for (i = 0; i < g_nreg; i++)
        if (strcmp(g_reg[i].path, relpath) == 0) found = i; /* keep last */
    return found < 0 ? NULL : &g_reg[found];
}

static void reg_load(const char *path)
{
    FILE *fp = fopen(path, "rb");
    char line[MAXLINE];
    if (!fp) bench("cannot open registry");
    while (fgets(line, sizeof line, fp)) {
        char *sp;
        size_t plen;
        if (line[0] == '#') continue;
        sp = strstr(line, "  ");         /* sha256sum format: hex␣␣path */
        if (!sp || sp - line != 64) continue;
        if (g_nreg >= MAXREG) bench("registry over MAXREG lines");
        memcpy(g_reg[g_nreg].hex, line, 64);
        g_reg[g_nreg].hex[64] = '\0';
        plen = strcspn(sp + 2, "\r\n");
        if (plen == 0 || plen >= MAXREGN) continue;
        memcpy(g_reg[g_nreg].path, sp + 2, plen);
        g_reg[g_nreg].path[plen] = '\0';
        g_reg[g_nreg].matched = 0;
        g_reg[g_nreg].ok = 0;
        g_nreg++;
    }
    fclose(fp);
}

/* write buffer to path via .tmp then rename (logs may replace themselves) */
static void publish(const char *path, const char *buf, size_t len)
{
    char tmp[MAXP];
    FILE *fp;
    int n = snprintf(tmp, sizeof tmp, "%s.tmp", path);
    if (n < 0 || (size_t)n >= sizeof tmp) bench("tmp path over cap");
    fp = fopen(tmp, "wb");
    if (!fp) bench("cannot open log .tmp for write");
    if (len && fwrite(buf, 1, len, fp) != len) { fclose(fp); bench("log write failed"); }
    if (fclose(fp) != 0) bench("log close failed");
    if (rename(tmp, path) != 0) bench("log rename failed");
}

/* ---------------- the walk ---------------- */

static char g_root[MAXP];                /* absolute-ish root as given  */

/* relpath of a dir from root ("" for root itself) into dirlog lines */
static void walk(const char *dir, const char *rel, int depth)
{
    char p[MAXP];
    DIR *dp;
    struct dirent *de;
    int n = 0, i;
    unsigned long d_files = 0, d_regok = 0, d_fail = 0, d_unreg = 0;
    char line[MAXLINE];

    if (depth >= MAXDEPTH) bench("directory depth over MAXDEPTH");

    /* PASS A: subdirectory names into THIS depth's own frame, sorted,
       then recurse — child frames can never overwrite ours.          */
    dp = opendir(dir);
    if (!dp) { fprintf(stderr, "BENCH: cannot open directory %s\n", dir); exit(2); }
    while ((de = readdir(dp)) != NULL) {
        struct stat st;
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
            continue;
        if (strcmp(de->d_name, ".git") == 0)
            continue;
        if (strlen(de->d_name) >= MAXN) { closedir(dp); bench("entry name over MAXN"); }
        if (join2(p, sizeof p, dir, de->d_name) != 0) { closedir(dp); bench("path over MAXP"); }
        if (stat(p, &st) != 0) { closedir(dp); bench("stat failed"); }
        if (!S_ISDIR(st.st_mode)) continue;
        if (n >= MAXSUB) { closedir(dp); bench("subdirs over MAXSUB"); }
        strcpy(g_subs[depth][n].name, de->d_name);
        g_subs[depth][n].isdir = 1;
        n++;
    }
    closedir(dp);
    qsort(g_subs[depth], (size_t)n, sizeof(ent_t), ent_cmp);
    for (i = 0; i < n; i++) {
        char sub[MAXP], srel[MAXP];
        if (join2(sub, sizeof sub, dir, g_subs[depth][i].name) != 0) bench("path over MAXP");
        if (rel[0]) {
            if (snprintf(srel, sizeof srel, "%s/%s", rel, g_subs[depth][i].name) >= (int)sizeof srel)
                bench("path over MAXP");
        } else {
            strcpy(srel, g_subs[depth][i].name);
        }
        walk(sub, srel, depth + 1);      /* post-order: child complete first */
    }

    /* PASS B: files only, sorted, hashed, logged — after every child */
    n = 0;
    dp = opendir(dir);
    if (!dp) { fprintf(stderr, "BENCH: cannot reopen directory %s\n", dir); exit(2); }
    while ((de = readdir(dp)) != NULL) {
        struct stat st;
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
            continue;
        if (strcmp(de->d_name, ".git") == 0)
            continue;
        if (strcmp(de->d_name, DIR_LOG) == 0 || strcmp(de->d_name, MASTER_LOG) == 0)
            continue;                    /* never hash our own logs      */
        if (strlen(de->d_name) >= MAXN) { closedir(dp); bench("entry name over MAXN"); }
        if (join2(p, sizeof p, dir, de->d_name) != 0) { closedir(dp); bench("path over MAXP"); }
        if (stat(p, &st) != 0) { closedir(dp); bench("stat failed"); }
        if (S_ISDIR(st.st_mode)) continue;
        if (n >= MAXF) { closedir(dp); bench("directory entries over MAXF"); }
        strcpy(g_ent[n].name, de->d_name);
        g_ent[n].isdir = 0;
        n++;
    }
    closedir(dp);
    qsort(g_ent, (size_t)n, sizeof(ent_t), ent_cmp);

    g_olen = 0;
    out_add("# hash log — sha256  name (files only, sorted; subdirs logged inside themselves)\n");
    for (i = 0; i < n; i++) {
        char fpath[MAXP], frel[MAXP], hex[65];
        reg_t *r;
        const char *tag;
        if (join2(fpath, sizeof fpath, dir, g_ent[i].name) != 0) bench("path over MAXP");
        if (rel[0]) {
            if (snprintf(frel, sizeof frel, "%s/%s", rel, g_ent[i].name) >= (int)sizeof frel)
                bench("path over MAXP");
        } else {
            if (strlen(g_ent[i].name) >= sizeof frel) bench("path over MAXP");
            strcpy(frel, g_ent[i].name);
        }
        if (hash_file(fpath, hex) != 0) bench("cannot read file for hashing");
        g_files++;
        d_files++;
        r = reg_find(frel);
        if (!r) { tag = "UNREGISTERED"; g_unreg++; d_unreg++; }
        else {
            r->matched = 1;
            if (strcmp(r->hex, hex) == 0) { tag = "OK"; g_regok++; d_regok++; r->ok = 1; }
            else { tag = "FAIL"; g_fail++; d_fail++;
                   printf("FAIL %s\n  registry %s\n  actual   %s\n", frel, r->hex, hex); }
        }
        if (snprintf(line, sizeof line, "%s  %s  %s\n", hex, g_ent[i].name, tag) < 0)
            bench("log line error");
        out_add(line);
    }

    /* write this dir's log AFTER children are done */
    {
        char lp[MAXP];
        if (join2(lp, sizeof lp, dir, DIR_LOG) != 0) bench("path over MAXP");
        publish(lp, g_out, g_olen);
    }
    g_dirs++;

    if (snprintf(line, sizeof line, "DIR %s files=%lu ok=%lu fail=%lu unreg=%lu %s\n",
                 rel[0] ? rel : ".", d_files, d_regok, d_fail, d_unreg,
                 d_fail ? "FAIL" : "OK") < 0) bench("master line error");
    mas_add(line);
}

int main(int argc, char **argv)
{
    int i;
    char mp[MAXP];
    char line[MAXLINE];

    if (argc == 2 && strcmp(argv[1], "--self-test") == 0)
        return sha_selftest();
    if (argc != 3) {
        fprintf(stderr, "usage: shakti-hasher ROOT_DIR REGISTRY_FILE\n");
        fprintf(stderr, "       shakti-hasher --self-test\n");
        return 2;
    }
    if (sha_selftest() != 0) return 2;   /* never trust a broken hasher  */

    reg_load(argv[2]);
    if (strlen(argv[1]) >= MAXP) bench("root path over MAXP");
    strcpy(g_root, argv[1]);
    if (g_root[0] && g_root[strlen(g_root)-1] == '/')
        g_root[strlen(g_root)-1] = '\0';

    walk(g_root, "", 0);

    /* registered paths never seen = MISSING */
    for (i = 0; i < g_nreg; i++) {
        if (g_reg[i].matched) continue;
        /* append-only: a path superseded by a LATER line is history, not missing */
        {
            int j, superseded = 0;
            for (j = i + 1; j < g_nreg; j++)
                if (strcmp(g_reg[j].path, g_reg[i].path) == 0) { superseded = 1; break; }
            if (superseded) continue;
        }
        if (snprintf(line, sizeof line, "MISSING %s  registry=%s\n",
                     g_reg[i].path, g_reg[i].hex) < 0) bench("master line error");
        mas_add(line);
        g_missing++;
        printf("MISSING %s\n", g_reg[i].path);
    }

    {
        char sum[MAXLINE];
        if (snprintf(sum, sizeof sum,
                "SUMMARY dirs=%lu files=%lu ok=%lu fail=%lu unreg=%lu missing=%lu\n",
                g_dirs, g_files, g_regok, g_fail, g_unreg, g_missing) < 0)
            bench("summary error");
        mas_add(sum);
        printf("%s", sum);
    }

    if (join2(mp, sizeof mp, g_root, MASTER_LOG) != 0) bench("path over MAXP");
    publish(mp, g_master, g_mlen);

    return (g_fail || g_missing) ? 2 : 0;
}
