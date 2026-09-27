#include "shakti_test_wav.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static int write_u16(FILE *file, uint16_t value)
{
    unsigned char bytes[2];

    bytes[0] = (unsigned char)(value & UINT16_C(0xFF));
    bytes[1] = (unsigned char)((value >> 8U) & UINT16_C(0xFF));

    return fwrite(bytes, 1U, sizeof(bytes), file) == sizeof(bytes);
}

static int write_u32(FILE *file, uint32_t value)
{
    unsigned char bytes[4];

    bytes[0] = (unsigned char)(value & UINT32_C(0xFF));
    bytes[1] = (unsigned char)((value >> 8U) & UINT32_C(0xFF));
    bytes[2] = (unsigned char)((value >> 16U) & UINT32_C(0xFF));
    bytes[3] = (unsigned char)((value >> 24U) & UINT32_C(0xFF));

    return fwrite(bytes, 1U, sizeof(bytes), file) == sizeof(bytes);
}

int shakti_write_test_wav(const char *path)
{
    FILE *file;
    uint32_t sample_count;
    uint32_t data_size;
    uint32_t index;
    int success;

    if (path == NULL) {
        return 0;
    }

    sample_count = UINT32_C(160);
    data_size = sample_count * UINT32_C(2);
    file = fopen(path, "wb");

    if (file == NULL) {
        perror("Could not create WAV");
        return 0;
    }

    success =
        fwrite("RIFF", 1U, 4U, file) == 4U &&
        write_u32(file, UINT32_C(36) + data_size) &&
        fwrite("WAVE", 1U, 4U, file) == 4U &&
        fwrite("fmt ", 1U, 4U, file) == 4U &&
        write_u32(file, UINT32_C(16)) &&
        write_u16(file, UINT16_C(1)) &&
        write_u16(file, UINT16_C(1)) &&
        write_u32(file, UINT32_C(16000)) &&
        write_u32(file, UINT32_C(32000)) &&
        write_u16(file, UINT16_C(2)) &&
        write_u16(file, UINT16_C(16)) &&
        fwrite("data", 1U, 4U, file) == 4U &&
        write_u32(file, data_size);

    for (index = 0U; success && index < sample_count; ++index) {
        success = write_u16(file, UINT16_C(0));
    }

    if (success) {
        success = fflush(file) == 0;
    }

    if (fclose(file) != 0) {
        success = 0;
    }

    return success;
}

#ifndef SHAKTI_TOOL_NO_MAIN
#define MAKE_WAV_FIXTURE_MAIN main
int MAKE_WAV_FIXTURE_MAIN(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s OUTPUT.wav\n", argv[0]);
        return EXIT_FAILURE;
    }

    if (!shakti_write_test_wav(argv[1])) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
#endif
