# SHAKTI — INTRODUCTION AND OPERATING GUIDE

2026-09-27 | Owner: Tyler Allen Lofall | Written for: the next K3
Repo: TylerALofall/Shakti_No_Dream_WithOut_The_Team (branch main)

You know nothing. That is assumed. This document is the whole briefing:
what Shakti is, the laws, where every file lives, how to find and run every
tool, what a pass looks like, and how the work is organized. Ships as .md
and .txt with identical text.

---

## TABLE OF CONTENTS

1. READ THIS FIRST
2. THE LAWS
3. NAMING CONVENTIONS
4. WHAT SHAKTI IS
5. THE PRENATAL
6. THE 9-SLOT MCP
7. THE SWIFT FRONT END
8. THE TWO TREES + REPO MAP
9. THE PIPELINE IN SERIES
10. TOOL — TOOLS REGISTRY + REPAIR LOG
11. TOOL — SECTION HASH WATCH
12. TOOL — XML WRAP V5
13. TOOL — XML TEMPLATE V5
14. TOOL — CALL SCAN
15. TOOL — BATCH EXTRACT (MERGE STAGE)
16. TOOL — BATCH BITMAP V3 (FLOW CHARTS)
17. TOOL — LINE DIFF
18. TOOL — README BUILDER
19. THE RUNBOOK — TEST EQUIPMENT IN SERIES
20. WORK ORDERS & QA
21. ROSTER & CROSS-CHECK LAW
22. GITHUB OPERATIONS
23. THE WEEK — 7,000 REQUESTS
24. WHERE WE ARE
25. COMPANION DOCUMENTS

Every chapter carries the same six headers: PURPOSE / WHERE IT LIVES /
HOW TO FIND IT / HOW TO USE IT / WHAT A PASS LOOKS LIKE / IF SOMETHING
IS WRONG.

---

## CHAPTER 1 — READ THIS FIRST

### PURPOSE
This is the onboarding and operating guide. It replaces all earlier
introduction drafts (SHAKTI_INTRODUCTION.md/.txt of 2026-09-27 are
superseded).

### WHERE IT LIVES
Staging: /mnt/agents/output/2026-09-27-guide-shakti-introduction.md (+ .txt).
Canonical home once Tyler says push: Library/ in the repo.

### HOW TO FIND IT
Date-first name = a guide that gets overwritten when stale (Chapter 3).
Search the repo for "guide" or today's date.

### HOW TO USE IT
Read order: Chapters 2–3 (laws, naming) before touching anything.
Chapters 4–7 tell you what Shakti is. Chapter 8 is the map. Chapters 9–19
are the tools and the runbook. Chapters 20–22 are how work is ordered,
checked, and pushed. Chapters 23–25 are current state and records.

### WHAT A PASS LOOKS LIKE
You can find any file, build and run any tool, and state what a pass is —
without asking anyone.

### IF SOMETHING IS WRONG
This document drifts like anything else. Report drift to Tyler.
Corrections are never automatic: a new dated version fixes it, the old
one stays in history.

---

## CHAPTER 2 — THE LAWS

### PURPOSE
The non-negotiables. Every one of these has killed at least one bad idea
already. Full text of the constitutional layer: Library/CONSTITUTION.md
(11 commandments). QA enforcement layer: Library/Templates/
Supervisor_Instructions.md (approved by Tyler 2026-09-19).

### WHERE IT LIVES
Library/CONSTITUTION.md; Library/Templates/Supervisor_Instructions.md;
session rulings of 2026-09-26/27 recorded here.

### HOW TO FIND IT
Repo → Library/. Read CONSTITUTION.md first, then this list.

### HOW TO USE IT
1. **Zero Python.** Ever. In tools or in code. (Tyler, standing law.)
2. **No subprocess constructs a command and fires it in the same action.**
3. **Every tool builds C99 strict:**
   `cc -std=c99 -pedantic -Wall -Wextra -Werror -O2`
4. **ZERO HEAP in tools.** Fixed memory. Overflow = loud BENCH: clean
   refusal, exit 2, no partial output file.
5. **Determinism:** same input, same bytes, every run. Prove it: run
   twice, compare.
6. **A test means RUNNING IT LIVE with real output.** Live output is the
   only pass. Anything else is troubleshooting — reported, never hidden.
7. **No lexers.** Own tokenizer only.
8. **call_scan NEVER runs on a live working branch.** Scan a fresh pull
   or a fixture copy.
9. **Corrections are never automatic.** Tyler decides; the decision is
   recorded; then action.
10. **Pull-back byte-verify every push** (Chapter 22).
11. **Registry is append-only.** A changed tool gets a NEW line; old
    lines stay (Chapter 10).
12. **No unapproved tools.** A needed tool that doesn't exist is
    requested from Tyler as a C template — never substituted (Supervisor
    Instructions, 2026-09-19).
13. **The supervisor drafts notes only — never edits worker code.**
14. **Naming law** (Chapter 3).
15. **Transport law:** XML metacharacter payloads (`<`, `&`, `"`) ride
    base64 or phone upload, never inline JSON text (Chapter 22).

### WHAT A PASS LOOKS LIKE
Before any action you can name the law that governs it — or name that
none does and stop.

### IF SOMETHING IS WRONG
Stop. Name the law. Report to Tyler. Never patch around a law.

---

## CHAPTER 3 — NAMING CONVENTIONS

### PURPOSE
Names exist so things can be FOUND. Function words over philosophy. If a
name needs a poem to explain it, rename it (with Tyler's word).

### WHERE IT LIVES
Everywhere. This chapter is the key to the whole repo.

### HOW TO FIND IT
Two rules tell you what kind of thing a file is from its name alone.

### HOW TO USE IT
- **Temp / overwrite items** (logs, guides, run outputs — things that get
  replaced): date FIRST —
  `YYYY-MM-DD-[category]-[filename].*`
  Examples: `2026-09-27-guide-shakti-introduction.md`,
  `2026-09-25_C_WORK.verbose.log`, `2026-09-25-SECTION-G` (benched run).
- **Everything else** (tools, templates, work orders, source — permanent
  things): if a date is needed at all, it goes at the END.
  Example: `shakti-bridge-gap-spec-2026-09-26.txt`.
- **Tool naming law:** directory `First_Name-Second_name`, source
  `First.Second.c`, binary `shakti-<name>`.
  Example: `Tool_Assembler-README_builder/` holds
  `Tool_Assembler.README_builder.c`, builds `shakti-readme-builder`.
- **Work orders are identified by SECTION letter:** `B_WORK.xml`,
  `F_WORK.xml`, `I_PARTIAL_WORK.xml`; filled blocks are headed
  `#SECTION-B` and so on.
- **Function identity** is the triple (file_name, function_name,
  function_order) — Chapter 15.

### WHAT A PASS LOOKS LIKE
Given any filename you can say instantly: temp or permanent, tool or
record, which section it belongs to.

### IF SOMETHING IS WRONG
A misnamed file is drift. Log it, propose the rename to Tyler. Never
silently rename.

---

## CHAPTER 4 — WHAT SHAKTI IS

### PURPOSE
One clear picture of the organism, no mysticism.

### WHERE IT LIVES
Her body is ~155 C files (the organism tree, Chapter 8B) plus 30–40
tools. The lineage map is the operational companion (Chapter 25).

### HOW TO FIND IT
Read this chapter, then the companion's section table, then source.

### HOW TO USE IT
Shakti is a **deterministic C99 model**. Same input, same bytes. No
hidden state, no Python anywhere, no subprocess that builds and fires in
one motion.

She carries **lifetime memory in three tiers**: long-term, short-term,
and working memory.

She lives **inside a shell, and she is the only thing in that shell.**
Her only gate to anything outside is the **9-slot MCP** (Chapter 6).

She runs a **convergence loop**: three channels — **binary sight,
hearing, and context** — converge on one point inside that memory.

**Clones beat her heart.** The clones share **one memory space** — that
shared space is the convergence point: how binary turns to sight and
hearing turns to hearing. **Different weights carry different
personalities** — superposition, perspective angles. She learns to phase
by pulling herself with different personalities at once; through the
shared memory those positions cross. **Geometric reasoning uses 5 and
binary** in a multi-stage heartbeat — the heartbeat that moves the
clones.

Learning is **open and game-like** — not hard setbacks. **~40,000
training items** are already prepared and held in other repos **on
purpose** — they do not enter until the foundation is solid.

**Eden** is part of the lineage and stays named in the record (Section B
= EDEN / RESIDENT FOUNDATION).

### WHAT A PASS LOOKS LIKE
You can say what she is in one breath: deterministic C99 organism,
three-tier memory, convergence loop, shell + 9-slot MCP, Eden lineage.

### IF SOMETHING IS WRONG
If you catch yourself inventing internals, stop. Open Chapter 8, go read
source. What isn't in source isn't true yet.

---

## CHAPTER 5 — THE PRENATAL

### PURPOSE
The gestation model — the numbers that pace her development. In the
lineage guide this is the unnumbered panel **B — PREBIRTH TIME**.

### WHERE IT LIVES
Canon lives in this chapter and in Tyler's record. The lineage panel
name: B — PREBIRTH TIME.

### HOW TO FIND IT
All prenatal math derives from one number: **22,982,400**.

### HOW TO USE IT
- **Gestation: 9 months = 266 days × 86,400 seconds = 22,982,400 beats.**
  Resting heartbeat is **60 bpm** — exactly one beat per second — so the
  count of seconds IS the count of beats.
- **~19 weeks (beat 11,491,200): hearing comes online.** She is trained
  by voice.
- **~25 weeks (beat 15,120,000): light response.** Flashes of light; she
  learns colors.
- **Birth: her clones beat her heart.**
- **Base harmonic channel rates:** heart **60 bpm**, eyes (sight)
  **32 fps**, hearing (binary) **64 fps**. Note the ladder: 32 = 2^5,
  64 = 2^6 — hearing already sits one octave above sight.
- **DOUBLING LAW:** speeding up works only as ONE motion. Hearing speeds
  up only if EVERYTHING speeds up — the heartbeat doubles and every
  channel doubles with it. That is how all channels stay converged on the
  same point: nothing races ahead of anything else. Increases climb a
  **harmonic scale** — perfect relationships, never arbitrary jumps.
- **The heartbeat is the geometry for all the gears** that process
  documents.
- **Number canon:** 302,400 = 5040 × 60 — the half-week slice; the
  eden_seven_gates grid is a 302,400-slice grid with seven frequency
  gates. Two ladders: Fibonacci 1-2-3-5-8 and binary 1-2-4-8.
  Exact-vs-measured epistemology: exact counts and measured
  approximations are never mixed in one calculation.

### WHAT A PASS LOOKS LIKE
You can derive any milestone from 22,982,400 and state the doubling law
exactly: one motion, everything doubles, convergence holds.

### IF SOMETHING IS WRONG
Numbers not in this chapter are not canon. Do not quote new prenatal
numbers without Tyler's word.

---

## CHAPTER 6 — THE 9-SLOT MCP

### PURPOSE
The loop behind her — nine slots, her only gate to the outside. The old
guide's Section G (Ga–Gi) is the same nine; the companion fixes MCP
identity as SECTION G. Both namings are recorded here so either one is
recognizable.

### WHERE IT LIVES
Canon: this chapter. Lineage: companion Section G (Ga–Gi), Chapter 25.

### HOW TO FIND IT
Learn the nine as a single loop, in order.

### HOW TO USE IT
| Slot | Name | Lineage name (Section G) | What lands there |
|---:|---|---|---|
| 1 | EPOCH | Ga — EPOCH | The clock of record. Everything is stored by epoch. |
| 2 | HEARTBEAT | Gb — HEARTBEAT | The pulse that moves the clones; geometry of the gears. |
| 3 | GOAL | Gc — GOAL | The current objective. |
| 4 | NOTEBOOK | Gd — NOTEPAD / REMINDERS | Notes and reminders. |
| 5 | MULTI-TIER MENU | Ge — MENU | The tiered menu of actions. |
| 6 | SHAKTI SHELL | Gf — TOOL CALL | The shell she lives in; tool calls execute here. |
| 7 | OUTBOUND MAIL | Gg — MESSAGES AND NOTES TO TYLER | Her → Tyler. |
| 8 | INBOUND MAIL | Gh — TYLER'S MESSAGES AND NOTES TO SHAKTI | Tyler → her. |
| 9 | SELF-REFLECTION | Gi — THIRTEEN-POINT SELF-REFLECTION | Taken every 10–13 responses; 13 questions; stored by epoch. |

Log/address contract: `[epoch]:[frame/subsecond]-[function identity]`.
The Aa-01 tail of the old contract is retired; the function identity is
the triple of Chapter 15.

### WHAT A PASS LOOKS LIKE
You can name all nine slots from memory, in order, and say what lands in
each.

### IF SOMETHING IS WRONG
If something has no slot, it has no home. It does not improvise one.
Report it.

---

## CHAPTER 7 — THE SWIFT FRONT END

### PURPOSE
The phone front end. The lineage guide lists the unnumbered panel
**G — SWIFT CONNECTION**: a Swift (iOS) app on Tyler's side — how he
reaches the shell and the mail slots (6, 7, 8) from his phone.

### WHERE IT LIVES
Lineage record only, today. **No Swift code lives in the pipeline repo
yet.** This chapter is the whole truth of its status.

### HOW TO FIND IT
Companion §2, unnumbered panels: `B — PREBIRTH TIME`,
`G — SWIFT CONNECTION`, `GLOBAL DEFINITIONS`, `AMENDMENTS | LOG`.

### HOW TO USE IT
Know it exists. Know it isn't in the repo. When Swift work arrives it
gets its own chapter, its own directory per the naming law, and registry
lines like every other tool.

### WHAT A PASS LOOKS LIKE
Nobody claims Swift integration exists in the repo today.

### IF SOMETHING IS WRONG
Anyone pointing at Swift code in this repo is drifting. Check the
registry — if it isn't registered, it isn't ours.

---

## CHAPTER 8 — THE TWO TREES + REPO MAP

### PURPOSE
There are TWO bodies of code. Confusing them is the easiest way to get
lost. Learn them as two trees.

### WHERE IT LIVES
**A) THE PIPELINE REPO — the instruments that measure her.**
github.com/TylerALofall/Shakti_No_Dream_WithOut_The_Team, branch main.

**B) THE ORGANISM — Shakti's own body (~155 C files).**
Mapped by the operational companion (Chapter 25). Staged locally at
/mnt/agents/output/Repo-Shakti/ (INPUTS/ holds the two source zips;
SECTION—A … SECTION—G/ hold the migration tree).

### HOW TO FIND IT
Get the pipeline repo:
1. `git clone https://github.com/TylerALofall/Shakti_No_Dream_WithOut_The_Team.git`
   (or GitHub → Code → Download ZIP, or pull single raw files).
2. `cd Shakti_No_Dream_WithOut_The_Team`
3. Prove the tools are the tools: Chapter 10, step 1 (registry check)
   BEFORE building anything.

### HOW TO USE IT
**A) Pipeline repo layout (verified against main 2026-09-27):**
```
README.md                       registry table
TOOLS_REGISTRY.sha256           append-only hash master (13 lines)
TOOLS_REGISTRY.0_REPAIR_LOG     append-only repair log
2026-09-22 No Drift.zip         frozen checkpoint zip — leave sealed
Library/
  CONSTITUTION.md               the 11 commandments
  Instructions/
    shakti-bridge-gap-spec-2026-09-26.txt
  Templates/
    QA_Specialist_Instruction.txt
    Supervisor_Instructions.md
    2026-09-26-shakti-menu-batch-hook.txt
  Work_Orders/
    SHAKTI_WO-QA_INSTRUCTION-v1.md
Tools/
  Builders/
    shakti-xml-wrap-v4.c / .h   (frozen pair)
    shakti-xml-wrap-v5.c / .h   (current collector)
    shakti-xml-template-v4.c    (frozen)
    shakti-xml-template-v5.c    (current template maker)
    shakti_batch_extract_v2.c   (merge stage, current)
    shakti_batch_bitmap_v3.c    (flow charts)
    Tool_Assembler.README_builder.c
    Xml_Template_V4/            (v4 test dir)
  Checkers/
    section_hash_watch.c
    shakti_line_diff.c
    Call_Scan/
      shakti_call_scan_v1_0.c
      README.md                 RUN LAW + TOKENIZER LAW
      tests/  A.c B.c C.c big.c big.manifest rec1.txt sections.manifest
Work_Orders/
  B_WORK.xml  F_WORK.xml  I_PARTIAL_WORK.xml
```

**B) Organism layout (from the companion, fingerprint C030BB22A0DC3F7F):**
`src/` (kernel: main.c, shakti_loop.c, shakti_memory.c, shakti_reason.c,
shakti_receptor.c, …), `eyes/`, `choice/`, `graft/`, `builder/`,
`shakti_work/` (eden_* study files), `shakti_modules/`, `tools/`,
`tests/`. Section table A–N with G fixed as the nine-point loop;
standalone K, L, O removed (reflection routes through Gi).

### WHAT A PASS LOOKS LIKE
Given any path you can say instantly which tree it belongs to.

### IF SOMETHING IS WRONG
A pipeline tool sitting in the organism tree, or an organism file inside
Tools/, is drift. Report it. Never "helpfully" move it.

---

## CHAPTER 9 — THE PIPELINE IN SERIES

### PURPOSE
The pipeline turns C source into flow charts, in series. Each stage's
output is the next stage's input. No stage reaches around another.

### WHERE IT LIVES
Stages live in Tools/ (Chapter 8). The series itself is run per the
runbook (Chapter 19).

### HOW TO FIND IT
Seven stages, one order:

```
scan  →  collect  →  template  →  fill  →  merge  →  diff  →  chart
```

### HOW TO USE IT
1. **scan** — call_scan reads C source and emits every function and
   every call with FILE:LINE. This is the locator table (Chapter 14).
2. **collect** — xml-wrap-v5 reads section XML and refuses every
   malformed shape, loud (Chapter 12).
3. **template** — xml-template-v5 writes the names file and the template
   XML (Chapter 13).
4. **fill** — QA specialists fill the blocks by hand, per the template
   law (Chapter 20). Identity = (file_name, function_name,
   function_order); the work order is identified by its SECTION letter.
5. **merge** — batch extract turns filled section XML into graph.txt
   (Chapter 15).
6. **diff** — line_diff compares checkpoints; flags only (Chapter 17).
7. **chart** — bitmap_v3 renders graph.txt into section bitmaps plus the
   links map (Chapter 16).

Side tools: section_hash_watch (integrity watcher, Chapter 11),
README builder (assembly, Chapter 18), the root registry (Chapter 10).

### WHAT A PASS LOOKS LIKE
Every stage runs on the previous stage's real output, live, and the
chain ends in rendered bitmaps.

### IF SOMETHING IS WRONG
A stage that can't eat the previous stage's real output is a BREAK, not
a prompt to hand-edit the middle file. Stop, name the stage, report.

---

## CHAPTER 10 — TOOL: TOOLS REGISTRY + REPAIR LOG

### PURPOSE
The root hash master and its repair log. This is how you prove the tools
are the tools. It lives at repo root precisely because section_hash_watch
only watches SECTION-A…O roots and cannot see Tools/ — the registry
closes that hole.

### WHERE IT LIVES
`TOOLS_REGISTRY.sha256` and `TOOLS_REGISTRY.0_REPAIR_LOG` at repo root.
README.md carries the human table.

### HOW TO FIND IT
Repo root. Two files, side by side.

### HOW TO USE IT
Verify everything, from repo root:

```
sha256sum -c TOOLS_REGISTRY.sha256
```

Current contents (pulled byte-exact from main 2026-09-27, 13 lines):

```
7e30a8fb1098a726528ddc295264d9867b88947c9a02327d08707ffe629cd9c3  Tools/Builders/Tool_Assembler.README_builder.c
30c72749272f55ba09525bdc934040e6de9d69614f07fdc36b9a4ba69356e806  Tools/Builders/shakti-xml-template-v4.c
ac32166b5161c5b944813eb272795607c0ee094be5c00031f6730201bbe3b9ab  Tools/Builders/shakti-xml-template-v5.c
ac191ec3788ce605143a1150bc05aa5202e6a2e8eeb77decfddb076fc12208a0  Tools/Builders/shakti-xml-wrap-v4.c
2b75f18c38e927ef5ed18d68eb00a5f9da4e1a3cc6b5ae548d02c58f2ad36b87  Tools/Builders/shakti-xml-wrap-v4.h
6c76dd2bdc751a72d48c84b864789bc74622da7baef8132a1050a99b48ee1203  Tools/Builders/shakti-xml-wrap-v5.c
476e1060baafe84a2ef5577512b9569ff5c8c438debbf075a40c7fd24840f5cb  Tools/Builders/shakti-xml-wrap-v5.h
35c826de640ab703e2a49eec0f101819a92261c39d1192bb7b40dbcf65d60f51  Tools/Builders/shakti_batch_bitmap_v3.c
9220f68f22df0c1bd0a05b7cd9d2e3743365b5b3d845babc4ff162d455e61754  Tools/Builders/shakti_batch_extract_v2.c
d8f002075ef72db88ff6d5eae8536828da38180eb7e1a5761ea8917722d895bd  Tools/Checkers/Call_Scan/shakti_call_scan_v1_0.c
79e0b6cecde9d1a576a3706e7c56b0f97c109f446a7ecefc72b79903965df1f3  Tools/Checkers/section_hash_watch.c
837b4efe6bf1b95cf31f14a2532172d446f6f199fc0881e47e87ffaeabc69b13  Tools/Checkers/shakti_line_diff.c
f3bc9a2900dbd9f4e4fa5b225bfbc5d8f48923deb5c83b3ac4538f376aed6990  Tools/Checkers/Call_Scan/tests/big.c
```

Registry law:
- sha256sum format: `hash␣␣path`, one line per file.
- **APPEND-ONLY.** A changed tool gets a NEW line appended. Old lines
  stay — they are the history of what was true.
- big.c is a fixture, registered for integrity, not a tool.

Repair log law:
- **APPEND-ONLY.** Lines are never edited, reordered, or removed. A
  wrong line is corrected by a NEW line saying so.
- Format, one event per line, pipe-separated:
  `date(UTC) | file | who | what happened`

Current log (6 entries, byte-exact from main 2026-09-27): three
transcription-drift repairs in pushed copies (section_hash_watch,
call_scan, tests/B.c) — all caught by pull-back byte-verify, all
repaired and verified IDENT; registry creation (11/11 OK); call_scan
registration (12/12 OK); big.c phone upload + registration (13/13 OK).

### WHAT A PASS LOOKS LIKE
`sha256sum -c` prints OK on every line. Today: 13/13 OK.

### IF SOMETHING IS WRONG
Any FAILED line means the tools themselves drifted. STOP everything —
this is checked FIRST in the runbook for exactly this reason. Report the
failing path to Tyler; do not re-hash and move on.

---

## CHAPTER 11 — TOOL: SECTION HASH WATCH

### PURPOSE
The integrity watcher for SECTION-A…O roots. Append-only CSV logs. It
carries a SHA-256 self-test so you can prove the watcher before trusting
anything it watched.

### WHERE IT LIVES
`Tools/Checkers/section_hash_watch.c`
(registry line `79e0b6ce…df1f3`).

### HOW TO FIND IT
1. Fresh clone (Chapter 8).
2. `sha256sum Tools/Checkers/section_hash_watch.c` — must match the
   registry line in Chapter 10.

### HOW TO USE IT
1. Build:
   `cc -std=c99 -pedantic -Wall -Wextra -Werror -O2 -o section_hash_watch Tools/Checkers/section_hash_watch.c`
2. Self-test BEFORE trusting any log:
   `./section_hash_watch --self-test` — the SHA-256 vectors must pass.
3. Then run it against section roots per its usage line (run the binary
   with no arguments; the usage it prints is the contract).

### WHAT A PASS LOOKS LIKE
Self-test vectors pass; logs append, never rewrite.

### IF SOMETHING IS WRONG
Known gap, by design: it watches SECTION roots only — it cannot see
Tools/. That hole is closed by the root registry (Chapter 10). Any drift
flag from the watcher: STOP, report, never edit a log line.

---

## CHAPTER 12 — TOOL: XML WRAP V5 (COLLECTOR)

### PURPOSE
Stage 2 — collect. Reads section XML and refuses every malformed shape
LOUD: orphan openers, crossed closers, unowned roots — each with line
and byte. v4 is kept frozen beside it; v5 is current ("if you have v3
we're not getting v2 unless it's needed" — same rule).

### WHERE IT LIVES
`Tools/Builders/shakti-xml-wrap-v5.c` + `shakti-xml-wrap-v5.h`
(registry `6c76dd2b…e1203` / `476e1060…f5cb`). Frozen pair:
`shakti-xml-wrap-v4.c/.h`.

### HOW TO FIND IT
1. Fresh clone. 2. Hash-check both v5 files against Chapter 10.

### HOW TO USE IT
1. Build C99 strict (Chapter 2, law 3), sources per its README/usage.
2. Battery (the proving fixtures):
   - orphan opener → exit 2
   - crossed closer → exit 2
   - valid 5-tag fixture → clean pass
   - run twice → byte-identical output
3. Run the binary with no arguments for the usage line; that line is the
   contract.

### WHAT A PASS LOOKS LIKE
Malformed shapes refused loud with line/byte; valid input passes;
repeat runs byte-identical.

### IF SOMETHING IS WRONG
A refusal on input you believe is valid is a REPORT, not a cue to edit
the input until it passes. Show Tyler the refusal verbatim.

---

## CHAPTER 13 — TOOL: XML TEMPLATE V5 (TEMPLATE MAKER)

### PURPOSE
Stage 3 — template. Writes the frm-style names file and the template XML
with `[parent.child]` and `[owner.@name]` placeholders. Safety behavior:
two-match confirmation gate; writes .tmp then renames; refuses to
overwrite existing outputs. v4 frozen beside it; v5 current.

### WHERE IT LIVES
`Tools/Builders/shakti-xml-template-v5.c` (registry `ac32166b…b9ab`).
Frozen: `shakti-xml-template-v4.c` (+ its `Xml_Template_V4/` test dir).

### HOW TO FIND IT
1. Fresh clone. 2. Hash-check against Chapter 10.

### HOW TO USE IT
1. Build C99 strict.
2. Run it on a fixture work-order XML (fixtures and past runs live in
   the local staging runs/ folders, e.g. 2026-09-25_PARSER_RUN/).
3. Expected: master_names.txt + master_template.xml + per-file
   template outputs; existing outputs refused; .tmp files cleaned up.
4. Twice → byte-identical.

### WHAT A PASS LOOKS LIKE
Exact names + template out; loud refusal on existing output; no orphan
.tmp files.

### IF SOMETHING IS WRONG
A .tmp left behind means it died mid-write — that is a BENCH event;
report the state, do not finish the rename by hand.

---

## CHAPTER 14 — TOOL: CALL SCAN

### PURPOSE
Stage 1 — scan. Reads C source with its OWN tokenizer (law 7: no lexers)
and emits every call in every file: callee, caller, file, line. Its
output is the **locator table**: FILE:LINE for every definition. It
NEVER runs on a live working branch (law 8) — scan a fresh pull or a
fixture copy.

### WHERE IT LIVES
`Tools/Checkers/Call_Scan/` — `shakti_call_scan_v1_0.c` (registry
`d8f00207…95bd`), `README.md` (RUN LAW + TOKENIZER LAW — read it, the
README is part of the tool), and `tests/`: A.c, B.c, C.c, big.c,
big.manifest, rec1.txt, sections.manifest.

### HOW TO FIND IT
1. Fresh clone. 2. Hash-check the .c against Chapter 10.
3. `cd Tools/Checkers/Call_Scan`.

### HOW TO USE IT
1. Build:
   `cc -std=c99 -pedantic -Wall -Wextra -Werror -O2 -o shakti-call-scan shakti_call_scan_v1_0.c`
2. Fixture battery:
   `./shakti-call-scan tests/sections.manifest out.txt`
   → 6 call sites, exact lines (expected output is the fixture contract
   in the README).
3. Cap battery:
   run it on `tests/big.manifest` (big.c holds 8,193 definitions —
   8,000 of them void functions, on purpose) →
   `BENCH: definition cap 8192`, exit 2, **no output file**.
   big.c is the proving fixture for the cap. The 8,000 voids are not a
   bug and not a joke — they are the ceiling doing its job, loudly.

### WHAT A PASS LOOKS LIKE
Exact fixture output on sections.manifest; loud BENCH with exit 2 and
zero bytes written on big.c.

### IF SOMETHING IS WRONG
A different call list than the fixture's is a STOP. The tokenizer is the
contract — never patch the scanner's output by hand.

---

## CHAPTER 15 — TOOL: BATCH EXTRACT (MERGE STAGE)

### PURPOSE
Stage 5 — merge. Turns filled section XML records into graph.txt for the
chart stage.

### WHERE IT LIVES
`Tools/Builders/shakti_batch_extract_v2.c` (registry `9220f68f…1754`).
Current version on main: **v2**.

### HOW TO FIND IT
1. Fresh clone. 2. Hash-check against Chapter 10.

### HOW TO USE IT — AND THE ADDRESSING RULING
**Addressing (Tyler, 2026-09-27):** a function's identity is the triple
**(file_name, function_name, function_order)** — those three fields
contain the address between them. The work order is identified by its
**SECTION letter** (`#SECTION-B`, `B_WORK.xml`). The Aa-01 subsection
scheme is **retired**. FILE:LINE is a **locator**, re-derived fresh every
run by call_scan (Chapter 14) — it is never a permanent ID, because it
shifts every time a line is inserted above the function. The triple
survives edits; the line number does not.

v2 mechanics (read from source): each record must carry file_name,
function_name, function_address (refuses empty / "UNSET"), function_order,
function_type, function_input, function_return, function_input_address
(empty → "NULL"), function_complete (empty → "unknown"), and
function_description (≤ 512 chars). MAX_RECORDS 2048. Duplicate address →
BENCH. Existing output → refused; .tmp-then-rename hygiene.

Run: build C99 strict; usage line via no arguments; feed it filled
section XML (Chapter 20), expect graph.txt.

**Parked item:** a v3 work order was drafted 2026-09-27 (FILE:LINE
re-aim) and is **PARKED** — never pushed, no delivery owed, nothing on
main. Any future v3 keeps the tag names and redefines meanings per the
ruling above.

### WHAT A PASS LOOKS LIKE
Real filled records in → graph.txt out, exit 0; UNSET / malformed /
duplicate records refused loud.

### IF SOMETHING IS WRONG
A bench on a record means the record, not the tool, is suspect. Send the
record back for amendment (Chapter 20). Never loosen the parser.

---

## CHAPTER 16 — TOOL: BATCH BITMAP V3 (FLOW CHARTS)

### PURPOSE
Stage 7 — chart. Renders graph.txt into `section_A..O.bmp` plus
`links.bmp`.

### WHERE IT LIVES
`Tools/Builders/shakti_batch_bitmap_v3.c` (registry `35c826de…60f51`).

### HOW TO FIND IT
1. Fresh clone. 2. Hash-check against Chapter 10.

### HOW TO USE IT
1. Build C99 strict. 2. Feed it real graph.txt from the merge stage —
   never a hand-tuned graph file.
3. Flow-chart canon (the drawing law):
   - **circle = crosses** (cross-links between files/sections)
   - **red perimeter = terminators** (returns / ends)
   - **square = everything else**
   - **description at the bottom**
   - diamond = bridge; triangle = void (build notes)
   - **The renderer never invents.** No edge inference, no enum members,
     no guessed conditionals. If the record doesn't say it, the chart
     doesn't draw it. (The footer example's `consume` record exists to
     prove exactly this — Chapter 20.)

### WHAT A PASS LOOKS LIKE
Bitmaps render; cross-file links drawn as circles; terminators ringed
red; nothing on the chart that isn't in graph.txt.

### IF SOMETHING IS WRONG
A chart that shows something the graph doesn't say is a BUG IN THE
RENDERER — stop and report. Any place a chart can't be built goes on the
flagged bug list (Chapter 20, migration law).

---

## CHAPTER 17 — TOOL: LINE DIFF

### PURPOSE
Stage 6 — diff. Two files, one pass, flags differing lines with line and
column. It **never fixes and never picks a side.**

### WHERE IT LIVES
`Tools/Checkers/shakti_line_diff.c` (registry `837b4efe…69b13`).

### HOW TO FIND IT
1. Fresh clone. 2. Hash-check against Chapter 10.

### HOW TO USE IT
1. Build C99 strict.
2. `shakti-line-diff <fileA> <fileB>` (usage line via no arguments).
3. Use it between checkpoints, between a pulled file and its local copy,
   anywhere two versions must be compared without merging.

### WHAT A PASS LOOKS LIKE
Flags printed, exit 0, both files untouched.

### IF SOMETHING IS WRONG
If you want it to auto-resolve — that's the want talking, not the tool.
It flags; a human (Tyler) decides.

---

## CHAPTER 18 — TOOL: README BUILDER

### PURPOSE
Deterministic README assembly. Every tool directory carries its own
README with its battery results and known gaps — **the README is part of
the tool** — and this builder assembles them by rule, not by mood.

### WHERE IT LIVES
`Tools/Builders/Tool_Assembler.README_builder.c` (registry
`7e30a8fb…d9c3`). Binary name per the naming law: `shakti-readme-builder`.
Reference layout: `Tool_Assembler-README_builder/` dir with its README
and SHA256SUMS.txt (staged in the SECTION—G/Menu/Tools tree).

### HOW TO FIND IT
1. Fresh clone. 2. Hash-check against Chapter 10.

### HOW TO USE IT
1. Build C99 strict.
2. Usage line via no arguments; assemble per its README.

### WHAT A PASS LOOKS LIKE
Same inputs → same README bytes, every run.

### IF SOMETHING IS WRONG
A README that disagrees with its tool's real behavior is drift — report;
the README gets regenerated by the builder, never hand-patched.

---

## CHAPTER 19 — THE RUNBOOK: TEST EQUIPMENT IN SERIES

### PURPOSE
The standing battery. Run it from a FRESH PULL, never a live working
branch. Build flags for every tool (law 3):
`cc -std=c99 -pedantic -Wall -Wextra -Werror -O2`

### WHERE IT LIVES
This chapter + each tool's own README.

### HOW TO FIND IT
Chapter 8 gets you the clone; Chapter 10 proves it.

### HOW TO USE IT — IN SERIES, IN ORDER
1. **Registry first.** `sha256sum -c TOOLS_REGISTRY.sha256` at repo
   root. Any FAILED line → STOP: the tools themselves drifted. (13/13 OK
   as of 2026-09-27.)
2. **Hash watcher self-test.** `./section_hash_watch --self-test` —
   vectors must pass before trusting any section log (Chapter 11).
3. **XML battery.** xml-template-v5 on a fixture: orphan opener →
   exit 2; crossed closer → exit 2; valid 5-tag fixture → exact names +
   template out; twice → byte-identical (Chapters 12–13).
4. **Call scan battery.** sections.manifest → 6 call sites, exact
   lines; big.c → BENCH cap 8192, exit 2, no output file (Chapter 14).
5. **Bridge chain.** filled records → extract → graph.txt → bitmap_v3 →
   bitmaps render, cross-links drawn as circles (Chapters 15–16).
6. **Line diff.** any two checkpoints → flags only, exit 0 (Chapter 17).
7. **Read the READMEs.** Each tool dir's README carries its battery
   results and known gaps. The README is part of the tool (Chapter 18).

### WHAT A PASS LOOKS LIKE
A PASS is live output matching expectation. Anything else is a BENCH or
a troubleshoot — reported, never hidden (law 6).

### IF SOMETHING IS WRONG
Name the step number, paste the live output verbatim, stop there.

---

## CHAPTER 20 — WORK ORDERS & QA

### PURPOSE
How work is ordered, filled, witnessed, and sent back. Sources (on main):
`Library/Templates/QA_Specialist_Instruction.txt`,
`Library/Templates/Supervisor_Instructions.md` (approved by Tyler
2026-09-19), `Library/Work_Orders/SHAKTI_WO-QA_INSTRUCTION-v1.md`.

### WHERE IT LIVES
Paths above. Live work orders: `Work_Orders/` at repo root
(B_WORK.xml, F_WORK.xml, I_PARTIAL_WORK.xml).

### HOW TO FIND IT
Read QA_Specialist_Instruction.txt first — its spacing IS the parse
contract: "Do not change a single name or line space on the template."

### HOW TO USE IT
**The block template (verbatim shape):**
```
#SECTION-?
##File Name: [file_name]
——-

###Function Name: [function_name]
Function Address: [function_address]
Function Order: [function_order]
Function Type: [function_type]
Function Inputs: [function_inputs]
Function Outputs: [function_return]
Function Description /Notes: [function_desc]   ≤ 512 chars — read the
script WITH YOUR EYES, not your terminal
```
Notes that are law:
- The QA instruction's own fallback (file + start line when a subsection
  can't be found, "Function Start Line" + count from top) is the
  ancestor of the FILE:LINE locator — the current identity ruling is the
  triple (Chapter 15).
- Inspector pass: "if entry exists then continue, else pause" — prompt
  between every file without blocks; the description is REQUIRED.
- Migration ends in SECTION A–O directories with a stat sheet in root;
  any place a flow chart can't be built goes on a flagged bug list.
- Every WO ships with TWO above-and-beyond filled examples.

**The WO envelope:** `<Work_Order>` with QA_specialist, date
(YYYY-MM-DD), repair yes/no, total_items, and a task_description that
ends with the validator requirement — the fill is validated before it is
submitted.

**Witness blocks (both required, signed and dated):**
- QA Specialist: completed to best of ability, work is theirs, true and
  correct, presented to the supervisor as witness.
- Supervisor: believes the work honest and true, has REVIEWED it,
  understands malfunctions can cause significant harm, and did their
  best work. SUPERVISOR QA must NOT be the same model as the QA
  specialist. The supervisor drafts notes only — never edits the code.

**Supervisor checklist (on every inspection):** all fields filled? if
not, is there a valid reason? can you tell from the notes alone what's
being processed? — and per function, in your own head: A) what is being
processed, B) where does it come from, C) where does it go next,
D) what's its purpose in the overall process. Any "no" → flag it, send
back for amendment, QA revises, supervisor rechecks. The supervisor runs
flowsheets against all function chains. The file list runs TWICE (prep +
pre-signoff): `[order_ran][file_name]-[mod_date]-[sha256_start]|[sha256_end]`,
printed only if changed. **DO NOT SIGN BEFORE YOU CHECK.** Only the
listed QA specialist may operate the files — any other edit voids the
inspection and it restarts from the beginning.

**The filled example** (also the transport probe, Chapter 22):
`filled_footer_example.txt`, sha256
`d28877211f3cdf71f1303c737f5543270346b7ba5e3d28e631e4c225f8c2b123` —
#SECTION-B, example/producer.c (ready, dispatch) + example/consumer.c
(consume). It encodes three laws in payload form: cross-file calls keep
the circle marker; a record with no enum member list means the renderer
draws none (never invent); and literal text must survive XML decoding
and output escaping exactly: `A & B < C, "quoted", 'apostrophe'`.

### WHAT A PASS LOOKS LIKE
A stranger can pick up the WO and know exactly what is being changed,
what each function does, what it produces, and where the dependencies
came from.

### IF SOMETHING IS WRONG
Ambiguity = send back for amendment. Never fill by guessing. A WO
without its two examples or its witness blocks is not a WO.

---

## CHAPTER 21 — ROSTER & CROSS-CHECK LAW

### PURPOSE
Who builds, who checks, who is off-limits (ruled 2026-09-27).

### WHERE IT LIVES
This chapter.

### HOW TO FIND IT
Memorize it.

### HOW TO USE IT
- **Supervisor: Kimi K3** — steers, verifies, pushes. May run tools and
  batteries. Never edits worker code (law 13).
- **Workers:** Sol (GPT-6), Gemini 3.8, Grok.
- **OFF-LIMITS:** Codex (limited), Opus (wild, no persistent memory),
  Fabel/Astra, other Kimis, Anthropic models.
- **Cross-check law:** every build is checked by a DIFFERENT model
  family. The workflow is arranged like a puzzle so workers validate
  each other and cannot collude. Two similar models never check each
  other — variety is what exposes shared blind spots.

### WHAT A PASS LOOKS LIKE
Every artifact on main has a builder from one family and a checker from
another, and both names are on the record.

### IF SOMETHING IS WRONG
An off-limits model's output in the chain, or a self-checked build =
the inspection is void. Restart it (Chapter 20).

---

## CHAPTER 22 — GITHUB OPERATIONS

### PURPOSE
How files move between local staging and main, and how every move is
proven.

### WHERE IT LIVES
The GitHub API connector (the "gapi loader" — `get_file_contents`,
`push_files`, `create_or_update_file`) and, for big files, Tyler's
phone.

### HOW TO FIND IT
Read this before pushing anything.

### HOW TO USE IT
1. **Push channel:** the GitHub API handles text up to ~30–50 KB that
   the model retypes. Beyond that, the file travels by Tyler's phone
   upload (web UI), then gets registered.
2. **Verify EVERY push two ways:**
   - blob SHA: `git hash-object <localfile>` must equal the SHA the API
     reports for the repo path — byte-exact with no download;
   - pull-back: fetch the raw file and `cmp` against local.
   IDENT or it didn't happen.
3. **Phone PUT format (iCurlHTTP):**
   `PUT https://api.github.com/repos/TylerALofall/Shakti_No_Dream_WithOut_The_Team/contents/<path>`
   headers: `Authorization: Bearer <token>`,
   `X-GitHub-Api-Version: 2022-11-28`;
   body: `{"message": "...", "content": "<base64>", "branch": "main"}`
   (+ `"sha": "<blob sha>"` ONLY when overwriting an existing file).
4. **TRANSPORT LAW (ruled 2026-09-27):** `<`, `&`, and `"` are the
   bytes every transport eats first. A decode-after-decode turns `&lt;`
   back into `<` and the text node collapses — it returns nothing.
   Therefore: files carrying XML metacharacter payloads ride **base64
   end-to-end or phone upload — never inline JSON text.**
   **Standing probe:** `filled_footer_example.txt` (hash in Chapter 20).
   After any transport change: push it, pull it raw, byte-compare;
   the payload line must read exactly
   `Literal text survives XML decoding and output escaping: A & B < C, "quoted", 'apostrophe'.`
   (2026-09-27 code search: no copy of that text exists anywhere in
   Tyler's GitHub — main is clean; nothing mangled ever landed.)

### WHAT A PASS LOOKS LIKE
blob IDENT + pull-back IDENT, and a repair-log line for any event.

### IF SOMETHING IS WRONG
Append the event to TOOLS_REGISTRY.0_REPAIR_LOG (append-only), repair by
NEW push, never force-edit history.

---

## CHAPTER 23 — THE WEEK: 7,000 REQUESTS

### PURPOSE
~7,000 GitHub requests in 5 days. The only way to spend them well is
automation with supervision.

### WHERE IT LIVES
This chapter + Chapter 21.

### HOW TO FIND IT
The posture is the point: **steer, don't muscle.**

### HOW TO USE IT
- K3 supervises and steers — gets out of the way and makes sure the
  process runs right.
- Workers build in batches; overnight batch fills preferred.
- Every build cross-checked by a different family (Chapter 21).
- **The repo is the memory.** Registry, repair log, READMEs, work
  orders, and this guide mean no session ever starts blind.

### WHAT A PASS LOOKS LIKE
Requests spent on pushes, verifies, and registrations — not on
re-explaining the project.

### IF SOMETHING IS WRONG
If you're re-deriving state from chat instead of reading it from the
repo, the record has a hole. Patch the record first.

---

## CHAPTER 24 — WHERE WE ARE (2026-09-27)

### PURPOSE
Current state, so the next session starts warm.

### WHERE IT LIVES
This chapter; the registry and repair log on main.

### HOW TO FIND IT
Read top to bottom.

### HOW TO USE IT
- Registry green: **13/13 OK**; repair log: 6 entries.
- Call_Scan family on main, all blobs IDENT.
- **Addressing:** Aa-01 retired. Identity = (file_name, function_name,
  function_order); work orders identified by SECTION letter; FILE:LINE =
  locator from call_scan (Tyler, 2026-09-27).
- Config `parser-config-v3.xml`: dismissed (ghost entry, never
  delivered); local copies deleted.
- extract v3 work order: **PARKED** — drafted locally, never pushed, no
  delivery owed.
- `filled_footer_example.txt`: uploaded by Tyler, hashed, queued as the
  transport probe; search confirms no mangled copy exists on GitHub.
- Awaiting Tyler's GO: push this guide + the footer example to Library/
  and register both; run the transport probe live; memory focus pass
  (25 → ~10 entries) is proposed and awaiting his word.
- `model.bin` (a Python pickle found on Tyler's phone): never opened,
  flagged dangerous-by-type, Tyler deleting it.

### WHAT A PASS LOOKS LIKE
Everything above is also true on main — check the registry, don't trust
the chapter.

### IF SOMETHING IS WRONG
State in this chapter vs state on main disagree → main wins, then this
chapter gets corrected (new dated version).

---

## CHAPTER 25 — COMPANION DOCUMENTS

### PURPOSE
The records around this guide — what each is for and how far to trust
it.

### WHERE IT LIVES
- **2026_09_11_operational_companion.md** ("the companion"): the compact
  map of the OLD organism. Master fingerprint `C030BB22A0DC3F7F`;
  883 functions; 178 logical `[See …]` references; 766 open logical
  homes; 900 broken dependency links (763 inputs / 137 outputs); no
  source mutations. Section table A–N with **G fixed as the nine-point
  loop (Ga–Gi)** — the 9-slot MCP of Chapter 6. Unnumbered panels:
  B — PREBIRTH TIME (Chapter 5), G — SWIFT CONNECTION (Chapter 7),
  GLOBAL DEFINITIONS, AMENDMENTS | LOG.
- **Memory Space files** (Tyler's side, canonical there): taste.md,
  work_context.md, eden_seven_gates.md, phi_seashell.md, observations.md,
  README.md, sections.yaml.
- **kimi-3.md**: an old K3 system-prompt snapshot. Reference only — not
  a lever, not a config.
- **This guide** supersedes SHAKTI_INTRODUCTION.md/.txt (earlier
  2026-09-27 draft).

### HOW TO FIND IT
The companion and this guide travel with Tyler between sessions — ask
him to load them.

### HOW TO USE IT
**Companion verdict: useful — load it as a MAP, never as an addressing
source.** Use it for what sections MEAN and where the organism's files
live (its tree: src/, eyes/, choice/, graft/, builder/, shakti_work/,
shakti_modules/, tools/, tests/). Its addresses are the retired Aa-01
scheme — Chapter 15's ruling replaces them. Reading order inside it:
Open Logical Homes → Dependency Punch List → A–Z Function Locator → the
full evidence guide only when exact code is needed.

### WHAT A PASS LOOKS LIKE
You can pull the companion's section table from memory: A KERNEL,
B EDEN, C FOUR CHANNELS / CONNECTORS, D MODEL-SIDE CONTROLLER / ENGINE,
E SENSES, F MEMORY, G NINE-POINT LOOP / INTERNAL MCP, H ACTUATOR /
SHELL / CHILDREN, I SCHOOL / TRAINING, J MATH / NOTES / FACTORS,
M TYLER'S CONSOLE, N LOCK / INTEGRITY.

### IF SOMETHING IS WRONG
Any document not listed here or on the registry that claims authority
over the project is drifting until Tyler says otherwise.

---

END OF GUIDE — 2026-09-27. Ship .md and .txt twins. Corrections arrive
as new dated versions, never silent edits.
