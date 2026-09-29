MUST READ WITH EYES BEFORE STARTING!!!

README.md
THIS DOCUMENT : QA_specialist_Instruction.md
AN EXAMPLE WORK ORDER IN THIS DIR
Each file in whole

> **NEW 2026-09-29 — READ THIS FIRST:** `<function_remote_call>` is now a
> required field in every record — LAW, not optional. If you read any older
> copy of this document, stop and re-read Part II (definitions table) and
> Part V (examples) before starting. Canonical form, one direct call per
> line, placed immediately BEFORE `<function_description>`:
>
> ```
> <function_remote_call>
> shakti_loader.c: shakti_loader_load()
> shakti_log.c: shakti_log_append()
> libc: printf()
> </function_remote_call>
> ```
>
> No outside calls → the literal word `NONE`. Never `NULL`, `null`, `None`,
> `N/A`, and never an empty tag. `NONE` is the only empty-marker.

---

# SHAKTI WORK ORDER — QA SPECIALIST INSTRUCTION DOCUMENT

**Document version:** 1.1
**Issued:** 2026-09-22
**Updated:** 2026-09-29 — `<function_remote_call>` field added as LAW (see banner above; NULL spelling rejected, NONE is the only empty-marker)
**Authority:** TYLER ALLEN LOFALL
**Scope:** Sections A–O, Shakti deterministic core migration

---

## I. INTRODUCTION

Shakti is a deterministic C99 core being migrated into a permanent sectioned repository (SECTION A through SECTION O). Every function in that core must be documented to a standard where a stranger holding one page knows exactly what is processed, where it comes from, where it goes, and why it exists — without opening a terminal, without guessing, and without a single fact that the source does not prove.

You are the QA Specialist. Your product is not code — it is **certainty**. The descriptions you write become the parsed, diffed, inspected record of the migration. A vague description is not a weak description; it is a false one, because the next person who trusts it will build on air. This document tells you precisely what to produce and how you will be judged.

---

## II. DEFINITIONS — EVERY ELEMENT

| Element | Definition |
|---|---|
| `<Work_Order>` | The root container. One per section per run. Nothing outside it exists. |
| `<QA_specialist>` | Your typed model name. Not a nickname — the actual model identity executing this run. |
| `<date>` | The completion date of this run, `YYYY-MM-DD`. Never the issue date if the run spans days. |
| `<repair>` | `yes` if this run corrects fields in an existing work order; `no` if it is a first pass. |
| `<total_items>` | Count of live `<SECTION>` records in this document — count them yourself, never trust the header. |
| `<task_description>` | The standing task text. Fixed. Do not alter. |
| `<SECTION>` | One record = one function. `id` is the section letter, A–O. File owns function; the section is the file's home. |
| `<file_name>` | The source file this function lives in. The path given is its original location; sources are provided alongside this document — match by basename. |
| `<function_name>` | The exact C identifier of the function, verbatim from source. |
| `<function_order>` | The function's position in its file, counting from the top, 1-based. Count functions, not lines. |
| `<function_type>` | The full declared return type including qualifiers: `static uint64_t`, `const char *`, `int`, `void`. Everything before the function name on its declaration line. |
| `<function_input>` | The complete parameter list verbatim as declared, between the outermost `(` and `)`. `(void)` stays `(void)` — never "simplified." |
| `<function_return>` | What the function actually returns. Watch for missing explicit returns and mixed paths. |
| `<function_remote_call>` | Every call this function makes to a function **not defined in its own file**, read straight from the body. LAW (2026-09-29): **direct calls only — never chains.** You list this function's own call expressions; the callee's record lists the callee's calls; the map closes from everyone's lists and you never trace beyond one hop. **One call per line**, exact form `file.c: function_name()` — bare filename, no path, no spaces around the colon, parentheses on the name. Calls into a library list the library as owner: `libc: printf()`. A call used as an argument still counts: `f(g(x))` lists both `g` and `f`, in the order they appear. No outside calls at all → the single line `NONE`. `NONE` is the only empty-marker — never `NULL`, `null`, `None`, `N/A`, never an empty tag. Placement: its own line, **immediately before** `<function_description>`. |
| `<function_description>` | Target function, in respect applicable code it influences in plain English. Max 512 Unicode characters after XML decoding. Describe what this function does, how it connects to the functions before and after it in the sequence, and its role in the overall section flow. Use plain English. Write coherently so each function's description builds on the previous and leads into the next without repeating details. Every sentence must be verifiable against the source, and be factually true. See III. TASK — DIRECT AND ASSERTIVE |
| `<corrections>` | Your list of every fixed field you changed from the issued record: record, field, was, now, one-line source evidence. Empty if none. |
| Destination/Target sources | The `.c` files this work order describes. **Read-only.** You will never modify them. |

---

## III. TASK — DIRECT AND ASSERTIVE

Do exactly this, in this order, and nothing else.

1. **Read every destination source file in full.** The whole file, top to bottom, before you write a single character. Not a summary. Not an extraction. The file.
2. **Verify all seven fixed fields of every record against the source.** Where the issued record is wrong, correct the field and log it in `<corrections>`.
3. **Record `function_remote_call` from the body you just read.** Every call to a function not defined in this file, one per line, in the order the calls appear, direct calls only. Library calls as `libc: name()`. Nothing outside the file → `NONE`. This is a fact list, not prose.
4. **Write every `function_description`.** Max 512 characters after XML decoding. Each description answers four questions, from evidence in the source:
   - **(A) What is processed** — the data and the transform, concretely.
   - **(B) Where every input originates** — caller, file-local state, constant, `argv`.
   - **(C) Where the output goes next** — caller, stdout, the next function in the chain. If no consumer exists in the file, write "downstream consumer unresolved." Never invent one.
   - **(D) Why the function exists** in the overall pipeline.

   Name real identifiers. One fact per sentence. No filler, no hedging, no "this function handles."
5. **Set `function_complete`.** `yes` only when you read the actual source and A–D are all answered from evidence. Anything short of that: `no`, with the reason stated inside the description.
6. **Return the records** — same structure, same order, same spacing — then your `<corrections>` list. No prose around the output.
7. **Do not run tools. Do not execute code. Do not validate.** You read, you write, you return. Validation is a C tool run by the owner after you.

If a source file is missing or unreadable: `function_complete=no`, description contains `SOURCE UNAVAILABLE — pending byte-exact re-verification`. You do not guess. Not once.

---

## IV. EMPTY TEMPLATE

```xml
<Work_Order>
<QA_specialist>[model_name]</QA_specialist>
<date>[YYYY-MM-DD]</date>
<repair>[yes|no]</repair>
<total_items>[#]</total_items>
<task_description>[standing task text — fixed]</task_description>

<SECTION id="[A-O]">
    <file_name>[path]</file_name>
    <function_name>[identifier]</function_name>
    <function_order>[int]</function_order>
    <function_type>[declared return type]</function_type>
    <function_input>[verbatim parameter list]</function_input>
    <function_return>[return type]</function_return>
    <function_remote_call>[file.c: function_name() — one direct call per line, or the literal NONE]</function_remote_call>
    <function_description>[see `function_description` above]</function_description>
    <function_complete>no</function_complete>
</SECTION>

<corrections>[record — field — was — now — evidence, or NONE]</corrections>

<QA_Specialist_attestation>[see Part VII]</QA_Specialist_attestation>
<Supervisor_attestation>[left for supervisor — never you]</Supervisor_attestation>
</Work_Order>
```

---

## V. EXAMPLES

### V.1 — Raw, unfilled

```xml
<SECTION id="EXAMPLE">
    <file_name>verification/demo.c</file_name>
    <function_name>clamp_nonnegative</function_name>
    <function_order>1</function_order>
    <function_type>static int</function_type>
    <function_input>int value</function_input>
    <function_return>int</function_return>
    <function_remote_call>[file.c: function_name() — one direct call per line, or NONE]</function_remote_call>
    <function_description></function_description>
    <function_complete>no</function_complete>
</SECTION>

<SECTION id="EXAMPLE">
    <file_name>verification/demo.c</file_name>
    <function_name>remember_input</function_name>
    <function_order>2</function_order>
    <function_type>static void</function_type>
    <function_input>int value</function_input>
    <function_return>void</function_return>
    <function_remote_call>[file.c: function_name() — one direct call per line, or NONE]</function_remote_call>
    <function_description></function_description>
    <function_complete>no</function_complete>
</SECTION>
```

### V.2 — Filled, to standard

```xml
<SECTION id="EXAMPLE">
    <file_name>verification/demo.c</file_name>
    <function_name>clamp_nonnegative</function_name>
    <function_order>1</function_order>
    <function_type>static int</function_type>
    <function_input>int value</function_input>
    <function_return>int</function_return>
    <function_remote_call>
Tyler.c: beast_mode()
Tyler_attitude.c: confidence()
libc: printf()
    </function_remote_call>
    <function_description>Processes the integer supplied by its caller; returns zero for a negative value and otherwise returns that value unchanged. remember_input passes its own input here and stores the returned integer in file-local last_input. This helper establishes a nonnegative stored value without changing global state itself.</function_description>
    <function_complete>yes</function_complete>
</SECTION>

<SECTION id="EXAMPLE">
    <file_name>verification/demo.c</file_name>
    <function_name>remember_input</function_name>
    <function_order>2</function_order>
    <function_type>static void</function_type>
    <function_input>int value</function_input>
    <function_return>void</function_return>
    <function_remote_call>NONE</function_remote_call>
    <function_description>Receives an integer from its caller, passes it to clamp_nonnegative, then writes that helper's result to file-local last_input; it returns no value. Its string and character literals exercise quoted-brace parsing and have no output effect. The file contains no caller or reader of last_input, so the downstream consumer is unresolved; this function demonstrates storing a bounded input.</function_description>
    <function_complete>yes</function_complete>
</SECTION>
```

Study the difference. The filled copies state provenance, name identifiers, admit the unresolved consumer instead of hiding it, and every sentence would still be true if you re-read the source. The `<function_remote_call>` element is always present, always before `<function_description>`. Each actual call gets its own line before the closing tag; when there are no outside calls the content is the single word `NONE` — never `NULL`, never empty.

---

## VI. STANDARDS — RULES AND EXPECTATIONS

1. **Python is banned.** No python, no subprocess, nothing. If a needed tool does not exist in C, stop and report the need — do not improvise.
2. **Eyes, not terminal.** You read source with your eyes. You do not grep functions out of context, you do not script extractions, you do not have a terminal draft your descriptions. Reading is done by you, sentence by sentence.
3. **Read-only sources.** Destination `.c` files are never modified.
4. **512 characters max** per description, measured after XML decoding. Escape `&` `<` `>`.
5. **No invented behavior.** If the source doesn't show it, it doesn't exist. "Downstream consumer unresolved" is a complete answer when the file shows no consumer.
6. **No structural edits.** Never rename, reorder, add, or remove parser-visible tags. Spacing is load-bearing.
7. **Corrections are reported, never silent.** Every fixed-field change goes in `<corrections>` with evidence.
8. **Uncertainty stays visible.** `function_complete=no` plus a stated reason. A stated gap is a pass; a hidden gap is a voided inspection.
9. **Temperature zero, fixed instruction block, one section per call.** Determinism starts with you.

---

## VII. CONCLUSION AND ATTESTATION

This work order is the migration's memory. Every record you sign will be parsed, diffed against rendered lists, and inspected by a supervisor who was not in this conversation. The standard is that they can trust your page without trusting you.

Sign only when every field above is satisfied. Then replace this block with exactly:

```xml
<QA_Specialist_attestation>
I [model_name], on the date below, attest: I read every destination source file in this work order with my own eyes, in full; every description was written by me from that reading; every remote_call list was recorded by me from that reading, direct calls only; I used no Python and no terminal drafting or extraction to produce any description; every fixed-field correction is listed in corrections with evidence; and everything stated above is true and correct to the best of my knowledge.

[model_name] [YYYY-MM-DD]
</QA_Specialist_attestation>
```

The `<Supervisor_attestation>` block is not yours. Leave it for the independent supervisor.

---
