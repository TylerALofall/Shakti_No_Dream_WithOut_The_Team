REQUIRED SCRIPTS: shakti_line_diff.c
# Line-Diff-v2-2026-10-17

FAMILY: Line
TOOL: Diff
VERSION: 2
DATE: 2026-10-17
ENTRY_KIND: single
ENTRY: shakti_line_diff.c
BUILD: cc -std=c99 -pedantic -Wall -Wextra -Werror -O2 shakti_line_diff.c -o shakti-line-diff
COMMAND: ./shakti-line-diff "{old_file}" "{new_file}" "{append_log}"
EXAMPLE: ./shakti-line-diff Example.Old.txt Example.New.txt Example.Diff.txt

INTRODUCTION:
Flag differing lines and columns in two local files and optionally append a log; ordinary differences return success.

PACKAGE DETAILS: Line-Diff-v2-2026-10-17.txt

ADD-ON FILES:
  Example.New.txt
  Example.Old.txt

FILE HASHES:
710605b946281e2b995d6e4f6bb9d88b9f72ba5139593bf5867eef5984deb5a1  Example.New.txt
c3f9c8c283a2b1f2f1896f27a01cbe3cddc0c9d93f752e4639035a0f5b36f6e8  Example.Old.txt
ecaee5e35f1c8da17db1103d089006941a9f9e4f6c83f7d8937671915e1f9ae3  Line-Diff-v2-2026-10-17.txt
837b4efe6bf1b95cf31f14a2532172d446f6f199fc0881e47e87ffaeabc69b13  shakti_line_diff.c
