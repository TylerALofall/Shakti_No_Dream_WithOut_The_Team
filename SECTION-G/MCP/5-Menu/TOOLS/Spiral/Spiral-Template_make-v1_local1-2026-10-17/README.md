REQUIRED SCRIPTS: Spiral.Template_make.c
# Spiral-Template_make-v1_local1-2026-10-17

FAMILY: Spiral
TOOL: Template_make
VERSION: 1_local1
DATE: 2026-10-17
ENTRY_KIND: single
ENTRY: Spiral.Template_make.c
BUILD: cc -std=c99 -pedantic -Wall -Wextra -Werror -O2 Spiral.Template_make.c -o shakti-template-make
COMMAND: ./shakti-template-make "{input_file}"
EXAMPLE: ./shakti-template-make Example.Work.xml

INTRODUCTION:
Walk XML or XML fences and emit distinct template forms to standard output; package revision 1 preserves the supplied source.

PACKAGE DETAILS: Spiral-Template_make-v1_local1-2026-10-17.txt

ADD-ON FILES:
  Example.Work.xml

FILE HASHES:
10791f8825f763d8c1607f944d6c3fef5d1fa811094a8cb988e4b25302ce9ba9  Example.Work.xml
e6230c448d70e0e42702ffd73e24f30ef4ebe623d287fa4100aaeccf6787ff86  Spiral-Template_make-v1_local1-2026-10-17.txt
acf7469aa80ebccba724d4d38e161c7e7530232c0c84be973ed9f4a0b8b1dca0  Spiral.Template_make.c
