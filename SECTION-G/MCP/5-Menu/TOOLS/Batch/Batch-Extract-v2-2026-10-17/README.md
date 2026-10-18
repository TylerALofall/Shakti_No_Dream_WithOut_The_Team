REQUIRED SCRIPTS: shakti_batch_extract_v2.c
# Batch-Extract-v2-2026-10-17

FAMILY: Batch
TOOL: Extract
VERSION: 2
DATE: 2026-10-17
ENTRY_KIND: single
ENTRY: shakti_batch_extract_v2.c
BUILD: cc -std=c99 -pedantic -Wall -Wextra -Werror -O2 shakti_batch_extract_v2.c -o shakti_batch_extract_v2
COMMAND: ./shakti_batch_extract_v2 "{xml_or_manifest}" "{graph_output}"
EXAMPLE: ./shakti_batch_extract_v2 Example.Work.xml Example.Graph.txt

INTRODUCTION:
Read work-order records with explicit function and upstream addresses and write exact graph records.

PACKAGE DETAILS: Batch-Extract-v2-2026-10-17.txt

ADD-ON FILES:
  Example.Work.xml

FILE HASHES:
0ad4dc777256218e4413f1191dcafb77be3c6fa24040fc019f560c122a481c5e  Batch-Extract-v2-2026-10-17.txt
c28b0ac8b1fa72ae57fca84b2ae8980189b86cb9a9f3fb87a39fe6ba9ace2661  Example.Work.xml
9220f68f22df0c1bd0a05b7cd9d2e3743365b5b3d845babc4ff162d455e61754  shakti_batch_extract_v2.c
