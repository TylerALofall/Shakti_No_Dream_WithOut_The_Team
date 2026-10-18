REQUIRED SCRIPTS: shakti_call_scan_v1_0.c
# Call-Scan-v1.0-2026-10-17

FAMILY: Call
TOOL: Scan
VERSION: 1.0
DATE: 2026-10-17
ENTRY_KIND: single
ENTRY: shakti_call_scan_v1_0.c
BUILD: cc -std=c99 -pedantic -Wall -Wextra -Werror -O2 shakti_call_scan_v1_0.c -o shakti_call_scan
COMMAND: ./shakti_call_scan "{section_manifest}" "{new_calls_output}"
EXAMPLE: ./shakti_call_scan tests/sections.manifest Example.Calls.txt

INTRODUCTION:
Scan C call sites from a section manifest in a disposable copy. The supplied publisher requires Linux renameat2; it is not an iOS port.

PACKAGE DETAILS: Call-Scan-v1.0-2026-10-17.txt

ADD-ON FILES:
  tests/A.c
  tests/B.c
  tests/C.c
  tests/big.c
  tests/big.manifest
  tests/rec1.txt
  tests/sections.manifest

FILE HASHES:
956fcc0e0c2785e94aa6887282ebee7de2ec0702236d5e91990ed017999df4fb  Call-Scan-v1.0-2026-10-17.txt
d8f002075ef72db88ff6d5eae8536828da38180eb7e1a5761ea8917722d895bd  shakti_call_scan_v1_0.c
1a628cfe8f571d1337922a50a06cf4368c7aeeef23d4170b6fdf9c297eefc1b5  tests/A.c
2abd37c35ea1aeb47a27b72bf712d787acacb92a0575f2fa98c1a926fcd089ea  tests/B.c
e8847ac40dc3a3d999530726295cad07ba6f09c3f4a2ee78380b35d784d735d9  tests/C.c
f3bc9a2900dbd9f4e4fa5b225bfbc5d8f48923deb5c83b3ac4538f376aed6990  tests/big.c
69f7f125b437c54f4c59ae876b8156cf2dcfd8d2987ef965bdb66503ec1a1006  tests/big.manifest
01a015c56aaac6dad83c08be2effa465b9aafb8f4c3fada3dfd92d0d48b092b8  tests/rec1.txt
b1923a12fff575ce96829db7dbd012cafda5802f394218272870d2bcda28e1b8  tests/sections.manifest

DIRECTORIES:
  tests/
