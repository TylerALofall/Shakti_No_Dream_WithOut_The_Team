REQUIRED SCRIPTS: junzip_nomalloc.c | junzip_nomalloc.h
# Zip-Core-v1_local2-2026-10-19

FAMILY: Zip
TOOL: Core
VERSION: 1_local2
DATE: 2026-10-19
ENTRY_KIND: single
ENTRY: junzip_nomalloc.c
BUILD: cc -std=c99 -pedantic -Wall -Wextra -Werror -O2 -c junzip_nomalloc.c -o junzip_nomalloc.o
COMMAND: cc -std=c99 -pedantic -Wall -Wextra -Werror -O2 -c junzip_nomalloc.c -o junzip_nomalloc.o
EXAMPLE: cc -std=c99 -pedantic -Wall -Wextra -Werror -O2 -c junzip_nomalloc.c -o junzip_nomalloc.o

INTRODUCTION:
Supplied fixed-buffer ZIP parsing library with its recovered companion header. No optional decoder backend is enabled. The unchanged implementation is exercised by the companion Vault package's explicit native RUN.

PACKAGE DETAILS: Zip-Core-v1_local2-2026-10-19.txt

FILE HASHES:
873a6b6e96360684d124c0a47c45ce00c5a638ccd8e03673e09cdcf923961cf2  Zip-Core-v1_local2-2026-10-19.txt
0c73b219ef057b9c211df2dbacb59a1599e5538bcd2fe0b50353d69dd1563b1e  junzip_nomalloc.c
2bb875eefa43875ffcbb06ddd02df35a5189a6b0d0a6afdf9e08299161acf7f4  junzip_nomalloc.h

NOTES:
  The recovered header is byte-identical to Tyler's supplied file; only its
  local filename is normalized to junzip_nomalloc.h. The existing C source
  is unchanged. This is a library, called directly by its host.
  
  Build with no HAVE_ZLIB or HAVE_PUFF define. Stored entries can be read;
  deflated entries return JZ_ERR_UNSUPPORTED without a compiled decoder.
  No decoder dependency is supplied or enabled by this recovery. The optional
  zlib branch uses zlib's default allocator and is outside the qualified build.
  
  JZScratch occupies 1,066,240 bytes with default caps and belongs to the
  caller. The qualified RUN uses static scratch and memory-backed I/O.
  Native standard-library internals are outside an explicit-allocator audit.
  Scratch filenames expire on the next directory entry; decoded output must
  be staged and published only after a successful status and CRC check.
  
  The companion Zip-Vault-v0.1_recovered1-2026-10-19 package includes the same
  core and header, plus Zip.Recovery.Run.c. Its 31-check native RUN covers
  stored-byte recovery, hard bounds, directory/name consistency, descriptors,
  CRC rejection and vault lifecycle/guard behavior. The core object also
  builds independently using the command above. This does not certify every
  ZIP layout, optional decoder, filesystem adapter or Apple device behavior.
