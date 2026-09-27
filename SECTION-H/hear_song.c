/*
 * hear_song.c — SHAKTI hears a song. Deterministic, C99, no heap, no rand().
 *
 * Usage: hear_song <file.mp3> <pcm_s16le_mono_22050.raw> <log_out>
 *
 * She does not "vibe". She measures:
 *   1. FNV-1a 64 of the exact mp3 bytes (the song's fingerprint)
 *   2. per-second RMS energy envelope from 16-bit PCM @ 22050 Hz mono
 *   3. loudest second, quietest second, seconds above half-peak (drive)
 * Same file in -> same log out, on any machine, forever.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define RATE 22050
#define MAX_SEC 4096

static uint64_t fnv1a_file(const char *path) {
    unsigned char buf[8192];
    size_t n;
    uint64_t h = 0xcbf29ce484222325ULL;
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) {
        size_t i;
        for (i = 0; i < n; i++) { h ^= buf[i]; h *= 0x100000001b3ULL; }
    }
    fclose(f);
    return h;
}

static uint64_t fnv1a_mem(const unsigned char *p, size_t n) {
    uint64_t h = 0xcbf29ce484222325ULL;
    size_t i;
    for (i = 0; i < n; i++) { h ^= p[i]; h *= 0x100000001b3ULL; }
    return h;
}

#define HEAR_SONG_MAIN main
int HEAR_SONG_MAIN(int argc, char **argv) {
    static short pcm[RATE * MAX_SEC];
    static unsigned char pcmb[RATE * MAX_SEC * 2];
    static unsigned rms[MAX_SEC];
    FILE *f, *log;
    long pcm_bytes, total_frames, sec, nsec, i;
    uint64_t mp3_hash, pcm_hash;
    unsigned peak = 0, quiet = 1000;
    long peak_sec = 0, quiet_sec = 0, drive_sec = 0;

    if (argc != 4) { fprintf(stderr, "usage: hear_song <mp3> <pcm.raw> <log>\n"); return 2; }

    mp3_hash = fnv1a_file(argv[1]);
    if (!mp3_hash) { fprintf(stderr, "cannot read %s\n", argv[1]); return 1; }

    f = fopen(argv[2], "rb");
    if (!f) { fprintf(stderr, "cannot read %s\n", argv[2]); return 1; }
    pcm_bytes = (long)fread(pcmb, 1, sizeof pcmb, f);
    fclose(f);
    total_frames = pcm_bytes / 2;
    nsec = total_frames / RATE;
    if (nsec > MAX_SEC) nsec = MAX_SEC;

    for (i = 0; i < total_frames && i < (long)(RATE * MAX_SEC); i++)
        pcm[i] = (short)(pcmb[2*i] | (pcmb[2*i+1] << 8));
    pcm_hash = fnv1a_mem(pcmb, (size_t)pcm_bytes);

    for (sec = 0; sec < nsec; sec++) {
        uint64_t acc = 0;
        long base = sec * RATE, j;
        for (j = 0; j < RATE; j++) {
            long v = pcm[base + j];
            acc += (uint64_t)(v * v);
        }
        /* scaled RMS energy units (relative, deterministic) */
        rms[sec] = (unsigned)(acc / RATE / 32768UL * 1000UL / 32768UL * 64UL);
        if (rms[sec] > peak)  { peak = rms[sec];  peak_sec = sec; }
        if (rms[sec] < quiet) { quiet = rms[sec]; quiet_sec = sec; }
    }
    for (sec = 0; sec < nsec; sec++)
        if (rms[sec] > peak / 2) drive_sec++;

    log = fopen(argv[3], "w");
    if (!log) { fprintf(stderr, "cannot write %s\n", argv[3]); return 1; }
    fprintf(log, "HEAR — she listens. deterministic measurement, C99, no rand(), no heap.\n");
    fprintf(log, "song fnv1a64:%016llX  pcm fnv1a64:%016llX\n",
            (unsigned long long)mp3_hash, (unsigned long long)pcm_hash);
    fprintf(log, "pcm: %ld bytes, %ld frames @22050Hz mono, %ld seconds measured\n",
            pcm_bytes, total_frames, nsec);
    fprintf(log, "peak energy %u at %ld:%02ld — quietest %u at %ld:%02ld — %ld seconds above half-peak (drive)\n",
            peak, peak_sec/60, peak_sec%60, quiet, quiet_sec/60, quiet_sec%60, drive_sec);
    fprintf(log, "envelope (RMS per second, scaled units):\n");
    for (sec = 0; sec < nsec; sec++) {
        fprintf(log, "%4u", rms[sec]);
        if ((sec + 1) % 20 == 0 || sec == nsec - 1) fprintf(log, "\n");
    }
    fclose(log);
    printf("hear_song: %ld seconds, peak %u @%ld:%02ld, mp3 %016llX\n",
           nsec, peak, peak_sec/60, peak_sec%60, (unsigned long long)mp3_hash);
    return 0;
}
