REQUIRED SCRIPTS: Tool.Assembler.c
# Tool-Legacy_Assembler-v4-2026-10-17

FAMILY: Tool
TOOL: Legacy_Assembler
VERSION: 4
DATE: 2026-10-17
ENTRY_KIND: single
ENTRY: Tool.Assembler.c
BUILD: cc -std=c99 -pedantic -Wall -Wextra -Werror -O2 Tool.Assembler.c -o shakti-assembler-v4
COMMAND: ./shakti-assembler-v4 "{tools_dir}" --write
EXAMPLE: ./shakti-assembler-v4 Example.Bank --write

INTRODUCTION:
Preserve the supplied v4 flat-bank README fixer as a separate tool; --write replaces disk-truth README fields in the explicitly selected bank.

PACKAGE DETAILS: Tool-Legacy_Assembler-v4-2026-10-17.txt

ADD-ON FILES:
  Example.Bank/Base64-Decode/Base64.Decode.c

FILE HASHES:
45b75fbfa2348a8f0120374edfa32d5267790c87d847444b3f32ec3a1fc80f45  Example.Bank/Base64-Decode/Base64.Decode.c
5eb9d0efee2e313d10e49d492f28f2bf0ca388caa76272ba6d06ce80dfe48693  Tool-Legacy_Assembler-v4-2026-10-17.txt
2c0795f50e8e0c9de3365ff59e985b6a573dd18c330f3046b3010760a8ac2bc9  Tool.Assembler.c

DIRECTORIES:
  Example.Bank/
  Example.Bank/Base64-Decode/
