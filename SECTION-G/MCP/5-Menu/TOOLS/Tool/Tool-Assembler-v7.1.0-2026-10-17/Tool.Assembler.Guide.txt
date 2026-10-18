REQUIRED SCRIPTS: Tool.Assembler.c | Tool.README_builder.c | Tool.Assembler.h | Tool.Hash.h | Tool.Assembler.v7.1.0-2026-10-17.manifest.txt
# Tool-Assembler v7.1

INTRODUCTION:
A package librarian. Supply an existing tool directory and a small package
record. It copies the files, writes README last, hashes the complete package
and appends the package seal to TOOLS_LEDGER.txt. Supplied tools are never
built or executed. The existing hash tools handle repository change reporting.

COMMAND:
./shakti-assembler "{bank_dir}" "{tool_dir}" "{package_record}"

EXAMPLE, from this directory, with ../bank already created:
./shakti-assembler ../bank . Tool.Assembler.v7.1.0-2026-10-17.manifest.txt
This packages the assembler's own source and companion files with this same
assembler. Replace each named COMMAND placeholder with the intended path.

BUILD THE ASSEMBLER ITSELF:
cc -std=c99 -pedantic -Wall -Wextra -Werror -O2 Tool.Assembler.c Tool.README_builder.c -o shakti-assembler

FAMILY AND PACKAGE NAMES:
BANK/Family/Family-Tool-vVersion-YYYY-MM-DD/
Example: bank/Base64/Base64-Decode-v1.1.0-2026-10-17/
Tools and versions in the same family share the Family directory. Each package
contains its source and companion files, a plain .txt record and README.
Family and tool begin with a capital letter; word separators are underscores.
Version begins with a digit and uses letters, digits, dots or underscores. Date
is supplied explicitly as a valid YYYY-MM-DD date; the assembler reads no clock.
Version identifies the supplied package revision, not an inferred source release.
Existing source names, includes, file bytes and file modes are retained.
Older source filenames are accepted; family/tool/version/date identity is
carried by the enclosing package directory and the package record.
Nested attachment directories, including empty ones, are copied and sealed.

PACKAGE RECORD:
Use one KEY: value per line, no tabs or carriage returns. All nine single fields
are mandatory and unique. BUILD is an optional, unique field recording the
exact compiler command; the assembler never executes it. Repeat REQUIRED for every required script, module,
header, manifest or supplied data input. Their order is kept in the README's
first row. Missing requirements, duplicate/unknown fields and overflow refuse.

FAMILY: Base64
TOOL: Decode
VERSION: 1.1.0
DATE: 2026-10-17
INTRODUCTION: Decode a local Base64 file into binary bytes.
BUILD: cc -std=c99 -pedantic -Wall -Wextra -Werror -O2 Base64.Decode.c -o shakti-decode
COMMAND: ./shakti-decode -- "{input_file}" "{output_file}"
EXAMPLE: ./shakti-decode -- Base64.Sample.Input.txt Base64.Sample.Output.bin
ENTRY_KIND: single
ENTRY: Base64.Decode.c
REQUIRED: Base64.Decode.c

The supplied record is copied as Family-Tool-vVersion-Date.txt so the exact
identity and command metadata are available to the caller. It is ordinary text
data for the assembler and menu, not an extra script needed to run the tool.
It can be read in a text viewer and can be outside
INCOMING or be a file already inside it. Root README.md is preserved as indented
text in the generated README. Repeat runs use the same original input directory.
INCOMING can be a tool already inside the Menu bank; it must not contain the
output bank or the destination package. Original input files are never edited.
The Base64 example input and expected binary output are supplied in
../Base64-Decode. Its README supplies the C99 build command. Replace the named
COMMAND placeholders with actual paths; EXAMPLE names the supplied input and
a new output file. These example data files are add-ons, not required scripts.

MULTIPLE SCRIPTS AND THE MENU:
More than one REQUIRED .c/.sh/.cmd/.bat file requires ENTRY_KIND: manifest or
ENTRY_KIND: batch. ENTRY must name a supplied .manifest or .batch file, optionally
ending in .txt for text viewers. It must be
listed as REQUIRED. A manifest lists the complete source/module set for the
menu; BUILD records how that set produces its program, and COMMAND/EXAMPLE
use that program's real arguments. A batch entry must appear in COMMAND and
EXAMPLE, and the named program must already consume its format. The assembler
verifies the record and files; it does not create a dispatcher or prove runtime behavior.
Both command lines name the same simple program token, optionally prefixed ./.
The program name is explicit, so existing names such as shakti-ticket-dealer
and section_hash_watch are accepted. The menu jumper can parse COMMAND, EXAMPLE,
ENTRY_KIND and ENTRY directly from the package record or generated README.
Tool.Assembler.v7.1.0-2026-10-17.manifest.txt is the multi-file example: the assembler
consumes that package record as its manifest argument. It does not compile or
execute the modules listed in it.

SAVING ORDER:
1. Read the supplied record and files; verify the declared requirements.
2. Copy payload files into an exclusively created staging directory.
3. Write README last, with required scripts in its first row, introduction,
   family/tool/version/date, entry, command, example, add-ons and payload hashes.
   Empty add-on/directory/note sections are omitted. Original README text is
   kept under NOTES only when present. Seal implementation notes stay here.
4. Hash the complete directory, including the final README.
5. Publish the versioned package and append its directory seal to the ledger.
An identical repeat leaves both package and ledger byte-identical. A different
submission at an occupied package name does not overwrite those files; choose a
new package version/date. Changes elsewhere do not block this operation.

README HASHES AND PACKAGE LEDGER:
README lists SHA-256 for each other payload file, including the package record.
It excludes its own hash to avoid a circular definition. The external ledger's
directory seal includes the final README. The seal is the SHA-256 of byte-sorted
LF-terminated index lines in these exact forms:
F SHA256 relative-path
D relative-directory/
Empty directories are included. File and directory permissions are omitted
from the seal, so Git checkouts retain the same content seal. The copy step
still retains supplied file permissions. New directories use mode 0700. The ledger
is outside the package, so nothing hashes itself. Its exact record format is:
SHA256 <64 lowercase hex digits>  Family/Family-Tool-vVersion-Date
TOOLS_LEDGER.txt is opened only for append. Existing bytes are never rewritten;
if a path has multiple historical records, the last is its current registration.
The assembler creates no replacement repository inventory or change detector.
A prior registered seal that differs from a supplied, matching existing package
is reported as NOTICE and a new ledger record is appended; it is not a gate.
Package publication and ledger append are separate operations. A failure after
publication reports that registration is incomplete instead of claiming success.

EXISTING CHANGE REPORTER, A SEPARATE VISIBLE COMMAND:
./section_hash_watch ROOT --all-files
The supplied section_hash_watch.c already recursively tracks SECTION-A through
SECTION-O, including Menu under SECTION-G. --all-files includes non-C files.
It appends NEW / EDITED / REMOVED, SHA-256, byte counts and byte deltas to the
per-section logs in ROOT/.00 and to ROOT/SECTION_MASTER_LOG.txt. Changes return
success. The scanner uses UTC dates. It does not scan loose root-level files or
directories outside those SECTION trees; Global.Hasher.c is the existing
whole-root hash logger. These are separate existing tools, not new assembler
implementations. The assembler does not invoke them as subprocesses or depend
on their exit status. Their run status can be shown separately by the menu.

FIXED LIMITS AND ERRORS:
1536 entries per package scan, 128 distinct package paths in the ledger, 64
required files, 12 directory levels, paths under 1024 bytes, file data at most
8 MiB, README/record under 128 KiB, package seal index under 2 MiB, ledger under
1 MiB. No explicit heap, subprocess, network, external library or SDK calls.
Symlinks, unsupported files, unreadable input, invalid metadata, occupied output
names, incomplete ledger data and busy/stale temporary paths are errors. These
are packaging/I/O failures, not a ban on operating after a repository change.

RUN STATUS:
2026-10-17, isolated Linux copies: the initial v7 build and existing unchanged
section_hash_watch.c both built with C99 -pedantic -Wall -Wextra -Werror -O2.
The watcher baselined 244 SECTION paths, then reported one EDITED file with a
72-byte delta and exit 0. Packaging other Menu tools continued successfully.
Five package registrations, including two revisions in one family and the
assembler's multi-file manifest package, passed independent directory-seal and
README-payload SHA-256 checks. The assembler's static BSS was 15,454,784 bytes.
An identical repeat preserved README and ledger bytes. Earlier ledger bytes
remained unchanged after new appends. A missing required manifest refused with
exit 2 and left the ledger unchanged. A deliberately different prior package
seal produced NOTICE and a successful append, retaining its historical record.
Only disposable copies were changed for these runs. Existing hash-tool sources
and main were untouched. The tool commands documented for submitted packages
were not executed by the assembler. Independent acceptance is not asserted.
The revised concise README and .txt record/manifest filenames also passed
strict compilation and independent seal/payload checks for Base64, Spiral
and the assembler's multi-file package. An identical Base64 repeat retained
both README and ledger bytes.
