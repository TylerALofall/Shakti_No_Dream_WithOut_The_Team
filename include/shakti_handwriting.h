#ifndef SHAKTI_HANDWRITING_H
#define SHAKTI_HANDWRITING_H
#include <stddef.h>
int shakti_handwriting_render_char(unsigned char character, char rows[8][9]);
int shakti_handwriting_supports_text(const char *text);
int shakti_handwriting_write_text_artifact(const char *text, const char *path);
#endif
