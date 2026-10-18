/*
 * base64_decode.c -- local Base64 decoder, ISO C99, version 1.1.0.
 * Original implementation of RFC 4648 sections 3.5, 4 and 5.
 *
 * Build: cc -std=c99 -pedantic -Wall -Wextra -Werror -O2 \
 *           base64_decode.c -o base64_decode
 * Run:   ./base64_decode                 (input.b64 -> decoded.bin)
 *        ./base64_decode source.b64 result.bin
 *        ./base64_decode --url --unpadded source.b64 result.bin
 *
 * Define B64_NO_MAIN to include just the reusable buffer decoder.
 * Core: caller-owned buffers, fixed local state, zero allocator calls.
 * Runner: static buffers and hosted C99 file I/O. No external packages.
 * Input is ASCII bytes; output is binary bytes, without a string terminator.
 */

#include <limits.h>
#include <stddef.h>

#if CHAR_BIT != 8
#error "This decoder requires 8-bit bytes."
#endif

#define B64_F_URL        1U
#define B64_F_WHITESPACE 2U
#define B64_F_UNPADDED   4U
#define B64_F_AUTO       8U
#define B64_ALL_FLAGS (B64_F_URL | B64_F_WHITESPACE | B64_F_UNPADDED | B64_F_AUTO)

typedef enum {
    B64_OK = 0,
    B64_BAD_ARGUMENT,
    B64_BAD_CHARACTER,
    B64_BAD_PADDING,
    B64_BAD_LENGTH,
    B64_NONZERO_PAD_BITS,
    B64_OUTPUT_LIMIT,
    B64_MIXED_ALPHABETS
} B64Status;

typedef struct {
    B64Status status;
    size_t size;         /* Decoded length on success; zero on failure. */
    size_t error_offset; /* Zero-based input byte; input_size at EOF. */
} B64Result;

static B64Result b64_result(B64Status status, size_t size, size_t offset)
{
    B64Result result;
    result.status = status;
    result.size = size;
    result.error_offset = offset;
    return result;
}

static int b64_value(unsigned char byte, unsigned flags)
{
    if (byte >= 0x41U && byte <= 0x5AU) {
        return (int)(byte - 0x41U);
    }
    if (byte >= 0x61U && byte <= 0x7AU) {
        return (int)(byte - 0x61U) + 26;
    }
    if (byte >= 0x30U && byte <= 0x39U) {
        return (int)(byte - 0x30U) + 52;
    }
    if ((flags & (B64_F_URL | B64_F_AUTO)) != 0U) {
        if (byte == 0x2DU) { return 62; }
        if (byte == 0x5FU) { return 63; }
    }
    if ((flags & B64_F_URL) == 0U) {
        if (byte == 0x2BU) { return 62; }
        if (byte == 0x2FU) { return 63; }
    }
    return -1;
}

static int b64_whitespace(unsigned char byte)
{
    return byte == 0x20U || (byte >= 0x09U && byte <= 0x0DU);
}

/* A custom alphabet is an explicit ordered map for sextet values 0..63. */
static int b64_alphabet_valid(const unsigned char *alphabet, size_t size)
{
    size_t first;
    size_t second;
    if (alphabet == NULL || size != 64U) { return 0; }
    for (first = 0U; first < size; ++first) {
        if (alphabet[first] < 0x21U || alphabet[first] > 0x7EU ||
            alphabet[first] == 0x3DU) { return 0; }
        for (second = 0U; second < first; ++second) {
            if (alphabet[first] == alphabet[second]) { return 0; }
        }
    }
    return 1;
}

static int b64_custom_value(unsigned char byte, const unsigned char *alphabet)
{
    unsigned value;
    for (value = 0U; value < 64U; ++value) {
        if (alphabet[value] == byte) { return (int)value; }
    }
    return -1;
}

/* output == NULL selects validation/counting. The emitting pass is run
 * only after complete validation and a capacity check in b64_decode(). */
static B64Result b64_scan(const unsigned char *input, size_t input_size,
                          unsigned char *output, unsigned flags,
                          const unsigned char *alphabet)
{
    unsigned group[4] = {0U, 0U, 0U, 0U};
    size_t position[4] = {0U, 0U, 0U, 0U};
    size_t index;
    size_t written = 0U;
    unsigned count = 0U;
    unsigned finished = 0U;
    unsigned family = 0U;

    for (index = 0U; index < input_size; ++index) {
        unsigned char byte = input[index];
        int value;
        unsigned bytes;

        if (b64_whitespace(byte) && (flags & B64_F_WHITESPACE) != 0U) {
            continue;
        }
        if (finished != 0U) {
            return b64_result(B64_BAD_PADDING, 0U, index);
        }
        if (byte == 0x3DU) {
            if (count < 2U) {
                return b64_result(B64_BAD_PADDING, 0U, index);
            }
            value = 64;
        } else {
            value = alphabet != NULL ? b64_custom_value(byte, alphabet) :
                                      b64_value(byte, flags);
            if (value < 0) {
                return b64_result(B64_BAD_CHARACTER, 0U, index);
            }
            if (alphabet == NULL && (flags & B64_F_AUTO) != 0U) {
                if (byte == 0x2BU || byte == 0x2FU) { family |= 1U; }
                if (byte == 0x2DU || byte == 0x5FU) { family |= 2U; }
                if (family == 3U) {
                    return b64_result(B64_MIXED_ALPHABETS, 0U, index);
                }
            }
            if (count == 3U && group[2] == 64U) {
                return b64_result(B64_BAD_PADDING, 0U, index);
            }
        }
        group[count] = (unsigned)value;
        position[count] = index;
        ++count;
        if (count != 4U) {
            continue;
        }

        if (group[2] == 64U) {
            if (group[3] != 64U) {
                return b64_result(B64_BAD_PADDING, 0U, position[3]);
            }
            if ((group[1] & 15U) != 0U) {
                return b64_result(B64_NONZERO_PAD_BITS, 0U, position[1]);
            }
            bytes = 1U;
            finished = 1U;
        } else if (group[3] == 64U) {
            if ((group[2] & 3U) != 0U) {
                return b64_result(B64_NONZERO_PAD_BITS, 0U, position[2]);
            }
            bytes = 2U;
            finished = 1U;
        } else {
            bytes = 3U;
        }

        if (output != NULL) {
            output[written] = (unsigned char)((group[0] << 2U) |
                                              (group[1] >> 4U));
            if (bytes >= 2U) {
                output[written + 1U] = (unsigned char)((group[1] << 4U) |
                                                       (group[2] >> 2U));
            }
            if (bytes == 3U) {
                output[written + 2U] = (unsigned char)((group[2] << 6U) |
                                                       group[3]);
            }
        }
        written += bytes;
        count = 0U;
    }

    if (count != 0U) {
        if (count == 3U && group[2] == 64U) {
            return b64_result(B64_BAD_PADDING, 0U, input_size);
        }
        if (count == 1U || (flags & B64_F_UNPADDED) == 0U) {
            return b64_result(B64_BAD_LENGTH, 0U, input_size);
        }
        if (count == 2U && (group[1] & 15U) != 0U) {
            return b64_result(B64_NONZERO_PAD_BITS, 0U, position[1]);
        }
        if (count == 3U && (group[2] & 3U) != 0U) {
            return b64_result(B64_NONZERO_PAD_BITS, 0U, position[2]);
        }
        if (output != NULL) {
            output[written] = (unsigned char)((group[0] << 2U) |
                                              (group[1] >> 4U));
            if (count == 3U) {
                output[written + 1U] = (unsigned char)((group[1] << 4U) |
                                                       (group[2] >> 2U));
            }
        }
        written += count - 1U;
    }
    return b64_result(B64_OK, written, input_size);
}

/* Validate and measure without accessing an output buffer. */
B64Result b64_measure(const unsigned char *input, size_t input_size,
                      unsigned flags)
{
    if ((input == NULL && input_size != 0U) ||
        (flags & ~B64_ALL_FLAGS) != 0U ||
        (flags & (B64_F_URL | B64_F_AUTO)) == (B64_F_URL | B64_F_AUTO)) {
        return b64_result(B64_BAD_ARGUMENT, 0U, 0U);
    }
    return b64_scan(input, input_size, NULL, flags, NULL);
}

/* Input must remain stable for both passes; buffers must not overlap.
 * Failure leaves the output buffer unchanged. NULL with size zero is valid. */
B64Result b64_decode(const unsigned char *input, size_t input_size,
                     unsigned char *output, size_t output_capacity,
                     unsigned flags)
{
    B64Result result;
    if (output == NULL && output_capacity != 0U) {
        return b64_result(B64_BAD_ARGUMENT, 0U, 0U);
    }
    result = b64_measure(input, input_size, flags);
    if (result.status != B64_OK) {
        return result;
    }
    if (result.size > output_capacity) {
        return b64_result(B64_OUTPUT_LIMIT, 0U, input_size);
    }
    if (result.size == 0U) {
        return result;
    }
    return b64_scan(input, input_size, output, flags, NULL);
}

/* Custom alphabets use RFC-style most-significant-bit-first packing.
 * Alphabet bytes must be 64 distinct printable ASCII symbols, excluding '='.
 * Flags may select whitespace/unpadded policy. URL/AUTO flags are incompatible.
 * Buffer lifetime, stability, non-overlap and failure guarantees are the same. */
B64Result b64_measure_custom(const unsigned char *input, size_t input_size,
                             unsigned flags, const unsigned char *alphabet,
                             size_t alphabet_size)
{
    if ((input == NULL && input_size != 0U) ||
        (flags & ~(B64_F_WHITESPACE | B64_F_UNPADDED)) != 0U ||
        !b64_alphabet_valid(alphabet, alphabet_size)) {
        return b64_result(B64_BAD_ARGUMENT, 0U, 0U);
    }
    return b64_scan(input, input_size, NULL, flags, alphabet);
}

B64Result b64_decode_custom(const unsigned char *input, size_t input_size,
                            unsigned char *output, size_t output_capacity,
                            unsigned flags, const unsigned char *alphabet,
                            size_t alphabet_size)
{
    B64Result result;
    if (output == NULL && output_capacity != 0U) {
        return b64_result(B64_BAD_ARGUMENT, 0U, 0U);
    }
    result = b64_measure_custom(input, input_size, flags, alphabet, alphabet_size);
    if (result.status != B64_OK) { return result; }
    if (result.size > output_capacity) {
        return b64_result(B64_OUTPUT_LIMIT, 0U, input_size);
    }
    if (result.size == 0U) { return result; }
    return b64_scan(input, input_size, output, flags, alphabet);
}

const char *b64_status_text(B64Status status)
{
    switch (status) {
        case B64_OK:               return "OK";
        case B64_BAD_ARGUMENT:     return "invalid pointer, size, or flags";
        case B64_BAD_CHARACTER:    return "invalid Base64 character";
        case B64_BAD_PADDING:      return "misplaced or incomplete padding";
        case B64_BAD_LENGTH:       return "incomplete group or missing padding";
        case B64_NONZERO_PAD_BITS: return "nonzero unused padding bits";
        case B64_OUTPUT_LIMIT:     return "BENCH: output capacity exceeded";
        case B64_MIXED_ALPHABETS:  return "mixed standard and URL-safe alphabets";
        default:                   return "unknown status";
    }
}

#ifndef B64_NO_MAIN

#include <stdio.h>
#include <string.h>

/* Presets can be supplied with compiler -D options; source edits are optional. */
#ifndef B64_INPUT_CAPACITY
#define B64_INPUT_CAPACITY 1048576UL
#endif
#ifndef B64_OUTPUT_CAPACITY
#define B64_OUTPUT_CAPACITY 786432UL
#endif
#ifndef B64_PRESET_INPUT
#define B64_PRESET_INPUT "input.b64"
#endif
#ifndef B64_PRESET_OUTPUT
#define B64_PRESET_OUTPUT "decoded.bin"
#endif
#ifndef B64_PRESET_FLAGS
#define B64_PRESET_FLAGS (B64_F_AUTO | B64_F_WHITESPACE | B64_F_UNPADDED)
#endif
#ifndef B64_PRESET_ALPHABET
#define B64_PRESET_ALPHABET NULL
#endif

#if B64_INPUT_CAPACITY < 1 || B64_OUTPUT_CAPACITY < 1
#error "Preset capacities must be positive."
#endif

static unsigned char b64_input_buffer[B64_INPUT_CAPACITY];
static unsigned char b64_output_buffer[B64_OUTPUT_CAPACITY];

static void b64_help(const char *program)
{
    printf("Usage: %s [OPTIONS] [--] [INPUT [OUTPUT]]\n"
           "Defaults: %s -> %s\n"
           "Limits: input %zu bytes; output %zu bytes.\n"
           "Default: auto standard/URL alphabet; padded/unpadded; whitespace allowed.\n"
           "--auto             Auto standard/URL alphabet (reject mixtures).\n"
           "--standard         Select standard alphabet (+ and /).\n"
           "--url              Select URL-safe alphabet (- and _).\n"
           "--alphabet TABLE   Select 64 ordered symbols for a custom alphabet.\n"
           "--unpadded         Accept a final group without equals padding.\n"
           "--require-padding  Require equals padding for incomplete groups.\n"
           "--strict           Reject all whitespace.\n"
           "--                 Treat following arguments as file paths.\n"
           "Output is binary; an existing output file is replaced after validation.\n",
           program, B64_PRESET_INPUT, B64_PRESET_OUTPUT,
           sizeof b64_input_buffer, sizeof b64_output_buffer);
}

int main(int argc, char **argv)
{
    const char *input_path = B64_PRESET_INPUT;
    const char *output_path = B64_PRESET_OUTPUT;
    unsigned flags = B64_PRESET_FLAGS;
    const unsigned char *alphabet = (const unsigned char *)B64_PRESET_ALPHABET;
    unsigned paths = 0U;
    unsigned options = 1U;
    int argument;
    FILE *file;
    size_t input_size;
    size_t output_size;
    int extra;
    int io_failed;
    B64Result result;

    if (alphabet != NULL) { flags &= ~(B64_F_AUTO | B64_F_URL); }

    for (argument = 1; argument < argc; ++argument) {
        const char *value = argv[argument];
        if (options != 0U && strcmp(value, "--") == 0) {
            options = 0U;
        } else if (options != 0U && strcmp(value, "--help") == 0) {
            b64_help(argv[0]);
            return 0;
        } else if (options != 0U && strcmp(value, "--url") == 0) {
            alphabet = NULL;
            flags &= ~B64_F_AUTO;
            flags |= B64_F_URL;
        } else if (options != 0U && strcmp(value, "--standard") == 0) {
            alphabet = NULL;
            flags &= ~(B64_F_AUTO | B64_F_URL);
        } else if (options != 0U && strcmp(value, "--auto") == 0) {
            alphabet = NULL;
            flags &= ~B64_F_URL;
            flags |= B64_F_AUTO;
        } else if (options != 0U && strcmp(value, "--alphabet") == 0) {
            if (argument + 1 >= argc) {
                fprintf(stderr, "ERROR: --alphabet requires a 64-symbol table.\n");
                return 2;
            }
            ++argument;
            alphabet = (const unsigned char *)argv[argument];
            flags &= ~(B64_F_AUTO | B64_F_URL);
        } else if (options != 0U && strcmp(value, "--unpadded") == 0) {
            flags |= B64_F_UNPADDED;
        } else if (options != 0U && strcmp(value, "--require-padding") == 0) {
            flags &= ~B64_F_UNPADDED;
        } else if (options != 0U && strcmp(value, "--strict") == 0) {
            flags &= ~B64_F_WHITESPACE;
        } else if (options != 0U && value[0] == '-') {
            fprintf(stderr, "ERROR: unknown option: %s\n", value);
            return 2;
        } else if (paths == 0U) {
            input_path = value;
            ++paths;
        } else if (paths == 1U) {
            output_path = value;
            ++paths;
        } else {
            fprintf(stderr, "ERROR: too many file paths; use --help.\n");
            return 2;
        }
    }

    if (alphabet != NULL &&
        !b64_alphabet_valid(alphabet, strlen((const char *)alphabet))) {
        fprintf(stderr, "ERROR: alphabet requires 64 distinct printable ASCII "
                        "symbols excluding equals.\n");
        return 2;
    }

    file = fopen(input_path, "rb");
    if (file == NULL) {
        fprintf(stderr, "ERROR: cannot open input: %s\n", input_path);
        return 4;
    }
    input_size = fread(b64_input_buffer, 1U, sizeof b64_input_buffer, file);
    extra = fgetc(file);
    io_failed = ferror(file);
    if (fclose(file) != 0) {
        io_failed = 1;
    }
    if (io_failed != 0) {
        fprintf(stderr, "ERROR: input read/close failed: %s\n", input_path);
        return 4;
    }
    if (extra != EOF) {
        fprintf(stderr, "BENCH: input exceeds %zu bytes.\n",
                sizeof b64_input_buffer);
        return 5;
    }

    if (alphabet == NULL) {
        result = b64_decode(b64_input_buffer, input_size, b64_output_buffer,
                            sizeof b64_output_buffer, flags);
    } else {
        result = b64_decode_custom(b64_input_buffer, input_size, b64_output_buffer,
                                   sizeof b64_output_buffer, flags, alphabet, 64U);
    }
    if (result.status != B64_OK) {
        fprintf(stderr, "ERROR: %s; input byte offset %zu.\n",
                b64_status_text(result.status), result.error_offset);
        return result.status == B64_OUTPUT_LIMIT ? 5 : 3;
    }

    file = fopen(output_path, "wb");
    if (file == NULL) {
        fprintf(stderr, "ERROR: cannot open output: %s\n", output_path);
        return 4;
    }
    output_size = fwrite(b64_output_buffer, 1U, result.size, file);
    io_failed = output_size != result.size;
    if (fclose(file) != 0) {
        io_failed = 1;
    }
    if (io_failed != 0) {
        fprintf(stderr, "ERROR: output write/close failed: %s\n", output_path);
        return 4;
    }
    fprintf(stderr, "OK: %zu decoded bytes -> %s\n", result.size, output_path);
    return 0;
}

#endif /* B64_NO_MAIN */
