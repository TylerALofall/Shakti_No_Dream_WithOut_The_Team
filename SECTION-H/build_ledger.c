#include "shakti_build_ledger.h"

#include <stdio.h>
#include <stdlib.h>

#include "shakti_manifest.h"

int shakti_build_ledger_file(
    const char *manifest_path,
    const char *ledger_path
)
{
    shakti_manifest_t manifest;
    int ready;

    if (manifest_path == NULL || ledger_path == NULL) {
        return 0;
    }

    shakti_manifest_init(&manifest);

    if (!shakti_manifest_load(&manifest, manifest_path)) {
        fputs("Manifest structure or prerequisites are invalid.\n", stderr);
        return 0;
    }

    ready = shakti_manifest_write_ledger(
        &manifest,
        manifest_path,
        ledger_path
    );

    shakti_manifest_print(&manifest);

    if (!ready) {
        fputs(
            "Ledger written, but one or more ready tablets failed validation.\n",
            stderr
        );
        return 0;
    }

    printf("Ledger written to %s.\n", ledger_path);

    return 1;
}

#ifndef SHAKTI_TOOL_NO_MAIN
#define BUILD_LEDGER_MAIN main
int BUILD_LEDGER_MAIN(int argc, char **argv)
{
    if (argc != 3) {
        fprintf(
            stderr,
            "Usage: %s MANIFEST.xml LEDGER.tsv\n",
            argv[0]
        );
        return EXIT_FAILURE;
    }

    if (!shakti_build_ledger_file(argv[1], argv[2])) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
#endif
