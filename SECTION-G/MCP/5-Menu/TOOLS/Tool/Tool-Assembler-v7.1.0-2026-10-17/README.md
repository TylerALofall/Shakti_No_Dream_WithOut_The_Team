REQUIRED SCRIPTS: Tool.Assembler.c | Tool.README_builder.c | Tool.Assembler.h | Tool.Hash.h | Tool.Assembler.v7.1.0-2026-10-17.manifest.txt
# Tool-Assembler-v7.1.0-2026-10-17

FAMILY: Tool
TOOL: Assembler
VERSION: 7.1.0
DATE: 2026-10-17
ENTRY_KIND: manifest
ENTRY: Tool.Assembler.v7.1.0-2026-10-17.manifest.txt
BUILD: cc -std=c99 -pedantic -Wall -Wextra -Werror -O2 Tool.Assembler.c Tool.README_builder.c -o shakti-assembler
COMMAND: ./shakti-assembler "{bank_dir}" "{tool_dir}" Tool.Assembler.v7.1.0-2026-10-17.manifest.txt
EXAMPLE: ./shakti-assembler ../bank . Tool.Assembler.v7.1.0-2026-10-17.manifest.txt

INTRODUCTION:
Package an explicitly supplied tool with a uniform README, content seal and append-only ledger entry; never compile or run the submitted tool.

PACKAGE DETAILS: Tool-Assembler-v7.1.0-2026-10-17.txt

ADD-ON FILES:
  Tool.Assembler.Guide.txt

FILE HASHES:
645710e8d599ee74dc0b0160ee64ff2544d40ce2dcb61b7ac5cbfe71178325e4  Tool-Assembler-v7.1.0-2026-10-17.txt
5e89638f6117fb6f807e9750d945cee612df9431bb035610f8739da6170dcaca  Tool.Assembler.Guide.txt
1abd363eeee35cae13adeeceaba801dd87e83b9eaaf75c319ec0dbd7b4e0cd41  Tool.Assembler.c
94f94947276f399a58a5c80e801d45b2092acf0ef439301ab03f742253328c91  Tool.Assembler.h
645710e8d599ee74dc0b0160ee64ff2544d40ce2dcb61b7ac5cbfe71178325e4  Tool.Assembler.v7.1.0-2026-10-17.manifest.txt
85f812fe2bbe6c2d619e73cd116ae9fc5eb8ca5055d7e1d0be12ba2886d53dbd  Tool.Hash.h
1a83ce18384ffdf0dfe2f70ac98f4fbb5b2b6d78e26ebc92b69a3b398abc4f51  Tool.README_builder.c
