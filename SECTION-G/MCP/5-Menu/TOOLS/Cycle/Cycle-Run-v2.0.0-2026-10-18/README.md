REQUIRED SCRIPTS: Cycle.Run.c | Memory.Run.c | Retention.Run.c | Sensory.Run.c | Cycle.Run.v2.0.0-2026-10-18.manifest.txt
# Cycle-Run-v2.0.0-2026-10-18

FAMILY: Cycle
TOOL: Run
VERSION: 2.0.0
DATE: 2026-10-18
ENTRY_KIND: manifest
ENTRY: Cycle.Run.v2.0.0-2026-10-18.manifest.txt
BUILD: cc -std=c99 -pedantic -Wall -Wextra -Werror -O2 -DSHAKTI_APP_NO_MAIN -Iinclude SECTION-A/*.c SECTION-G/MCP/5-Menu/TOOLS/Cycle/Cycle-Run-v2.0.0-2026-10-18/Cycle.Run.c -o shakti-cycle-run
COMMAND: ./shakti-cycle-run --run "{new_fixture_directory}"
EXAMPLE: ./shakti-cycle-run --run /tmp/shakti-cycle-v2-001

INTRODUCTION:
Run native synthetic full-size convergence, static five-reflection memory, fixed movie arena, transcript retention and restart recovery with the actual Section-A modules.

PACKAGE DETAILS: Cycle-Run-v2.0.0-2026-10-18.txt

ADD-ON FILES:
  Cycle.Run.Guide.txt

FILE HASHES:
9776c11df84ff9d3a52851d0cb32186ce677baa5292899040ed2fd2280620447  Cycle-Run-v2.0.0-2026-10-18.txt
4468fa6173aeeca41ab9e03944bbfe886656af80f4942aab40c81e258f8e8fb7  Cycle.Run.Guide.txt
3b31d95fe06fe5ab46be61c398da5e2e4aaf16f619f395fe402d66acd3915eb9  Cycle.Run.c
9776c11df84ff9d3a52851d0cb32186ce677baa5292899040ed2fd2280620447  Cycle.Run.v2.0.0-2026-10-18.manifest.txt
163f54f85929d0b73da5af7e7c292674899a3b19e5bfe0defca52fc877fba89a  Memory.Run.c
866d1e3482d8f2e0e92cd4cd85acc2dcebfb03652a55d7965c0a934a6370b6a7  Retention.Run.c
4ac9e944f0616eaf7c82b32c9d63da389407ae6929a33da23f7ab70f32bdf6b9  Sensory.Run.c
