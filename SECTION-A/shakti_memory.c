#include "shakti_memory.h"

#include <stdio.h>
#include <string.h>

#include "shakti_config.h"

static void copy_text__memory_h(
    char *destination,
    size_t destination_size,
    const char *source
)
{
    size_t length;

    if (destination == NULL || destination_size == 0U) {
        return;
    }

    if (source == NULL) {
        destination[0] = '\0';
        return;
    }

    length = strlen(source);

    if (length >= destination_size) {
        length = destination_size - 1U;
    }

    memcpy(destination, source, length);
    destination[length] = '\0';
}

static int load_text_file(
    const char *path,
    char *destination,
    size_t destination_size
)
{
    FILE *file;
    size_t used;

    if (path == NULL || destination == NULL || destination_size == 0U) {
        return 0;
    }

    file = fopen(path, "r");

    if (file == NULL) {
        return 0;
    }

    used = fread(destination, 1U, destination_size - 1U, file);

    if (ferror(file)) {
        fclose(file);
        destination[0] = '\0';
        return 0;
    }

    destination[used] = '\0';

    if (!feof(file)) {
        fclose(file);
        destination[0] = '\0';
        return 0;
    }

    return fclose(file) == 0;
}

static int readable_file_ready(const char *path)
{
    FILE *file;

    file = fopen(path, "a+");

    if (file == NULL) {
        return 0;
    }

    return fclose(file) == 0;
}

void shakti_memory_init(shakti_memory_state_t *memory)
{
    if (memory == NULL) {
        return;
    }

    memset(memory, 0, sizeof(*memory));
}

int shakti_memory_load_always(
    shakti_memory_state_t *memory,
    const char *goal_path,
    const char *notebook_path,
    const char *menu_path,
    const char *long_term_path
)
{
    if (memory == NULL) {
        return 0;
    }

    memory->goal_loaded = (unsigned char)load_text_file(
        goal_path,
        memory->locked_goal,
        sizeof(memory->locked_goal)
    );

    memory->notebook_loaded = (unsigned char)load_text_file(
        notebook_path,
        memory->locked_notebook,
        sizeof(memory->locked_notebook)
    );

    memory->menu_loaded = (unsigned char)load_text_file(
        menu_path,
        memory->locked_menu,
        sizeof(memory->locked_menu)
    );

    memory->long_term_ready = (unsigned char)readable_file_ready(
        long_term_path
    );

    return memory->goal_loaded &&
           memory->notebook_loaded &&
           memory->menu_loaded &&
           memory->long_term_ready;
}

int shakti_memory_reload_notebook(
    shakti_memory_state_t *memory,
    const char *notebook_path
)
{
    if (memory == NULL) {
        return 0;
    }

    memory->notebook_loaded = (unsigned char)load_text_file(
        notebook_path,
        memory->locked_notebook,
        sizeof(memory->locked_notebook)
    );

    return memory->notebook_loaded;
}

int shakti_memory_reload_goal(
    shakti_memory_state_t *memory,
    const char *goal_path
)
{
    if (memory == NULL) {
        return 0;
    }

    memory->goal_loaded = (unsigned char)load_text_file(
        goal_path,
        memory->locked_goal,
        sizeof(memory->locked_goal)
    );

    return memory->goal_loaded;
}

void shakti_memory_remember(
    shakti_memory_state_t *memory,
    const shakti_tick_t *tick,
    const char *text
)
{
    size_t working_index;
    size_t short_index;

    if (memory == NULL || tick == NULL || text == NULL) {
        return;
    }

    if (memory->working_count < SHAKTI_WORKING_SLOTS) {
        working_index = memory->working_count;
        memory->working_count++;
    } else {
        size_t index;

        for (index = 1U; index < SHAKTI_WORKING_SLOTS; ++index) {
            memory->working[index - 1U] = memory->working[index];
        }

        working_index = SHAKTI_WORKING_SLOTS - 1U;
    }

    memory->working[working_index].tick = *tick;
    copy_text__memory_h(
        memory->working[working_index].text,
        sizeof(memory->working[working_index].text),
        text
    );

    short_index = memory->short_term_next;
    memory->short_term[short_index].tick = *tick;
    copy_text__memory_h(
        memory->short_term[short_index].text,
        sizeof(memory->short_term[short_index].text),
        text
    );

    memory->short_term_next =
        (memory->short_term_next + 1U) % SHAKTI_SHORT_TERM_SLOTS;

    if (memory->short_term_count < SHAKTI_SHORT_TERM_SLOTS) {
        memory->short_term_count++;
    }
}

void shakti_memory_print_status(const shakti_memory_state_t *memory)
{
    if (memory == NULL) {
        return;
    }

    printf(
        "Convergence memory: %lu of %u isolated slots.\n",
        (unsigned long)memory->working_count,
        (unsigned int)SHAKTI_WORKING_SLOTS
    );

    printf(
        "Recent event cache: %lu of %u entries. Complete reflection blocks remain append-only in long-term memory.\n",
        (unsigned long)memory->short_term_count,
        (unsigned int)SHAKTI_SHORT_TERM_SLOTS
    );

    printf(
        "Always-loaded blocks: Eden=%s School=resident Goal=%s Notebook=%s "
        "Menu=%s Long-term=%s.\n",
        "resident",
        memory->goal_loaded ? "loaded" : "missing",
        memory->notebook_loaded ? "loaded" : "missing",
        memory->menu_loaded ? "loaded" : "missing",
        memory->long_term_ready ? "ready" : "missing"
    );

    printf(
        "Fixed memory structure: %lu bytes.\n",
        (unsigned long)sizeof(*memory)
    );

    puts(
        "Long-term memory is append-only and callable in submitted chunks."
    );
}

int shakti_memory_recall_text_file(
    const char *path,
    const char *term,
    unsigned int maximum_matches
)
{
    FILE *file;
    char line[SHAKTI_LINE_CAPACITY];
    unsigned int matches;

    if (path == NULL || term == NULL || term[0] == '\0') {
        return 0;
    }

    file = fopen(path, "r");

    if (file == NULL) {
        return 0;
    }

    matches = 0U;

    while (fgets(line, sizeof(line), file) != NULL) {
        if (strstr(line, term) == NULL) {
            continue;
        }

        fputs(line, stdout);
        matches++;

        if (maximum_matches > 0U && matches >= maximum_matches) {
            break;
        }
    }

    if (fclose(file) != 0) {
        return 0;
    }

    printf("Recall matches: %u.\n", matches);

    return 1;
}
