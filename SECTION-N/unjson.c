/* unjson.c — recover a pasted source file from a session transcript.
 *
 * A transcript line is one JSON object; any source the user pasted lives
 * inside it with real newlines encoded as the two characters \ and n. This
 * program copies one line out and turns those escapes back into bytes, so
 * the original paste can be read and compiled instead of retyped from memory.
 *
 * It decodes only what JSON strings actually contain: \" \\ \/ \b \f \n \r
 * \t and \uXXXX. A \u above the ASCII range is written back as UTF-8, so
 * text the user typed in another script survives the trip. Every other byte
 * passes through untouched.
 *
 * Reusable: nothing here knows about this project. Point it at any
 * .jsonl transcript and any line number.
 *
 * C99. No heap. Streams; the line may be any size.
 *
 *   cc -std=c99 -pedantic -Wall -Wextra -Werror -O2 unjson.c -o unjson
 *   ./unjson <file.jsonl> <line-number> > recovered.txt
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int hexval(int c)
{
    if (c >= '0' && c <= '9') { return c - '0'; }
    if (c >= 'a' && c <= 'f') { return c - 'a' + 10; }
    if (c >= 'A' && c <= 'F') { return c - 'A' + 10; }
    return -1;
}

/* Write one Unicode scalar as UTF-8. Returns 0 if the value is not a scalar. */
static int put_utf8(unsigned long cp, FILE *out)
{
    if (cp < 0x80UL) {
        fputc((int)cp, out);
    } else if (cp < 0x800UL) {
        fputc((int)(0xC0UL | (cp >> 6)), out);
        fputc((int)(0x80UL | (cp & 0x3FUL)), out);
    } else if (cp < 0x10000UL) {
        fputc((int)(0xE0UL | (cp >> 12)), out);
        fputc((int)(0x80UL | ((cp >> 6) & 0x3FUL)), out);
        fputc((int)(0x80UL | (cp & 0x3FUL)), out);
    } else if (cp <= 0x10FFFFUL) {
        fputc((int)(0xF0UL | (cp >> 18)), out);
        fputc((int)(0x80UL | ((cp >> 12) & 0x3FUL)), out);
        fputc((int)(0x80UL | ((cp >> 6) & 0x3FUL)), out);
        fputc((int)(0x80UL | (cp & 0x3FUL)), out);
    } else {
        return 0;
    }
    return 1;
}

/* Read four hex digits. Returns -1 if fewer than four are present. */
static long read_u4(FILE *in)
{
    long v = 0L;
    int k;
    for (k = 0; k < 4; ++k) {
        int c = fgetc(in);
        int h;
        if (c == EOF) { return -1L; }
        h = hexval(c);
        if (h < 0) { return -1L; }
        v = (v << 4) | (long)h;
    }
    return v;
}

int main(int argc, char **argv)
{
    FILE *in;
    long want, line = 1L;
    int c;
    long escapes = 0L, newlines = 0L, unicode = 0L, bad = 0L;

    if (argc != 3) {
        fprintf(stderr, "usage: unjson <file.jsonl> <line-number>\n");
        return 2;
    }
    want = strtol(argv[2], NULL, 10);
    if (want < 1L) { fprintf(stderr, "line number must be 1 or more\n"); return 2; }

    in = fopen(argv[1], "rb");
    if (in == NULL) { fprintf(stderr, "cannot read %s\n", argv[1]); return 3; }

    /* skip to the wanted line */
    while (line < want) {
        c = fgetc(in);
        if (c == EOF) {
            fprintf(stderr, "file has only %ld lines\n", line);
            fclose(in);
            return 3;
        }
        if (c == '\n') { line++; }
    }

    while ((c = fgetc(in)) != EOF && c != '\n') {
        if (c != '\\') { fputc(c, stdout); continue; }
        c = fgetc(in);
        if (c == EOF) { fputc('\\', stdout); break; }
        escapes++;
        switch (c) {
        case 'n':  fputc('\n', stdout); newlines++; break;
        case 't':  fputc('\t', stdout); break;
        case 'r':  fputc('\r', stdout); break;
        case 'b':  fputc('\b', stdout); break;
        case 'f':  fputc('\f', stdout); break;
        case '"':  fputc('"',  stdout); break;
        case '/':  fputc('/',  stdout); break;
        case '\\': fputc('\\', stdout); break;
        case 'u': {
            long hi = read_u4(in);
            if (hi < 0L) { bad++; fputs("\\u", stdout); break; }
            /* a surrogate pair encodes one scalar above the BMP */
            if (hi >= 0xD800L && hi <= 0xDBFFL) {
                int a = fgetc(in);
                int b = fgetc(in);
                if (a == '\\' && b == 'u') {
                    long lo = read_u4(in);
                    if (lo >= 0xDC00L && lo <= 0xDFFFL) {
                        unsigned long cp = 0x10000UL +
                            (((unsigned long)(hi - 0xD800L) << 10) |
                              (unsigned long)(lo - 0xDC00L));
                        if (!put_utf8(cp, stdout)) { bad++; }
                        unicode++;
                        break;
                    }
                    bad++;
                    break;
                }
                if (a != EOF) { ungetc(a, in); }
                bad++;
                break;
            }
            if (!put_utf8((unsigned long)hi, stdout)) { bad++; }
            unicode++;
            break;
        }
        default:
            /* not a JSON escape; keep both bytes so nothing is invented */
            fputc('\\', stdout);
            fputc(c, stdout);
            escapes--;
            break;
        }
    }
    fclose(in);

    fprintf(stderr,
        "unjson: line %ld  escapes decoded %ld  (newlines %ld, \\u %ld)  undecodable %ld\n",
        want, escapes, newlines, unicode, bad);
    return bad != 0L;
}
