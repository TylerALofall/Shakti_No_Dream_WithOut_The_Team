REQUIRED SCRIPTS: zip_vault.c | zip_vault.h | junzip_nomalloc.c | junzip_nomalloc.h | Zip.Recovery.Run.c | Zip.Vault.manifest.txt
# Zip-Vault-v0.1_recovered1-2026-10-19

FAMILY: Zip
TOOL: Vault
VERSION: 0.1_recovered1
DATE: 2026-10-19
ENTRY_KIND: manifest
ENTRY: Zip.Vault.manifest.txt
BUILD: cc -std=c99 -pedantic -Wall -Wextra -Werror -O2 -DZV_TEST_HOOKS Zip.Recovery.Run.c zip_vault.c junzip_nomalloc.c -o zip-vault-run
COMMAND: ./zip-vault-run
EXAMPLE: ./zip-vault-run

INTRODUCTION:
Supplied static A/B/C loading-zone vault with recovered ZIP header, unchanged parser and vault sources, and an explicit synthetic qualification RUN. Stored ZIP entries only. The library is called in-process; the example executable exercises its APIs and existing damage hooks.

PACKAGE DETAILS: Zip-Vault-v0.1_recovered1-2026-10-19.txt

FILE HASHES:
f80bc2bd7f06940d030c16169fcbf8a35105caca3b97b068688e014223b63b9e  Zip-Vault-v0.1_recovered1-2026-10-19.txt
bac3f26478ec33a1142067a594fbee6b12dbe3775af6ecc84a2f02cef8fc2e1a  Zip.Recovery.Run.c
60a8aab640b0d50f3f7299d749ca9069ee968cc56572137ca9d2c6605eb20d8d  Zip.Vault.manifest.txt
0c73b219ef057b9c211df2dbacb59a1599e5538bcd2fe0b50353d69dd1563b1e  junzip_nomalloc.c
2bb875eefa43875ffcbb06ddd02df35a5189a6b0d0a6afdf9e08299161acf7f4  junzip_nomalloc.h
45ff3e29377984441c052c8dbe370aef63226ac61e2367fff76a1fd443921da0  zip_vault.c
eb14729b7254a2a653d34359e7fd0293a08e0363a7745d4834591218c7333ccc  zip_vault.h

NOTES:
  The sandbox described by these supplied files is the fixed A/B/C loading
  zone. B contains names, index and file bytes. A and C are unused guard
  chambers, checked for changes. Damage detected by the guard/fingerprint
  or per-file CRC checks closes and wipes the vault as documented by its APIs.
  These checks occur when the APIs are called; read-only pointer ownership is
  a caller contract. The host must serialize access and keep input immutable
  through the two-pass fill. Borrowed B pointers expire on wipe or breach.
  
  The three supplied implementation/header files and the recovered ZIP header
  are unchanged. Zip.Recovery.Run.c is a separate, explicitly invoked native
  qualification program; it never runs automatically inside Shakti. Build
  the two library sources without ZV_TEST_HOOKS for host integration. The
  primary BUILD above enables only the supplied guard inspection/damage
  hooks for the synthetic RUN. It constructs no production training input.
  
  Default static vault chambers total 31,457,280 bytes, plus 1,066,240 bytes
  of ZIP scratch and small metadata. The RUN reserves a separate static
  scratch and 70,000-byte fixture buffer. No explicit allocator, subprocess,
  network or optional decoder is used in this qualified path. Standard
  library internals and operating-system memory commitment are not proven
  allocation-free or guaranteed by memset. The archive and decoded output
  must fit the supplied fixed bounds; the vault does not grow.
  
  Stored entries only. Compressed entries, encryption, multi-disk and ZIP64
  are refused. A/B/C persistence, the airlock, heartbeat/eye-loader wiring
  and a compressed-entry decoder are outside this recovery.
  
  Strict C99 native RUN on Linux: 31 passed, zero BENCH, exit 0. It exercises
  both parser and vault, all 256 payload byte values, two entries, empty ZIP,
  maximum ZIP comment crossing a scan boundary, signed and signatureless
  descriptors, 255/256-byte names, input/output bounds, unsafe and duplicate
  names, CRC/header/descriptor damage, truncation, unsupported flags/layouts,
  oversized claimed payloads, lifecycle refusal and damage in A, B and C.
  This is bounded qualification with synthetic inputs; no blanket ZIP,
  hardware isolation, iOS runtime or untrusted-program execution claim is made.
