#include "usb_disk_mode.h"

#include <ctype.h>
#include <stdint.h>
#include <string.h>

#include "flash.h"
#include "globals.h"
#include "scene_serialization.h"

// libavr32
#include "font.h"
#include "interrupts.h"
#include "region.h"
#include "util.h"

// this
#include "filename.h"

// asf
#include "delay.h"
#include "fat.h"
#include "file.h"
#include "fs_com.h"
#include "navigation.h"
#include "print_funcs.h"
#include "uhi_msc.h"
#include "uhi_msc_mem.h"
#include "usb_protocol_msc.h"

// Change to 1 to enable debug "fail" output from early test code.
#define USB_DISK_TEST 0

// Local functions for usb filesystem serialization
static void tele_usb_putc(void* self_data, uint8_t c);
static void tele_usb_write_buf(void* self_data, uint8_t* buffer, uint16_t size);
static uint16_t tele_usb_getc(void* self_data);
static bool tele_usb_eof(void* self_data);
static void tele_usb_disk_init(void);
static void tele_usb_disk_finish(void);
static void tele_usb_exec(void);
static bool tele_usb_parse_target_filename(char *buffer, uint8_t preset);
static bool tele_usb_disk_iterate_filename(char *output, char *pattern);
static void tele_usb_disk_render_line(char *text, int line_no, int marker);
static void tele_usb_disk_render_menu_line(int item, int line_no, int marker);
static void tele_usb_disk_write_file(char *filename, int preset);
static void tele_usb_disk_read_file(char *filename, int preset);
static void tele_usb_disk_browse_init(char *filename,
                                      char *nextname,
                                      int preset);

static void main_menu_short_press(void);
static void main_menu_button_timeout(void);
static void main_menu_long_press(void);

static void (*short_press_action)(void);
static void (*button_timeout_action)(void);
static void (*long_press_action)(void);

void tele_usb_putc(void* self_data, uint8_t c) {
    file_putc(c);
}

void tele_usb_write_buf(void* self_data, uint8_t* buffer, uint16_t size) {
    file_write_buf(buffer, size);
}

uint16_t tele_usb_getc(void* self_data) {
    return file_getc();
}

bool tele_usb_eof(void* self_data) {
    return file_eof() != 0;
}

static int button_counter = 0;
static bool long_press = false;
static int menu_selection = 4;

            // ARB:
            // These buffers can be offsets into `copy_buffer`.

static char filename_buffer[FNAME_BUFFER_LEN];
static char nextname_buffer[FNAME_BUFFER_LEN];

enum { kBlank = 0, kCurrent, kSelected };
enum {
    kHelpText = -1,
    kReadFile = 0,
    kWriteFile,
    kWriteNextInSeries,
    kBrowse,
    kExit
};

static
void main_menu_short_press() {
    // cycle menu selection
    tele_usb_disk_render_menu_line(menu_selection, menu_selection + 1,
                                   kBlank);

    menu_selection = (menu_selection + 1) % 5;

    if (menu_selection == kWriteNextInSeries && !nextname_buffer[0]) {
        menu_selection = (menu_selection + 1) % 5;
    }

    tele_usb_disk_render_menu_line(menu_selection, menu_selection + 1,
                                   kCurrent);
}

static
void main_menu_button_timeout() {
    tele_usb_disk_render_menu_line(menu_selection, menu_selection + 1,
                                   kSelected);
}

static
void main_menu_long_press() {
    tele_usb_exec();
}

void tele_usb_disk_handler_Front(int32_t data) {
    if (0 == data) {
        // button down; start timer
        button_counter = 7;
    }
    else {
        // button up; cancel timer
        button_counter = 0;

        if (long_press) {
            long_press = false;
            (*long_press_action)();
        }
        else {
            (*short_press_action)();
        }
    }
}

void tele_usb_disk_handler_KeyTimer(int32_t data) {
    // This `if` statement only stops the decrement, avoiding wraparound and or
    // undefined behavior in the extreme case.  Arming of the long press action
    // is triggered by passing through the value 1, and further decrements are
    // harmless.

    if (0 < button_counter) {
        button_counter--;
    }

    // Note that we arm the long press action on 1, not 0, because the button
    // counter is initialized to 0.  We could as easily arm on 0 and initialize
    // to -1, which would change the stop-decrement condition slightly.

    if (1 == button_counter) {
        // long press action
        button_counter = 0;
        long_press = true;

        (*button_timeout_action)();
    }
}

void tele_usb_exec() {
    uint8_t preset = flash_last_saved_scene();

    switch (menu_selection) {
        case kReadFile: {
            // Read from file
            tele_usb_disk_read_file(filename_buffer, preset);
        } break;
        case kWriteFile: {
            // Write to file
            tele_usb_disk_write_file(filename_buffer, preset);
        } break;
        case kWriteNextInSeries: {
            // Write to next file in series
            tele_usb_disk_write_file(nextname_buffer, preset);
        } break;
        case kBrowse: {
            tele_usb_disk_browse_init(filename_buffer,
                                      nextname_buffer,
                                      preset);
        } break;
        case kExit: {
            // Exit
            tele_usb_disk_finish();
            return;
        } break;
        default: {
            // error
            print_dbg("\r\ninvalid action");
            return;
        } break;
    }

    nav_filelist_reset();
    nav_exit();
}

bool tele_usb_parse_target_filename(char *buffer, uint8_t preset) {
    for (int i = 0; i < SCENE_TEXT_LINES; ++i) {
        const char *text = flash_scene_text(preset, i);

        if (0 == strncmp(text, ":FNAME:", 7)) {
            strncpy(buffer, text + 7, FNAME_BUFFER_LEN);
            for (int j = 0; j < strlen(buffer); ++j) {
                buffer[j] = tolower((int) buffer[j]);
            }
            return true;
        }
    }
    return false;
}

bool tele_usb_disk_iterate_filename(char *output, char *pattern) {
    return nav_filelist_findname(pattern, false)
        && nav_filelist_validpos()
        && nav_file_getname(output, FNAME_BUFFER_LEN);
}

void tele_usb_disk_render_line(char *text, int line_no, int marker) {
    char text_buffer[40];
    u8 gutter = font_string_position(">", 1) + 2;

    region_fill(&line[line_no], 0);

    if (kCurrent == marker) {
        strcpy(text_buffer, ">");
    }
    else if (kSelected == marker) {
        strcpy(text_buffer, "*");
    }
    else {
        strcpy(text_buffer, " ");
    }

    font_string_region_clip(&line[line_no], text_buffer, 2, 0, 0xa, 0);

    font_string_region_clip(&line[line_no], text, gutter + 2, 0, 0xa, 0);

    region_draw(&line[line_no]);
}

void tele_usb_disk_render_menu_line(int item, int line_no, int marker) {
    switch (item) {
        case kHelpText: { // Menu line 0: Read from file 'abcd.123'
            tele_usb_disk_render_line("short press: next; long: exec",
                                      line_no, marker);
        } break;
        case kReadFile: { // Menu line 0: Read from file 'abcd.123'
            char text_buffer[40];
            strcpy(text_buffer, "Read '");
            strcat(text_buffer, filename_buffer);
            strcat(text_buffer, "'");

            tele_usb_disk_render_line(text_buffer, line_no, marker);
        } break;

        case kWriteFile: { // Menu line 1: Write to file 'abcd.123'
            char text_buffer[40];
            strcpy(text_buffer, "Write '");
            strcat(text_buffer, filename_buffer);
            strcat(text_buffer, "'");

            tele_usb_disk_render_line(text_buffer, line_no, marker);
        } break;

        case kWriteNextInSeries: { // Menu line 2: filename iterator
            if (nextname_buffer[0]) {
                char text_buffer[40];
                strcpy(text_buffer, "Write '");
                strcat(text_buffer, nextname_buffer);
                strcat(text_buffer, "'");

                tele_usb_disk_render_line(text_buffer, line_no, marker);
            }
        } break;

        case kBrowse: { // Menu line 3: Browse filesystem
            tele_usb_disk_render_line("Browse USB disk", line_no, marker);
        } break;

        case kExit: { // Menu line 4: Exit USB disk mode
            tele_usb_disk_render_line("Exit USB disk mode", line_no, marker);
        } break;

        default: {} break;
    }
}

void tele_usb_disk_init() {
    // initial button handlers (main menu)
    short_press_action = &main_menu_short_press;
    button_timeout_action = &main_menu_button_timeout;
    long_press_action = &main_menu_long_press;

    button_counter = 0;
    long_press = false;

    // clear screen
    for (size_t i = 0; i < 8; i++) {
        region_fill(&line[i], 0);
        region_draw(&line[i]);
    }

    // We assume that there is one and only one available LUN, otherwise it is
    // not safe to iterate through all possible LUN even after we finish
    // writing and reading scenes.

    for (uint8_t lun = 0; (lun < uhi_msc_mem_get_lun()) && (lun < 8); lun++) {
        // print_dbg("\r\nlun: ");
        // print_dbg_ulong(lun);

        // Mount drive
        nav_drive_set(lun);
        if (!nav_partition_mount()) {
#if USB_DISK_TEST == 1
            if (fs_g_status == FS_ERR_HW_NO_PRESENT) {
                continue;
            }
            print_dbg("\r\nfail");
#endif
            continue;
        }

        // Parse selected preset number
        uint8_t preset = flash_last_saved_scene();
        char preset_buffer[3];
        itoa(preset, preset_buffer, 10);

        // Parse selected preset title
        char preset_title[40];
        strcpy(preset_title, flash_scene_text(preset, 0));

        // Parse or generate target filename for selected preset
        nextname_buffer[0] = '\0';
        int wc_start;
        if (!tele_usb_parse_target_filename(filename_buffer, preset)) {
            strcpy(filename_buffer, "tt00");
            if (10 <= preset) {
                strcpy(filename_buffer + 2, preset_buffer);
            }
            else if (0 < preset) {
                strcpy(filename_buffer + 3, preset_buffer);
            }
            strcpy(filename_buffer + 4, ".txt");
        }
        else if (filename_find_wildcard_range(&wc_start, filename_buffer))
        {
            while(tele_usb_disk_iterate_filename(nextname_buffer,
                                                 filename_buffer))
            {
                ; // Do nothing
            }

            strncpy(filename_buffer, nextname_buffer, sizeof(filename_buffer));

            filename_increment_version(nextname_buffer, wc_start);

            nav_filelist_reset();
        }

        // Print selected preset number and title
        {
            region_fill(&line[0], 0);
            font_string_region_clip(&line[0], preset_buffer, 2, 0, 0xa, 0);
            u8 pos = font_string_position(preset_buffer,
                                          strlen(preset_buffer));
            pos += font_string_position(" ", 1);
            font_string_region_clip(&line[0],
                                    preset_title,
                                    pos + 2, 0, 0xa, 0);
            region_draw(&line[0]);
        }

        // Menu items
        tele_usb_disk_render_menu_line(kReadFile,          1, kBlank);
        tele_usb_disk_render_menu_line(kWriteFile,         2, kBlank);
        tele_usb_disk_render_menu_line(kWriteNextInSeries, 3, kBlank);
        tele_usb_disk_render_menu_line(kBrowse,            4, kBlank);
        tele_usb_disk_render_menu_line(kExit,              5, kCurrent);
        menu_selection = 4;

        // Help text
        tele_usb_disk_render_menu_line(kHelpText,          7, kBlank);

        break;
    }
}

void tele_usb_disk_finish() {
    // renable teletype
    set_mode(M_LIVE);
    assign_main_event_handlers();
    default_timers_enabled = true;
}

// usb disk mode entry point
void tele_usb_disk() {
    print_dbg("\r\nusb");

    // disable event handlers while doing USB write
    assign_msc_event_handlers();

    // disable timers
    default_timers_enabled = false;

    tele_usb_disk_init();
}

void tele_usb_disk_write_file(char *filename, int preset) {
    scene_state_t scene;
    ss_init(&scene);

    char text[SCENE_TEXT_LINES][SCENE_TEXT_CHARS];
    memset(text, 0, SCENE_TEXT_LINES * SCENE_TEXT_CHARS);

    flash_read(preset, &scene, &text, 1, 1, 1);

    if (!nav_file_create((FS_STRING)filename)) {
        if (fs_g_status != FS_ERR_FILE_EXIST) {
#if USB_DISK_TEST == 1
            if (fs_g_status == FS_LUN_WP) {
                // Test can be done only on no write protected
                // device
                return;
            }
            print_dbg("\r\nfail");
#endif
            return;
        }
    }

    if (!file_open(FOPEN_MODE_W)) {
#if USB_DISK_TEST == 1
        if (fs_g_status == FS_LUN_WP) {
            // Test can be done only on no write protected
            // device
            return;
        }
        print_dbg("\r\nfail");
#endif
        return;
    }

    tt_serializer_t tele_usb_writer;
    tele_usb_writer.write_char = &tele_usb_putc;
    tele_usb_writer.write_buffer = &tele_usb_write_buf;
    tele_usb_writer.print_dbg = &print_dbg;
    tele_usb_writer.data =
        NULL;  // asf disk i/o holds state, no handles needed
    serialize_scene(&tele_usb_writer, &scene, &text);

    file_close();
}

void tele_usb_disk_read_file(char *filename, int preset) {
    scene_state_t scene;
    ss_init(&scene);
    char text[SCENE_TEXT_LINES][SCENE_TEXT_CHARS];
    memset(text, 0, SCENE_TEXT_LINES * SCENE_TEXT_CHARS);

    if (nav_filelist_findname((FS_STRING)filename, 0)) {
        // print_dbg("\r\nfound: ");
        // print_dbg(filename_buffer);
        if (!file_open(FOPEN_MODE_R)) {
            char text_buffer[40];
            strcpy(text_buffer, "fail: ");
            strcat(text_buffer, filename);

            region_fill(&line[0], 0);
            font_string_region_clip(&line[0], text_buffer, 2, 0, 0xa, 0);
            region_draw(&line[0]);
            // print_dbg("\r\ncan't open");
        }
        else {
            tt_deserializer_t tele_usb_reader;
            tele_usb_reader.read_char = &tele_usb_getc;
            tele_usb_reader.eof = &tele_usb_eof;
            tele_usb_reader.print_dbg = &print_dbg;
            tele_usb_reader.data =
                NULL;  // asf disk i/o holds state, no handles needed
            deserialize_scene(&tele_usb_reader, &scene, &text);

            file_close();
            flash_write(preset, &scene, &text);
        }
    }
    else {
        char text_buffer[40];
        strcpy(text_buffer, "no file: ");
        strcat(text_buffer, filename);

        region_fill(&line[0], 0);
        font_string_region_clip(&line[0], text_buffer, 2, 0, 0xa, 0);
        region_draw(&line[0]);
    }
}

static void disk_browse_short_press(void) {
    static int state = 0;

    if (++state % 2) {
        tele_usb_disk_render_line("==============", 0, kCurrent);
    }
    else {
        tele_usb_disk_render_line("<exit_browser>", 0, kCurrent);
    }
}

static void disk_browse_button_timeout(void) {
    tele_usb_disk_render_line("<exit_browser>", 0, kSelected);
}

static void disk_browse_long_press(void) {
    tele_usb_disk_init();
}

static void tele_usb_disk_browse_init(char *filename,
                                      char *nextname,
                                      int preset)
{
    // Set event handlers
    short_press_action = &disk_browse_short_press;
    button_timeout_action = &disk_browse_button_timeout;
    long_press_action = &disk_browse_long_press;

    button_counter = 0;
    long_press = false;

    // clear screen
    for (size_t i = 0; i < 8; i++) {
        region_fill(&line[i], 0);
        region_draw(&line[i]);
    }

    // render browser
    tele_usb_disk_render_line("<exit_browser>", 0, kCurrent);
}
