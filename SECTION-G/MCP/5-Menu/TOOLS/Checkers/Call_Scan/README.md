# Call_Scan

## I. INTRODUCTION

shakti_call_scan finds every function call in every file of every section — same file or cross-file — and writes down callee, caller, file, and line. Two passes: a definitions index (with line numbers and 1-based order), then a call-site scan. Output feeds the WO template's call-site fields and, from there, the flow charts. Edges from this tool are scan-proven: each one points at a physical line you can open.

**RUN LAW: this tool NEVER runs on a live working branch.** Pull a fresh copy down and run it on that. It reads only — but the law stands: scans happen on a pulled copy, never on the branch being worked.

**TOKENIZER LAW: no lexers on Shakti.** This tool carries its own tokenizer, written in plain C in this same file. We do not link lex, flex, or any lexer library, and no black boxes that resemble smart code are acceptable anywhere in Shakti. If a tool cannot read C with its own eyes, it does not read C.

## II. REQUIREMENTS

C99 strict, zero heap, Linux (atomic publish uses renameat2 RENAME_NOREPLACE):

```
cc -std=c99 -pedantic -Wall -Wextra -Werror -O2 shakti_call_scan_v1_0.c -o shakti_call_scan
./shakti_call_scan sections.manifest new-records.txt
```

Manifest: one `SECTION_LETTER PATH` per line, sections in A..O order, .c paths only. Relative paths resolve from the manifest's directory.

## III. RUNNING NOTES

K3 acceptance battery, own fixtures, 2026-09-26 — all verified:

- 3-file fixture (5 definitions, 4 prototypes): same-file call found 1003 lines below the definition at exact line; cross-file calls marked PROTOTYPE_RELIANT; fake calls inside a comment and a string literal never counted; `printf` and an undefined name listed EXTERNAL — run exits 0
- Two consecutive runs: byte-identical records
- Existing output path: `BENCH: publish rec1.txt without overwrite: File exists`, exit 2, original untouched
- Over-cap fixture (8,193 definitions): `BENCH: definition cap 8192`, exit 2, no partial output file written

KNOWN GAPS (carried from the builder's contract, verbatim intent): lexical scan only — no #include, macro, or conditional-compilation expansion; header-only declarations surface as DECLARATION_UNSEEN; function pointers, shadowed names, macro invocations, trigraphs, and complex declarators are not resolved. Atomic publication requires Linux renameat2; unsupported platforms BENCH rather than clobber.

## IV. REFERENCES

Test samples in tests/, named after the files they came from:

- A.c
- B.c
- C.c
- big.c
- sections.manifest
- big.manifest
- rec1.txt
