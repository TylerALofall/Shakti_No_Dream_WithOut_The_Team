REQUIRED SCRIPTS: Tool_Assembler.README_builder.c
# Tool-Legacy_README_builder-v1_local1-2026-10-17

FAMILY: Tool
TOOL: Legacy_README_builder
VERSION: 1_local1
DATE: 2026-10-17
ENTRY_KIND: single
ENTRY: Tool_Assembler.README_builder.c
BUILD: cc -std=c99 -pedantic -Wall -Wextra -Werror -O2 Tool_Assembler.README_builder.c -o shakti-readme-builder && cc -std=c99 -pedantic -Wall -Wextra -Werror -O2 Example.Bank/Base64-Decode/Base64.Decode.c -o Example.Bank/Base64-Decode/shakti-decode
COMMAND: ./shakti-readme-builder "{tools_dir}"
EXAMPLE: ./shakti-readme-builder Example.Bank

INTRODUCTION:
Preserve the original four-section README builder. It requires admitted family/source names and already built binaries; the example build prepares its Base64 fixture.

PACKAGE DETAILS: Tool-Legacy_README_builder-v1_local1-2026-10-17.txt

ADD-ON FILES:
  Example.Bank/Base64-Decode/Base64.Decode.c

FILE HASHES:
45b75fbfa2348a8f0120374edfa32d5267790c87d847444b3f32ec3a1fc80f45  Example.Bank/Base64-Decode/Base64.Decode.c
d9f3b9895f91b852574a04e27ea0ed60a8a8f7db7f08e5efbd97a7ec8112f965  Tool-Legacy_README_builder-v1_local1-2026-10-17.txt
7e30a8fb1098a726528ddc295264d9867b88947c9a02327d08707ffe629cd9c3  Tool_Assembler.README_builder.c

DIRECTORIES:
  Example.Bank/
  Example.Bank/Base64-Decode/
