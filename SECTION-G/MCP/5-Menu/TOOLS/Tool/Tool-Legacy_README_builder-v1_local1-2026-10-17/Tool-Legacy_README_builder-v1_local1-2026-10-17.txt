FAMILY: Tool
TOOL: Legacy_README_builder
VERSION: 1_local1
DATE: 2026-10-17
INTRODUCTION: Preserve the original four-section README builder. It requires admitted family/source names and already built binaries; the example build prepares its Base64 fixture.
BUILD: cc -std=c99 -pedantic -Wall -Wextra -Werror -O2 Tool_Assembler.README_builder.c -o shakti-readme-builder && cc -std=c99 -pedantic -Wall -Wextra -Werror -O2 Example.Bank/Base64-Decode/Base64.Decode.c -o Example.Bank/Base64-Decode/shakti-decode
COMMAND: ./shakti-readme-builder "{tools_dir}"
EXAMPLE: ./shakti-readme-builder Example.Bank
ENTRY_KIND: single
ENTRY: Tool_Assembler.README_builder.c
REQUIRED: Tool_Assembler.README_builder.c
