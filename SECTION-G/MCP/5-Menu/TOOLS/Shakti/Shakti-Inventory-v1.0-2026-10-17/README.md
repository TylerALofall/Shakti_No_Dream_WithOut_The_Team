REQUIRED SCRIPTS: shakti_inventory.c
# Shakti-Inventory-v1.0-2026-10-17

FAMILY: Shakti
TOOL: Inventory
VERSION: 1.0
DATE: 2026-10-17
ENTRY_KIND: single
ENTRY: shakti_inventory.c
BUILD: cc -std=c99 -pedantic -Wall -Wextra -Werror -O2 shakti_inventory.c -o shakti_inventory
COMMAND: ./shakti_inventory "{function_list}" "{call_records}" "{inventory_output}"
EXAMPLE: ./shakti_inventory Example.List.md Example.Calls.txt Example.Inventory.xml

INTRODUCTION:
Join a function list and call records by file, function name and order, then emit function-inventory XML.

PACKAGE DETAILS: Shakti-Inventory-v1.0-2026-10-17.txt

ADD-ON FILES:
  Example.Calls.txt
  Example.List.md

FILE HASHES:
01a015c56aaac6dad83c08be2effa465b9aafb8f4c3fada3dfd92d0d48b092b8  Example.Calls.txt
4dcc398a3dc94981067b6e70068b8cb9145616a9aac3ca068f4a410fa06a053d  Example.List.md
50364e3d504777357fe4db8179b45d1ed088247634159729d5249b92d125f73c  Shakti-Inventory-v1.0-2026-10-17.txt
a8c82fdb5a9febb69b2f7b60a70a9392c9cf1819872ccbb81f89e172a3fc3607  shakti_inventory.c
