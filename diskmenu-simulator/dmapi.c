#include <stdio.h>

#include "diskmenu.h"

#define dbg printf("at: %s %s %d\n", __FILE__, __FUNCTION__, __LINE__);

// ============================================================================
//                           HARDWARE ABSTRACTION
// ----------------------------------------------------------------------------

// Local functions for test/simulator abstraction
// file IO
void diskmenu_io_close(void) {
}

bool diskmenu_io_open(uint8_t *status, uint8_t fopen_mode) {
    return true;
}

     // ARB: remove after refactor
uint16_t diskmenu_io_read_buf(uint8_t *buffer,
                              uint16_t u16_buf_size) {
    static char *msg = "hello\n";

    strncpy((char *) buffer, msg, u16_buf_size);

    return strlen(msg);
}

void diskmenu_io_putc(uint8_t c) {
}

void diskmenu_io_write_buf(uint8_t* buffer, uint16_t size) {
}

uint16_t diskmenu_io_getc(void) {
    return 'X';
}

bool diskmenu_io_eof(void) {
    return true;
}

bool diskmenu_io_create(uint8_t *status, char *filename) {
    return false;
}

// filesystem navigation
int files_index = 0;
char files[12][40] = {
    "file one-001.txt",
    "file two-001.txt",
    "file three-001.txt",
    "file four-001.txt",
    "file five-001.txt",
    "file six-001.txt",
    "file seven-001.txt",
    "file eight-001.txt",
    "file nine-001.txt",
    "file ten-001.txt",
    "file eleven-001.txt",
    "file twelve-001.txt"
};

bool diskmenu_device_open(void) {
    return true;
}

bool diskmenu_device_close(void) {
    return true;
}

bool diskmenu_filelist_init(int *num_entries) {
    if (num_entries) {
        *num_entries = 12;
    }
    //files_index = 0;
    return true;
}

bool diskmenu_filelist_find(char *output, uint8_t length, char *pattern) {
    int n;
    diskmenu_filelist_init(&n);

    for (int i = 0; i < n; ++i) {
        if (0 == strncmp(pattern, files[i], strlen(pattern))) {
            files_index = i;

            if (output) {
                strncpy(output, files[i], length);
            }
            return true;
        }
    }

    return false;
}

bool diskmenu_filelist_goto(char *output, int length, uint8_t index) {
    int n;
    diskmenu_filelist_init(&n);

    if (index < n) {
        files_index = index;

        if (output) {
            strncpy(output, files[index], length);
        }
        return true;
    }

    return false;
}

void diskmenu_filelist_close(void) {
}

// display
#define DISPLAY_LINE_MAXLEN 42
char display_lines[8][DISPLAY_LINE_MAXLEN + 5 + 1];

void diskmenu_display_clear(int line_no, uint8_t bg) {
    for (int i = 0; i < DISPLAY_LINE_MAXLEN; ++i) {
        display_lines[line_no][i] = ' ';
    }
    display_lines[line_no][DISPLAY_LINE_MAXLEN] = '\0';
    strncpy(display_lines[line_no] + DISPLAY_LINE_MAXLEN + 1,
            "omfg", 4);
    display_lines[line_no][DISPLAY_LINE_MAXLEN + 5] = '\0';
}

void diskmenu_display_set(int line_no,
                          uint8_t offset,
                          const char *text,
                          uint8_t fg,
                          uint8_t bg) {
    if (!text) {
        return;
    }

    for (int i = offset; i < DISPLAY_LINE_MAXLEN; ++i) {
        if (text[i - offset]) {
            display_lines[line_no][i] = text[i - offset];
        }
        else {
            break;
        }
    }

    // display_lines[line_no][DISPLAY_LINE_MAXLEN] = '\0';
}

void diskmenu_display_draw(int line_no) {
}

void diskmenu_display_line(int line_no, const char *text) {
    diskmenu_display_clear(line_no, 0);
    diskmenu_display_set(line_no, 0, text, 0xa, 0);
    diskmenu_display_draw(line_no);
}

void diskmenu_display_init(void) {
    for (int i = 0; i < 8; ++i) {
        diskmenu_display_clear(i, 0);
    }
}

void diskmenu_display_print(void) {
    printf("===============================\n");
    for (int i = 0; i < 8; ++i) {
        printf("%s\n", display_lines[i]);
    }
    printf("-------------------------------\n");
}

uint8_t display_font_string_position(const char* str, uint8_t pos) {
    return strlen(str) + pos;
}

// flash
uint8_t diskmenu_flash_scene_id(void) {
    return 0;
}

void diskmenu_flash_read(
                      uint8_t scene_id,
                      scene_state_t *scene,
                      char (*text)[SCENE_TEXT_LINES][SCENE_TEXT_CHARS]) {
}

const char *diskmenu_flash_scene_text(uint8_t scene_id) {
    return "file seven";
}

void diskmenu_flash_write(
                     uint8_t scene_id,
                     scene_state_t *scene,
                     char (*text)[SCENE_TEXT_LINES][SCENE_TEXT_CHARS]) {
}

void diskmenu_dbg(const char *str) {
    printf("dbg: %s\n", str);
}
