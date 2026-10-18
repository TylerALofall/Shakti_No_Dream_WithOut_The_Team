# Prenatal input loader — 2026-10-18

Base: `TylerALofall/Shakti_No_Dream_WithOut_The_Team` at
`ff5a03a0a809bff86b3bb2b5f433dc3ca2860a30`.
Branch: `shakti/clean-start-2026-10-18`.

The loader prepares supplied input samples and deposits ordinary descriptors
through `shakti_app_queue_frame()`. The existing workflow owns phase advancement,
binary breakdown, reconstruction, validation, convergence, retention, release,
training order, interruption/replay and memory transitions.

## Files and boundary

The two new model-side files are `SECTION-B/shakti_prenatal_loader.c` and
`include/shakti_prenatal_loader.h`. Existing Section-A, Section-B and include
sources remain unchanged. The new Menu package contains this guide, the native
RUN, XML data, eight original color recordings and one prepared pulse atom.
The existing Tool-Assembler packages it and appends its seal to the tool ledger.

The loader has no clock, training counter, release routine, memory transition,
speaker, network call, heap allocation or runtime build. Its model-facing
function calls the existing intake exactly once and returns the actual admission
result. The RUN driver explicitly invokes downstream functions to test the
handoff; those calls belong to the RUN, never the loader.

## Definitions and configurable inputs

| Name | Definition |
|---|---|
| Heart trigger | Externally supplied event that owns the complete beat. |
| Beat | One logical second by default, containing 64 hearing slots, 32 sight frames and one pulse atom. Physical pace belongs to the external owner. |
| `hearing_slot` | Zero-based phase 0–63 supplied by the workflow; user table row N corresponds to slot N−1. |
| Sight phase | `hearing_slot / 2`; both adjacent hearing slots select the same density entry. |
| `sample_offset` | Continuous position in a particular recording, supplied by the workflow. It continues beyond a beat when the recording is longer. |
| WAV sample | One signed 16-bit mono sound value, stored as two little-endian bytes at 16,000 samples per source second. |
| Audio input chunk | 250 samples / 500 bytes per hearing slot at the default source rate. |
| `width`, `height` | Complete generated RGB frame dimensions. Default 1920×1080. Width is a positive multiple of four so density groups are exact; height is positive. Capacity failures refuse the whole frame. |
| RGB | Three exact channel bytes. The general helper preserves 0x80 brightness as well as 0x00 and 0xFF. The optional three-bit shorthand uses bit0=red, bit1=green, bit2=blue. |
| `density` | Numerator 0–4 over four pixels: off, quarter, half, three-quarter, full. Lit pixels keep their original RGB values; the remaining pixels are black. |
| `pulse_on_numerator`, `pulse_on_denominator` | Rational active fraction of one pulse atom. Default 2/3; denominator is bounded at 1,000,000. |
| `voice_to_pulse_ratio` | Peak ratio relative to the supplied reference voice. Default 4; accepted range 2–32,768. A requested level that quantizes entirely to zero refuses preparation visibly. |
| `monitor_enabled` | Native host output preference. Default 0 requests muted phone playback. It never zeros Shakti's input samples. The native host must honor this field when wiring playback. |
| Input bank | Caller-owned fixed storage containing whole WAV files and their borrowed sample ranges. Keep it alive while its samples are used. |
| Reference / binary / rebuilt buffers | Existing workflow-owned processing buffers described by `shakti_media_frame_t`. The loader never treats preparation output as rebuilt perception. |
| `SHAKTI_PRENATAL_TOTAL_BEATS` | Metadata constant 22,982,400 = 2^8×3^3×5^2×7×19. The workflow owns counting and completion. |

`Prenatal.Profile.xml` supplies every runtime profile field. The native host can
parse those bytes with `shakti_prenatal_config_parse()` or supply the same
configuration struct directly. Configuration changes require no C source edit.
The parser accepts the documented flat root and fields, with whitespace; it
refuses duplicate/unknown fields, missing values, unsupported syntax and overflow.

The supplied 32-entry density profile reproduces the illustrative 64-slot ramp:
slots 1–2 quarter, 3–4 half, 5–6 three-quarter, 7–36 full, 37–38 three-quarter,
39–40 half, 41–42 quarter, 43–64 off. Those are one-based user table rows.
Pixel masks use nested diagonal/checker patterns with exact quarter counts.

Pulse timing and sight sampling have different resolutions. At 16 kHz, 2/3 of
one second rounds to 10,667 PCM samples, with 5,333 remaining zero samples.
The density example has 21 active sight frames; its off boundary is the nearest
whole-frame boundary to 2/3 second. That introduces a 1/96-second boundary
difference. The exact 64:32:1 count relationship is preserved. These disclosed
sample/frame choices do not assert an exact irrational phi boundary.
For an explicit phi approximation, profile fraction 1597/2584 produces 9,889
active PCM samples; the sight profile remains separately supplied configuration.

## Original recordings and quiet pulse

The eight color WAVs are byte-for-byte copies from the supplied nested archive:
`eden 3/media/voice69/`. Their lengths, padding, metadata and spoken content are
preserved. In particular, black has 17,280 samples and magenta has 17,600;
both extend beyond one default beat. The audio helper preserves every recorded
sample and zero-pads only the final partial 250-sample chunk. The workflow owns
continuation and decides when to move to another recording/color.

`Assets/pulse.wav` is one reusable 16,000-sample atom prepared from the first
complete second of the supplied 90-second mono heartbeat. Preparation resamples
that source second into the configured active interval, applies a 1,500-sample
rise/fall edge (bounded by the active length), and normalizes its peak relative
to the reference voice. This deliberately changes the source heartbeat atom;
the original archive and voice recordings remain intact.

The default reference is magenta, the quietest supplied color recording: peak
8,441. The prepared pulse peaks at 2,110, so every supplied color voice is at
least four times higher in peak amplitude. Other voices are louder still.
The relative setting can be made much quieter; 100:1 was also exercised.
These are digital amplitude ratios, not calibrated sound-pressure dB readings.
No audio device was opened during preparation or qualification.

Use `Catalog.xml` for host-side file/RGB/provenance information. Keep its names,
paths and lesson bookkeeping outside prenatal perception. The RUN uses the
same opaque source id `prenatal.input` and empty context for all colors.

The initial black/white request is cataloged as 5,040 workflow-owned cycles:
master slots 1–32 black and 33–64 white. On the 64-slot master grid these are
16 black sight frames followed by 16 white sight frames per cycle. The loader
provides ordinary black/white pixels when requested; it never executes or
credits that schedule. The workflow owns how those cycles fit its prenatal
timeline, balance, replay and remaining training budget.

## Native host / HTML handoff

The supplied `shakti_host.html` was read as a reference. Its Files handler stores
file metadata and readable text locally, and its heartbeat sheet uses a
minute-based UI timer. It has no native C loading bridge in the supplied file.
The compiled Swift/WKWebView bridge source was not supplied.

The concrete native handoff is:

1. File picker passes complete local bytes to bounded native storage.
2. WAV bytes enter `shakti_prenatal_wave_parse()`, or native paths enter
   `shakti_prenatal_wave_load()`. The returned sample range borrows that storage.
3. Prepare the one pulse input atom at asset setup. When the workflow requests
   a sample, use `shakti_prenatal_audio()` with its continuous sample offset,
   or `shakti_prenatal_pixels_rgb()` with its chosen RGB/density.
4. The native owner supplies the ordinary complete media descriptor and its
   distinct input/reference/binary/rebuilt buffers, opaque source id and actual
   source position. Prenatal context is empty.
5. `shakti_prenatal_deposit()` hands that descriptor to the actual existing MCP
   intake. On refusal, ownership remains with the caller and the workflow
   decides when to try again. Acceptance itself performs no downstream work.
6. The existing workflow takes, breaks down, rebuilds, verifies, publishes and
   releases. Display consumers use its validated rebuilt sight buffer. The
   native playback adapter honors muted monitoring independently of input gain.

The present media group contains sight plus two consecutive audio chunks.
Its two audio lanes are adjacent times, not heartbeat/voice channels. The
independent simultaneous heartbeat input/convergence connection is still an
explicit integration requirement. This package proves pulse intake separately
through the real binary path; it does not mix heartbeat into founder speech or
reinterpret those two lanes. The host owner must connect its separate pulse
port before claiming the complete three-stream live arrangement.

## Build and run

From the repository root, build the new qualification program with the actual
Section-A sources. The RUN includes the real main.c under a renamed console
entry point; the normal console entry is not executed.

```sh
cc -std=c99 -pedantic -Wall -Wextra -Werror -O2 -Iinclude -ISECTION-A SECTION-G/MCP/5-Menu/TOOLS/Prenatal/Prenatal-Input-v1.0.0-2026-10-18/Prenatal.Run.c SECTION-B/shakti_prenatal_loader.c SECTION-A/shakti_*.c -o prenatal-run
./prenatal-run --run SECTION-G/MCP/5-Menu/TOOLS/Prenatal/Prenatal-Input-v1.0.0-2026-10-18/Assets SECTION-G/MCP/5-Menu/TOOLS/Prenatal/Prenatal-Input-v1.0.0-2026-10-18/Prenatal.Profile.xml /tmp/shakti-prenatal-new-fixture
```

The fixture path must be new. The program creates only its disposable fixture
files and refuses an occupied fixture. To prepare a new pulse from a preserved
source WAV, give a profile, original heartbeat, chosen reference voice and a
new output filename:

```sh
./prenatal-run --prepare PROFILE.xml SOURCE_HEART.wav REFERENCE_VOICE.wav NEW_PULSE.wav
```

Read/review, edit, compile and execute remain separate actions. The assembler
only packages the submitted RUN and assets. `Build_Run_Record.txt` and the
included stdout/stderr and atomic receipt export describe actual qualification
and its limits, including the observed live-log workspace discrepancy.
