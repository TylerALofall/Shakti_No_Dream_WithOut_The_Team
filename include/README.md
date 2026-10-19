# C runtime interfaces

Section A previously included these headers without shipping them. The retained
definitions in COPILOT_INTAKE/shakti_master_architecture.md provided the starting
interfaces. They are restored as buildable headers, with updated loop, memory,
clock, school and log definitions. Formatting is not a byte-for-byte recovery.

shakti_app.h and shakti_media.h expose the serialized native controller API.
shakti_retention.h exposes fixed arena retention and native transcript input;
shakti_memory.h exposes timed stages and five-reflection capsule promotion.
See ../SECTION-A/Runtime.Flow.txt for ownership, timing, limits and unfinished
Swift/ML integration. The 64-pixel receptor declaration belongs to handwriting;
the full-size media interface has no 8x8 spatial limit.
