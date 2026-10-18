REQUIRED SCRIPTS: Cycle.Run.c
# Cycle-Run-v1.0.0-2026-10-18

FAMILY: Cycle
TOOL: Run
VERSION: 1.0.0
DATE: 2026-10-18
ENTRY_KIND: single
ENTRY: Cycle.Run.c
BUILD: cc -std=c99 -pedantic -Wall -Wextra -Werror -O2 -DSHAKTI_APP_NO_MAIN -Iinclude SECTION-A/*.c SECTION-G/MCP/5-Menu/TOOLS/Cycle/Cycle-Run-v1.0.0-2026-10-18/Cycle.Run.c -o shakti-cycle-run
COMMAND: ./shakti-cycle-run --run "{new_fixture_directory}"
EXAMPLE: ./shakti-cycle-run --run /tmp/shakti-cycle-fixture-001

INTRODUCTION:
Run the real Section-A C runtime against new synthetic fixtures for continuous scheduling, whole memory, full-size convergence and restart recovery.

PACKAGE DETAILS: Cycle-Run-v1.0.0-2026-10-18.txt

ADD-ON FILES:
  Cycle.Run.Guide.txt
  Cycle.Run.v1.0.0-2026-10-18.txt

FILE HASHES:
b36184bdec2de5e567e1f5c10c37c340169f67c2c289f38c135969430cd08d9a  Cycle-Run-v1.0.0-2026-10-18.txt
1b4c3a7da2891cd5a154ee2754d5524fe4ab939230888fa6d7f63c15cedaf583  Cycle.Run.Guide.txt
9beea0d1536e7a033306e48ef5a55559d3f2d7ac683e3022b2d83eb6b1ef53f6  Cycle.Run.c
b36184bdec2de5e567e1f5c10c37c340169f67c2c289f38c135969430cd08d9a  Cycle.Run.v1.0.0-2026-10-18.txt
