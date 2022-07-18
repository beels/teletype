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
static void tele_usb_disk_render_menu_line(int item, int line_no, int marker);
static void tele_usb_disk_write_file(char *filename, int preset);
static void tele_usb_disk_read_file(char *filename, int preset);
static void tele_usb_disk_write_and_read(void);

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

static int front_counter = 0;
static int key_counter = 0;
static bool long_press = false;
static int menu_selection = 3;
static char filename_buffer[FNAME_BUFFER_LEN];
static char nextname_buffer[FNAME_BUFFER_LEN];

enum { kBlank = 0, kCurrent, kSelected };

void tele_usb_disk_handler_Front(int32_t data) {
    if (0 == data) {
        // button down; start timer
        key_counter = 7;
    }
    else {
        // button up; cancel timer
        key_counter = 0;

        if (long_press) {
            long_press = false;

            tele_usb_exec();
            tele_usb_disk_finish();
        }
        else {
            // cycle menu selection
            tele_usb_disk_render_menu_line(menu_selection, menu_selection + 2,
                                           kBlank);

            menu_selection = (menu_selection + 1) % 4;

            tele_usb_disk_render_menu_line(menu_selection, menu_selection + 2,
                                           kCurrent);
        }
    }
}

void tele_usb_disk_handler_KeyTimer(int32_t data) {
    if (0 < key_counter) {
        key_counter--;
    }

    if (1 == key_counter) {
        // long press action
        key_counter = 0;

        long_press = true;

        tele_usb_disk_render_menu_line(menu_selection, menu_selection + 2,
                                       kSelected);
    }
}

void tele_usb_exec() {
    switch (menu_selection) {
        case 0: {
            // Write to file
            tele_usb_disk_write_file(filename_buffer, preset_select);
            nav_filelist_reset();
            nav_exit();
        } break;
        case 1: {
            // Read from file
            tele_usb_disk_read_file(filename_buffer, preset_select);
            nav_filelist_reset();
            nav_exit();
        } break;
        case 2: {
            // Legacy action

            // clear screen
            for (size_t i = 0; i < 8; i++) {
                region_fill(&line[i], 0);
                region_draw(&line[i]);
            }

            // read/write files
            tele_usb_disk_write_and_read();
        } break;
        case 3: {
            // Exit
            // do nothing
        } break;
        default: {
            // error
            print_dbg("\r\ninvalid action");
        } break;
    }
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

void tele_usb_disk_render_menu_line(int item, int line_no, int marker) {
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

    switch (item) {
        case 0: { // Menu line 1: Write to file 'abcd.123'
            strcpy(text_buffer, "Write to file '");
            strcat(text_buffer, filename_buffer);
            strcat(text_buffer, "'");

            font_string_region_clip(&line[line_no], text_buffer,
                                    gutter + 2, 0, 0xa, 0);
        } break;

        case 1: { // Menu line 2: Read from file 'abcd.123'
            strcpy(text_buffer, "Read from file '");
            strcat(text_buffer, filename_buffer);
            strcat(text_buffer, "'");

            font_string_region_clip(&line[line_no], text_buffer,
                                    gutter + 2, 0, 0xa, 0);
        } break;

        case 2: { // Menu line 3: Legacy WRITE/READ operation
            strcpy(text_buffer, "Legacy WRITE/READ operation");

            font_string_region_clip(&line[line_no], text_buffer,
                                    gutter + 2, 0, 0xa, 0);
        } break;

        case 3: { // Menu line 4: Exit USB disk mode
            strcpy(text_buffer, "Exit USB disk mode");
            font_string_region_clip(&line[line_no], text_buffer,
                                    gutter + 2, 0, 0xa, 0);
        } break;

        case 4: { // Menu line 0: filename iterator
            if (nextname_buffer[0]) {
                font_string_region_clip(&line[line_no], nextname_buffer,
                                        gutter + 2, 0, 0xa, 0);
            }
            else {
                strcpy(text_buffer, "(");
                strcat(text_buffer, filename_buffer);
                strcat(text_buffer, ")");
            }
        } break;

        default: {} break;
    }

    region_draw(&line[line_no]);
}

void tele_usb_disk_init() {
    // disable event handlers while doing USB write
    assign_msc_event_handlers();

    // disable timers
    default_timers_enabled = false;

    // clear screen
    for (size_t i = 0; i < 8; i++) {
        region_fill(&line[i], 0);
        region_draw(&line[i]);
    }

    front_counter = 0;
    key_counter = 0;
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

    tele_usb_disk_init();

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
        char preset_buffer[3];
        itoa(preset_select, preset_buffer, 10);

        // Parse selected preset title
        char preset_title[40];
        strcpy(preset_title, flash_scene_text(preset_select, 0));

        // Parse or generate target filename for selected preset
        nextname_buffer[0] = '\0';
        int wc_start;
        if (!tele_usb_parse_target_filename(filename_buffer, preset_select)) {
            strcpy(filename_buffer, "tt00");
            if (10 <= preset_select) {
                strcpy(filename_buffer + 2, preset_buffer);
            }
            else if (0 < preset_select) {
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
        tele_usb_disk_render_menu_line(4, 1, kBlank);
        tele_usb_disk_render_menu_line(0, 2, kBlank);
        tele_usb_disk_render_menu_line(1, 3, kBlank);
        tele_usb_disk_render_menu_line(2, 4, kBlank);
        tele_usb_disk_render_menu_line(3, 5, kCurrent);
        menu_selection = 3;

        break;
    }
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
        if (!file_open(FOPEN_MODE_R))
            print_dbg("\r\ncan't open");
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
}

void tele_usb_disk_write_and_read() {
    // WRITE SCENES
    char text_buffer[40];
    char filename[FNAME_BUFFER_LEN];
    strcpy(filename, "tt00s.txt");

    print_dbg("\r\nwriting scenes");
    strcpy(text_buffer, "WRITE");
    region_fill(&line[0], 0);
    font_string_region_clip(&line[0], text_buffer, 2, 0, 0xa, 0);
    region_draw(&line[0]);

    for (int i = 0; i < SCENE_SLOTS; i++) {
        strcat(text_buffer, ".");  // strcat is dangerous, make sure the
                                   // buffer is large enough!
        region_fill(&line[0], 0);
        font_string_region_clip(&line[0], text_buffer, 2, 0, 0xa, 0);
        region_draw(&line[0]);

        tele_usb_disk_write_file(filename, i);

        if (filename[3] == '9') {
            filename[3] = '0';
            filename[2]++;
        }
        else
            filename[3]++;

        print_dbg(".");
    }

    nav_filelist_reset();


    // READ SCENES
    strcpy(filename, "tt00.txt");
    print_dbg("\r\nreading scenes...");

    strcpy(text_buffer, "READ");
    region_fill(&line[1], 0);
    font_string_region_clip(&line[1], text_buffer, 2, 0, 0xa, 0);
    region_draw(&line[1]);

    for (int i = 0; i < SCENE_SLOTS; i++) {
        scene_state_t scene;
        ss_init(&scene);
        char text[SCENE_TEXT_LINES][SCENE_TEXT_CHARS];
        memset(text, 0, SCENE_TEXT_LINES * SCENE_TEXT_CHARS);

        strcat(text_buffer, ".");  // strcat is dangerous, make sure the
                                   // buffer is large enough!
        region_fill(&line[1], 0);
        font_string_region_clip(&line[1], text_buffer, 2, 0, 0xa, 0);
        region_draw(&line[1]);
        if (nav_filelist_findname(filename, 0)) {
            print_dbg("\r\nfound: ");
            print_dbg(filename);
            if (!file_open(FOPEN_MODE_R))
                print_dbg("\r\ncan't open");
            else {
                tt_deserializer_t tele_usb_reader;
                tele_usb_reader.read_char = &tele_usb_getc;
                tele_usb_reader.eof = &tele_usb_eof;
                tele_usb_reader.print_dbg = &print_dbg;
                tele_usb_reader.data =
                    NULL;  // asf disk i/o holds state, no handles needed
                deserialize_scene(&tele_usb_reader, &scene, &text);

                file_close();
                flash_write(i, &scene, &text);
            }
        }

        nav_filelist_reset();

        if (filename[3] == '9') {
            filename[3] = '0';
            filename[2]++;
        }
        else
            filename[3]++;
    }

    nav_exit();
}
