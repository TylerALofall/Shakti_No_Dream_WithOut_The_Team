/* shakti_batch_bitmap_v3.c | 2026-09-26 | C99, static pixel buffer, no heap.
 * QA SPECIALIST: GPT-6 (Codex); LOCATION: ChatGPT Work workspace.
 * SUPERVISOR QA: pending an independent model; LOCATION: pending.
 * SUPERVISOR NOTES: reserved for that model's review; no approval asserted.
 * Build: cc -std=c99 -O2 -Wall -Wextra -Werror -pedantic shakti_batch_bitmap_v3.c -o shakti_batch_bitmap_v3
 * Run:   ./shakti_batch_bitmap_v3 graph.txt output/section 1500
 * Reads exact text records from shakti_batch_extract; writes section_A.bmp .. section_O.bmp.
 * One page per Section, up to 128 functions each. Addresses form edges ONLY when explicit.
 * Descriptions render in each section; a combined links BMP connects section maps.
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define W 1500
#define H 3500
#define COLS 5
#define CELL_H 105
#define MAX_NODES 1500
#define MAX_EDGES 8192
#define MAX_ROUTES 128
static unsigned char pixels[H][W][3];
typedef struct {
    char section, address[32], file[192], name[128], type[160];
    char input[256], result[96], upstream[256], complete[24], desc[1024];
    int order, x, y, cross, bridge, outgoing;
} Node;
typedef struct { int from, to; } Edge;
static Node nodes[MAX_NODES];
static Edge edges[MAX_EDGES];
typedef struct { int x1, y1, x2, y2, track; } Route;
static Route routes[MAX_ROUTES];
static unsigned char composed[15 * W * 3 + 3], scanline[W * 3 + 3];
static int nnode, nedge, canvas_width;
static const unsigned char white[3] = {255, 255, 255};
static const unsigned char dark[3] = {40, 53, 69};
static const unsigned char blue[3] = {220, 235, 255};
static const unsigned char amber[3] = {255, 240, 205};
static const unsigned char green[3] = {221, 245, 221};
static const unsigned char red[3] = {188, 38, 38};
typedef char static_budget[(sizeof pixels + sizeof nodes + sizeof edges + sizeof routes +
    sizeof composed + sizeof scanline <= 20u * 1024u * 1024u) ? 1 : -1];

static int line(FILE *f, char *out, size_t cap)
{
    size_t n;
    if (!fgets(out, (int)cap, f)) return 0;
    n = strlen(out);
    if (n && out[n - 1u] != '\n' && !feof(f)) return -1;
    while (n && (out[n - 1u] == '\n' || out[n - 1u] == '\r')) out[--n] = 0;
    return 1;
}
static int read_field(FILE *f, const char *key, char *dst, size_t cap)
{
    char buf[1200];
    size_t n = strlen(key), len;
    if (line(f, buf, sizeof(buf)) != 1 || strncmp(buf, key, n) != 0 || buf[n] != '=') return 0;
    len = strlen(buf + n + 1u);
    if (len >= cap) return 0;
    memcpy(dst, buf + n + 1u, len + 1u);
    return 1;
}
static int load(const char *path)
{
    FILE *f = fopen(path, "r");
    char buf[1200], value[32];
    int rc, bad = 0;
    if (!f) { perror(path); return 0; }
    while ((rc = line(f, buf, sizeof(buf))) == 1) {
        Node *p;
        char *end;
        long order;
        if (strcmp(buf, "NODE") != 0 || nnode >= MAX_NODES) { bad = 1; break; }
        p = &nodes[nnode];
        if (!read_field(f, "section", value, sizeof(value)) || strlen(value) != 1u ||
            value[0] < 'A' || value[0] > 'O') { bad = 1; break; }
        p->section = value[0];
#define READ(key, member) read_field(f, key, p->member, sizeof(p->member))
        if (!READ("address", address) || !READ("file", file) ||
            !read_field(f, "order", value, sizeof(value)) || !READ("name", name) ||
            !READ("type", type) || !READ("input", input) || !READ("return", result) ||
            !READ("input_addresses", upstream) || !READ("complete", complete) ||
            !READ("description", desc) || line(f, buf, sizeof(buf)) != 1 ||
            strcmp(buf, "END") != 0) { bad = 1; break; }
#undef READ
        order = strtol(value, &end, 10);
        if (!*value || *end || order < 0 || order > 1000000 || !*p->name || !*p->file ||
            !*p->address || !strcmp(p->address, "UNSET")) {
            bad = 1; break;
        }
        p->order = (int)order;
        ++nnode;
    }
    if (bad || ferror(f) || rc < 0 || !feof(f)) {
        fprintf(stderr, "BENCH: invalid graph record near function %d\n", nnode + 1);
        fclose(f); return 0;
    }
    fclose(f);
    if (!nnode) { fputs("BENCH: empty graph\n", stderr); return 0; }
    return 1;
}
static int resolve_edges(void)
{
    int i, j;
    for (i = 0; i < nnode; ++i) {
        for (j = i + 1; j < nnode; ++j)
            if (strcmp(nodes[i].address, "UNSET") != 0 &&
                strcmp(nodes[i].address, nodes[j].address) == 0) {
                fprintf(stderr, "BENCH: duplicate address %s\n", nodes[i].address); return 0;
            }
    }
    for (i = 0; i < nnode; ++i) {
        char words[256], *word;
        strcpy(words, nodes[i].upstream);
        for (word = strtok(words, " ,;\t"); word; word = strtok(NULL, " ,;\t")) {
            if (strcmp(word, "NULL") == 0 || strcmp(word, "-") == 0) continue;
            if (strcmp(word, "UNSET") == 0) {
                fputs("BENCH: an unresolved address cannot be an edge\n", stderr); return 0;
            }
            for (j = 0; j < nnode; ++j)
                if (strcmp(nodes[j].address, word) == 0) break;
            if (j == nnode || nedge >= MAX_EDGES) {
                fprintf(stderr, "BENCH: unresolved address or full edges: %s\n", word); return 0;
            }
            edges[nedge].from = j; edges[nedge++].to = i;
            nodes[j].outgoing = 1;
            if (nodes[j].section != nodes[i].section) {
                nodes[i].cross = 1; nodes[j].cross = 1;
            } else nodes[i].bridge = 1;
        }
    }
    return 1;
}
static void pixel(int x, int y, const unsigned char c[3])
{
    if (x < 0 || y < 0 || x >= canvas_width || y >= H) return;
    memcpy(pixels[y][x], c, 3u);
}
static void rect(int x, int y, int w, int h, const unsigned char c[3])
{
    int a, b;
    for (b = y; b < y + h; ++b) for (a = x; a < x + w; ++a) pixel(a, b, c);
}
static void wire(int x, int y, int a, int b)
{
    int m = (x + a) / 2, i;
    for (i = (x < m ? x : m); i <= (x > m ? x : m); ++i) pixel(i, y, dark);
    for (i = (y < b ? y : b); i <= (y > b ? y : b); ++i) pixel(m, i, dark);
    for (i = (m < a ? m : a); i <= (m > a ? m : a); ++i) pixel(i, b, dark);
}
/* Compact 5x7 lettering keeps function names readable in a plain C bitmap. */
static const char *glyphs = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._-/? :>";
static const unsigned char font[][7] = {
 {14,17,17,31,17,17,17},{30,17,17,30,17,17,30},{14,17,16,16,16,17,14},
 {30,17,17,17,17,17,30},{31,16,16,30,16,16,31},{31,16,16,30,16,16,16},
 {14,17,16,23,17,17,14},{17,17,17,31,17,17,17},{14,4,4,4,4,4,14},
 {7,2,2,2,18,18,12},{17,18,20,24,20,18,17},{16,16,16,16,16,16,31},
 {17,27,21,21,17,17,17},{17,25,21,19,17,17,17},{14,17,17,17,17,17,14},
 {30,17,17,30,16,16,16},{14,17,17,17,21,18,13},{30,17,17,30,20,18,17},
 {15,16,16,14,1,1,30},{31,4,4,4,4,4,4},{17,17,17,17,17,17,14},
 {17,17,17,17,17,10,4},{17,17,17,21,21,21,10},{17,17,10,4,10,17,17},
 {17,17,10,4,4,4,4},{31,1,2,4,8,16,31},{14,17,19,21,25,17,14},
 {4,12,4,4,4,4,14},{14,17,1,2,4,8,31},{30,1,1,14,1,1,30},
 {2,6,10,18,31,2,2},{31,16,16,30,1,1,30},{14,16,16,30,17,17,14},
 {31,1,2,4,8,8,8},{14,17,17,14,17,17,14},{14,17,17,15,1,1,14},
 {0,0,0,0,0,12,12},{0,0,0,0,0,0,31},{0,0,0,31,0,0,0},
 {1,2,2,4,8,8,16},{14,17,1,2,4,0,4},{0,0,0,0,0,0,0},
 {0,4,4,0,4,4,0},{16,8,4,2,4,8,16}
};
static int supported(const char *s)
{
    for (; *s; ++s) if (!strchr(glyphs, toupper((unsigned char)*s))) return 0;
    return 1;
}
static void label(int x, int y, const char *s, int maxchars)
{
    int k, row, col, dx, dy;
    for (k = 0; s[k] && k < maxchars; ++k) {
        const char *p = strchr(glyphs, toupper((unsigned char)s[k]));
        int idx = p ? (int)(p - glyphs) : (int)(strchr(glyphs, '?') - glyphs);
        for (row = 0; row < 7; ++row) for (col = 0; col < 5; ++col)
            if (font[idx][row] & (16 >> col))
                for (dy = 0; dy < 2; ++dy) for (dx = 0; dx < 2; ++dx)
                    pixel(x + k * 12 + col * 2 + dx, y + row * 2 + dy, dark);
    }
}
static void tiny_label(int x, int y, const char *s, int count)
{
    int k, row, col;
    for (k = 0; s[k] && k < count; k++) {
        const char *p = strchr(glyphs, toupper((unsigned char)s[k]));
        int idx = p ? (int)(p - glyphs) : (int)(strchr(glyphs, '?') - glyphs);
        for (row = 0; row < 7; row++) for (col = 0; col < 5; col++)
            if (font[idx][row] & (16 >> col)) pixel(x + k * 6 + col, y + row, dark);
    }
}
static void circle(int x, int y)
{
    int a, b;
    for (b = -10; b <= 10; b++) for (a = -10; a <= 10; a++) {
        int squared = a * a + b * b;
        if (squared >= 64 && squared <= 100) pixel(x + a, y + b, dark);
    }
}
static int exists(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (f) fclose(f);
    return f != NULL;
}
static int save_bmp(const char *path, int height)
{
    FILE *f;
    char temp[1100];
    int y, i, row_bytes = (canvas_width * 3 + 3) & ~3, bad = 0;
    unsigned bytes = 54u + (unsigned)row_bytes * (unsigned)height;
    unsigned char header[54] = {0};
    if (snprintf(temp, sizeof temp, "%s.tmp", path) >= (int)sizeof temp ||
        exists(path) || exists(temp)) { fputs("BENCH: bitmap output exists\n", stderr); return 0; }
    f = fopen(temp, "wb");
    if (!f) { fputs("BENCH: bitmap temp cannot open\n", stderr); return 0; }
    header[0] = 'B'; header[1] = 'M';
    header[2] = (unsigned char)bytes; header[3] = (unsigned char)(bytes >> 8);
    header[4] = (unsigned char)(bytes >> 16); header[5] = (unsigned char)(bytes >> 24);
    header[10] = 54; header[14] = 40;
    header[18] = (unsigned char)canvas_width;
    header[19] = (unsigned char)(canvas_width >> 8);
    header[22] = (unsigned char)(height & 255);
    header[23] = (unsigned char)(height >> 8); header[26] = 1; header[28] = 24;
    if (fwrite(header, 1u, sizeof header, f) != sizeof header) bad = 1;
    for (y = height - 1; y >= 0; --y) {
        for (i = 0; i < canvas_width; ++i) {
            unsigned char bgr[3] = {pixels[y][i][2], pixels[y][i][1], pixels[y][i][0]};
            if (fwrite(bgr, 1u, 3u, f) != 3u) { bad = 1; break; }
        }
        for (i = canvas_width * 3; i < row_bytes; ++i) if (fputc(0, f) == EOF) bad = 1;
        if (bad) break;
    }
    if (ferror(f)) bad = 1;
    if (fclose(f)) bad = 1;
    if (bad || exists(path) || rename(temp, path))
        { remove(temp); fputs("BENCH: bitmap write or rename failed\n", stderr); return 0; }
    return 1;
}
static int render(char section, const char *prefix, int *map_height, int *footer)
{
    int ids[128], count = 0, i, j, row, height, internal = 0, cross = 0;
    int step = (canvas_width - 20) / COLS, box = step - 14, columns;
    char path[1024], title[64];
    for (i = 0; i < nnode; ++i) if (nodes[i].section == section) {
        if (count >= 128) { fprintf(stderr, "BENCH: Section %c exceeds 128 functions\n", section); return 0; }
        ids[count++] = i;
    }
    if (!count) return 1;
    for (i = 1; i < count; ++i) {
        int key = ids[i], at = i;
        while (at && (strcmp(nodes[ids[at - 1]].file, nodes[key].file) > 0 ||
              (strcmp(nodes[ids[at - 1]].file, nodes[key].file) == 0 &&
               nodes[ids[at - 1]].order > nodes[key].order))) {
            ids[at] = ids[at - 1]; --at;
        }
        ids[at] = key;
    }
    columns = (box - 20) / 6;
    for (i = 0; i < nedge; i++)
        if (nodes[edges[i].from].section != nodes[edges[i].to].section &&
            (nodes[edges[i].from].section == section || nodes[edges[i].to].section == section)) cross++;
    height = 80 + ((count + COLS - 1) / COLS) * CELL_H + 22 * cross;
    if (height > H) { fputs("BENCH: bitmap height overflow\n", stderr); return 0; }
    for (i = 0; i < count; i++) {
        Node *p = &nodes[ids[i]];
        if (!*p->desc || !supported(p->desc) || !supported(p->name) ||
            !supported(p->address) || strlen(p->desc) > (size_t)columns * 5u ||
            strlen(p->name) * 6u > (size_t)(box - 112) ||
            strlen(p->address) * 6u > (size_t)(box - 20)) {
            fprintf(stderr, "BENCH: label/description cannot fit %s\n", p->address); return 0;
        }
    }
    for (row = 0; row < height; ++row) rect(0, row, W, 1, white);
    snprintf(title, sizeof(title), "SECTION %c : %d FUNCTIONS", section, count);
    label(30, 22, title, 30);
    label(30, 42, "SOLID VERIFIED  CIRCLE CROSS", 32);
    for (i = 0; i < count; ++i) {
        Node *p = &nodes[ids[i]];
        p->x = 10 + (i % COLS) * step;
        p->y = 65 + (i / COLS) * CELL_H;
    }
    for (i = 0; i < nedge; ++i) {
        Node *a = &nodes[edges[i].from], *b = &nodes[edges[i].to];
        if (a->section != section || b->section != section) continue;
        if (b->x >= a->x) wire(a->x + box, a->y + 45, b->x - 2, b->y + 45);
        else wire(a->x - 2, a->y + 45, b->x + box, b->y + 45);
        internal++;
    }
    for (i = 0; i < count; ++i) {
        Node *p = &nodes[ids[i]];
        const unsigned char *fill = strcmp(p->complete, "yes") == 0 ? blue : amber;
        const unsigned char *border = (nedge && strcmp(p->address, "UNSET") && !p->outgoing) ? red : dark;
        int x = p->x, y = p->y;
        if (p->bridge) fill = green;
        rect(x, y, box, 99, border);
        rect(x + 2, y + 2, box - 4, 95, fill);
        if (p->bridge) {
            for (j = -10; j <= 10; ++j) {
                int span = 10 - (j < 0 ? -j : j);
                rect(x + 51 - span, y + 23 + j, 2 * span + 1, 1, dark);
            }
        }
        if (p->cross) circle(x + 21, y + 23);
        if (!strcmp(p->result, "void"))
            for (j = 0; j < 17; j++) rect(x + 82 - j, y + 16 + j, 2 * j + 1, 1, dark);
        tiny_label(x + 108, y + 13, p->name, (box - 112) / 6);
        tiny_label(x + 10, y + 37, p->address, (box - 20) / 6);
        for (j = 0; p->desc[j]; j += columns)
            tiny_label(x + 10, y + 51 + (j / columns) * 9, p->desc + j, columns);
    }
    for (i = 0; i < nedge; ++i) {
        Node *a = &nodes[edges[i].from], *b = &nodes[edges[i].to];
        int tip, dir, dy;
        if (a->section != section || b->section != section) continue;
        dir = b->x >= a->x ? 1 : -1;
        tip = b->x + (dir == 1 ? -2 : box);
        for (dy = -4; dy <= 4; ++dy) {
            int spread = dy < 0 ? -dy : dy;
            pixel(tip - dir * spread, b->y + 44 + dy, dark);
        }
    }
    row = 80 + ((count + COLS - 1) / COLS) * CELL_H;
    for (i = 0; i < nedge; i++) {
        Node *a = &nodes[edges[i].from], *b = &nodes[edges[i].to];
        char line_text[128];
        if (a->section == b->section || (a->section != section && b->section != section)) continue;
        snprintf(line_text, sizeof line_text, "LINK %d %c:%s > %c:%s", i + 1,
                 a->section, a->address, b->section, b->address);
        if (!supported(line_text) || strlen(line_text) * 6u + 90u > (unsigned)canvas_width)
            { fputs("BENCH: cross-link label cannot fit\n", stderr); return 0; }
        wire(42, row + 10, canvas_width - 42, row + 10);
        circle(30, row + 10); circle(canvas_width - 30, row + 10);
        rect(82, row + 2, (int)strlen(line_text) * 6 + 6, 11, white);
        tiny_label(85, row + 4, line_text, (int)strlen(line_text));
        row += 22;
    }
    if (snprintf(path, sizeof(path), "%s_%c.bmp", prefix, section) >= (int)sizeof(path))
        { fputs("BENCH: section bitmap path too long\n", stderr); return 0; }
    if (!save_bmp(path, height)) return 0;
    *map_height = height; *footer = 80 + ((count + COLS - 1) / COLS) * CELL_H + 10;
    printf("SECTION %c: %d functions, %d internal links, %d cross-link connectors, descriptions drawn, %s (%dx%d)\n",
           section, count, internal, cross, path, canvas_width, height);
    return 2; /* A new map was committed; 1 means this Section was absent. */
}
static void put32(unsigned char *h, int at, unsigned long n)
{
    int i;
    for (i = 0; i < 4; ++i) { h[at + i] = (unsigned char)n; n >>= 8; }
}
static void rowdot(int x, int width)
{
    if (x >= 0 && x < width) {
        composed[x * 3] = dark[2]; composed[x * 3 + 1] = dark[1];
        composed[x * 3 + 2] = dark[0];
    }
}
/* Join the actual section BMPs in one streamed view; fixed row storage. */
static int save_links(const char *prefix, const int heights[15], const int footers[15])
{
    FILE *ins[15] = {0}, *out = NULL;
    char path[1024], temp[1100], map[1024];
    int col[15], prior[15] = {0}, count = 0, maxh = 0, cross = 0;
    int width, height, rowbytes, sectionbytes, y, i, j, created = 0, ok = 1;
    unsigned long bytes;
    unsigned char h[54] = {0}, inhead[54];
    for (i = 0; i < 15; ++i) {
        col[i] = -1;
        if (heights[i]) {
            col[i] = count++;
            if (heights[i] > maxh) maxh = heights[i];
        }
    }
    for (i = 0; i < nedge; ++i)
        if (nodes[edges[i].from].section != nodes[edges[i].to].section) cross++;
    if (!cross) return 1;
    if (cross > MAX_ROUTES) {
        fputs("BENCH: cross-section link map exceeds fixed route capacity\n", stderr); return 0;
    }
    width = count * canvas_width; height = maxh + 30 + cross * 22;
    rowbytes = (width * 3 + 3) & ~3;
    sectionbytes = (canvas_width * 3 + 3) & ~3;
    bytes = 54ul + (unsigned long)rowbytes * (unsigned long)height;
    if (rowbytes > (int)sizeof composed || bytes > 200000000ul) {
        fputs("BENCH: combined BMP exceeds fixed output limit\n", stderr); return 0;
    }
    if (snprintf(path, sizeof path, "%s_links.bmp", prefix) >= (int)sizeof path ||
        snprintf(temp, sizeof temp, "%s.tmp", path) >= (int)sizeof temp ||
        exists(path) || exists(temp)) {
        fputs("BENCH: combined BMP path exists or is too long\n", stderr); return 0;
    }
    for (i = 0, j = 0; i < nedge; ++i) {
        int a = nodes[edges[i].from].section - 'A';
        int b = nodes[edges[i].to].section - 'A';
        int dir;
        if (a == b) continue;
        if (col[a] < 0 || col[b] < 0) {
            fputs("BENCH: link section bitmap missing\n", stderr); return 0;
        }
        dir = col[a] < col[b] ? 1 : -1;
        routes[j].x1 = col[a] * canvas_width + (dir > 0 ? canvas_width - 30 : 30);
        routes[j].x2 = col[b] * canvas_width + (dir > 0 ? 30 : canvas_width - 30);
        routes[j].y1 = footers[a] + 22 * prior[a]++;
        routes[j].y2 = footers[b] + 22 * prior[b]++;
        routes[j].track = maxh + 15 + 22 * j;
        j++;
    }
    for (i = 0; i < 15; ++i) if (heights[i]) {
        if (snprintf(map, sizeof map, "%s_%c.bmp", prefix, 'A' + i) >= (int)sizeof map ||
            !(ins[i] = fopen(map, "rb")) || fread(inhead, 1u, sizeof inhead, ins[i]) != sizeof inhead ||
            inhead[0] != 'B' || inhead[1] != 'M' ||
            (int)(inhead[18] | (unsigned)inhead[19] << 8) != canvas_width ||
            (int)(inhead[22] | (unsigned)inhead[23] << 8) != heights[i]) {
            fputs("BENCH: generated section bitmap cannot be read\n", stderr); ok = 0; goto done;
        }
    }
    out = fopen(temp, "wb");
    if (!out) { fputs("BENCH: combined BMP temp cannot open\n", stderr); ok = 0; goto done; }
    created = 1;
    h[0] = 'B'; h[1] = 'M'; put32(h, 2, bytes); put32(h, 10, 54);
    put32(h, 14, 40); put32(h, 18, (unsigned long)width);
    put32(h, 22, (unsigned long)height); h[26] = 1; h[28] = 24;
    if (fwrite(h, 1u, sizeof h, out) != sizeof h) ok = 0;
    for (y = height - 1; y >= 0 && ok; --y) {
        memset(composed, 255, (size_t)rowbytes);
        for (i = 0; i < 15; ++i) if (heights[i] && y < heights[i]) {
            long at = 54L + (long)(heights[i] - 1 - y) * sectionbytes;
            if (fseek(ins[i], at, SEEK_SET) ||
                fread(scanline, 1u, (size_t)sectionbytes, ins[i]) != (size_t)sectionbytes)
                { ok = 0; break; }
            memcpy(composed + (size_t)col[i] * (size_t)canvas_width * 3u,
                   scanline, (size_t)canvas_width * 3u);
        }
        if (!ok) break;
        for (i = 0; i < cross; ++i) {
            const Route *r = &routes[i];
            int lo = r->x1 < r->x2 ? r->x1 : r->x2;
            int hi = r->x1 > r->x2 ? r->x1 : r->x2;
            if (y >= r->track - 1 && y <= r->track + 1)
                for (j = lo; j <= hi; ++j) rowdot(j, width);
            if (y >= r->y1 && y <= r->track)
                for (j = -1; j <= 1; ++j) rowdot(r->x1 + j, width);
            if (y >= r->y2 && y <= r->track)
                for (j = -1; j <= 1; ++j) rowdot(r->x2 + j, width);
        }
        if (fwrite(composed, 1u, (size_t)rowbytes, out) != (size_t)rowbytes) ok = 0;
    }
done:
    for (i = 0; i < 15; ++i) if (ins[i]) {
        if (ferror(ins[i])) ok = 0;
        if (fclose(ins[i])) ok = 0;
    }
    if (out) { if (ferror(out)) ok = 0; if (fclose(out)) ok = 0; }
    if (!ok || (created && (exists(path) || rename(temp, path)))) {
        if (created) remove(temp);
        fputs("BENCH: combined BMP write or rename failed\n", stderr); return 0;
    }
    printf("LINK MAP: %d cross-section link%s drawn between %d section bitmaps, %s (%dx%d)\n",
           cross, cross == 1 ? "" : "s", count, path, width, height);
    return 1;
}
int main(int argc, char **argv)
{
    char section, *end;
    long scale;
    int created[15] = {0}, heights[15] = {0}, footers[15] = {0}, i, cross = 0;
    char link_path[1024], link_temp[1100];
    if (argc != 4) {
        fputs("Usage: shakti_batch_bitmap_v3 graph.txt output_prefix SCALE\n", stderr); return 2;
    }
    scale = strtol(argv[3], &end, 10);
    if (!*argv[3] || *end || scale < 750 || scale > W)
        { fputs("BENCH: scale must be 750..1500\n", stderr); return 2; }
    canvas_width = (int)scale;
    if (!load(argv[1]) || !resolve_edges()) return 2;
    for (i = 0; i < nedge; ++i)
        if (nodes[edges[i].from].section != nodes[edges[i].to].section) cross++;
    if (cross > MAX_ROUTES) {
        fputs("BENCH: cross-section link map exceeds fixed route capacity\n", stderr); return 2;
    }
    if (cross && (snprintf(link_path, sizeof link_path, "%s_links.bmp", argv[2]) >= (int)sizeof link_path ||
        snprintf(link_temp, sizeof link_temp, "%s.tmp", link_path) >= (int)sizeof link_temp ||
        exists(link_path) || exists(link_temp))) {
        fputs("BENCH: combined BMP path exists or is too long\n", stderr); return 2;
    }
    for (section = 'A'; section <= 'O'; ++section) {
        int outcome = render(section, argv[2], &heights[section - 'A'], &footers[section - 'A']);
        if (!outcome) {
            char prior;
            for (prior = 'A'; prior <= section; prior++) if (created[prior - 'A']) {
                char path[1024];
                if (snprintf(path, sizeof path, "%s_%c.bmp", argv[2], prior) < (int)sizeof path)
                    remove(path); /* Only maps created by this failed run. */
            }
            return 2;
        }
        created[section - 'A'] = outcome == 2;
    }
    if (!save_links(argv[2], heights, footers)) {
        for (section = 'A'; section <= 'O'; ++section) if (created[section - 'A']) {
            char path[1024];
            if (snprintf(path, sizeof path, "%s_%c.bmp", argv[2], section) < (int)sizeof path)
                remove(path);
        }
        return 2;
    }
    printf("TOTAL: %d functions, %d exact address links\n", nnode, nedge);
    return 0;
}
