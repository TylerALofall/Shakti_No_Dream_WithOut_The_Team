REQUIRED SCRIPTS: shakti_batch_bitmap_v3.c
# Batch-Bitmap-v3-2026-10-17

FAMILY: Batch
TOOL: Bitmap
VERSION: 3
DATE: 2026-10-17
ENTRY_KIND: single
ENTRY: shakti_batch_bitmap_v3.c
BUILD: cc -std=c99 -pedantic -Wall -Wextra -Werror -O2 shakti_batch_bitmap_v3.c -o shakti_batch_bitmap_v3
COMMAND: ./shakti_batch_bitmap_v3 "{graph_input}" "{output_prefix}" "{scale_750_to_1500}"
EXAMPLE: ./shakti_batch_bitmap_v3 Example.Graph.txt Example.Map 1500

INTRODUCTION:
Render explicit graph records into section BMP files and a cross-section link BMP using native C pixels; scale is 750 through 1500.

PACKAGE DETAILS: Batch-Bitmap-v3-2026-10-17.txt

ADD-ON FILES:
  Example.Graph.txt

FILE HASHES:
7d30dddf528a98990289d4e072823ba6a4a60097072974c3152ce6baef28fb03  Batch-Bitmap-v3-2026-10-17.txt
678388cdbbe5af18ac82489c4d289d0b4cc87221e52e7c169141b4aa037feefa  Example.Graph.txt
35c826de640ab703e2a49eec0f101819a92261c39d1192bb7b40dbcf65d60f51  shakti_batch_bitmap_v3.c
