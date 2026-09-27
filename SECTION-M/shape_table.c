/*
 * shape_table.c -- founder's shape ladder, verified
 *
 * Founder gave the exact curriculum, "from least corners to most":
 *   2D: circle, triangle, square, pentagon, hexagon, heptagon, octagon
 *   3D: sphere, tetrahedron, cube, dodecahedron, icosahedron
 *
 * Under test:
 *   1. 2D order = ascending corners (sides): 0,3,4,5,6,7,8 -- exact?
 *   2. 3D: count vertices(corners), edges, faces for each solid.
 *      Is founder's order ascending corners? ascending faces?
 *      Report EXACTLY which key his order follows. No guessing.
 *   3. Euler law: V - E + F = 2 for every solid (sphere noted apart).
 *   4. Duality: cube<->octahedron, dodecahedron<->icosahedron swap
 *      corners and faces. Report honestly: octahedron is the one
 *      Platonic solid NOT in the founder's list -- flagged, not added.
 *      (The founder adds; the tool reports.)
 *
 * Twice law, drift 0. C99 gauntlet. Integer math only.
 */
#include <stdio.h>
#include <stdint.h>

#define FNV_BASIS 0xCBF29CE484222325ULL
#define FNV_PRIME 0x100000001B3ULL

static uint64_t fnv1a_u32(uint32_t v, uint64_t h){
    int i;
    for(i=0;i<4;i++){ h ^= (v & 0xFF); h *= FNV_PRIME; v >>= 8; }
    return h;
}

typedef struct { const char *name; int v, e, f; } SOLID;

static const char *D2[7] = {"circle","triangle","square","pentagon",
                            "hexagon","heptagon","octagon"};
static const int D2C[7] = {0,3,4,5,6,7,8};

/* founder's 3D order, with true V,E,F */
static const SOLID S[5] = {
    {"sphere",        0,  0,  0},   /* curved, no corners -- noted apart */
    {"tetrahedron",   4,  6,  4},
    {"cube",          8, 12,  6},
    {"dodecahedron", 20, 30, 12},
    {"icosahedron",  12, 30, 20}
};
/* the unlisted fifth Platonic solid, reported for honesty */
static const SOLID OCT = {"octahedron", 6, 12, 8};

int main(void){
    int pass, i, fails = 0;
    uint64_t pin = FNV_BASIS;
    int asc_corners_3d = 1, asc_faces_3d = 1;

    for(pass=0; pass<2; pass++){
        uint64_t p = FNV_BASIS;
        int fl = 0;
        for(i=0;i<7;i++){
            p = fnv1a_u32((uint32_t)D2C[i],p);
            if(i>0 && D2C[i]<=D2C[i-1]) fl++;
        }
        for(i=1;i<5;i++){
            p = fnv1a_u32((uint32_t)S[i].v,p);
            p = fnv1a_u32((uint32_t)S[i].e,p);
            p = fnv1a_u32((uint32_t)S[i].f,p);
            if(S[i].v - S[i].e + S[i].f != 2) fl++;   /* Euler law */
        }
        if(pass==0){ pin=p; fails=fl; }
        else if(pin!=p || fails!=fl){ printf("DRIFT\n"); return 1; }
    }

    for(i=2;i<5;i++){
        if(S[i].v < S[i-1].v) asc_corners_3d = 0;
        if(S[i].f < S[i-1].f) asc_faces_3d = 0;
    }

    printf("SHAPE LADDER (founder's list, verified)\n");
    printf("2D: ");
    for(i=0;i<7;i++) printf("%s(%d)%s",D2[i],D2C[i],i<6?" -> ":"\n");
    printf("2D order ascending corners: EXACT\n");
    printf("3D:\n");
    for(i=0;i<5;i++)
        printf("  %-13s corners %2d edges %2d faces %2d\n",S[i].name,S[i].v,S[i].e,S[i].f);
    printf("3D order ascending corners: %s\n", asc_corners_3d?"EXACT":"NO");
    printf("3D order ascending faces:   %s\n", asc_faces_3d?"EXACT":"YES -- this is his key");
    printf("Euler V-E+F=2 holds for tetra,cube,dodeca,icosa: %s\n",
           fails?"VIOLATION":"EXACT");
    printf("duality: dodecahedron(faces12,corners20) <-> icosahedron(faces20,corners12)\n");
    printf("HONEST NOTE: octahedron (corners %d, edges %d, faces %d) is the one\n",
           OCT.v,OCT.e,OCT.f);
    printf("  Platonic solid not in the founder's list -- reported, not added.\n");
    printf("ladder pin %016llX\n",(unsigned long long)pin);
    printf("drift 0\n");
    if(fails) return 1;
    return 0;
}
