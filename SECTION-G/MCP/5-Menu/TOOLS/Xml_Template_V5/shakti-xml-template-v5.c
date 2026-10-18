/* SCRIPT TWO v5: C99 template maker. MODIFIED: 2026-09-26.
 * QA SPECIALIST: GPT-6 (Codex); LOCATION: ChatGPT Work workspace.
 * SUPERVISOR QA: pending different model; LOCATION: pending inspection.
 * INSTRUCTIONS: build with script one; run shakti-xml-template-v5
 * INPUT.txt REPORT.txt NAMES.txt TEMPLATE.xml MASTER.txt [fragments]
 * Append to MASTER only after confirmation; refuse existing outputs; invalid
 * wraps yield no output and leave the original input read only.
 */
#include "shakti-xml-wrap-v5.h"
#include <stdio.h>
#include <string.h>
#define MAX_WORDS 4096
#define PATH_SIZE 1100
static XmlScan scan_result;     /* Fixed storage, no heap and no subprocess. */
static char words[MAX_WORDS][XML_NAME], known[MAX_WORDS][XML_NAME];
static int occurrences[MAX_WORDS], patterns[MAX_WORDS];
static int nwords, nknown, repeats;
typedef char FixedBudget[(sizeof scan_result + sizeof words + sizeof known +
    sizeof occurrences + sizeof patterns <= 20u * 1024u * 1024u) ? 1 : -1];
static void lowercase(const char *src, char *dst)
{
    size_t i;
    for (i = 0; src[i] && i + 1 < XML_NAME; i++) {
        int c = (unsigned char)src[i];
        dst[i] = (char)((c == '-' || c == ':' || c == '.') ? '_' :
                        ((c >= 'A' && c <= 'Z') ? c + ('a' - 'A') : c));
    }
    dst[i] = 0;
}
static int contains(char list[][XML_NAME], int count, const char *word)
{
    int i;
    for (i = 0; i < count; i++) if (!strcmp(list[i], word)) return 1;
    return 0;
}
static int collect_words(void)
{
    int i, k;
    for (i = 0; i < scan_result.count; i++) {
        const char *word = scan_result.node[i].name;
        for (k = 0; k < nwords && strcmp(words[k], word); k++) { }
        if (k == nwords) {
            if (nwords == MAX_WORDS) return 0;
            strcpy(words[nwords++], word);
        }
        occurrences[k]++;
    }
    return 1;
}
static int valid_master_name(const char *s)
{
    size_t i, n = strlen(s);
    if (n < 3 || s[0] != '<' || s[n - 1] != '>') return 0;
    for (i = 1; i < n - 1; i++) {
        int c = (unsigned char)s[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_') continue;
        if (i > 1 && ((c >= '0' && c <= '9') || c == '-' || c == ':' || c == '.')) continue;
        return 0;
    }
    return 1;
}
/* Master is read with fgetc too. A damaged master is never extended. */
static int read_master(const char *path)
{
    FILE *in = fopen(path, "rb");
    char line[XML_NAME + 3];
    int c;
    size_t n = 0;
    if (!in) return 1; /* A missing master is created after successful output. */
    while ((c = fgetc(in)) != EOF) {
        if (c == '\n') {
            if (n && line[n - 1] == '\r') n--;
            line[n] = 0;
            if (!valid_master_name(line) || nknown == MAX_WORDS) { fclose(in); return 0; }
            line[n - 1] = 0;
            if (contains(known, nknown, line + 1)) { fclose(in); return 0; }
            memcpy(known[nknown], line + 1, n - 2);
            known[nknown++][n - 2] = 0;
            n = 0;
        } else {
            if (n + 1 >= sizeof line) { fclose(in); return 0; }
            line[n++] = (char)c;
        }
    }
    if (ferror(in)) { fclose(in); return 0; }
    if (n) {
        line[n] = 0;
        if (!valid_master_name(line) || nknown == MAX_WORDS) { fclose(in); return 0; }
        line[n - 1] = 0;
        if (contains(known, nknown, line + 1)) { fclose(in); return 0; }
        memcpy(known[nknown], line + 1, n - 2);
        known[nknown++][n - 2] = 0;
    }
    fclose(in);
    return 1;
}
static void tabs(FILE *out, int depth) { while (depth-- > 0) fputc('\t', out); }
static void attrs(FILE *out, const XmlNode *node)
{
    char owner[XML_NAME], key[XML_NAME], lower[XML_NAME];
    size_t at = 0, k;
    lowercase(node->name, owner);
    fprintf(out, " lvl=\"%d\"", node->depth + 1);
    while (node->attrs[at]) {
        k = 0;
        while (node->attrs[at] && node->attrs[at] != ' ') {
            if (k + 1 < sizeof key) key[k++] = node->attrs[at];
            at++;
        }
        key[k] = 0;
        if (node->attrs[at] == ' ') at++;
        lowercase(key, lower);
        fprintf(out, " %s=\"[%s.@%s]\"", key, owner, lower);
    }
}
static void render(int i, FILE *xml, FILE *verbose)
{
    const XmlNode *node = &scan_result.node[i];
    char owner[XML_NAME], child[XML_NAME], ref[2 * XML_NAME + 2];
    int j, k, previous = -1;
    for (k = 0; k < nwords; k++)
        if (!strcmp(words[k], node->name)) { patterns[k]++; break; }
    lowercase(node->name, child);
    if (node->parent >= 0) {
        lowercase(scan_result.node[node->parent].name, owner);
        snprintf(ref, sizeof ref, "%s.%s", owner, child);
    } else snprintf(ref, sizeof ref, "%s", child);
    tabs(verbose, node->depth);
    fprintf(verbose, "%s\t[%s]\tlevel=%d\tindent=%d\tat=%ld\t%s\n",
            node->name, ref, node->depth + 1, node->indent, node->offset,
            node->after > i + 1 ? "BRANCH" : "LEAF");
    tabs(xml, node->depth);
    fprintf(xml, "<%s", node->name);
    attrs(xml, node);
    if (node->self) { fputs("/>\n", xml); return; }
    if (node->after == i + 1) {
        fprintf(xml, ">[%s]</%s>\n", ref, node->name);
        return;
    }
    fputs(">\n", xml);
    for (j = i + 1; j < node->after; j = scan_result.node[j].after) {
        if (previous >= 0 && shakti_xml_same_shape(&scan_result, previous, j)) {
            if (scan_result.node[j].after > j + 1) repeats++;
            continue; /* Fold identical sibling leaves as well as branches. */
        }
        if (node->depth == 0 && previous >= 0) {
            fputc('\n', xml); fputc('\n', verbose);
        }
        render(j, xml, verbose);
        previous = j;
    }
    tabs(xml, node->depth);
    fprintf(xml, "</%s>\n", node->name);
}
static int exists(const char *path)
{
    FILE *file = fopen(path, "rb");
    if (file) fclose(file);
    return file != NULL;
}
static void remove_temps(char paths[][PATH_SIZE])
{
    int i;
    for (i = 0; i < 3; i++) if (paths[i][0]) remove(paths[i]);
}
/* Publish a complete append-only master version after closing its temp file. */
static int append_master(const char *path)
{
    FILE *in, *out;
    char temp[PATH_SIZE];
    int i, added = 0, last = '\n', c, ok = 1;
    for (i = 0; i < nwords; i++) if (!contains(known, nknown, words[i])) added++;
    if (!added) return 0;
    if (strlen(path) + 5 >= sizeof temp) return -1;
    snprintf(temp, sizeof temp, "%s.tmp", path);
    if (exists(temp)) return -1;
    in = fopen(path, "rb"); out = fopen(temp, "wb");
    if (!out) { if (in) fclose(in); return -1; }
    if (in) {
        while ((c = fgetc(in)) != EOF) { last = c; if (fputc(c, out) == EOF) ok = 0; }
        if (ferror(in)) ok = 0;
        if (fclose(in)) ok = 0;
    }
    if (last != '\n' && fputc('\n', out) == EOF) ok = 0;
    for (i = 0; i < nwords; i++)
        if (!contains(known, nknown, words[i]) && fprintf(out, "<%s>\n", words[i]) < 0) ok = 0;
    if (ferror(out)) ok = 0;
    if (fclose(out)) ok = 0;
    if (!ok || rename(temp, path)) { remove(temp); return -1; }
    return added;
}
int main(int argc, char **argv)
{
    FILE *source, *verbose = NULL, *names = NULL, *xml = NULL;
    char temp[3][PATH_SIZE] = {{0}}, *out[3];
    int i, j, previous_root = -1, rc = 2, added, close_error, confirmed, printed = 0;
    if (argc != 6 && !(argc == 7 && !strcmp(argv[6], "fragments"))) {
        fputs("Usage: shakti-xml-template-v5 INPUT.txt TEMPLATE.txt NAMES.txt TEMPLATE.xml MASTER.txt [fragments]\n", stderr);
        return 2;
    }
    if (!strcmp(argv[1], argv[5]))
        { fputs("REFUSE: input and master paths must differ\n", stderr); return 2; }
    source = fopen(argv[1], "rb");
    if (!source) { fprintf(stderr, "REFUSE: cannot read %s\n", argv[1]); return 2; }
    if (!shakti_xml_scan(source, &scan_result)) {
        fprintf(stderr, "REFUSE: %s\n", scan_result.error);
        fclose(source);
        return 2;
    }
    fclose(source);
    if (scan_result.roots != 1 && argc == 6)
        { fputs("REFUSE: several wrapped objects; use fragments mode\n", stderr); return 2; }
    confirmed = shakti_xml_confirmed(&scan_result);
    if (!collect_words() || !read_master(argv[5])) {
        fputs("REFUSE: element limit reached or master list is damaged\n", stderr);
        return 2;
    }
    for (i = 0; i < 3; i++) {
        int j;
        out[i] = argv[i + 2];
        if (strlen(out[i]) + 5 >= PATH_SIZE) goto fail;
        snprintf(temp[i], sizeof temp[i], "%s.tmp", out[i]);
        if (exists(out[i]) || exists(temp[i]) || !strcmp(out[i], argv[1]) ||
            !strcmp(out[i], argv[5]) || !strcmp(temp[i], argv[1]) ||
            !strcmp(temp[i], argv[5])) goto fail;
        for (j = 0; j < i; j++) if (!strcmp(out[i], out[j])) goto fail;
    }
    verbose = fopen(temp[0], "wb");
    names = fopen(temp[1], "wb");
    xml = fopen(temp[2], "wb");
    if (!verbose || !names || !xml) goto fail;
    for (i = 0; i < scan_result.count; i = scan_result.node[i].after) {
        if (previous_root >= 0 &&
            shakti_xml_same_shape(&scan_result, previous_root, i)) { repeats++; continue; }
        if (previous_root >= 0) { fputs("\n\n", xml); fputs("\n\n", verbose); }
        fprintf(verbose, "# structure from %s; root %s\n", argv[1], scan_result.node[i].name);
        render(i, xml, verbose);
        previous_root = i;
    }
    for (i = 0; i < nwords; i++) fprintf(names, "%s\n", words[i]);
    fputs("\n# discovery order\telement\topen->close\tlines\tparent\tlevel\tboundary\n", verbose);
    for (i = 0; i < scan_result.count; i++) {
        const XmlNode *n = &scan_result.node[i];
        fprintf(verbose, "%d\t%s\t%u->%u\t%u\t%s\t%d\t%s\n", i + 1,
                n->name, n->line, n->close_line, n->close_line - n->line + 1,
                n->parent < 0 ? "(root)" : scan_result.node[n->parent].name,
                n->depth + 1, n->self ? "self-closing" : "matched");
    }
    fprintf(verbose, "\n# match confirmation: %s\n",
            confirmed ? "two matching records" : "one structure; master unchanged");
    fputs("\n# element\tfound\ttemplate_patterns\n", verbose);
    for (i = 0; i < nwords; i++)
        fprintf(verbose, "%s\t%d\t%d\n", words[i], occurrences[i], patterns[i]);
    for (i = 0; i < scan_result.count; i++) {
        const XmlNode *n = &scan_result.node[i];
        const char *p = n->attrs, *v = n->values;
        while (*p) {
            const char *end = strchr(p, ' ');
            size_t len = end ? (size_t)(end - p) : strlen(p);
            if (!printed++) fputs("\n# attribute values: node\tname\tvalue\n", verbose);
            fprintf(verbose, "%d\t%.*s\t", i + 1, (int)len, p);
            for (; *v; v++) {
                const char *esc = *v == '\n' ? "\\n" : *v == '\r' ? "\\r" :
                                  *v == '\t' ? "\\t" : *v == '\\' ? "\\\\" : NULL;
                if (esc) fputs(esc, verbose); else fputc(*v, verbose);
            }
            fputc('\n', verbose); v++; p = end ? end + 1 : p + len;
        }
    }
    if (ferror(verbose) || ferror(names) || ferror(xml)) goto fail;
    close_error = fclose(verbose);
    close_error |= fclose(names);
    close_error |= fclose(xml);
    verbose = names = xml = NULL;
    if (close_error) goto fail;
    for (i = 0; i < 3; i++) if (rename(temp[i], out[i])) {
        for (j = 0; j < i; j++) remove(out[j]);
        remove_temps(temp);
        fputs("REFUSE: output move failed; generated files removed\n", stderr);
        return 2;
    }
    added = confirmed ? append_master(argv[5]) : 0;
    if (added < 0) {
        for (i = 0; i < 3; i++) remove(out[i]);
        fputs("REFUSE: master append failed; generated outputs removed\n", stderr);
        return 2;
    }
    fprintf(stderr, "OK: %d wrapped object(s), %d unique names, %d master additions, %d repeated shapes; %s\n",
            scan_result.roots, nwords, added, repeats,
            confirmed ? "confirmed" : "single structure; master unchanged");
    return 0;
fail:
    if (verbose) fclose(verbose);
    if (names) fclose(names);
    if (xml) fclose(xml);
    remove_temps(temp);
    fputs("REFUSE: output path exists, cannot be written, or output failed; originals retained\n", stderr);
    return rc;
}
