/* shakti_batch_extract_v2.c | 2026-09-26 | C99, fixed storage, no heap.
 * QA SPECIALIST: GPT-6 (Codex); LOCATION: ChatGPT Work workspace.
 * SUPERVISOR QA: pending an independent model; LOCATION: pending.
 * SUPERVISOR NOTES: reserved for that model's review; no approval asserted.
 * Build: cc -std=c99 -O2 -Wall -Wextra -Werror -pedantic shakti_batch_extract_v2.c -o shakti_batch_extract_v2
 * Run: ./shakti_batch_extract_v2 XML_OR_LIST graph.txt; list entries: A..O PATH.
 * Each SECTION supplies work-order fields plus an explicit function_address.
 * No inferred edges; malformed/oversized input BENCH; output uses .tmp-rename.
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define FILE_CAP (4u * 1024u * 1024u)
#define TEXT_CAP 1024u
#define MAX_RECORDS 2048
static char source[FILE_CAP + 1u];
static char addresses[MAX_RECORDS][32];
static char upstreams[MAX_RECORDS][256];
static int records;
typedef struct {
    char file[192], name[128], address[32], order[32];
    char type[160], input[256], result[96], upstream[256];
    char complete[24], desc[TEXT_CAP];
} Record;
typedef struct { char name[64]; size_t begin, end; int close, self; } Tag;
/* One lexical pass respects quotes in tags and ignores comments and CDATA. */
static int next_tag(const char *s, size_t limit, size_t *at, Tag *t)
{
    size_t i, a, n;
    char quote;
again:
    i = *at; quote = 0;
    while (i < limit && s[i] != '<') ++i;
    if (i >= limit) { *at = limit; return 0; }
    if (i + 4u <= limit && memcmp(s + i, "<!--", 4u) == 0) {
        for (i += 4u; i + 3u <= limit; ++i)
            if (memcmp(s + i, "-->", 3u) == 0) { *at = i + 3u; goto again; }
        return -1;
    }
    if (i + 9u <= limit && memcmp(s + i, "<![CDATA[", 9u) == 0) {
        for (i += 9u; i + 3u <= limit; ++i)
            if (memcmp(s + i, "]]>", 3u) == 0) { *at = i + 3u; goto again; }
        return -1;
    }
    t->begin = i++;
    if (i >= limit) return -1;
    if (s[i] == '!' || s[i] == '?') {
        while (i < limit && s[i] != '>') ++i;
        if (i == limit) return -1;
        *at = i + 1u;
        goto again;
    }
    t->close = (s[i] == '/');
    if (t->close) ++i;
    a = i;
    while (i < limit && (isalnum((unsigned char)s[i]) || s[i] == '_' || s[i] == '-' || s[i] == ':')) ++i;
    n = i - a;
    if (!n || n >= sizeof(t->name)) { *at = t->begin + 1u; goto again; }
    memcpy(t->name, s + a, n); t->name[n] = 0;
    for (; i < limit; ++i) {
        if (quote) { if (s[i] == quote) quote = 0; }
        else if (s[i] == '\'' || s[i] == '"') quote = s[i];
        else if (s[i] == '>') break;
    }
    if (i >= limit) return -1;
    t->self = !t->close && i > a && s[i - 1u] == '/';
    t->end = i + 1u; *at = t->end;
    return 1;
}

static int bounded_text(const char *s, size_t a, size_t b, char *out, size_t cap)
{
    size_t n = 0;
    while (a < b && isspace((unsigned char)s[a])) ++a;
    while (b > a && isspace((unsigned char)s[b - 1u])) --b;
    while (a < b) {
        unsigned char c = (unsigned char)s[a++];
        if (n + 1u >= cap) return 0;
        out[n++] = isspace(c) ? ' ' : (char)c;
    }
    out[n] = 0;
    return 1;
}

static int field(const char *s, size_t a, size_t b, const char *name, char *out, size_t cap)
{
    Tag t, close;
    int rc;
    out[0] = 0;
    while ((rc = next_tag(s, b, &a, &t)) > 0) {
        size_t seek, depth = 1u;
        if (t.close || strcmp(t.name, name) != 0) continue;
        if (t.self) return 1;
        seek = t.end;
        while ((rc = next_tag(s, b, &seek, &close)) > 0) {
            if (strcmp(close.name, name) != 0) continue;
            if (close.close) {
                if (--depth == 0u) return bounded_text(s, t.end, close.begin, out, cap);
            } else if (!close.self) ++depth;
        }
        return 0;
    }
    return 0;
}

static int get_record(size_t begin, size_t end, Record *r)
{
    memset(r, 0, sizeof(*r));
#define GET(xml, member) field(source, begin, end, xml, r->member, sizeof(r->member))
    if (!GET("file_name", file) || !GET("function_name", name) ||
        !GET("function_address", address) || !GET("function_order", order) ||
        !GET("function_type", type) || !GET("function_input", input) ||
        !GET("function_return", result) || !GET("function_input_address", upstream) ||
        !GET("function_complete", complete) || !GET("function_description", desc)) return 0;
#undef GET
    if (!r->file[0] || !r->name[0] || !r->order[0]) return 0;
    if (!r->address[0] || !strcmp(r->address, "UNSET")) return 0;
    if (!r->upstream[0]) strcpy(r->upstream, "NULL");
    if (!r->complete[0]) strcpy(r->complete, "unknown");
    return 1;
}

static int write_record(FILE *out, char section, const Record *r)
{
    int i;
    if (records >= MAX_RECORDS) return 0;
    if (strcmp(r->address, "UNSET") != 0) {
        for (i = 0; i < records; ++i)
            if (strcmp(addresses[i], r->address) == 0) {
                fprintf(stderr, "BENCH: duplicate address %s\n", r->address); return 0;
            }
    }
    strcpy(addresses[records], r->address);
    strcpy(upstreams[records++], r->upstream);
    return fprintf(out,
        "NODE\nsection=%c\naddress=%s\nfile=%s\norder=%s\nname=%s\n"
        "type=%s\ninput=%s\nreturn=%s\ninput_addresses=%s\n"
        "complete=%s\ndescription=%s\nEND\n",
        section, r->address, r->file, r->order, r->name, r->type,
        r->input, r->result, r->upstream, r->complete, r->desc) > 0;
}

/* Explicit upstream tokens are the only edges. No type/order guessing. */
static int link_count(void)
{
    int i, j, links = 0;
    for (i = 0; i < records; i++) {
        char input[256], *token;
        strcpy(input, upstreams[i]);
        for (token = strtok(input, " ,;\t"); token; token = strtok(NULL, " ,;\t")) {
            if (!strcmp(token, "NULL") || !strcmp(token, "-")) continue;
            for (j = 0; j < records; j++) if (!strcmp(addresses[j], token)) break;
            if (j == records) { fprintf(stderr, "BENCH: unresolved upstream address %s\n", token); return -1; }
            links++;
        }
    }
    return links;
}

/* A direct XML file needs its own Section letter; a manifest may confirm it. */
static int section_id(const Tag *t, char expected, char *found)
{
    size_t at = t->begin + 1u + 7u;
    char id = 0;
    while (at < t->end && source[at] != '>') {
        size_t start, length, value;
        char quote;
        while (at < t->end && isspace((unsigned char)source[at])) at++;
        if (at >= t->end || source[at] == '>') break;
        start = at;
        while (at < t->end && (isalnum((unsigned char)source[at]) || source[at] == '_')) at++;
        if (at == start) return 0;
        length = at - start;
        while (at < t->end && isspace((unsigned char)source[at])) at++;
        if (at >= t->end || source[at++] != '=') return 0;
        while (at < t->end && isspace((unsigned char)source[at])) at++;
        if (at >= t->end) return 0;
        quote = source[at++]; if (quote != '\'' && quote != '"') return 0;
        value = at;
        while (at < t->end && source[at] != quote) at++;
        if (at == t->end) return 0;
        if (length == 2u && source[start] == 'i' && source[start + 1u] == 'd') {
            if (id || at != value + 1u || source[value] < 'A' || source[value] > 'O') return 0;
            id = source[value];
        }
        at++;
    }
    if (id && expected && id != expected) return 0;
    *found = id ? id : expected;
    return *found != 0;
}

static int parse_file(const char *path, char section, FILE *out)
{
    FILE *f = fopen(path, "rb");
    Tag t, q;
    size_t n = 0, at = 0;
    int c, rc, count = 0;
    if (!f) { fprintf(stderr, "BENCH: cannot open %s\n", path); return -1; }
    while ((c = fgetc(f)) != EOF) {
        if (n >= FILE_CAP) { fclose(f); fprintf(stderr, "BENCH: file too large %s\n", path); return -1; }
        source[n++] = (char)c;
    }
    if (ferror(f)) { fclose(f); return -1; }
    fclose(f); source[n] = 0;
    while ((rc = next_tag(source, n, &at, &t)) > 0) {
        size_t seek, depth, close_begin = 0;
        char actual;
        Record r;
        if (t.close || t.self || strcmp(t.name, "SECTION") != 0) continue;
        if (!section_id(&t, section, &actual)) {
            fprintf(stderr, "BENCH: invalid SECTION id in %s\n", path); return -1;
        }
        seek = t.end; depth = 1u;
        while ((rc = next_tag(source, n, &seek, &q)) > 0) {
            if (strcmp(q.name, "SECTION") != 0) continue;
            if (q.close) { if (--depth == 0u) { close_begin = q.begin; break; } }
            else if (!q.self) ++depth;
        }
        if (!close_begin || !get_record(t.end, close_begin, &r) || !write_record(out, actual, &r)) {
            fprintf(stderr, "BENCH: missing address, invalid or full SECTION in %s\n", path); return -1;
        }
        ++count; at = q.end;
    }
    if (rc < 0) { fprintf(stderr, "BENCH: malformed tag in %s\n", path); return -1; }
    if (!count) { fprintf(stderr, "BENCH: empty section in %s\n", path); return -1; }
    printf("%s %c: %d functions from %s\n", section ? "SECTION" : "DIRECT", section ? section : '*', count, path);
    return count;
}

static int exists(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (f) fclose(f);
    return f != NULL;
}
int main(int argc, char **argv)
{
    FILE *manifest = NULL, *out = NULL, *probe;
    char line[1024], path[1000], temp[1100], section, prior = '@';
    int total = 0, files = 0, links, c, direct, bad;
    if (argc != 3) { fputs("Usage: shakti_batch_extract_v2 XML_OR_LIST graph.txt\n", stderr); return 2; }
    if (strlen(argv[2]) + 5u >= sizeof temp || !strcmp(argv[1], argv[2]))
        { fputs("BENCH: output path is invalid\n", stderr); return 2; }
    snprintf(temp, sizeof temp, "%s.tmp", argv[2]);
    if (exists(argv[2]) || exists(temp))
        { fputs("BENCH: output or temporary path exists\n", stderr); return 2; }
    probe = fopen(argv[1], "rb");
    if (!probe) { fputs("BENCH: cannot read input\n", stderr); return 2; }
    do { c = fgetc(probe); } while (c != EOF && isspace((unsigned char)c));
    direct = c == '<';
    bad = ferror(probe); bad |= fclose(probe) != 0;
    if (bad) { fputs("BENCH: input read failed\n", stderr); return 2; }
    out = fopen(temp, "wb");
    if (!out) { fputs("BENCH: cannot open temporary graph\n", stderr); return 2; }
    if (direct) {
        total = parse_file(argv[1], 0, out); files = 1;
        if (total < 0) goto fail;
    } else {
        manifest = fopen(argv[1], "r");
        if (!manifest) goto fail;
        while (fgets(line, sizeof line, manifest)) {
            size_t len = strlen(line);
            int found;
            if (len && line[len - 1u] != '\n' && !feof(manifest))
                { fputs("BENCH: manifest line too long\n", stderr); goto fail; }
            if (sscanf(line, " %c %999[^\n]", &section, path) != 2) {
                if (strspn(line, " \t\r\n") == len) continue;
                fputs("BENCH: invalid manifest entry\n", stderr); goto fail;
            }
            if (section < 'A' || section > 'O' || section < prior)
                { fputs("BENCH: section order must be A..O\n", stderr); goto fail; }
            prior = section;
            while (*path && isspace((unsigned char)path[strlen(path) - 1u]))
                path[strlen(path) - 1u] = 0;
            found = parse_file(path, section, out);
            if (found < 0) goto fail;
            total += found; files++;
        }
        bad = ferror(manifest); bad |= fclose(manifest) != 0;
        manifest = NULL;
        if (bad) goto fail;
    }
    links = link_count();
    if (links < 0 || ferror(out) || files == 0 || total == 0) goto fail;
    if (fclose(out)) { out = NULL; goto fail; }
    out = NULL;
    if (exists(argv[2]) || rename(temp, argv[2])) goto fail;
    printf("RECORDED: %d exact function records, %d exact address links across %d file(s)\n",
           total, links, files);
    return 0;
fail:
    if (manifest) fclose(manifest);
    if (out) fclose(out);
    remove(temp);
    fputs("BENCH: no graph committed\n", stderr);
    return 2;
}
