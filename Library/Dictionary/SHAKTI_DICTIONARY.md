# SHAKTI DICTIONARY

**Version:** 1.1
**Issued:** 2026-09-27
**Authority:** TYLER ALLEN LOFALL
**Status:** LAW once approved. Entries change only by Tyler's word, recorded — never silent edits.
**Amendments:** 1.1 (2026-09-27) — added per Tyler: actuator, vision, eyes, memory tiers, hearing, binary, input, output, return, hash, mark, synthetic, mock up, mirror, shell, phase, clone.

Two sides. Side One is the builders' contract — the words we use on the
record, defined so a stranger applies them and gets the same answer.
Side Two is the general dictionary for her — lands later, its own work order.

---

## SIDE ONE — THE BUILDERS' WORDS

### Proof words

| Word | Definition | How you check |
|---|---|---|
| **registered** | The file has a line in TOOLS_REGISTRY.sha256 whose hash matches its bytes on main right now. | `sha256sum -c` prints OK on its line. |
| **proven** | The claim carries live output that anyone can re-run and get the same bytes. | Re-run it. Same bytes = proven. |
| **test** | Running the real thing on real input and reading the real output. | If nothing ran, there was no test. |
| **pass** | The live output matches the expected contract exactly — byte for byte where bytes apply. | Compare against the expected output. |
| **fail** | The live output differs, or the run refuses or errors. | Reported verbatim. A fail is never edited into a pass. |
| **complete** | Every required field filled from evidence, and the whole verified. For a WO record: A–D answered from source. | Verification exists or it isn't complete. |
| **verified** | Compared byte-exact: blob hash or `cmp` says IDENT. | Hash both sides; they match. |
| **IDENT** | Two byte sequences are the same bytes — same length, same hash. | `cmp` silent, hashes equal. |

### State words

| Word | Definition | How you check |
|---|---|---|
| **frozen** | Never edited again. Change arrives as a new version or a new line. | The old bytes still exist unchanged. |
| **append-only** | Lines are never edited, reordered, or removed; a wrong line is corrected by a new line saying so. | History only grows. |
| **current** | The highest version of a thing by rule. Older versions stay frozen beside it. | v5 over v4; the rule says which. |
| **canon / canonical** | The single source a disagreement is settled by. For repo state, main is canon. | Main wins; then the claim is corrected. |
| **drift** | Bytes changed where bytes were not supposed to change — or a claim disagrees with main. | Hash compare against the record. |
| **delivered** | On main, byte-verified, and registered if it is a tool. | All three checks pass. |
| **done** | Not a project word. A feeling. | Use delivered, complete, or parked. |

### Process words

| Word | Definition | How you check |
|---|---|---|
| **BENCH** | Loud refusal: exit 2, a stated reason, zero partial output. | Exit code 2 and no output file. |
| **gauntlet** | The strict build plus battery: C99 strict flags, -O0==-O2, fixtures, live runs. | The README lists it; run it. |
| **battery** | The fixed set of live runs a tool must pass, written in its README. | Every listed run executed live. |
| **witness** | The signed attestation block. The supervisor is never the builder. | Two names, two families. |
| **supervisor** | Steers and verifies; never edits worker code. Currently K3. | Check the roster. |
| **worker** | Builds to the work order; checked by a different model family. Sol, Gemini, Grok. | Check the roster. |
| **correction** | Tyler decides, the decision is recorded, then action. | The decision exists on the record. |
| **flag** | A problem placed on the record — not fixed silently. | It's on the list. |
| **parked** | Decidedly waiting on Tyler's word. Not lost, not active. | It's named as parked. |

### Structure words

| Word | Definition | How you check |
|---|---|---|
| **tool** | A C99 program in Tools/ that builds under the gauntlet and carries its README. Not a script, not a prompt. | It's registered and its battery passes. |
| **slot** | One of the nine gates of the MCP (Epoch, Heartbeat, Goal, Notebook, Menu, Shell, Outbound_Mail, Inbound_Mail, Self_Reflection) — her only contact with outside. | SECTION-G/MCP/ lists nine. |
| **gate** | The check that decides whether a call proceeds: registered? cleared? | The call passes or is refused. |
| **container** | The execution walls around a tool call: fixed whitelist, fixed paths, the parent waits outside. | No path in or out except the chalkboard. |
| **chalkboard** | The fixed drop folder inside the container — input path and result path agreed before launch. | Paths exist before the run. |
| **library** | The repo Library/ — her system32: the rulebook of record (this dictionary, instructions, templates, the guide). Tools never move; tools live in Tools/. | Library/ holds records, Tools/ holds tools. |
| **menu** | Slot 5 — the generated, tiered list of calls she may make. Generated from the registry by a tool, never by hand. | Menu matches registry or drift is flagged. |
| **shell (slot 6)** | The container that executes tool calls. NOT the iOS app, NOT a terminal window. | See flagged ambiguities. |
| **work order (WO)** | The unit of work, identified by its SECTION letter. | B_WORK.xml = section B's order. |
| **function identity** | The triple (file_name, function_name, function_order). | All three fields present. |
| **locator** | FILE:LINE — re-derived fresh every run, never an identity. | It shifts when lines move; the triple doesn't. |
| **epoch / beat** | The clock of record. One beat = one second at the 60 bpm rest rate. | Beat counts derive from 22,982,400. |

### Her body's words

| Word | Definition | How you check |
|---|---|---|
| **actuator** | Section H — the part of her that acts; the doer at the end of the circuit. | It moves something outside her. |
| **eyes** | The sight organs themselves — the parts. | Eyes exist; what they do is vision. |
| **vision** | The act of seeing — and the verify step between circuit hops. | Vision happens; eyes are the parts. |
| **hearing** | The audio channel; she is trained by voice (online ~19 weeks, beat 11,491,200). Base rate 64 fps. | The rate ladder: 60 bpm, 32, 64. |
| **binary** | The raw 0s-and-1s stream — her native input. Not "two" of anything; no bicycle. | If it isn't 0s and 1s, it isn't binary. |
| **long-term memory** | The tier that persists; stored by epoch; survives restarts. | It's still there next run. |
| **short-term memory** | The current-run tier; fades by design when the run ends. | It's gone next run. |
| **working memory** | The live tier in use right now, between short and long. | It's what's in hand. |
| **clone** | A working instance of her. The clones beat her heart and share one memory space — the convergence point. Different weights carry different personalities. | Not a backup copy — a heartbeat. |
| **phase** | The alignment of her personality positions through shared memory; also the 64-card phase deck that trains it (F7 shadow cards on k%13==0). | Context says which. |

### Data words

| Word | Definition | How you check |
|---|---|---|
| **input** | What a function is handed — the declared parameter list, verbatim. | `(void)` stays `(void)`. |
| **output** | Everything that leaves a function: the return value plus anything written (stdout, files). | Name every exit path. |
| **return** | The value handed back to the caller through the return statement. | Read the return line. |
| **hash** | The fixed-length fingerprint of bytes. Same bytes, same hash, always. SHA-256 for files; FNV-1a 64 for her pins. | Re-hash and compare. |
| **mark** | A value recorded at a known point so later runs can compare; the pins of record are marks. | The mark exists before the test. |
| **synthetic** | Made-up data invented on the spot. Never evidence. | A fixture is different: a chosen real file that IS the contract. |
| **mock up** | A pretend stand-in for a component. Labeled, never shipped as the real thing, never evidence. | If it pretends, it's a mock up. |
| **mirror** | A generated copy that must match its source exactly; it reflects, it never originates. | Diff against source is empty. |

---

## FLAGGED AMBIGUITIES — settled 2026-09-27

1. **registered** — lived only in the supervisor's head until today. Now law: a registry line that hashes OK.
2. **tested** — has been used to mean "I read it carefully." That use is banned. Test = live run.
3. **shell** — three meanings collided. Split: **shakti-shell** = the iOS apartment (separate repo); **Shell (slot 6)** = the tool-call container; a **terminal window** = a display, not a component.
4. **library** — meant the repo Library/ and a runtime tool store in the same sentence. Law: Library/ = system32 rulebook; tools stay in Tools/; the menu mirrors the registry.
5. **complete** — a record field (function_complete) vs a task state. The field is evidence-backed; the task state is delivered.
6. **done** — banned as a status. Use delivered, complete, or parked.
7. **proven vs verified** — a file is verified (bytes match); a claim is proven (the live run supports it).
8. **eyes vs vision** — eyes are the organs; vision is the act, and the circuit's verify step. Never swapped.
9. **binary** — not "two" of anything. The 0/1 stream.
10. **synthetic / mock up vs fixture** — invented stand-ins never prove anything; a fixture is a chosen real file that is the contract.
11. **output vs return** — return is the value handed to the caller; output is everything that leaves.

---

## EXAMPLE DIALOGUE

> **Worker:** "The scanner is done — I read the output code twice."
> **Supervisor:** "Done isn't a state. Did anything run?"
> **Worker:** "Not yet."
> **Supervisor:** "Then it's untested. A test is live output — run the battery."
> **Worker:** "Battery matches the README on all fixtures. Twice, same bytes."
> **Supervisor:** "Now it has passed. It becomes registered when its hash line checks OK on main. Then it's delivered."

---

## SIDE TWO — THE GENERAL DICTIONARY (reserved)

The ~80,000-word general English dictionary — her vocabulary reference.
Not built today. When it lands: one entry per word, alphabetical,
deterministic order, no half-entries. Source and build get their own
work order.
