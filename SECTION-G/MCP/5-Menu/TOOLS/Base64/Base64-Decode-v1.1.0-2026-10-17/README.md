REQUIRED SCRIPTS: Base64.Decode.c
# Base64-Decode-v1.1.0-2026-10-17

FAMILY: Base64
TOOL: Decode
VERSION: 1.1.0
DATE: 2026-10-17
ENTRY_KIND: single
ENTRY: Base64.Decode.c
BUILD: cc -std=c99 -pedantic -Wall -Wextra -Werror -O2 Base64.Decode.c -o shakti-decode
COMMAND: ./shakti-decode -- "{input_file}" "{output_file}"
EXAMPLE: ./shakti-decode -- Base64.Sample.Input.txt Base64.Sample.Output.bin

INTRODUCTION:
Decode Base64 text into binary bytes with the supplied single C99 source file.

PACKAGE DETAILS: Base64-Decode-v1.1.0-2026-10-17.txt

ADD-ON FILES:
  Base64.Decode.Guide.txt
  Base64.Sample.Expected.bin
  Base64.Sample.Input.txt

FILE HASHES:
cd211cd0a38a9ae299ef15699eb23b077d5da8fb2aa2ad80683b195d2eff4104  Base64-Decode-v1.1.0-2026-10-17.txt
6e86ccecbaabf33c784510c9dac9841e53978f5ad1fca9eaa11c770dbb66d1e7  Base64.Decode.Guide.txt
45b75fbfa2348a8f0120374edfa32d5267790c87d847444b3f32ec3a1fc80f45  Base64.Decode.c
55b89add11244efa136a554ac8b5ebbdbdeec4d2f78e0ee603dde861c3caf434  Base64.Sample.Expected.bin
3c08561ef550b1c4936e03f6dedf3ff72baf5419b50ce80e893071c3a54fe292  Base64.Sample.Input.txt
