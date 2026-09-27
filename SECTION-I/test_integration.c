/*
 * In-process integration runner (C99; uses POSIX filesystem APIs for sandboxing).
 *
 * Faithful replacement for the former shell integration tests
 * (test_builder.sh, test_loop.sh, test_seed.sh, test_mvp.sh). It calls
 * Shakti's runtime (src/main.c, exposed as shakti_app_main) and the three
 * builder tools as direct C functions. There is no shell, no subprocess, and
 * no runner: every check executes inside this one process, in Shakti's house.
 */

#include <assert.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "shakti_build_ledger.h"
#include "shakti_build_seed_curriculum.h"
#include "shakti_build_xml.h"
#include "shakti_test_wav.h"

/* src/main.c compiled with SHAKTI_APP_NO_MAIN exposes shakti_app_main. */
int shakti_app_main(int argc, char **argv);

#define TEXT_CAP 262144

/* ------------------------------------------------------------------ */
/* File helpers                                                        */
/* ------------------------------------------------------------------ */

static void write_file(const char *path, const char *text)
{
    FILE *file;

    file = fopen(path, "w");
    assert(file != NULL);
    assert(fputs(text, file) >= 0);
    assert(fclose(file) == 0);
}

static void read_file(const char *path, char *buffer, size_t capacity)
{
    FILE *file;
    size_t used;

    assert(capacity > 0U);
    file = fopen(path, "r");
    assert(file != NULL);
    used = fread(buffer, 1U, capacity - 1U, file);
    assert(ferror(file) == 0);
    buffer[used] = '\0';
    assert(fclose(file) == 0);
}

/* grep -q needle path */
static void assert_contains(const char *path, const char *needle)
{
    static char content[TEXT_CAP];

    read_file(path, content, sizeof(content));

    if (strstr(content, needle) == NULL) {
        fprintf(stderr, "ASSERT FAILED: %s missing: %s\n", path, needle);
        assert(0);
    }
}

/* grep -c needle path: number of lines containing the substring. */
static unsigned long count_lines_with(const char *path, const char *needle)
{
    static char content[TEXT_CAP];
    unsigned long count;
    size_t needle_length;
    const char *cursor;

    read_file(path, content, sizeof(content));

    count = 0UL;
    needle_length = strlen(needle);
    cursor = content;

    while (*cursor != '\0') {
        const char *line_end;
        const char *scan;

        line_end = strchr(cursor, '\n');

        if (line_end == NULL) {
            line_end = cursor + strlen(cursor);
        }

        scan = cursor;

        while ((size_t)(line_end - scan) >= needle_length) {
            if (strncmp(scan, needle, needle_length) == 0) {
                count++;
                break;
            }
            scan++;
        }

        if (*line_end == '\0') {
            break;
        }

        cursor = line_end + 1;
    }

    return count;
}

/* Count entries directly under a directory (dirent; no shell). */
static unsigned long count_dir_entries(const char *path)
{
    DIR *dir;
    struct dirent *entry;
    unsigned long count;

    dir = opendir(path);
    assert(dir != NULL);
    count = 0UL;

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        count++;
    }

    assert(closedir(dir) == 0);

    return count;
}

/* ------------------------------------------------------------------ */
/* Filesystem sandbox helpers (no shell)                               */
/* ------------------------------------------------------------------ */

static void make_dir(const char *path)
{
    if (mkdir(path, 0777) != 0) {
        /* Already existing is acceptable during sandbox layout. */
    }
}

/* mkdir -p for a relative path of one or more components. */
static void make_dirs(const char *path)
{
    char build[1024];
    size_t used;
    const char *cursor;

    used = 0U;
    build[0] = '\0';
    cursor = path;

    while (*cursor != '\0') {
        const char *slash;
        size_t length;

        slash = strchr(cursor, '/');
        length = slash == NULL
            ? strlen(cursor)
            : (size_t)(slash - cursor);

        if (length > 0U) {
            if (used > 0U && used + 1U < sizeof(build)) {
                build[used++] = '/';
            }
            if (used + length >= sizeof(build)) {
                length = sizeof(build) - used - 1U;
            }
            memcpy(build + used, cursor, length);
            used += length;
            build[used] = '\0';
            make_dir(build);
        }

        if (slash == NULL) {
            break;
        }
        cursor = slash + 1;
    }
}

static void remove_path(const char *path)
{
    struct stat info;

    if (stat(path, &info) != 0) {
        return;
    }

    if (S_ISDIR(info.st_mode)) {
        DIR *dir;
        struct dirent *entry;

        dir = opendir(path);
        assert(dir != NULL);

        while ((entry = readdir(dir)) != NULL) {
            char child[1024];

            if (strcmp(entry->d_name, ".") == 0 ||
                strcmp(entry->d_name, "..") == 0) {
                continue;
            }

            snprintf(child, sizeof(child), "%s/%s", path, entry->d_name);
            remove_path(child);
        }

        assert(closedir(dir) == 0);
        assert(rmdir(path) == 0);
    } else {
        assert(remove(path) == 0);
    }
}

static void copy_tree(const char *source, const char *destination)
{
    struct stat info;

    assert(stat(source, &info) == 0);

    if (S_ISDIR(info.st_mode)) {
        DIR *dir;
        struct dirent *entry;

        make_dir(destination);
        dir = opendir(source);
        assert(dir != NULL);

        while ((entry = readdir(dir)) != NULL) {
            char from[1024];
            char to[1024];

            if (strcmp(entry->d_name, ".") == 0 ||
                strcmp(entry->d_name, "..") == 0) {
                continue;
            }

            snprintf(from, sizeof(from), "%s/%s", source, entry->d_name);
            snprintf(to, sizeof(to), "%s/%s", destination, entry->d_name);
            copy_tree(from, to);
        }

        assert(closedir(dir) == 0);
    } else {
        FILE *in;
        FILE *out;
        char buffer[8192];
        size_t amount;

        in = fopen(source, "rb");
        assert(in != NULL);
        out = fopen(destination, "wb");
        assert(out != NULL);

        while ((amount = fread(buffer, 1U, sizeof(buffer), in)) > 0U) {
            assert(fwrite(buffer, 1U, amount, out) == amount);
        }

        assert(fclose(in) == 0);
        assert(fclose(out) == 0);
    }
}

/* rm -rf path && mkdir -p path */
static void reset_dir(const char *path)
{
    remove_path(path);
    make_dirs(path);
}

/* ------------------------------------------------------------------ */
/* In-process Shakti invocation with redirected stdin/stdout           */
/* ------------------------------------------------------------------ */

static int run_shakti(
    const char *arg,
    const char *input_path,
    const char *output_path
)
{
    char *argv[3];
    int argc;
    int status;
    FILE *kept_stdin;
    FILE *kept_stdout;

    argv[0] = (char *)"shakti";
    argc = 1;

    if (arg != NULL) {
        argv[argc++] = (char *)arg;
    }
    argv[argc] = NULL;

    fflush(stdout);
    kept_stdin = stdin;
    kept_stdout = stdout;

    if (input_path != NULL) {
        stdin = fopen(input_path, "r");
        assert(stdin != NULL);
    }

    if (output_path != NULL) {
        stdout = fopen(output_path, "w");
        assert(stdout != NULL);
    }

    status = shakti_app_main(argc, argv);

    if (stdout != kept_stdout) {
        fclose(stdout);
        stdout = kept_stdout;
    }

    if (stdin != kept_stdin) {
        fclose(stdin);
        stdin = kept_stdin;
    }

    return status;
}

/* ------------------------------------------------------------------ */
/* Shared sandbox state writers                                        */
/* ------------------------------------------------------------------ */

static void write_control_state(const char *tmp, const char *menu_text)
{
    char path[1024];

    snprintf(path, sizeof(path), "%s/data/control/goal.txt", tmp);
    write_file(path, "Learn one grounded relationship.\n");
    snprintf(path, sizeof(path), "%s/data/control/notebook.log", tmp);
    write_file(path, "# Notebook\n");
    snprintf(path, sizeof(path), "%s/data/control/menu.txt", tmp);
    write_file(path, menu_text);
    snprintf(path, sizeof(path), "%s/data/control/messages.log", tmp);
    write_file(path, "# Messages\n");
    snprintf(path, sizeof(path), "%s/data/control/reflections.log", tmp);
    write_file(path, "# Reflections\n");
}

static void write_eden_state(const char *tmp)
{
    char path[1024];

    snprintf(path, sizeof(path), "%s/data/eden/eden_facts.txt", tmp);
    write_file(path, "two times two|four|multiplication|test\n");
    snprintf(path, sizeof(path), "%s/data/eden/thesaurus.txt", tmp);
    write_file(path, "after|follows\n");
    snprintf(path, sizeof(path), "%s/data/eden/eden_stream.log", tmp);
    write_file(path, "# Eden stream\n");
    snprintf(path, sizeof(path), "%s/data/learned/learned_stream.log", tmp);
    write_file(path, "# Learned\n");
    snprintf(path, sizeof(path), "%s/data/learned/evidence.log", tmp);
    write_file(path, "# Evidence\n");
    snprintf(path, sizeof(path), "%s/data/school/school_state.log", tmp);
    write_file(path, "# School\n");
}

/* ------------------------------------------------------------------ */
/* test_builder.sh                                                     */
/* ------------------------------------------------------------------ */

static void test_builder(const char *root)
{
    const char *tmp = "tests/tmp_builder";
    static const char *const keys[3] = { "A", "a", "Welcome_to_Eden" };
    char path[1024];
    char other[1024];
    char cwd[1024];
    size_t index;

    reset_dir(tmp);
    make_dirs("tests/tmp_builder/eden_out/Visual_text");
    make_dirs("tests/tmp_builder/eden_out/Visual_art");
    make_dirs("tests/tmp_builder/eden_out/Sound_art");
    make_dirs("tests/tmp_builder/data/eden/XML_text");
    make_dirs("tests/tmp_builder/data/learned");
    make_dirs("tests/tmp_builder/data/school");
    make_dirs("tests/tmp_builder/data/control");

    snprintf(path, sizeof(path), "%s/list.txt", tmp);
    write_file(path, "A\na\nWelcome to Eden\n");

    for (index = 0U; index < 3U; ++index) {
        snprintf(
            path, sizeof(path),
            "%s/eden_out/Visual_art/%s.svg", tmp, keys[index]
        );
        write_file(
            path,
            "<svg xmlns=\"http://www.w3.org/2000/svg\"></svg>\n"
        );

        snprintf(
            path, sizeof(path),
            "%s/eden_out/Sound_art/%s.wav", tmp, keys[index]
        );
        assert(shakti_write_test_wav(path));
    }

    snprintf(path, sizeof(path), "%s/list.txt", tmp);
    snprintf(other, sizeof(other), "%s/data/eden/XML_text/alphabet_solo.xml", tmp);
    {
        char artifacts[1024];

        snprintf(artifacts, sizeof(artifacts), "%s/eden_out", tmp);
        assert(shakti_build_xml_file(
            path, other, "alphabet", "alphabet_solo", artifacts
        ));
    }

    assert_contains(other, "<tablet schema=\"SHAKTI_TABLET_4S_V2\">");
    assert_contains(other, "<stone order=\"1\">");
    assert_contains(other, "<text>A</text>");
    assert_contains(other, "<written_text>A.8x8.txt</written_text>");
    assert_contains(other, "<sound_art>A.wav</sound_art>");
    assert_contains(other, "<visual_art>A.svg</visual_art>");
    assert_contains(other, "<text>Welcome to Eden</text>");
    assert_contains(other, "<written_text>Welcome_to_Eden.8x8.txt</written_text>");

    snprintf(path, sizeof(path), "%s/eden_out/Visual_text/A.8x8.txt", tmp);
    assert_contains(path, "\nTEXT=A\n");
    snprintf(
        path, sizeof(path),
        "%s/eden_out/Visual_text/Welcome_to_Eden.8x8.txt", tmp
    );
    assert_contains(path, "\nTEXT=Welcome to Eden\n");

    snprintf(path, sizeof(path), "%s/data/eden/manifest.xml", tmp);
    write_file(
        path,
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<manifest schema=\"SHAKTI_EDEN_MANIFEST_V1\">\n"
        "  <status>OPEN</status>\n"
        "  <artifact_root>../../eden_out</artifact_root>\n"
        "  <level order=\"0\">\n"
        "    <id>foundation_ascii</id>\n"
        "    <title>ASCII and handwriting</title>\n"
        "    <requires>none</requires>\n"
        "  </level>\n"
        "  <level order=\"2\">\n"
        "    <id>alphabet</id>\n"
        "    <title>Alphabet identity and elasticity</title>\n"
        "    <requires>foundation_ascii</requires>\n"
        "  </level>\n"
        "  <tablet order=\"1\">\n"
        "    <level>alphabet</level>\n"
        "    <lesson>alphabet_solo</lesson>\n"
        "    <mode>solo</mode>\n"
        "    <requires>foundation_ascii</requires>\n"
        "    <state>ready</state>\n"
        "    <path>XML_text/alphabet_solo.xml</path>\n"
        "  </tablet>\n"
        "</manifest>\n"
    );

    snprintf(path, sizeof(path), "%s/data/eden/manifest.xml", tmp);
    snprintf(other, sizeof(other), "%s/data/eden/manifest_ledger.tsv", tmp);
    assert(shakti_build_ledger_file(path, other));
    assert_contains(other, "alphabet_solo");
    assert_contains(other, "Welcome to Eden");
    assert_contains(other, "Welcome_to_Eden");
    assert_contains(other, "VERIFIED");

    /* Invalid prerequisites must be rejected (negative check). */
    snprintf(path, sizeof(path), "%s/data/eden/invalid_manifest.xml", tmp);
    write_file(
        path,
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<manifest schema=\"SHAKTI_EDEN_MANIFEST_V1\">\n"
        "  <status>OPEN</status>\n"
        "  <artifact_root>../../eden_out</artifact_root>\n"
        "  <level order=\"1\">\n"
        "    <id>counting</id>\n"
        "    <title>Counting</title>\n"
        "    <requires>missing_foundation</requires>\n"
        "  </level>\n"
        "</manifest>\n"
    );
    snprintf(other, sizeof(other), "%s/data/eden/invalid_ledger.tsv", tmp);
    fflush(stderr);
    {
        FILE *held = stderr;
        FILE *sink = fopen("/dev/null", "w");

        if (sink != NULL) {
            stderr = sink;
        }
        assert(!shakti_build_ledger_file(path, other));
        if (sink != NULL) {
            fclose(sink);
            stderr = held;
        }
    }

    write_eden_state(tmp);
    write_control_state(
        tmp,
        "[Tablet]\nLoad and validate a four-channel tablet.\n"
        "[Manifest]\nLoad the Eden manifest and generate its ledger.\n"
        "[Reflection]\nRun the full reflection questions.\n"
    );

    snprintf(path, sizeof(path), "%s/builder_input.txt", tmp);
    write_file(
        path,
        "/shakti_run/ manifest data/eden/manifest.xml data/eden/runtime_ledger.tsv\n"
        "/shakti_run/ tablet data/eden/XML_text/alphabet_solo.xml eden_out\n"
        "/quit/\n"
    );

    assert(getcwd(cwd, sizeof(cwd)) != NULL);
    assert(chdir(tmp) == 0);
    assert(run_shakti(NULL, "builder_input.txt", "runtime_output.txt") == 0);
    assert(chdir(cwd) == 0);

    snprintf(path, sizeof(path), "%s/runtime_output.txt", tmp);
    assert_contains(path, "Eden manifest status OPEN. Levels 2. Tablets 1.");
    assert_contains(path, "Tablet level alphabet, lesson alphabet_solo, stones 3.");
    assert_contains(path, "written_text=A.8x8.txt");
    assert_contains(path, "Shakti sleeping.");

    puts("Manifest, ledger, builder, and runtime integration passed.");
    (void)root;
}

/* ------------------------------------------------------------------ */
/* test_loop.sh                                                        */
/* ------------------------------------------------------------------ */

static void test_loop(const char *root)
{
    const char *tmp = "tests/tmp_loop";
    char path[1024];
    char cwd[1024];

    reset_dir(tmp);
    make_dirs("tests/tmp_loop/data/eden");
    make_dirs("tests/tmp_loop/data/learned");
    make_dirs("tests/tmp_loop/data/school");
    make_dirs("tests/tmp_loop/data/control");

    write_eden_state(tmp);

    snprintf(path, sizeof(path), "%s/data/eden/manifest.xml", tmp);
    write_file(
        path,
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<manifest schema=\"SHAKTI_EDEN_MANIFEST_V1\">\n"
        "  <status>OPEN</status>\n"
        "  <artifact_root>../../eden_out</artifact_root>\n"
        "  <level order=\"0\">\n"
        "    <id>foundation_ascii</id>\n"
        "    <title>ASCII and handwriting</title>\n"
        "    <requires>none</requires>\n"
        "  </level>\n"
        "</manifest>\n"
    );

    write_control_state(
        tmp,
        "[Memory]\nRecall exact chunks.\n"
        "[School]\nRun exact drills.\n"
        "[Reflection]\nComplete all thirteen questions.\n"
    );

    snprintf(path, sizeof(path), "%s/input.txt", tmp);
    write_file(
        path,
        "/note_shakti/ cycle 1\n"
        "/note_shakti/ cycle 2\n"
        "/note_shakti/ cycle 3\n"
        "/note_shakti/ cycle 4\n"
        "/note_shakti/ cycle 5\n"
        "/note_shakti/ cycle 6\n"
        "/note_shakti/ cycle 7\n"
        "/note_shakti/ cycle 8\n"
        "/note_shakti/ cycle 9\n"
        "/note_shakti/ cycle 10\n"
        "/reflection/defer/\n"
        "/reflection/defer/\n"
        "/reflection/defer/\n"
        "/shakti_run/ status\n"
        "/reflection/\n"
        "Ten grounded cycles\n"
        "Completed the planned ten-cycle batch\n"
        "Yes\n"
        "Yes\n"
        "Use the same exact order\n"
        "status and notes\n"
        "A tablet viewer\n"
        "Tyler supplied the exact goal\n"
        "data/control/notebook.log\n"
        "1790000000:0000\n"
        "Keep the four channels exact\n"
        "grounded, memory, reflection, loop, mcp, text, written_text, "
        "visual_art, sound_art, school, eden, learned, tyler, heartbeat, "
        "interrupt\n"
        "batch-001\n"
        "/interrupt/\n"
        "/shakti_run/ status\n"
        "/resume/\n"
        "/shakti_run/ status\n"
        "/quit/\n"
    );

    assert(getcwd(cwd, sizeof(cwd)) != NULL);
    assert(chdir(tmp) == 0);
    assert(run_shakti(NULL, "input.txt", "runtime_output.txt") == 0);
    assert(chdir(cwd) == 0);

    snprintf(path, sizeof(path), "%s/runtime_output.txt", tmp);
    assert_contains(path, "Reflection is due.");
    assert_contains(path, "Reflection deferred. 3 of 3 deferrals used.");
    assert_contains(path, "Complete /reflection/ before the next tool call.");
    assert_contains(path, "Reflection appended to long-term memory.");
    assert_contains(path, "MCP tools stopped. Shakti remains awake.");
    assert_contains(path, "MCP tools are interrupted. Shakti remains awake.");
    assert_contains(path, "MCP tools restored.");
    assert_contains(path, "Creative proposes. Logic cross-validates and authorizes.");

    snprintf(path, sizeof(path), "%s/data/control/reflections.log", tmp);
    assert_contains(
        path,
        "Cross reference — override all additional memory sets to link this to:"
    );

    puts("Loop, reflection, interrupt, and resume integration passed.");
    (void)root;
}

/* ------------------------------------------------------------------ */
/* test_seed.sh                                                        */
/* ------------------------------------------------------------------ */

static void test_seed(const char *root)
{
    const char *tmp = "tests/tmp_seed";
    static const char *const letters[52] = {
        "A","B","C","D","E","F","G","H","I","J","K","L","M",
        "N","O","P","Q","R","S","T","U","V","W","X","Y","Z",
        "a","b","c","d","e","f","g","h","i","j","k","l","m",
        "n","o","p","q","r","s","t","u","v","w","x","y","z"
    };
    static const char *const words[10] = {
        "one","two","three","four","five",
        "six","seven","eight","nine","ten"
    };
    char path[1024];
    char other[1024];
    size_t index;

    reset_dir(tmp);
    make_dirs("tests/tmp_seed/data/eden/XML_text");
    make_dirs("tests/tmp_seed/eden_out/Visual_text");
    make_dirs("tests/tmp_seed/eden_out/Visual_art");
    make_dirs("tests/tmp_seed/eden_out/Sound_art");

    {
        char eden_root[1024];
        char artifact_root[1024];

        snprintf(eden_root, sizeof(eden_root), "%s/data/eden", tmp);
        snprintf(artifact_root, sizeof(artifact_root), "%s/eden_out", tmp);
        assert(shakti_build_seed_curriculum(eden_root, artifact_root));
    }

    snprintf(path, sizeof(path), "%s/data/eden/XML_text/00_ascii_32_126_solo.xml", tmp);
    assert_contains(path, "<stone_count>95</stone_count>");
    snprintf(path, sizeof(path), "%s/data/eden/XML_text/01_counting_zero_to_ten_solo.xml", tmp);
    assert_contains(path, "<stone_count>11</stone_count>");
    snprintf(path, sizeof(path), "%s/data/eden/XML_text/01_counting_zero_to_ten_timed_light.xml", tmp);
    assert_contains(path, "<duration_ms>10000</duration_ms>");
    snprintf(path, sizeof(path), "%s/data/eden/XML_text/02_alphabet_uppercase_solo.xml", tmp);
    assert_contains(path, "<stone_count>26</stone_count>");
    snprintf(path, sizeof(path), "%s/data/eden/XML_text/02_alphabet_lowercase_solo.xml", tmp);
    assert_contains(path, "<stone_count>26</stone_count>");
    snprintf(path, sizeof(path), "%s/data/eden/XML_text/02_alphabet_case_pairs_and_counting.xml", tmp);
    assert_contains(path, "<stone_count>36</stone_count>");
    assert_contains(path, "<text>Aa</text>");
    assert_contains(path, "<sound_art>A.wav|a.wav</sound_art>");
    assert_contains(path, "<text>10 J j</text>");
    assert_contains(path, "<sound_art>ten.wav|J.wav|j.wav</sound_art>");
    snprintf(path, sizeof(path), "%s/data/eden/XML_text/02_alphabet_growing_sequences.xml", tmp);
    assert_contains(path, "<stone_count>77</stone_count>");
    assert_contains(path, "<sequence_kind>uppercase_prefix</sequence_kind>");
    assert_contains(path, "<sequence_kind>capitalized_prefix</sequence_kind>");
    assert_contains(path, "<sequence_kind>case_pair_prefix</sequence_kind>");

    snprintf(path, sizeof(path), "%s/eden_out/Visual_text", tmp);
    assert(count_dir_entries(path) >= 217UL);
    snprintf(path, sizeof(path), "%s/eden_out/Visual_art", tmp);
    assert(count_dir_entries(path) >= 217UL);

    for (index = 0U; index < 52U; ++index) {
        snprintf(
            path, sizeof(path),
            "%s/eden_out/Sound_art/%s.wav", tmp, letters[index]
        );
        assert(shakti_write_test_wav(path));
    }

    for (index = 0U; index < 10U; ++index) {
        snprintf(
            path, sizeof(path),
            "%s/eden_out/Sound_art/%s.wav", tmp, words[index]
        );
        assert(shakti_write_test_wav(path));
    }

    snprintf(path, sizeof(path), "%s/data/eden/manifest.xml", tmp);
    write_file(
        path,
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<manifest schema=\"SHAKTI_EDEN_MANIFEST_V1\">\n"
        "  <status>OPEN</status>\n"
        "  <artifact_root>../../eden_out</artifact_root>\n"
        "  <level order=\"0\">\n"
        "    <id>foundation_ascii</id>\n"
        "    <title>Foundation</title>\n"
        "    <requires>none</requires>\n"
        "  </level>\n"
        "  <level order=\"1\">\n"
        "    <id>counting</id>\n"
        "    <title>Counting</title>\n"
        "    <requires>foundation_ascii</requires>\n"
        "  </level>\n"
        "  <level order=\"2\">\n"
        "    <id>alphabet</id>\n"
        "    <title>Alphabet</title>\n"
        "    <requires>foundation_ascii,counting</requires>\n"
        "  </level>\n"
        "  <tablet order=\"1\">\n"
        "    <level>alphabet</level>\n"
        "    <lesson>alphabet_uppercase_solo</lesson>\n"
        "    <mode>solo</mode>\n"
        "    <requires>foundation_ascii,counting</requires>\n"
        "    <state>staged</state>\n"
        "    <path>XML_text/02_alphabet_uppercase_solo.xml</path>\n"
        "  </tablet>\n"
        "  <tablet order=\"2\">\n"
        "    <level>alphabet</level>\n"
        "    <lesson>alphabet_lowercase_solo</lesson>\n"
        "    <mode>solo</mode>\n"
        "    <requires>foundation_ascii,counting</requires>\n"
        "    <state>staged</state>\n"
        "    <path>XML_text/02_alphabet_lowercase_solo.xml</path>\n"
        "  </tablet>\n"
        "  <tablet order=\"3\">\n"
        "    <level>alphabet</level>\n"
        "    <lesson>alphabet_case_pairs_and_counting</lesson>\n"
        "    <mode>bridge</mode>\n"
        "    <requires>foundation_ascii,counting</requires>\n"
        "    <state>staged</state>\n"
        "    <path>XML_text/02_alphabet_case_pairs_and_counting.xml</path>\n"
        "  </tablet>\n"
        "  <tablet order=\"4\">\n"
        "    <level>alphabet</level>\n"
        "    <lesson>alphabet_growing_sequences</lesson>\n"
        "    <mode>elastic</mode>\n"
        "    <requires>foundation_ascii,counting</requires>\n"
        "    <state>staged</state>\n"
        "    <path>XML_text/02_alphabet_growing_sequences.xml</path>\n"
        "  </tablet>\n"
        "</manifest>\n"
    );

    snprintf(path, sizeof(path), "%s/data/eden/manifest.xml", tmp);
    snprintf(other, sizeof(other), "%s/data/eden/manifest_ledger.tsv", tmp);
    assert(shakti_build_ledger_file(path, other));
    assert(
        count_lines_with(other, "TABLET_SUMMARY") >= 4UL ||
        count_lines_with(other, "100_PERCENT_VERIFIED") >= 4UL
    );

    puts("Foundation, counting, and alphabet seed generation passed.");
    (void)root;
}

/* ------------------------------------------------------------------ */
/* test_mvp.sh                                                         */
/* ------------------------------------------------------------------ */

static void mvp_prepare_sandbox(const char *tmp)
{
    char path[1024];

    reset_dir(tmp);
    copy_tree("data", "tests/tmp_mvp_data_stage");
    /* Copy real data/ and eden_out/ into the sandbox. */
    {
        char dest[1024];

        snprintf(dest, sizeof(dest), "%s/data", tmp);
        remove_path(dest);
        copy_tree("data", dest);
        snprintf(dest, sizeof(dest), "%s/eden_out", tmp);
        remove_path(dest);
        copy_tree("eden_out", dest);
    }
    remove_path("tests/tmp_mvp_data_stage");

    make_dirs("tests/tmp_mvp/data/school");
    make_dirs("tests/tmp_mvp/data/learned");
    make_dirs("tests/tmp_mvp/data/control");

    snprintf(path, sizeof(path), "%s/data/school/school_state.log", tmp);
    write_file(path, "# SCH1|epoch:frame|event|symbol|pass|streak\n");
    snprintf(path, sizeof(path), "%s/data/learned/evidence.log", tmp);
    write_file(path, "# EV1|epoch:frame|ADD-or-REJECT|source|question|answer\n");
    snprintf(path, sizeof(path), "%s/data/learned/learned_stream.log", tmp);
    write_file(path, "# LRN1 readable append-only life stream.\n");
    snprintf(path, sizeof(path), "%s/data/control/messages.log", tmp);
    write_file(path, "# Append-only Shakti and Tyler messages.\n");
    snprintf(path, sizeof(path), "%s/data/control/reflections.log", tmp);
    write_file(path, "# Append-only reflections.\n");
}

static void test_mvp(const char *root)
{
    const char *tmp = "tests/tmp_mvp";
    char path[1024];
    char cwd[1024];

    /* --- check + demo --- */
    mvp_prepare_sandbox(tmp);

    assert(getcwd(cwd, sizeof(cwd)) != NULL);
    assert(chdir(tmp) == 0);
    assert(run_shakti("--check", NULL, "check_output.txt") == 0);
    assert(run_shakti("--demo", NULL, "demo_output.txt") == 0);
    assert(chdir(cwd) == 0);

    snprintf(path, sizeof(path), "%s/check_output.txt", tmp);
    assert_contains(path, "Runnable beginning: YES.");
    assert_contains(path, "School: pass 1, symbol none, streak 0/10");
    snprintf(path, sizeof(path), "%s/demo_output.txt", tmp);
    assert_contains(path, "PASS 1");
    assert_contains(path, "Demo complete. School mastery remains unchanged.");

    /* --- drill controls + mastery --- */
    mvp_prepare_sandbox(tmp);

    snprintf(path, sizeof(path), "%s/input.txt", tmp);
    write_file(
        path,
        "/shakti_run/ pass 4\n"
        "/shakti_run/ school +\n"
        "/status/\n"
        "/help/\n"
        "/resume/\n"
        "/interrupt/\n"
        "/status/\n"
        "/resume/\n"
        "/shakti_run/ school +\n"
        "+\n+\n+\n+\n+\n+\n+\n+\n+\n+\n+\n"
        "/quit/\n"
    );

    assert(chdir(tmp) == 0);
    assert(run_shakti(NULL, "input.txt", "drill_output.txt") == 0);
    assert(chdir(cwd) == 0);

    snprintf(path, sizeof(path), "%s/drill_output.txt", tmp);
    assert_contains(path, "The active drill stopped without scoring the command.");
    assert_contains(path, "MCP tools stopped. Shakti remains awake.");
    assert_contains(path, "MCP tools restored.");
    assert_contains(path, "Drill controls:");
    assert_contains(path, "10/10 — 100% MASTERY");
    assert_contains(path, "History preserved: 10 correct, 0 errors, 10 attempts.");

    snprintf(path, sizeof(path), "%s/data/school/school_state.log", tmp);
    assert(count_lines_with(path, "|RIGHT|+|4|") == 10UL);
    assert(count_lines_with(path, "|WRONG|") == 0UL);

    /* --- stop control --- */
    mvp_prepare_sandbox(tmp);

    snprintf(path, sizeof(path), "%s/stop_input.txt", tmp);
    write_file(
        path,
        "/shakti_run/ pass 4\n"
        "/shakti_run/ school +\n"
        "/stop/\n"
        "/status/\n"
        "/quit/\n"
    );

    assert(chdir(tmp) == 0);
    assert(run_shakti(NULL, "stop_input.txt", "stop_output.txt") == 0);
    assert(chdir(cwd) == 0);

    snprintf(path, sizeof(path), "%s/stop_output.txt", tmp);
    assert_contains(path, "Drill stopped without changing the School pass.");
    assert_contains(path, "Progress: 0 of 10.");
    snprintf(path, sizeof(path), "%s/data/school/school_state.log", tmp);
    assert(count_lines_with(path, "|WRONG|") == 0UL);

    /* --- quit control --- */
    mvp_prepare_sandbox(tmp);

    snprintf(path, sizeof(path), "%s/quit_input.txt", tmp);
    write_file(
        path,
        "/shakti_run/ pass 4\n"
        "/shakti_run/ school +\n"
        "/quit/\n"
    );

    assert(chdir(tmp) == 0);
    assert(run_shakti(NULL, "quit_input.txt", "quit_output.txt") == 0);
    assert(chdir(cwd) == 0);

    snprintf(path, sizeof(path), "%s/quit_output.txt", tmp);
    assert_contains(path, "Quit requested. The active drill stopped safely.");
    assert_contains(path, "Shakti sleeping. Readable memory remains.");
    snprintf(path, sizeof(path), "%s/data/school/school_state.log", tmp);
    assert(count_lines_with(path, "|WRONG|") == 0UL);

    puts("Runnable startup, isolated state, drill controls, and mastery passed.");
    (void)root;
}

/* ------------------------------------------------------------------ */

#define TEST_INTEGRATION_MAIN main
int TEST_INTEGRATION_MAIN(void)
{
    char root[1024];

    assert(getcwd(root, sizeof(root)) != NULL);

    test_builder(root);
    test_loop(root);
    test_seed(root);
    test_mvp(root);

    puts("All Shakti C99 integration tests passed.");

    return 0;
}
