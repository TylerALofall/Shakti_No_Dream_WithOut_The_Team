/*
 * junzip_nomalloc.c -- malloc-free, strict-C99 ZIP parsing core.
 * See junzip_nomalloc.h for the contract.
 */

#include "junzip_nomalloc.h"

#include <string.h>

#ifdef HAVE_ZLIB
#include <zlib.h>
#else
#define Z_OK 0
#define Z_ERRNO (-1)
#endif

#ifdef HAVE_PUFF
#include "puff.h"
#endif

/* ---- Raw ZIP record sizes (fixed by the format) ---- */
#define JZ_LOCAL_RAW   30u
#define JZ_CENTRAL_RAW 46u
#define JZ_END_RAW     22u
#define JZ_MAX_COMMENT 65535u
#define JZ_TAIL_OVERLAP 21u   /* END_RAW - 1: straddle window between chunks */

#define JZ_SIG_LOCAL   0x04034B50u
#define JZ_SIG_CENTRAL 0x02014B50u
#define JZ_SIG_END     0x06054B50u
#define JZ_SIG_DDESC   0x08074B50u

/* ---- Explicit little-endian field decode: no packed structs, no alignment
 *      assumptions, endian-correct by construction. ---- */
static uint16_t jz_rd16(const unsigned char *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t jz_rd32(const unsigned char *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

uint32_t jz_crc32(uint32_t crc, const void *data, size_t size)
{
    uint32_t table[256];
    const unsigned char *p = (const unsigned char *)data;
    size_t i;
    int j;

    for (i = 0; i < 256u; i++) {
        uint32_t r = (uint32_t)i;
        for (j = 0; j < 8; j++)
            r = (r & 1u) ? (r >> 1) ^ 0xEDB88320UL : (r >> 1);
        table[i] = r;
    }

    crc ^= 0xFFFFFFFFUL;
    for (i = 0; i < size; i++)
        crc = table[(crc ^ p[i]) & 0xFFu] ^ (crc >> 8);

    return crc ^ 0xFFFFFFFFUL;
}

int jzFilenameIsSafe(const char *filename)
{
    const char *p = filename;
    const char *component = filename;
    size_t componentLength = 0;

    if (p == NULL || *p == '\0' || *p == '/' || *p == '\\')
        return 0;

    if (((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z')) &&
            p[1] == ':')
        return 0;

    for (; *p != '\0'; p++) {
        if ((unsigned char)*p < 32u || (unsigned char)*p == 0x7Fu || *p == ':')
            return 0;

        if (*p == '/' || *p == '\\') {
            if (componentLength == 0)
                return 0;
            if ((componentLength == 1u && component[0] == '.') ||
                    (componentLength == 2u && component[0] == '.' &&
                     component[1] == '.'))
                return 0;
            component = p + 1;
            componentLength = 0;
        } else {
            componentLength++;
        }
    }

    /* A single trailing separator is a valid directory entry. */
    if (componentLength == 0)
        return p > filename && (p[-1] == '/' || p[-1] == '\\');

    if ((componentLength == 1u && component[0] == '.') ||
            (componentLength == 2u && component[0] == '.' &&
             component[1] == '.'))
        return 0;

    return 1;
}

static int jzValidateEntryFlags(uint16_t flags, uint16_t method)
{
    uint16_t allowed = 0x0808u; /* UTF-8 names and data descriptors */

    if (flags & 0x0001u)
        return JZ_ERR_UNSUPPORTED; /* encrypted */

    if (method == 8u)
        allowed |= 0x0006u;        /* deflate compression options */
    else if (method != 0u)
        return JZ_ERR_UNSUPPORTED;

    if (flags & (uint16_t)~allowed)
        return JZ_ERR_UNSUPPORTED;

    return JZ_OK;
}

/*
 * Decode a 22-byte end-record image (already validated for position and
 * comment length) into host-endian form, then apply layout checks.
 */
/* Byte j of the scan window W = scan[0..want) ++ tail[0..tailLen).
 * Every window byte goes through here, so no pointer crosses an array end. */
static unsigned jzWindowByte(const unsigned char *scan, size_t want,
        const unsigned char *tail, size_t j)
{
    return (j < want) ? (unsigned)scan[j] : (unsigned)tail[j - want];
}

static int jzEndRecordFromImage(const unsigned char *r, JZEndRecord *out)
{
    out->signature = jz_rd32(r + 0);
    out->diskNumber = jz_rd16(r + 4);
    out->centralDirectoryDiskNumber = jz_rd16(r + 6);
    out->numEntriesThisDisk = jz_rd16(r + 8);
    out->numEntries = jz_rd16(r + 10);
    out->centralDirectorySize = jz_rd32(r + 12);
    out->centralDirectoryOffset = jz_rd32(r + 16);
    out->zipCommentLength = jz_rd16(r + 20);
    return JZ_OK;
}

int jzReadEndRecord(JZFile *zip, JZEndRecord *endRecord, JZScratch *s)
{
    JZOffset fileSize, base, floorPos;
    unsigned char tail[JZ_TAIL_OVERLAP];
    size_t tailLen = 0;
    int found = 0;
    JZOffset foundPos = 0;

    if (zip == NULL || endRecord == NULL || s == NULL ||
            zip->seek == NULL || zip->tell == NULL || zip->read == NULL)
        return JZ_ERR;

    if (zip->seek(zip, 0, SEEK_END))
        return JZ_ERR;

    if (zip->tell(zip, &fileSize) || fileSize < (JZOffset)JZ_END_RAW)
        return JZ_ERR_CORRUPT; /* too small even for an empty archive */

    /* A record with a maximal comment cannot start earlier than this. */
    floorPos = (fileSize > (JZOffset)(JZ_END_RAW + JZ_MAX_COMMENT))
        ? fileSize - (JZOffset)(JZ_END_RAW + JZ_MAX_COMMENT)
        : 0;

    base = fileSize;
    for (;;) {
        size_t want = (base > (JZOffset)JZ_SCAN_CHUNK)
            ? (size_t)JZ_SCAN_CHUNK : (size_t)base;
        size_t span = want + tailLen;
        size_t i;

        if (zip->seek(zip, (int64_t)(base - (JZOffset)want), SEEK_SET))
            return JZ_ERR;
        if (zip->read(zip, s->scan, want) < want)
            return JZ_ERR;
        base -= (JZOffset)want;

        /*
         * Window W = scan[0..want) ++ tail[0..tailLen): the chunk just read,
         * followed by the first bytes of the previous (higher) window, which
         * sit directly above it in the file. Absolute position of W[i] is
         * (base + i). A record needs 22 bytes, so i stops at span - 22.
         */
        if (span >= (size_t)JZ_END_RAW) {
            for (i = span - ((size_t)JZ_END_RAW - 1u); i-- > 0; ) {
                JZOffset candPos = base + (JZOffset)i;
                uint16_t comment;

                if (jzWindowByte(s->scan, want, tail, i) != 0x50u ||
                        jzWindowByte(s->scan, want, tail, i + 1u) != 0x4Bu ||
                        jzWindowByte(s->scan, want, tail, i + 2u) != 0x05u ||
                        jzWindowByte(s->scan, want, tail, i + 3u) != 0x06u)
                    continue;
                if (candPos < floorPos)
                    continue;

                comment = (uint16_t)(jzWindowByte(s->scan, want, tail, i + 20u) |
                        (jzWindowByte(s->scan, want, tail, i + 21u) << 8));
                if (candPos + (JZOffset)JZ_END_RAW + (JZOffset)comment ==
                        fileSize) {
                    found = 1;
                    foundPos = candPos;
                    break;
                }
            }
        }

        /* Below floorPos no legal record can start: stop (bounded work). */
        if (found || base == 0 || base <= floorPos)
            break;

        /* Carry the FIRST JZ_TAIL_OVERLAP bytes of W: they sit directly
         * above the next (lower) chunk and complete any straddling record. */
        {
            unsigned char next[JZ_TAIL_OVERLAP];
            size_t keep = span < JZ_TAIL_OVERLAP ? span : JZ_TAIL_OVERLAP;
            size_t k;
            for (k = 0; k < keep; k++)
                next[k] = (unsigned char)jzWindowByte(s->scan, want, tail, k);
            memcpy(tail, next, keep);
            tailLen = keep;
        }
    }

    if (!found)
        return JZ_ERR_CORRUPT;

    /* Decode from the file directly at the validated position: bounded,
     * deterministic, and independent of the scan window state. */
    {
        unsigned char raw[JZ_END_RAW];
        if (zip->seek(zip, (int64_t)foundPos, SEEK_SET))
            return JZ_ERR;
        if (zip->read(zip, raw, JZ_END_RAW) < JZ_END_RAW)
            return JZ_ERR;
        jzEndRecordFromImage(raw, endRecord);
    }

    if (endRecord->signature != JZ_SIG_END)
        return JZ_ERR_CORRUPT; /* re-read bytes must still be a record */

    if (endRecord->diskNumber || endRecord->centralDirectoryDiskNumber ||
            endRecord->numEntries != endRecord->numEntriesThisDisk)
        return JZ_ERR_UNSUPPORTED; /* multi-disk */

    if (endRecord->numEntries == 0xFFFFu ||
            endRecord->centralDirectoryOffset == 0xFFFFFFFFu ||
            endRecord->centralDirectorySize == 0xFFFFFFFFu)
        return JZ_ERR_UNSUPPORTED; /* ZIP64 */

    /* 64-bit layout math: the 32-bit sum can wrap and bypass the check. */
    if ((uint64_t)endRecord->centralDirectoryOffset +
            (uint64_t)endRecord->centralDirectorySize >
            (uint64_t)foundPos)
        return JZ_ERR_CORRUPT; /* directory overlaps/extends past record */

    if ((uint64_t)endRecord->numEntries * (uint64_t)JZ_CENTRAL_RAW >
            (uint64_t)endRecord->centralDirectorySize)
        return JZ_ERR_CORRUPT; /* entry count cannot fit the directory */

    return JZ_OK;
}

int jzReadCentralDirectory(JZFile *zip, JZEndRecord *endRecord,
        JZRecordCallback callback, void *user_data, JZScratch *s)
{
    uint64_t remaining;
    int i, keepGoing;

    if (zip == NULL || endRecord == NULL || callback == NULL || s == NULL ||
            zip->seek == NULL || zip->read == NULL)
        return JZ_ERR;

    remaining = endRecord->centralDirectorySize;

    if (zip->seek(zip, (int64_t)endRecord->centralDirectoryOffset, SEEK_SET))
        return JZ_ERR;

    for (i = 0; i < (int)endRecord->numEntries; i++) {
        JZFileHeader h;
        uint16_t nameLen, extraLen, commentLen;

        if (remaining < (uint64_t)JZ_CENTRAL_RAW)
            return JZ_ERR_CORRUPT;
        if (zip->read(zip, s->scan, JZ_CENTRAL_RAW) < JZ_CENTRAL_RAW)
            return JZ_ERR;
        remaining -= (uint64_t)JZ_CENTRAL_RAW;

        if (jz_rd32(s->scan + 0) != JZ_SIG_CENTRAL)
            return JZ_ERR_CORRUPT;

        h.versionNeededToExtract = jz_rd16(s->scan + 6);
        h.generalPurposeBitFlag = jz_rd16(s->scan + 8);
        h.compressionMethod = jz_rd16(s->scan + 10);
        h.lastModFileTime = jz_rd16(s->scan + 12);
        h.lastModFileDate = jz_rd16(s->scan + 14);
        h.crc32 = jz_rd32(s->scan + 16);
        h.compressedSize = jz_rd32(s->scan + 20);
        h.uncompressedSize = jz_rd32(s->scan + 24);
        nameLen = jz_rd16(s->scan + 28);
        extraLen = jz_rd16(s->scan + 30);
        commentLen = jz_rd16(s->scan + 32);
        h.offset = jz_rd32(s->scan + 42);

        if (h.versionNeededToExtract > 20u)
            return JZ_ERR_UNSUPPORTED;
        if (h.compressedSize == 0xFFFFFFFFu ||
                h.uncompressedSize == 0xFFFFFFFFu ||
                h.offset == 0xFFFFFFFFu)
            return JZ_ERR_UNSUPPORTED; /* ZIP64 entry */
        if (jz_rd16(s->scan + 34) != 0u)
            return JZ_ERR_UNSUPPORTED; /* multi-disk entry */
        if ((JZOffset)h.offset >=
                (JZOffset)endRecord->centralDirectoryOffset)
            return JZ_ERR_CORRUPT; /* local header overlaps metadata */
        if (jzValidateEntryFlags(h.generalPurposeBitFlag,
                    h.compressionMethod) != JZ_OK)
            return JZ_ERR_UNSUPPORTED;

        if ((uint64_t)nameLen + (uint64_t)extraLen +
                (uint64_t)commentLen > remaining)
            return JZ_ERR_CORRUPT;
        if ((size_t)nameLen + 1u > (size_t)JZ_FILENAME_CAP)
            return JZ_ERR_SIZE; /* deliberate fixed cap on entry names */

        if (nameLen > 0u &&
                zip->read(zip, s->filename, nameLen) < (size_t)nameLen)
            return JZ_ERR;
        remaining -= (uint64_t)nameLen;

        if (memchr(s->filename, 0, nameLen) != NULL)
            return JZ_ERR_CORRUPT; /* embedded NUL */
        s->filename[nameLen] = '\0';

        if (zip->seek(zip, (int64_t)extraLen, SEEK_CUR) ||
                zip->seek(zip, (int64_t)commentLen, SEEK_CUR))
            return JZ_ERR;
        remaining -= (uint64_t)extraLen + (uint64_t)commentLen;

        keepGoing = callback(zip, i, &h, (const char *)s->filename,
                user_data);
        if (!keepGoing)
            break;
    }

    /* Early stop is success; full iteration must consume the directory
     * exactly. */
    if (i == (int)endRecord->numEntries && remaining != 0u)
        return JZ_ERR_CORRUPT;

    return JZ_OK;
}

int jzReadLocalFileHeaderChecked(JZFile *zip,
        const JZFileHeader *centralHeader, const char *centralFilename,
        JZScratch *s)
{
    uint16_t version, flags, method, nameLen, extraLen;
    uint32_t crc, csize, usize;
    size_t centralLen;

    if (zip == NULL || centralHeader == NULL || centralFilename == NULL ||
            s == NULL || zip->read == NULL || zip->seek == NULL)
        return JZ_ERR;

    centralLen = strlen(centralFilename);
    if (centralLen >= (size_t)JZ_FILENAME_CAP)
        return JZ_ERR_CORRUPT;

    if (zip->read(zip, s->scan, JZ_LOCAL_RAW) < JZ_LOCAL_RAW)
        return JZ_ERR;

    if (jz_rd32(s->scan + 0) != JZ_SIG_LOCAL)
        return JZ_ERR_CORRUPT;

    version = jz_rd16(s->scan + 4);
    flags = jz_rd16(s->scan + 6);
    method = jz_rd16(s->scan + 8);
    crc = jz_rd32(s->scan + 14);
    csize = jz_rd32(s->scan + 18);
    usize = jz_rd32(s->scan + 22);
    nameLen = jz_rd16(s->scan + 26);
    extraLen = jz_rd16(s->scan + 28);

    if (version > 20u)
        return JZ_ERR_UNSUPPORTED;
    if (jzValidateEntryFlags(flags, method) != JZ_OK)
        return JZ_ERR_UNSUPPORTED;
    if (csize == 0xFFFFFFFFu || usize == 0xFFFFFFFFu)
        return JZ_ERR_UNSUPPORTED; /* ZIP64 */
    if (method == 0u && csize != usize)
        return JZ_ERR_CORRUPT; /* store with mismatched sizes */
    if ((size_t)nameLen + 1u > (size_t)JZ_FILENAME_CAP)
        return JZ_ERR_SIZE;

    /* Compare the local name against the central name in pieces through
     * s->scan. s->filename is never written here: callers commonly pass
     * s->filename itself as centralFilename, and overwriting it would make
     * the comparison check the buffer against itself. */
    if ((size_t)nameLen != centralLen)
        return JZ_ERR_CORRUPT;
    {
        size_t done = 0;
        while (done < centralLen) {
            size_t n = centralLen - done;
            if (n > (size_t)JZ_SCAN_CHUNK)
                n = (size_t)JZ_SCAN_CHUNK;
            if (zip->read(zip, s->scan, n) < n)
                return JZ_ERR;
            if (memchr(s->scan, 0, n) != NULL)
                return JZ_ERR_CORRUPT; /* embedded NUL */
            if (memcmp(s->scan, centralFilename + done, n) != 0)
                return JZ_ERR_CORRUPT;
            done += n;
        }
    }

    if (zip->seek(zip, (int64_t)extraLen, SEEK_CUR))
        return JZ_ERR;

    /* Verification only: central values are never modified. */
    if (version != centralHeader->versionNeededToExtract ||
            flags != centralHeader->generalPurposeBitFlag ||
            method != centralHeader->compressionMethod)
        return JZ_ERR_CORRUPT;

    if (flags & 0x08u) {
        /* Bit 3: zero local CRC/sizes are legitimate (values live in the
         * descriptor); nonzero values must agree with central. */
        if ((crc != 0u && crc != centralHeader->crc32) ||
                (csize != 0u && csize != centralHeader->compressedSize) ||
                (usize != 0u && usize != centralHeader->uncompressedSize))
            return JZ_ERR_CORRUPT;
    } else {
        if (crc != centralHeader->crc32 ||
                csize != centralHeader->compressedSize ||
                usize != centralHeader->uncompressedSize)
            return JZ_ERR_CORRUPT;
    }

    return JZ_OK;
}

/*
 * Verify a trailing data descriptor (bit 3) against the values that bounded
 * the read. The descriptor is a checksum-like trailer, never a source of
 * new truth; the caller's header is const and untouched.
 */
static int jzVerifyDataDescriptor(JZFile *zip, const JZFileHeader *header,
        JZScratch *s)
{
    uint32_t sigOrCrc, crc, csize, usize;

    if (zip->read(zip, s->scan, 4u) < 4u)
        return JZ_ERR;
    sigOrCrc = jz_rd32(s->scan);

    if (sigOrCrc == JZ_SIG_DDESC) {
        if (zip->read(zip, s->scan, 4u) < 4u)
            return JZ_ERR;
        crc = jz_rd32(s->scan);
    } else {
        crc = sigOrCrc; /* signatureless descriptor: first word was CRC */
    }

    if (zip->read(zip, s->scan, 4u) < 4u)
        return JZ_ERR;
    csize = jz_rd32(s->scan);
    if (zip->read(zip, s->scan, 4u) < 4u)
        return JZ_ERR;
    usize = jz_rd32(s->scan);

    if (crc != header->crc32 || csize != header->compressedSize ||
            usize != header->uncompressedSize)
        return JZ_ERR_CORRUPT;

    return JZ_OK;
}

int jzReadDataLimited(JZFile *zip, const JZFileHeader *header, void *buffer,
        size_t bufferSize, uint32_t compressedSizeLimit,
        uint32_t *bytesProduced, JZScratch *s)
{
    int hasDataDescriptor;

    if (zip == NULL || header == NULL || bytesProduced == NULL ||
            s == NULL || zip->read == NULL || zip->error == NULL ||
            (buffer == NULL && bufferSize > 0u))
        return JZ_ERR;

    hasDataDescriptor = (header->generalPurposeBitFlag & 0x08u) != 0;
    *bytesProduced = 0;

    if (jzValidateEntryFlags(header->generalPurposeBitFlag,
                header->compressionMethod) != JZ_OK)
        return JZ_ERR_UNSUPPORTED;
    if (header->compressedSize == 0xFFFFFFFFu ||
            header->uncompressedSize == 0xFFFFFFFFu)
        return JZ_ERR_UNSUPPORTED; /* ZIP64 */

    /* Hard output bound: never produce more than the caller's buffer. */
    if ((uint64_t)header->uncompressedSize > (uint64_t)bufferSize)
        return JZ_ERR_SIZE;
    /* Independent input bound, caller-adjustable per context. */
    if (header->compressedSize > compressedSizeLimit)
        return JZ_ERR_SIZE;

    if (header->compressionMethod == 0u) { /* store */
        if (header->compressedSize != header->uncompressedSize)
            return JZ_ERR_CORRUPT;
        if (header->uncompressedSize > 0u &&
                (zip->read(zip, buffer, header->uncompressedSize) <
                 header->uncompressedSize || zip->error(zip)))
            return JZ_ERR;
        *bytesProduced = header->uncompressedSize;
    } else if (header->compressionMethod == 8u) { /* deflate */
        if (header->compressedSize == 0u)
            return JZ_ERR_CORRUPT;
#ifdef HAVE_ZLIB
        {
            z_stream strm;
            unsigned char dummyOut;
            uint32_t remainingIn = header->compressedSize;
            int zret = Z_OK;

            memset(&strm, 0, sizeof(strm));
            if (inflateInit2(&strm, -MAX_WBITS) != Z_OK)
                return JZ_ERR;

            strm.next_out = header->uncompressedSize > 0u
                ? (unsigned char *)buffer : &dummyOut;
            strm.avail_out = header->uncompressedSize > 0u
                ? (uInt)header->uncompressedSize : 1u;

            while (remainingIn > 0u) {
                size_t chunk = remainingIn < (uint32_t)JZ_IN_CHUNK
                    ? (size_t)remainingIn : (size_t)JZ_IN_CHUNK;

                if (zip->read(zip, s->inbuf, chunk) < chunk ||
                        zip->error(zip)) {
                    inflateEnd(&strm);
                    return JZ_ERR;
                }
                remainingIn -= (uint32_t)chunk;

                strm.next_in = s->inbuf;
                strm.avail_in = (uInt)chunk;

                zret = inflate(&strm, Z_NO_FLUSH);
                if (zret == Z_STREAM_END)
                    break;
                if (zret != Z_OK ||
                        (header->uncompressedSize == 0u
                            ? strm.total_out != 0u
                            : strm.avail_out == 0u)) {
                    /* corrupt stream, or more output than claimed */
                    inflateEnd(&strm);
                    return JZ_ERR_CORRUPT;
                }
            }

            /* Exact termination: no truncation, no trailing garbage. */
            if (zret != Z_STREAM_END ||
                    strm.total_out != header->uncompressedSize ||
                    strm.total_in != header->compressedSize) {
                inflateEnd(&strm);
                return JZ_ERR_CORRUPT;
            }

            inflateEnd(&strm);
            *bytesProduced = header->uncompressedSize;
        }
#else
#ifdef HAVE_PUFF
        {
            unsigned long destlen, sourcelen;
            int ret;

            /* Whole-input backend: bound the archive read by fixed cap. */
            if (header->compressedSize > (uint32_t)JZ_COMP_CAP)
                return JZ_ERR_SIZE;

            if (zip->read(zip, s->compbuf, header->compressedSize) <
                    header->compressedSize || zip->error(zip))
                return JZ_ERR;

            destlen = header->uncompressedSize;
            sourcelen = header->compressedSize;
            ret = puff((unsigned char *)buffer, &destlen,
                    s->compbuf, &sourcelen);
            if (ret != 0 || destlen != header->uncompressedSize ||
                    sourcelen != header->compressedSize)
                return JZ_ERR_CORRUPT; /* bad stream / size mismatch / junk */

            *bytesProduced = header->uncompressedSize;
        }
#else
        return JZ_ERR_UNSUPPORTED; /* no decompression backend compiled in */
#endif
#endif
    } else {
        return JZ_ERR_UNSUPPORTED;
    }

    if (hasDataDescriptor &&
            jzVerifyDataDescriptor(zip, header, s) != JZ_OK)
        return JZ_ERR_CORRUPT;

    /* Integrity: produced bytes must match the declared CRC. CRC of an
     * empty payload is 0, handled without dereferencing a NULL buffer. */
    {
        uint32_t actual = (*bytesProduced == 0u)
            ? 0u
            : jz_crc32(0u, buffer, *bytesProduced);
        if (actual != header->crc32)
            return JZ_ERR_CORRUPT;
    }

    return JZ_OK;
}

int jzReadData(JZFile *zip, const JZFileHeader *header, void *buffer,
        size_t bufferSize, uint32_t *bytesProduced, JZScratch *scratch)
{
    return jzReadDataLimited(zip, header, buffer, bufferSize, 0xFFFFFFFFu,
            bytesProduced, scratch);
}

/* ---- stdio adapter with caller-owned storage ---- */

static size_t jz_stdio_read(JZFile *file, void *buf, size_t size)
{
    JZStdioFile *h = (JZStdioFile *)file;
    return fread(buf, 1, size, h->fp);
}

static int jz_stdio_tell(JZFile *file, JZOffset *offset)
{
    JZStdioFile *h = (JZStdioFile *)file;
    JZOffset position;

    if (offset == NULL)
        return -1;
#ifdef _WIN32
    position = (JZOffset)_ftelli64(h->fp);
    if (position == (JZOffset)-1)
        return -1;
#else
    {
        long p = ftell(h->fp); /* ISO C99; tanks are far below LONG_MAX */
        if (p < 0L)
            return -1;
        position = (JZOffset)p;
    }
#endif
    *offset = position;
    return 0;
}

static int jz_stdio_seek(JZFile *file, int64_t offset, int whence)
{
    JZStdioFile *h = (JZStdioFile *)file;
#ifdef _WIN32
    return _fseeki64(h->fp, offset, whence);
#else
    {
        /* Range-check first: converting an out-of-range value is
         * implementation-defined in C99, so refuse instead of converting. */
        if (offset < (int64_t)LONG_MIN || offset > (int64_t)LONG_MAX)
            return -1;
        return fseek(h->fp, (long)offset, whence);
    }
#endif
}

static int jz_stdio_error(JZFile *file)
{
    JZStdioFile *h = (JZStdioFile *)file;
    return ferror(h->fp);
}

static void jz_stdio_close(JZFile *file)
{
    JZStdioFile *h = (JZStdioFile *)file;
    fclose(h->fp); /* storage itself remains caller-owned */
}

JZFile *jzfile_stdio_init(JZStdioFile *storage, FILE *fp)
{
    if (storage == NULL || fp == NULL)
        return NULL;

    storage->handle.read = jz_stdio_read;
    storage->handle.tell = jz_stdio_tell;
    storage->handle.seek = jz_stdio_seek;
    storage->handle.error = jz_stdio_error;
    storage->handle.close = jz_stdio_close;
    storage->fp = fp;

    return &storage->handle;
}
