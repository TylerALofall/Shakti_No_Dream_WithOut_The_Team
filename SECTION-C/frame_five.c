/* frame_five.c — the frame: five cubes inside one dodecahedron.
 *
 * The Doctor's law, 2026-08-26: memories are stored in a line; the USE
 * of them is geometric, reconstructed. This organ computes the frame
 * that reconstruction happens INSIDE — and it computes it, it never
 * looks it up.
 *
 * The claim under test:
 *   a regular dodecahedron has 20 vertices
 *   exactly 5 cubes inscribe in it, each using 8 of those vertices
 *   5 x 8 = 40 over 20 vertices, so EVERY vertex sits in exactly 2 cubes
 *
 * If that holds, the frame hands us an incidence rule: a USE event placed
 * at one vertex participates in exactly two frames. The assignment rule and
 * the meanings of the frames and poles remain outside this proof.
 *
 * NO FLOAT. The dodecahedron's coordinates need the golden ratio, and
 * phi is irrational, so this works in the ring Z[phi]: every number is
 * a + b*phi with a,b integers, and phi*phi = phi + 1 closes it. All
 * arithmetic below is exact integer arithmetic on those pairs. Nothing
 * is rounded, so nothing can drift.
 *
 * Vertices (scaled to clear denominators, 1/phi = phi - 1):
 *     (+-1,       +-1,       +-1  )   8   <- one of the five cubes
 *     ( 0,        +-(phi-1), +-phi)   4
 *     (+-(phi-1), +-phi,      0   )   4
 *     (+-phi,      0,        +-(phi-1)) 4
 *
 * Pure C99. No heap. No float. No child process. No clock.
 * Gauntlet: -std=c99 -pedantic -Wall -Wextra -Werror, -O0 == -O2.
 */
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

#define FNV_BASIS 0xCBF29CE484222325ULL
#define FNV_PRIME 0x100000001B3ULL

static uint64_t fnv1(uint64_t h, uint64_t v)
{
    int b;
    for (b = 0; b < 8; b++) { h ^= (unsigned char)((v >> (8 * b)) & 0xFF); h *= FNV_PRIME; }
    return h;
}

/* ---- Z[phi]: a + b*phi, exact ---------------------------------------- */
typedef struct { int64_t a, b; } zp;

static zp zp_of(int64_t a, int64_t b) { zp z; z.a = a; z.b = b; return z; }
static zp zp_sub(zp x, zp y)      { return zp_of(x.a - y.a, x.b - y.b); }
static zp zp_add(zp x, zp y)      { return zp_of(x.a + y.a, x.b + y.b); }
static int zp_eq(zp x, zp y)      { return x.a == y.a && x.b == y.b; }
/* (a+b.phi)(c+d.phi) = ac + (ad+bc).phi + bd.phi^2
 *                    = (ac+bd) + (ad+bc+bd).phi                        */
static zp zp_mul(zp x, zp y)
{
    return zp_of(x.a * y.a + x.b * y.b,
                 x.a * y.b + x.b * y.a + x.b * y.b);
}

/* ---- the solid -------------------------------------------------------- */
#define NV 20
#define CUBE_V 8
#define CUBE_MAX 5

typedef struct { zp x, y, z; } vec;
static vec V[NV];

static void build_vertices(void)
{
    const zp ONE  = { 1, 0}, NONE = {-1, 0};
    const zp PHI  = { 0, 1}, NPHI = { 0,-1};
    const zp INV  = {-1, 1}, NINV = { 1,-1};   /* phi-1 = 1/phi          */
    const zp ZERO = { 0, 0};
    int n = 0, i, j, k;

    for (i = 0; i < 2; i++)                     /* (+-1, +-1, +-1)        */
        for (j = 0; j < 2; j++)
            for (k = 0; k < 2; k++) {
                V[n].x = i ? NONE : ONE;
                V[n].y = j ? NONE : ONE;
                V[n].z = k ? NONE : ONE;
                n++;
            }
    for (j = 0; j < 2; j++)                     /* (0, +-1/phi, +-phi)    */
        for (k = 0; k < 2; k++) {
            V[n].x = ZERO;
            V[n].y = j ? NINV : INV;
            V[n].z = k ? NPHI : PHI;
            n++;
        }
    for (i = 0; i < 2; i++)                     /* (+-1/phi, +-phi, 0)    */
        for (j = 0; j < 2; j++) {
            V[n].x = i ? NINV : INV;
            V[n].y = j ? NPHI : PHI;
            V[n].z = ZERO;
            n++;
        }
    for (i = 0; i < 2; i++)                     /* (+-phi, 0, +-1/phi)    */
        for (k = 0; k < 2; k++) {
            V[n].x = i ? NPHI : PHI;
            V[n].y = ZERO;
            V[n].z = k ? NINV : INV;
            n++;
        }
}

/* squared distance, exact */
static zp dist2(int p, int q)
{
    zp dx = zp_sub(V[p].x, V[q].x);
    zp dy = zp_sub(V[p].y, V[q].y);
    zp dz = zp_sub(V[p].z, V[q].z);
    return zp_add(zp_add(zp_mul(dx, dx), zp_mul(dy, dy)), zp_mul(dz, dz));
}

static int is_edge(int p, int q)
{
    const zp EDGE2 = { 8, -4 };
    return p != q && zp_eq(dist2(p, q), EDGE2);
}

/* The coordinate graph must be connected and have exactly twelve induced
 * five-cycles. Each edge belongs to two of them and every vertex to three. */
static int check_faces(void)
{
    int face_count = 0;
    int vertex_faces[NV] = {0};
    int edge_faces[NV][NV] = {{0}};
    int a, b, c, d, e, p, q, bad = 0;

    for (a = 0; a < NV - 4; a++)
        for (b = a + 1; b < NV - 3; b++)
            for (c = b + 1; c < NV - 2; c++)
                for (d = c + 1; d < NV - 1; d++)
                    for (e = d + 1; e < NV; e++) {
                        int v[5] = {a, b, c, d, e};
                        int degree[5] = {0, 0, 0, 0, 0};
                        int edges = 0;
                        int cycle = 1;

                        for (p = 0; p < 5; p++)
                            for (q = p + 1; q < 5; q++)
                                if (is_edge(v[p], v[q])) {
                                    degree[p]++;
                                    degree[q]++;
                                    edges++;
                                }

                        for (p = 0; p < 5; p++)
                            if (degree[p] != 2) cycle = 0;

                        if (cycle && edges == 5) {
                            face_count++;
                            for (p = 0; p < 5; p++) vertex_faces[v[p]]++;
                            for (p = 0; p < 5; p++)
                                for (q = p + 1; q < 5; q++)
                                    if (is_edge(v[p], v[q])) {
                                        edge_faces[v[p]][v[q]]++;
                                        edge_faces[v[q]][v[p]]++;
                                    }
                        }
                    }

    if (face_count != 12) bad++;
    for (p = 0; p < NV; p++) {
        if (vertex_faces[p] != 3) bad++;
        for (q = p + 1; q < NV; q++)
            if (is_edge(p, q) && edge_faces[p][q] != 2) bad++;
    }

    printf("  induced pentagonal faces = %d, each vertex 3, edge 2 : %s\n",
           face_count, bad ? "NO" : "yes");
    return !bad;
}

/* ---- self-test: is this actually a dodecahedron? ---------------------- */
/* Dodecahedron edge (this scaling) = 2/phi, so d^2 = 4(2-phi) = 8 - 4phi.
 * Every vertex must have exactly 3 neighbours at that distance, giving
 * 20*3/2 = 30 edges. A random cloud will not do that.                    */
static int check_solid(void)
{
    const zp EDGE2 = { 8, -4 };
    int p, q, edges = 0, bad = 0;
    int queue[NV], seen[NV] = {0}, head = 0, tail = 0;
    int face_ok;

    for (p = 0; p < NV; p++) {
        int deg = 0;
        for (q = 0; q < NV; q++)
            if (q != p && zp_eq(dist2(p, q), EDGE2)) deg++;
        if (deg != 3) { printf("  vertex %2d has %d neighbours, expected 3\n", p, deg); bad++; }
        edges += deg;
    }
    edges /= 2;
    queue[tail++] = 0;
    seen[0] = 1;
    while (head < tail) {
        p = queue[head++];
        for (q = 0; q < NV; q++)
            if (!seen[q] && is_edge(p, q)) {
                seen[q] = 1;
                queue[tail++] = q;
            }
    }
    if (tail != NV) bad++;

    printf("  dodecahedron edge d^2 = %lld + %lld.phi\n",
           (long long)EDGE2.a, (long long)EDGE2.b);
    printf("  every vertex has 3 neighbours at that distance : %s\n", bad ? "NO" : "yes");
    printf("  edges = %d (a dodecahedron has 30)             : %s\n",
           edges, edges == 30 ? "yes" : "NO");
    printf("  edge graph reaches all %d vertices             : %s\n",
           NV, tail == NV ? "yes" : "NO");
    face_ok = check_faces();
    return !bad && edges == 30 && face_ok;
}

/* ---- is this 8-subset a cube? ----------------------------------------- */
/* A cube's 28 pairwise squared distances are exactly
 *   12 edges (d2), 12 face diagonals (2.d2), 4 space diagonals (3.d2).
 * The inscribed cube's edge is a pentagon diagonal = phi x dodeca edge,
 * which in this scaling is 2, so d2 = 4 with NO phi part at all.        */
static int is_cube(const int *s)
{
    const zp E = {4, 0}, F = {8, 0}, S = {12, 0};
    int i, j, ne = 0, nf = 0, ns = 0, other = 0;

    for (i = 0; i < CUBE_V; i++) {
        int ve = 0, vf = 0, vs = 0;
        for (j = i + 1; j < CUBE_V; j++) {
            zp d = dist2(s[i], s[j]);
            if      (zp_eq(d, E)) ne++;
            else if (zp_eq(d, F)) nf++;
            else if (zp_eq(d, S)) ns++;
            else other++;
        }
        for (j = 0; j < CUBE_V; j++)
            if (i != j) {
                zp d = dist2(s[i], s[j]);
                if      (zp_eq(d, E)) ve++;
                else if (zp_eq(d, F)) vf++;
                else if (zp_eq(d, S)) vs++;
            }
        if (ve != 3 || vf != 3 || vs != 1) return 0;
    }
    return ne == 12 && nf == 12 && ns == 4 && other == 0;
}

/* ---- the search: every 8-subset of 20, no shortcuts -------------------- */
static int CUBE[CUBE_MAX][CUBE_V];
static int cube_count;
static int cube_matches;

static uint64_t search(void)
{
    int idx[CUBE_V];
    uint64_t tried = 0U;
    int k, j;

    for (k = 0; k < CUBE_V; k++) idx[k] = k;
    for (;;) {
        tried++;
        if (is_cube(idx)) {
            if (cube_count < CUBE_MAX) {
                for (k = 0; k < CUBE_V; k++)
                    CUBE[cube_count][k] = idx[k];
                cube_count++;
            }
            cube_matches++;
        }
        k = CUBE_V - 1;
        while (k >= 0 && idx[k] == NV - CUBE_V + k) k--;
        if (k < 0) break;
        idx[k]++;
        for (j = k + 1; j < CUBE_V; j++) idx[j] = idx[j - 1] + 1;
    }
    return tried;
}

/* ---- the pair law: how many vertices does each pair of cubes share? ---- */
static int check_pairs(void)
{
    int a, b, p, k, bad = 0, pairs = 0;
    for (a = 0; a < cube_count; a++)
        for (b = a + 1; b < cube_count; b++) {
            int shared = 0;
            for (p = 0; p < NV; p++) {
                int in_a = 0, in_b = 0;
                for (k = 0; k < CUBE_V; k++) {
                    if (CUBE[a][k] == p) in_a = 1;
                    if (CUBE[b][k] == p) in_b = 1;
                }
                if (in_a && in_b) shared++;
            }
            printf("  cube %d & cube %d share %d vertices\n", a + 1, b + 1, shared);
            pairs++;
            if (shared != 2) bad++;
        }
    printf("  pairs of cubes = %d (C(5,2) = 10), %d x 2 = %d vertices\n",
           pairs, pairs, pairs * 2);
    return !bad;
}

/* ---- the pole law (GPT, 2026-08-26): are the two shared vertices of a
 * frame-pair exact geometric opposites? If so a vertex is not just "in two
 * cubes" — it is TWO FRAMES PLUS A POLE, and the binary sits in the solid
 * rather than being bolted onto it. Checked, not assumed. */
static int is_antipodal(int p, int q)
{
    return V[q].x.a == -V[p].x.a && V[q].x.b == -V[p].x.b
        && V[q].y.a == -V[p].y.a && V[q].y.b == -V[p].y.b
        && V[q].z.a == -V[p].z.a && V[q].z.b == -V[p].z.b;
}

static int check_poles(void)
{
    int a, b, p, k, bad = 0, checked = 0;
    for (a = 0; a < cube_count; a++)
        for (b = a + 1; b < cube_count; b++) {
            int sh[NV], n = 0;
            for (p = 0; p < NV; p++) {
                int in_a = 0, in_b = 0;
                for (k = 0; k < CUBE_V; k++) {
                    if (CUBE[a][k] == p) in_a = 1;
                    if (CUBE[b][k] == p) in_b = 1;
                }
                if (in_a && in_b && n < NV) sh[n++] = p;
            }
            if (n != 2) { printf("  cubes %d&%d share %d, expected 2\n", a+1, b+1, n); bad++; continue; }
            checked++;
            printf("  frames %d & %d -> v%-2d and v%-2d : %s\n", a + 1, b + 1, sh[0], sh[1],
                   is_antipodal(sh[0], sh[1]) ? "exact opposites" : "NOT OPPOSITE");
            if (!is_antipodal(sh[0], sh[1])) bad++;
        }
    printf("  %d frame-pairs checked, each an antipodal pole pair : %s\n",
           checked, bad ? "NO" : "yes");
    return !bad;
}

/* ---- report ------------------------------------------------------------ */
static void print_zp(zp z)
{
    if (z.b == 0)      printf("%lld", (long long)z.a);
    else if (z.a == 0) printf("%lldphi", (long long)z.b);
    else               printf("%lld%+lldphi", (long long)z.a, (long long)z.b);
}

int main(void)
{
    int p, k, c, bad = 0, ok_solid, ok_pairs, ok_poles;
    uint64_t tried;
    uint64_t pin = FNV_BASIS;
    uint64_t proof_pin = FNV_BASIS;
    int seen[NV];

    puts("SHAKTI_FRAME_FIVE_V1");
    puts("the frame: five cubes inside one dodecahedron");
    puts("exact arithmetic in Z[phi], phi^2 = phi + 1. no float anywhere.");
    puts("");

    build_vertices();

    puts("-- the 20 vertices --------------------------------------------");
    for (p = 0; p < NV; p++) {
        printf("  v%-2d ( ", p);
        print_zp(V[p].x); printf(", ");
        print_zp(V[p].y); printf(", ");
        print_zp(V[p].z); printf(" )\n");
    }
    puts("");

    puts("-- self-test: is it a dodecahedron? ---------------------------");
    ok_solid = check_solid();
    puts("");

    puts("-- exhaustive search: every 8-subset of the 20 ----------------");
    tried = search();
    printf("  subsets tested : %" PRIu64 "  (C(20,8) = 125970)\n", tried);
    printf("  cubes found    : %d\n", cube_matches);
    printf("  cubes retained : %d (capacity %d)\n", cube_count, CUBE_MAX);
    puts("");

    puts("-- the five frames --------------------------------------------");
    for (c = 0; c < cube_count; c++) {
        printf("  cube %d : ", c + 1);
        for (k = 0; k < CUBE_V; k++) printf("v%-2d ", CUBE[c][k]);
        putchar('\n');
    }
    puts("");

    puts("-- the incidence law: how many cubes hold each vertex ---------");
    for (p = 0; p < NV; p++) seen[p] = 0;
    for (c = 0; c < cube_count; c++)
        for (k = 0; k < CUBE_V; k++) seen[CUBE[c][k]]++;
    for (p = 0; p < NV; p++) {
        printf("  v%-2d in %d cube(s):", p, seen[p]);
        for (c = 0; c < cube_count; c++)
            for (k = 0; k < CUBE_V; k++)
                if (CUBE[c][k] == p) printf(" %d", c + 1);
        putchar('\n');
        if (seen[p] != 2) bad++;
    }
    puts("");

    puts("-- the pair law: overlap between any two frames ---------------");
    ok_pairs = check_pairs();
    puts("");

    puts("-- the pole law: are those two shared vertices opposite? ------");
    ok_poles = check_poles();
    puts("");

    puts("-- verdict -----------------------------------------------------");
    printf("  dodecahedron graph (edges, faces, reach) : %s\n", ok_solid ? "PROVEN" : "FAILED");
    printf("  cubes inscribed                          : %d %s\n",
           cube_matches, cube_matches == 5 ? "(PROVEN exactly five)" : "(NOT five)");
    printf("  5 x 8 = %d slots over %d vertices         : %s\n",
           cube_count * CUBE_V, NV,
           cube_count * CUBE_V == NV * 2 ? "each vertex twice" : "does not divide");
    printf("  every vertex in exactly 2 cubes          : %s\n", bad ? "FAILED" : "PROVEN");
    printf("  every PAIR of cubes shares exactly 2     : %s\n", ok_pairs ? "PROVEN" : "FAILED");
    printf("  those 2 are exact geometric opposites    : %s\n", ok_poles ? "PROVEN" : "FAILED");
    puts("");

    for (c = 0; c < cube_count; c++)
        for (k = 0; k < CUBE_V; k++)
            pin = fnv1(pin, (uint64_t)CUBE[c][k]);
    printf("  incidence pin %016llX\n", (unsigned long long)pin);

    proof_pin = fnv1(proof_pin, 0x5348414B54494631ULL);
    for (p = 0; p < NV; p++) {
        proof_pin = fnv1(proof_pin, (uint64_t)V[p].x.a);
        proof_pin = fnv1(proof_pin, (uint64_t)V[p].x.b);
        proof_pin = fnv1(proof_pin, (uint64_t)V[p].y.a);
        proof_pin = fnv1(proof_pin, (uint64_t)V[p].y.b);
        proof_pin = fnv1(proof_pin, (uint64_t)V[p].z.a);
        proof_pin = fnv1(proof_pin, (uint64_t)V[p].z.b);
    }
    proof_pin = fnv1(proof_pin, (uint64_t)tried);
    proof_pin = fnv1(proof_pin, (uint64_t)cube_matches);
    for (c = 0; c < cube_count; c++)
        for (k = 0; k < CUBE_V; k++)
            proof_pin = fnv1(proof_pin, (uint64_t)CUBE[c][k]);
    printf("  proof pin     %016llX\n", (unsigned long long)proof_pin);

    return (ok_solid && ok_pairs && ok_poles && cube_matches == 5 &&
            cube_count == CUBE_MAX && !bad) ? 0 : 1;
}
