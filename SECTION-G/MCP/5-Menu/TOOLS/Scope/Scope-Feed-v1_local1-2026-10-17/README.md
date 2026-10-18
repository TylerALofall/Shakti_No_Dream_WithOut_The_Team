REQUIRED SCRIPTS: Scope.Feed.c
# Scope-Feed-v1_local1-2026-10-17

FAMILY: Scope
TOOL: Feed
VERSION: 1_local1
DATE: 2026-10-17
ENTRY_KIND: single
ENTRY: Scope.Feed.c
BUILD: cc -std=c99 -pedantic -Wall -Wextra -Werror -O2 Scope.Feed.c -o shakti-feed
COMMAND: ./shakti-feed "{scope_output}" "{work_order}"
EXAMPLE: ./shakti-feed Example.Scope.xml Example.Work.xml

INTRODUCTION:
Convert filled work orders into scope XML using recorded direct calls; absent remote-call fields remain pending.

PACKAGE DETAILS: Scope-Feed-v1_local1-2026-10-17.txt

ADD-ON FILES:
  Example.Work.xml

FILE HASHES:
10791f8825f763d8c1607f944d6c3fef5d1fa811094a8cb988e4b25302ce9ba9  Example.Work.xml
fbeb0f67ed1369f1b4a0287e0a9b64e5f8f7d66b50fdd5cfe9bb170a300b99fc  Scope-Feed-v1_local1-2026-10-17.txt
1aaf50f5441ab4f7072f9519ea3956ef9649758f75045b678403e5dc2dca2cd3  Scope.Feed.c
