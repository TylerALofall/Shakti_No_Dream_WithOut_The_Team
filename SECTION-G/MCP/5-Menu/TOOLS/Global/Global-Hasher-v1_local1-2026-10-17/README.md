REQUIRED SCRIPTS: Global.Hasher.c
# Global-Hasher-v1_local1-2026-10-17

FAMILY: Global
TOOL: Hasher
VERSION: 1_local1
DATE: 2026-10-17
ENTRY_KIND: single
ENTRY: Global.Hasher.c
BUILD: cc -std=c99 -pedantic -Wall -Wextra -Werror -O2 Global.Hasher.c -o shakti-hasher
COMMAND: ./shakti-hasher "{root_dir}" "{registry_file}"
EXAMPLE: ./shakti-hasher Example.Tree Example.Registry.txt

INTRODUCTION:
Hash and log a local tree and compare registered file hashes. This source writes hash logs; its older seal-edition README describes a different interface.

PACKAGE DETAILS: Global-Hasher-v1_local1-2026-10-17.txt

ADD-ON FILES:
  Example.Registry.txt
  Example.Tree/a.txt
  Example.Tree/nested/b.txt

FILE HASHES:
ce501ea28d0d559470c242ca68e79fe107ded92854d190e0aba1c8b186ed524c  Example.Registry.txt
3c08561ef550b1c4936e03f6dedf3ff72baf5419b50ce80e893071c3a54fe292  Example.Tree/a.txt
2abd37c35ea1aeb47a27b72bf712d787acacb92a0575f2fa98c1a926fcd089ea  Example.Tree/nested/b.txt
5c8bc067a724a13cd5837461728c02a9cb85e0b3b0725e55e33e88c41df477ac  Global-Hasher-v1_local1-2026-10-17.txt
423d130a2ddbf5e23f666d1fe9739c6ab42a81ba1a08533ba25bbc4a248f3b7d  Global.Hasher.c

DIRECTORIES:
  Example.Tree/
  Example.Tree/nested/
