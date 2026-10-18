# Shakti_Inventory

## I. INTRODUCTION

shakti_inventory is the joiner. It eats the extractor's LIST.md (function identities + per-function SHA-256) and call_scan's CALL records (every edge — same-file, cross-file, external), proves the two inputs came from the same source state, and emits the function_inventory XML that scopeflow draws from. Re-generated every run — never a cold list. As files are added, the inventory changes because the inputs change.

The join is exact, on the identity triple (file_name, function_name, function_order) per the 2026-09-27 addressing law. An orphan call record — a caller, or a resolved callee, missing from the list — means the two inputs came from different source states: BENCH with evidence, exit 2, nothing written.

Address law: the emitted address is a display locator (SECTION:basename:line), re-derived per run; it is not identity. scopeflow requires address[0] == the work order's SECTION letter, length <= 32, unique per run — collision or overflow BENCHes loud.

## II. REQUIREMENTS

C99 strict, zero heap, static arenas:

```
cc -std=c99 -pedantic -Wall -Wextra -Werror -O2 shakti_inventory.c -o shakti_inventory
./shakti_inventory LIST.md CALLS.txt OUT.xml
```

## III. RUNNING NOTES

K3 first live chain, registered Call_Scan fixtures (A.c/B.c/C.c from main), 2026-09-28 — all verified:

- extract -> fill -> shakti_work_order validate -> call_scan -> shakti_inventory -> scopeflow validate/build/notes: every step exit 0
- JOINED: functions=5 calls=6 direct=4 external=2; scopeflow READ: internal_links=4 open_or_external_links=2 descriptions_pending=0
- long-distance edge drawn (far at A.c:1005 -> alpha at A.c:3); cross-file bridges drawn (from_b -> alpha, from_b -> beta); externals (printf, missing) listed under OPEN / OUTSIDE-SCOPE LINKS
- join + build run twice: byte-identical inventory and SVG
- extractor's FILE_SHA256 of A.c matched the registry line on main — pulled file is the registered file

STATUS: registered 2026-09-28. NOT yet proven — battery on larger trees and cross-check by a different family are still ahead.

KNOWN LIMITS: reads the extractor's render format and call_scan's CALL blocks only; regenerate both inputs from the same source state before joining. Caps: 4096 functions, 32768 calls, 8 MiB per input — over-cap BENCHes with no partial output. External calls carry no filename by design (unresolved). visibility (PROTOTYPE_RELIANT, etc.) stays in CALLS.txt; the inventory keeps the direct/external distinction that scopeflow draws.

## IV. REFERENCES

- shakti_extract (Xml-half) — emits LIST.md
- shakti_call_scan (Tools/Checkers/Call_Scan) — emits CALL records
- scopeflow v3.2 (controlled-scope-flow) — consumes function_inventory
