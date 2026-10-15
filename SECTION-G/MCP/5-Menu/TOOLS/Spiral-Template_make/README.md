# Spiral-Template_make

## I. INTRODUCTION

shakti-template-make is the whole-file template extractor, Tyler's spiral law. One file in, every design out; no other tools touch the input. Pure XML documents are walked whole; in mixed markdown, prose outside ```xml fences is skipped and every fenced block is walked as a document. Repeated records merge into their distinct FORMS, written in first-seen order with repeat counts. The first tag and its final match are the document frame, seated at lvl="0". Static BSS memory edition — zero malloc/realloc/free; overflow BENCHes, never truncates.

## II. REQUIREMENTS

C99 strict, zero heap, static BSS arenas:

```
cc -std=c99 -pedantic -Wall -Wextra -Werror -O2 Spiral.Template_make.c -o shakti-template-make
./shakti-template-make <file> [full]
```

Caps: 20 MB line buffer, 100,000 nodes, 1,024 children per parent, 10,000 roots, 4,096 depth.

## III. RUNNING NOTES

K3 acceptance run, 2026-10-02, on repo fixtures — all verified live:

- B_WORK_FILLED.xml -> 1 form, two-match CONFIRMED: <SECTION> level=1 repeats=24, repeat written once, exit 0
- C_WORK_FILLED.xml -> exit 0; output byte-IDENT to the malloc edition's output (byte-parity, per the 2026-09-29 ruling)
- two consecutive runs: byte-identical output
- malformed input (<a><b>x</a>): loud BENCH "attach failed line 1", nonzero exit, no output written
- strict build clean, zero diagnostics

KNOWN LIMITS: same-line tags are neutral (no descent); later record variants unseen unless 'full'; over-cap benches instead of growing.

## IV. REFERENCES

- Source verified against Tyler's upload (sha256 below); malloc edition deleted 2026-09-29 per Tyler's ruling, repair log line 21.
- Feeds: filled work-order XML -> template shapes for the extraction chain.
