/*
 * sound_point.c -- SHAKTI_SOUND_POINT_V1
 *
 * The sound point organ: first thing, every lesson (CROSS_TEACH S1).
 * Every artifact gets 2 seconds of silence on EACH side; the sound
 * never touches the edges. No text solo, no sight solo, no sound solo.
 *
 * Input : 16 kHz, 16-bit PCM, mono WAV (her 69 foundation atoms).
 * Output: canonical 44-byte header + 32000 zero samples + artifact
 *         samples + 32000 zero samples. Metadata chunks (LIST/INFO,
 *         Lavf, etc.) are stripped -- the point is the sound, nothing
 *         else rides along.
 *
 * Laws kept:
 *   - C99 only. No heap, no float, no clock, no subprocess.
 *   - Deterministic: same input bytes -> same output bytes, always.
 *   - Every refusal path writes teach_me to MOMMA_OUTBOX.txt (law 5).
 *   - Atomic write: temp file, then rename.
 *   - Ledger: appends one line to SOUND_POINT.ndx per artifact and
 *     folds a running FNV-1a-64 stream pin printed at exit.
 *
 * Usage: sound_point <in.wav> <out.wav> <ledger> [stream_seed_hex]
 *   The optional seed chains the stream pin across a batch: pass the
 *   previous file's stream= value; the first file omits it (or 0).
 * Exit:  0 ok; 2 bad args; 3 unreadable; 4 not RIFF/WAVE;
 *        5 not PCM/mono/16k/16-bit; 6 no data chunk; 7 too long;
 *        8 write failure.
 *
 * Build (the gauntlet):
 *   gcc -std=c99 -pedantic -Wall -Wextra -Werror -O0 -o sp_O0 sound_point.c
 *   gcc -std=c99 -pedantic -Wall -Wextra -Werror -O2 -o sp_O2 sound_point.c
 *   outputs of sp_O0 and sp_O2 must be byte-identical (cmp).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define SP_RATE        16000u          /* her atoms: 16 kHz           */
#define SP_PAD_SAMPLES 32000u          /* 2 s at 16 kHz, each side    */
#define SP_MAX_FRAMES  262144u         /* longest atom seen: 154624   */
#define SP_DATA_MAX    (SP_MAX_FRAMES * 2u)

static unsigned char g_in[SP_DATA_MAX];   /* artifact PCM (static)    */
static unsigned char g_out[SP_DATA_MAX + 2u * SP_PAD_SAMPLES * 2u];

static const char *g_teach_path = "MOMMA_OUTBOX.txt";

/* FNV-1a 64 -- the same oracle the rest of her body uses */
static uint64_t fnv1a64(const unsigned char *p, size_t n, uint64_t h)
{
    size_t i;
    if (h == 0) h = 14695981039346656037ULL; /* 0xCBF29CE484222325 -- body basis (Claude finding 01: dropped digit fixed) */
    for (i = 0; i < n; ++i) {
        h ^= (uint64_t)p[i];
        h *= 1099511628211ULL;
    }
    return h;
}

static uint32_t rd_u32le(const unsigned char *p)
{
    return ((uint32_t)p[0]) | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint16_t rd_u16le(const unsigned char *p)
{
    return (uint16_t)(((uint16_t)p[0]) | ((uint16_t)p[1] << 8));
}

static void wr_u32le(unsigned char *p, uint32_t v)
{
    p[0] = (unsigned char)(v & 0xffu);
    p[1] = (unsigned char)((v >> 8) & 0xffu);
    p[2] = (unsigned char)((v >> 16) & 0xffu);
    p[3] = (unsigned char)((v >> 24) & 0xffu);
}

static void wr_u16le(unsigned char *p, uint16_t v)
{
    p[0] = (unsigned char)(v & 0xffu);
    p[1] = (unsigned char)((v >> 8) & 0xffu);
}

/* law 5: every refusal is a trigger -- it writes teach_me, never dies quiet */
static int refuse(const char *file, const char *reason, int code)
{
    FILE *f = fopen(g_teach_path, "ab");
    if (f != NULL) {
        fprintf(f, "teach_me|sound_point|%s|%s\n", file, reason);
        fclose(f);
    }
    fprintf(stderr, "REFUSE %s: %s\n", file, reason);
    return code;
}

int main(int argc, char **argv)
{
    static unsigned char hdr[65536];   /* finding 05: metadata blocks may exceed 4K before data (the 32 punctuation WAVs) */
    FILE *f;
    size_t got, pos, file_size, data_off = 0, data_len = 0;
    uint32_t frames, out_data_len, out_total;
    uint16_t audio_fmt = 0, channels = 0, bits = 0;
    uint32_t rate = 0;
    int have_fmt = 0, have_data = 0;
    uint64_t file_hash, stream_pin;
    char tmp_path[1024];
    const char *in_path, *out_path, *ledger_path;

    if (argc != 4 && argc != 5) {
        fprintf(stderr, "usage: sound_point <in.wav> <out.wav> <ledger> [stream_seed_hex]\n");
        return 2;
    }
    in_path = argv[1];
    out_path = argv[2];
    ledger_path = argv[3];

    f = fopen(in_path, "rb");
    if (f == NULL) return refuse(in_path, "cannot_open", 3);
    got = fread(hdr, 1, sizeof hdr, f);
    if (got < 44) { fclose(f); return refuse(in_path, "header_too_short", 4); }

    if (memcmp(hdr, "RIFF", 4) != 0 || memcmp(hdr + 8, "WAVE", 4) != 0) {
        fclose(f);
        return refuse(in_path, "not_riff_wave", 4);
    }

    /* walk chunks: fmt and data, wherever they sit */
    pos = 12;
    while (pos + 8 <= got) {
        uint32_t csize = rd_u32le(hdr + pos + 4);
        if (memcmp(hdr + pos, "fmt ", 4) == 0 && pos + 8 + 16 <= got) {
            audio_fmt = rd_u16le(hdr + pos + 8);
            channels  = rd_u16le(hdr + pos + 10);
            rate      = rd_u32le(hdr + pos + 12);
            bits      = rd_u16le(hdr + pos + 22);
            have_fmt = 1;
        } else if (memcmp(hdr + pos, "data", 4) == 0) {
            data_off = pos + 8;
            data_len = csize;
            have_data = 1;
        }
        pos += 8 + (size_t)csize + (csize & 1u);   /* chunks pad to even */
    }

    if (!have_fmt) { fclose(f); return refuse(in_path, "no_fmt_chunk", 4); }
    /* Claude finding F5: name the field that was WRONG, not the shape wanted */
    if (audio_fmt != 1) { fclose(f); return refuse(in_path, "not_pcm", 5); }
    if (channels != 1)  { fclose(f); return refuse(in_path, "not_mono", 5); }
    if (rate != SP_RATE){ fclose(f); return refuse(in_path, "rate_not_16k", 5); }
    if (bits != 16)     { fclose(f); return refuse(in_path, "bits_not_16", 5); }
    if (!have_data) { fclose(f); return refuse(in_path, "no_data_chunk", 6); }
    if ((data_len & 1u) != 0) { fclose(f); return refuse(in_path, "odd_data", 6); }
    frames = (uint32_t)(data_len / 2u);
    if (frames == 0) { fclose(f); return refuse(in_path, "empty_data", 6); }
    if (frames > SP_MAX_FRAMES) { fclose(f); return refuse(in_path, "too_long", 7); }

    /* read exactly the artifact samples, nothing more */
    if (fseek(f, (long)data_off, SEEK_SET) != 0) {
        fclose(f); return refuse(in_path, "seek_failed", 3);
    }
    got = fread(g_in, 1, data_len, f);
    fclose(f);
    if (got != data_len) return refuse(in_path, "short_read", 3);
    (void)file_size;

    /* assemble: pad + artifact + pad (g_out is zero-initialized static) */
    memset(g_out, 0, sizeof g_out);
    memcpy(g_out + (size_t)SP_PAD_SAMPLES * 2u, g_in, data_len);
    out_data_len = (uint32_t)(2u * SP_PAD_SAMPLES * 2u + data_len);
    out_total = 44u + out_data_len;

    /* atomic write: temp, then rename */
    if (strlen(out_path) + 8 >= sizeof tmp_path)
        return refuse(in_path, "path_too_long", 8);
    snprintf(tmp_path, sizeof tmp_path, "%s.sp_tmp", out_path);
    f = fopen(tmp_path, "wb");
    if (f == NULL) return refuse(in_path, "tmp_open_failed", 8);

    {
        unsigned char oh[44];
        memcpy(oh, "RIFF", 4);
        wr_u32le(oh + 4, out_total - 8u);
        memcpy(oh + 8, "WAVE", 4);
        memcpy(oh + 12, "fmt ", 4);
        wr_u32le(oh + 16, 16u);
        wr_u16le(oh + 20, 1u);            /* PCM                    */
        wr_u16le(oh + 22, 1u);            /* mono                   */
        wr_u32le(oh + 24, SP_RATE);
        wr_u32le(oh + 28, SP_RATE * 2u);  /* byte rate              */
        wr_u16le(oh + 32, 2u);            /* block align            */
        wr_u16le(oh + 34, 16u);           /* bits                   */
        memcpy(oh + 36, "data", 4);
        wr_u32le(oh + 40, out_data_len);
        if (fwrite(oh, 1, 44, f) != 44) {
            fclose(f); remove(tmp_path);
            return refuse(in_path, "write_header_failed", 8);
        }
    }
    if (fwrite(g_out, 1, out_data_len, f) != out_data_len) {
        fclose(f); remove(tmp_path);
        return refuse(in_path, "write_data_failed", 8);
    }
    if (fclose(f) != 0) { remove(tmp_path); return refuse(in_path, "close_failed", 8); }
    if (rename(tmp_path, out_path) != 0) {
        remove(tmp_path);
        return refuse(in_path, "rename_failed", 8);
    }

    /* hash the exact bytes on disk -- the pin is the oracle */
    f = fopen(out_path, "rb");
    if (f == NULL) return refuse(in_path, "reopen_failed", 3);
    file_hash = fnv1a64(NULL, 0, 0);
    {
        size_t r;
        while ((r = fread(g_in, 1, SP_DATA_MAX, f)) > 0)
            file_hash = fnv1a64(g_in, r, file_hash);
    }
    fclose(f);

    /* stream pin: optional seed chains the batch, then path, then pin */
    stream_pin = 0;
    if (argc == 5) {
        const char *s = argv[4];
        char *end = NULL;
        stream_pin = (uint64_t)strtoull(s, &end, 16);
        if (end == s || *end != '\0')
            return refuse(in_path, "bad_stream_seed", 2);
    }
    stream_pin = fnv1a64(NULL, 0, stream_pin);
    stream_pin = fnv1a64((const unsigned char *)in_path, strlen(in_path),
                         stream_pin);
    {
        unsigned char hb[8];
        int i;
        for (i = 0; i < 8; ++i)
            hb[i] = (unsigned char)((file_hash >> (56 - 8 * i)) & 0xffu);
        stream_pin = fnv1a64(hb, 8, stream_pin);
    }

    /* ledger line: name, original frames, staged frames, file pin, stream */
    f = fopen(ledger_path, "ab");
    if (f == NULL) return refuse(in_path, "ledger_failed", 8);
    fprintf(f, "%s\t%u\t%u\t%016llX\t%016llX\n", in_path, frames,
            frames + 2u * SP_PAD_SAMPLES,
            (unsigned long long)file_hash,
            (unsigned long long)stream_pin);
    fclose(f);

    printf("%s frames=%u staged=%u pin=%016llX stream=%016llX\n",
           in_path, frames, frames + 2u * SP_PAD_SAMPLES,
           (unsigned long long)file_hash,
           (unsigned long long)stream_pin);
    return 0;
}
