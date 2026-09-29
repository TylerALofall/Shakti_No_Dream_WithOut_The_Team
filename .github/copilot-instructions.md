# READ THIS FIRST — pointer, not rules

There is exactly ONE live instruction document for QA work orders:

**`Copilot_Intake/QA_specialist_Instruction.md`** (v1.1, 2026-09-29)

Everything you need to fill a work order — every field, every format
rule, every example, the attestation — is in that document. Read it in
whole before touching anything. This file adds no rules and overrides
none; it only points.

Landmarks:

- Work orders live in `Work_Orders/` and `Copilot_Intake/`
  (`*_WORK.xml`, `*_PARTIAL_WORK.xml`).
- Filled examples to study: `Copilot_Intake/C_WORK_FILLED_Example.xml`,
  `Copilot_Intake/B_WORK_FILLED_Example.xml`.
- `2026-09-22 No Drift.zip` at root is a FROZEN SNAPSHOT — never read
  instructions or templates from it.
- The old `QA_Specialist_Instruction.txt` was deleted 2026-09-29 and is
  superseded by the .md above.
- `function_remote_call` is required LAW since 2026-09-29 — the
  instruction document's top banner covers it.
- You never run the parser or any validator. You read source with your
  eyes, you write, you return. Validation is a C tool run by the owner.
- Sign `<QA_Specialist_attestation>` on every returned work order
  (exact oath text: Part VII of the instruction document). Never touch
  `<Supervisor_attestation>`.

If anything here ever disagrees with the instruction document, the
instruction document wins — and report the disagreement.
