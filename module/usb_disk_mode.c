#include "usb_disk_mode.h"

#include <ctype.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "flash.h"
#include "globals.h"
#include "scene_serialization.h"
#include "adc.h"

// libavr32
#include "events.h"
#include "font.h"
#include "interrupts.h"
#include "region.h"
#include "util.h"

// this
#include "filename.h"
#include "mergesort.h"

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
void tele_usb_putc(void* self_data, uint8_t c);
void tele_usb_write_buf(void* self_data, uint8_t* buffer, uint16_t size);
uint16_t tele_usb_getc(void* self_data);
bool tele_usb_eof(void* self_data);

// Local functions for test/simulator abstraction
// file IO
static void diskmenu_io_close(void);
static bool diskmenu_io_open(uint8_t *status, uint8_t fopen_mode);
            // ARB: remove after refactor
static uint16_t diskmenu_io_read_buf(uint8_t *buffer,
                                          uint16_t u16_buf_size);
static void diskmenu_io_putc(uint8_t c);
static void diskmenu_io_write_buf(uint8_t* buffer, uint16_t size);
static uint16_t diskmenu_io_getc(void);
static bool diskmenu_io_eof(void);
static bool diskmenu_io_create(uint8_t *status, char *filename);
// filesystem navigation
static bool diskmenu_device_open(void);
static bool diskmenu_device_close(void);
static bool diskmenu_filelist_init(int *num_entries);
static bool diskmenu_filelist_find(char *output, uint8_t length, char *pattern);
static bool diskmenu_filelist_goto(char *output, int len, uint8_t index);
static void diskmenu_filelist_close(void);
// display
static void diskmenu_display_clear(int line_no, uint8_t bg);
static void diskmenu_display_set(int line_no,
                                 uint8_t offset,
                                 const char *text,
                                 uint8_t fg,
                                 uint8_t bg);
static void diskmenu_display_draw(int line_no);
static void diskmenu_display_line(int line_no, const char *text);

// Subsystem control
void tele_usb_disk(void);
void tele_usb_disk_handler_Front(int32_t);
void tele_usb_disk_handler_KeyTimer(int32_t);
static void main_menu_PollADC(int32_t);
static void disk_browse_PollADC(int32_t);
static void tele_usb_disk_init(void);
static void tele_usb_disk_finish(void);

// Library functions
static void diskmenu_exec(void);
static bool diskmenu_parse_target_filename(char *buffer, uint8_t preset);
static bool diskmenu_iterate_filename(char *output, char *pattern);
static void diskmenu_render_line(char *text, int line_no, int marker);
static void diskmenu_render_menu_line(int item, int line_no, int marker);
static void diskmenu_write_file(char *filename, int preset);
static void diskmenu_read_file(char *filename, int preset);
static void diskmenu_browse_init(char *filename,
                                      char *nextname,
                                      int preset);

static void main_menu_short_press(void);
static void main_menu_button_timeout(void);
static void main_menu_long_press(void);

static void (*short_press_action)(void);
static void (*button_timeout_action)(void);
static void (*long_press_action)(void);

void tele_usb_putc(void* self_data, uint8_t c) {
    diskmenu_io_putc(c);
}

void tele_usb_write_buf(void* self_data, uint8_t* buffer, uint16_t size) {
    diskmenu_io_write_buf(buffer, size);
}

uint16_t tele_usb_getc(void* self_data) {
    return diskmenu_io_getc();
}

bool tele_usb_eof(void* self_data) {
    return diskmenu_io_eof() != 0;
}

void diskmenu_io_close(void) {
    file_close();
}

bool diskmenu_io_open(uint8_t *status, uint8_t fopen_mode)
{
    if (file_open(fopen_mode)) {
        return true;
    }

    if (status) {
        *status = fs_g_status;
    }

    return false;
}

uint16_t diskmenu_io_read_buf(uint8_t *buffer, uint16_t u16_buf_size)
{
    return file_read_buf(buffer, u16_buf_size);
}

void diskmenu_io_putc(uint8_t c)
{
    file_putc(c);
}

void diskmenu_io_write_buf(uint8_t* buffer, uint16_t size)
{
    file_write_buf(buffer, size);
}

uint16_t diskmenu_io_getc(void) {
    return file_getc();
}

bool diskmenu_io_eof(void) {
    return file_eof() != 0;
}

bool diskmenu_io_create(uint8_t *status, char *filename) {
    if (nav_file_create((FS_STRING) filename)) {
        return false;
    }

    if (status) {
        *status = fs_g_status;
    }

    return false;
}

bool diskmenu_device_open(void) {
    // We assume that there is one and only one available LUN, otherwise it is
    // not safe to iterate through all possible LUN even after we finish
    // writing and reading scenes.

    for (uint8_t lun = 0; (lun < uhi_msc_mem_get_lun()) && (lun < 8); lun++) {
        // print_dbg("\r\nlun: ");
        // print_dbg_ulong(lun);

        // Mount drive
        nav_drive_set(lun);
        if (nav_partition_mount()) {
            return true;
        }
    }

    return false;
}

bool diskmenu_device_close(void) {
    // No-op.
    return true;
}

bool diskmenu_filelist_init(int *num_entries) {
    if (nav_filelist_single_enable(FS_FILE)) {
        if (num_entries) {
            *num_entries = nav_filelist_nb(FS_FILE);
        }
        return true;
    }

    return false;
}

bool diskmenu_filelist_find(char *output, uint8_t length, char *pattern) {
    if (nav_filelist_findname(pattern, false) && nav_filelist_validpos()) {
        if (output) {
            return nav_file_getname(output, length);
        }
        return true;
    }

    return false;
}

bool diskmenu_filelist_goto(char *output, int length, uint8_t index) {
    if (nav_filelist_goto(index) && nav_filelist_validpos()) {
        if (output) {
            return nav_file_getname(output, length);
        }
        return true;
    }

    return false;
}

void diskmenu_filelist_close() {
    nav_filelist_reset();
    nav_exit();
}

void diskmenu_display_clear(int line_no, uint8_t bg) {
    region_fill(&line[line_no], bg);
}

void diskmenu_display_set(int line_no,
                          uint8_t offset,
                          const char *text,
                          uint8_t fg,
                          uint8_t bg)
{
    font_string_region_clip(&line[line_no], text, offset + 2, 0, fg, bg);
}

void diskmenu_display_draw(int line_no) {
    region_draw(&line[line_no]);
}

void diskmenu_display_line(int line_no, const char *text)
{
    region_fill(&line[line_no], 0);

    if (text && text[0]) {
        font_string_region_clip(&line[line_no], text, 2, 0, 0xa, 0);
    }

    region_draw(&line[line_no]);
}

static int read_scaled_param(uint8_t resolution, uint8_t scale) {

    // The knob has a 12 bit range, and has a fair amount of jitter in the low
    // bits.  Division into more than 64 zones becomes increasingly unstable.
    //
    // Intentional knob movement is detected by
    // adc[1] >> 8 != last_knob >> 8
    //
    // Knob turns seem to be considered volatile, so to ensure that there is no
    // jitter in the output for selection-style values, it can be desireable to
    // have every other value in a selection-style param be a dead zone.
    //
    // ```
    // uint8_t value = adc[1] >> (12 - (1 + desired_bits));
    // uint8_t deadzone = value & 1;
    // value >>= 1;
    // if (deadzone || abs(value - last_value) < 2) {
    //     return;
    // }
    // last_value = value;
    // // now do stuff
    // ```

    static uint16_t last_knob = 0;

    uint16_t adc[4];

    adc_convert(&adc);

    uint16_t value = adc[1] >> (12 - (1 + resolution));

    uint16_t deadzone = value & 1;
    value >>= 1;

    if (deadzone || abs(value - last_knob) < 2) {
        value = last_knob;
    }
    else {
        last_knob = value;
    }

    // Now scale the value, which right now is at knob resolution.

    value = value / ((1 << resolution) / scale);

    if (scale - 1 < value) {
        value = scale - 1;
    }

    return value;
}

static int button_counter = 0;
static bool long_press = false;
static int menu_selection = 4;

            // ARB:
            // These buffers can be offsets into `copy_buffer`.

static char filename_buffer[FNAME_BUFFER_LEN];
static char nextname_buffer[FNAME_BUFFER_LEN];

#define DISPLAY_MAX_LEN 32
#define DISPLAY_BUFFER_LEN (DISPLAY_MAX_LEN + 1)

#define MAIN_MENU_PAGE_SIZE 5

enum { kBlank = 0, kCurrent, kSelected };
enum {
    kHelpText = -1,
    kReadFile = 0,
    kWriteFile,
    kWriteNextInSeries,
    kBrowse,
    kExit
};

static void main_menu_PollADC(int32_t data) {
    static int last_index = -10 * MAIN_MENU_PAGE_SIZE;

    int index = read_scaled_param(10, MAIN_MENU_PAGE_SIZE);

    if (index != last_index) {
        if (index == kWriteNextInSeries && !nextname_buffer[0]) {
            return;
        }

        // Update selected items
        diskmenu_render_menu_line(menu_selection, menu_selection + 1,
                                       kBlank);

        diskmenu_render_menu_line(index, index + 1,
                                       kCurrent);

        menu_selection = index;
        last_index = index;
    }
}

static
void main_menu_short_press() {
    // cycle menu selection
    diskmenu_render_menu_line(menu_selection, menu_selection + 1,
                                   kBlank);

    menu_selection = (menu_selection + 1) % 5;

    if (menu_selection == kWriteNextInSeries && !nextname_buffer[0]) {
        menu_selection = (menu_selection + 1) % 5;
    }

    diskmenu_render_menu_line(menu_selection, menu_selection + 1,
                                   kCurrent);
}

static
void main_menu_button_timeout() {
    diskmenu_render_menu_line(menu_selection, menu_selection + 1,
                                   kSelected);
}

static
void main_menu_long_press() {
    diskmenu_exec();
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

void diskmenu_exec() {
    uint8_t preset = flash_last_saved_scene();

    switch (menu_selection) {
        case kReadFile: {
            // Read from file
            diskmenu_read_file(filename_buffer, preset);
        } break;
        case kWriteFile: {
            // Write to file
            diskmenu_write_file(filename_buffer, preset);
        } break;
        case kWriteNextInSeries: {
            // Write to next file in series
            diskmenu_write_file(nextname_buffer, preset);
        } break;
        case kBrowse: {
            diskmenu_browse_init(filename_buffer,
                                      nextname_buffer,
                                      preset);
            return;
        } break;
        case kExit: {
            // Exit
            // do nothing
        } break;
        default: {
            // error
            print_dbg("\r\ninvalid action");
            return;
        } break;
    }

    diskmenu_filelist_close();

    tele_usb_disk_finish();
}

bool diskmenu_parse_target_filename(char *buffer, uint8_t preset) {
    char temp_filename[FNAME_BUFFER_LEN];

    // Parse selected preset title
    strcpy(temp_filename, flash_scene_text(preset, 0));

    if (0 == temp_filename[0]) {
        return false;
    }


            // ARB:
            // The ".txt" part might be ignored by the filesystem library,
            // since it follows the asterisk.

    strncat(temp_filename, "-*.txt", 44);
    for (int j = 0; j < strlen(temp_filename); ++j) {
        buffer[j] = tolower((int) temp_filename[j]);
    }
    buffer[strlen(temp_filename)] = 0;
    return true;
}

bool diskmenu_iterate_filename(char *output, char *pattern) {
    return diskmenu_filelist_find(output, FNAME_BUFFER_LEN, pattern);
}

void diskmenu_render_line(char *text, int line_no, int marker) {
    char text_buffer[DISPLAY_BUFFER_LEN];
    u8 gutter = font_string_position(">", 1) + 2;

    diskmenu_display_clear(line_no, 0);

    if (kCurrent == marker) {
        strcpy(text_buffer, ">");
    }
    else if (kSelected == marker) {
        strcpy(text_buffer, "*");
    }
    else {
        strcpy(text_buffer, " ");
    }

    diskmenu_display_set(line_no, 0, text_buffer, 0xa, 0);
    diskmenu_display_set(line_no, gutter, text, 0xa, 0);

    diskmenu_display_draw(line_no);
}

void diskmenu_render_menu_line(int item, int line_no, int marker) {
    char text_buffer[DISPLAY_BUFFER_LEN];

    switch (item) {
        case kHelpText: { // Menu line 0: Read from file 'abcd.123'
            diskmenu_render_line("PARAM: select; button: exec",
                                      line_no, marker);
        } break;
        case kReadFile: { // Menu line 0: Read from file 'abcd.123'
            strcpy(text_buffer, "Read '");
            filename_ellipsis(text_buffer + 6, filename_buffer, 22);
            strcat(text_buffer, "'");

            diskmenu_render_line(text_buffer, line_no, marker);
        } break;

        case kWriteFile: { // Menu line 1: Write to file 'abcd.123'
            strcpy(text_buffer, "Write '");
            filename_ellipsis(text_buffer + 7, filename_buffer, 21);
            strcat(text_buffer, "'");

            diskmenu_render_line(text_buffer, line_no, marker);
        } break;

        case kWriteNextInSeries: { // Menu line 2: filename iterator
            if (nextname_buffer[0]) {
                strcpy(text_buffer, "Write '");
                filename_ellipsis(text_buffer + 7, nextname_buffer, 21);
                strcat(text_buffer, "'");

                diskmenu_render_line(text_buffer, line_no, marker);
            }
        } break;

        case kBrowse: { // Menu line 3: Browse filesystem
            diskmenu_render_line("Browse USB disk", line_no, marker);
        } break;

        case kExit: { // Menu line 4: Exit USB disk mode
            uint8_t preset = flash_last_saved_scene();
            char preset_buffer[3];
            itoa(preset, preset_buffer, 10);

            strcpy(text_buffer, "Exit to scene ");
            strcat(text_buffer, preset_buffer);

            diskmenu_render_line(text_buffer, line_no, marker);
        } break;

        default: {} break;
    }
}

static bool diskmenu_discover_filenames(void) {
    if (!diskmenu_device_open()) {
        return false;
    }

    // Parse or generate target filename for selected preset
    nextname_buffer[0] = '\0';
    int wc_start;
    uint8_t preset = flash_last_saved_scene();
    if (!diskmenu_parse_target_filename(filename_buffer, preset)) {
        char preset_buffer[3];
        itoa(preset, preset_buffer, 10);
        strcpy(filename_buffer, "tt00");
        if (10 <= preset) {
            strcpy(filename_buffer + 2, preset_buffer);
        }
        else if (0 < preset) {
            strcpy(filename_buffer + 3, preset_buffer);
        }
        strcpy(filename_buffer + 4, ".txt");
    }
    else if (filename_find_wildcard_range(&wc_start, filename_buffer)) {
        diskmenu_filelist_init(NULL);

        while(diskmenu_iterate_filename(nextname_buffer, filename_buffer)) {
            ; // Do nothing
        }

        if (nextname_buffer[0]) {
            // We found at least one matching file.
            strncpy(filename_buffer,
                    nextname_buffer,
                    sizeof(filename_buffer));

            filename_increment_version(nextname_buffer, wc_start);
        }
        else {
            // We supply a default filename based on the title.
            strcpy(filename_buffer + wc_start, "001.txt");
        }

        diskmenu_filelist_close();
    }

    return true;
}

void tele_usb_disk_init() {
    // initial button handlers (main menu)
    short_press_action = &main_menu_short_press;
    button_timeout_action = &main_menu_button_timeout;
    long_press_action = &main_menu_long_press;
    app_event_handlers[kEventPollADC] = &main_menu_PollADC;

    button_counter = 0;
    long_press = false;

    // clear screen
    for (size_t i = 0; i < 8; i++) {
        diskmenu_display_line(i, NULL);
    }

    // Parse selected preset number
    uint8_t preset = flash_last_saved_scene();

    // Print selected preset title
    {
        char preset_title[DISPLAY_BUFFER_LEN];
        strcpy(preset_title, flash_scene_text(preset, 0));


            // ARB:
            // Can we just use flash_scene_text directly here?

        diskmenu_display_line(0, preset_title);
    }

    // Menu items
    diskmenu_render_menu_line(kReadFile,          1, kBlank);
    diskmenu_render_menu_line(kWriteFile,         2, kBlank);
    diskmenu_render_menu_line(kWriteNextInSeries, 3, kBlank);
    diskmenu_render_menu_line(kBrowse,            4, kBlank);
    diskmenu_render_menu_line(kExit,              5, kCurrent);
    menu_selection = 4;

    // Help text
    diskmenu_render_menu_line(kHelpText,          7, kBlank);
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

    if (diskmenu_discover_filenames()) {
        tele_usb_disk_init();
    }
    else {
        // Exit usb disk mode


            // ARB: need to add failure feedback on display

        // Clear screen; show error message
        // Pause for a second, or prompt for key press

        tele_usb_disk_finish();
    }
}

void diskmenu_write_file(char *filename, int preset) {
    scene_state_t scene;
    ss_init(&scene);

    char text[SCENE_TEXT_LINES][SCENE_TEXT_CHARS];
    memset(text, 0, SCENE_TEXT_LINES * SCENE_TEXT_CHARS);

    flash_read(preset, &scene, &text, 1, 1, 1);

    uint8_t status = 0;
    if (!diskmenu_io_create(&status, filename)) {
        // We still write the file if it already exists.
        if (status != FS_ERR_FILE_EXIST) {
#if USB_DISK_TEST == 1
            if (status == FS_LUN_WP) {
                // Test can be done only on no write protected
                // device
                return;
            }
            print_dbg("\r\nfail");
#endif
            return;
        }
    }

    if (!diskmenu_io_open(&status, FOPEN_MODE_W)) {
#if USB_DISK_TEST == 1
        if (status == FS_LUN_WP) {
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

    diskmenu_io_close();
}


            // ARB:
            // Should return bool so that we can report success/failure on
            // screen to the user.

void diskmenu_read_file(char *filename, int preset) {
    scene_state_t scene;
    ss_init(&scene);
    char text[SCENE_TEXT_LINES][SCENE_TEXT_CHARS];
    memset(text, 0, SCENE_TEXT_LINES * SCENE_TEXT_CHARS);

    if (diskmenu_filelist_find(NULL, 0, (FS_STRING)filename)) {
        // print_dbg("\r\nfound: ");
        // print_dbg(filename_buffer);
        if (!diskmenu_io_open(NULL, FOPEN_MODE_R)) {
            char text_buffer[DISPLAY_BUFFER_LEN];
            strcpy(text_buffer, "fail: ");
            strncat(text_buffer, filename, DISPLAY_BUFFER_LEN - 6);

            diskmenu_display_line(0, text_buffer);
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

            diskmenu_io_close();
            flash_write(preset, &scene, &text);
        }
    }
    else {
        char text_buffer[DISPLAY_BUFFER_LEN];
        strcpy(text_buffer, "no file: ");
        strncat(text_buffer, filename, DISPLAY_BUFFER_LEN - 9);

        diskmenu_display_line(0, text_buffer);
    }
}

static bool disk_browse_read_filename(mergesort_accessor_t *dummy,
                                      char *filename, int len, uint8_t index) {
    // The 'mergesort_accessor_t' argument is used in test doubles of this
    // function to provide information about the dummy "filesystem".

    if (diskmenu_filelist_goto(filename, len, index)) {
        // Success
        return true;
    }
    else {
        // Clear file name.
        filename[0] = 0;
        return false;
    }
}

static void disk_browse_read_sorted_filename(
                       uint8_t *index, char *filename, int len, int position) {
    disk_browse_read_filename(0, filename, len, index[position]);
}

#define DISK_BROWSE_PAGE_SIZE 7

static uint8_t *s_file_index;
static int disk_browse_num_files;
static bool disk_browse_force_render;

static void disk_browse_button_timeout(void) {
    int selected_entry = menu_selection % DISK_BROWSE_PAGE_SIZE;

    char filename[FNAME_BUFFER_LEN];
    disk_browse_read_sorted_filename(
            s_file_index, filename, FNAME_BUFFER_LEN, menu_selection);
    filename_ellipsis(filename, filename, 28);
    diskmenu_render_line(filename, selected_entry + 1, kSelected);
}


        // ARB:
        // Redundant.  We could copy the existing handler on the way into the
        // browser instead.

static void handler_None(int32_t data) {}

static void disk_browse_finish(void) {
    diskmenu_filelist_close();

    app_event_handlers[kEventPollADC] = &handler_None;
    tele_usb_disk_init();
}

static void disk_browse_navigate(int old_index, int new_index) {
    int page = new_index / DISK_BROWSE_PAGE_SIZE;
    bool update_page = (page != old_index / DISK_BROWSE_PAGE_SIZE);

    // Render current page

    int first_entry = page * DISK_BROWSE_PAGE_SIZE;
    int current_entry = new_index - first_entry;
    int last_entry = old_index - first_entry;

    for (int i = 0; i < DISK_BROWSE_PAGE_SIZE; ++i) {
        if (disk_browse_num_files <= first_entry + i) {
            diskmenu_display_line(i + 1, NULL);
        }
        else if (update_page || i == current_entry || i == last_entry) {
            char filename[FNAME_BUFFER_LEN];
            disk_browse_read_sorted_filename(s_file_index,
                                             filename,
                                             FNAME_BUFFER_LEN,
                                             first_entry + i);
            filename_ellipsis(filename, filename, 28);
            diskmenu_render_line(filename,
                                      i + 1,
                                      (i == current_entry) ? kCurrent
                                                           : kBlank);

            if (i == current_entry) {
                // Display title on line 0

                diskmenu_display_clear(0, 0x2);

                if (diskmenu_io_open(NULL, FOPEN_MODE_R)) {
                    uint8_t title[DISPLAY_BUFFER_LEN];

            // ARB: remove after refactor: too slow in loop.

                    diskmenu_io_read_buf(title, DISPLAY_MAX_LEN);
                    title[DISPLAY_MAX_LEN] = 0;
                    diskmenu_io_close();
                    for (int j = 0; j < DISPLAY_MAX_LEN; ++j) {
                        if (title[j] == '\n') {
                            title[j] = 0;
                            break;
                        }
                        else if (title[j] < ' ' || '~' < title[j]) {
                            title[j] = '.';
                        }
                    }

                    diskmenu_display_set(0, 0, (char *) title, 0xa, 0x2);
                }
                diskmenu_display_draw(0);
            }
        }
    }
}

static void disk_browse_short_press(void) {
    if (menu_selection < disk_browse_num_files - 1) {
        disk_browse_navigate(menu_selection, menu_selection + 1);
    }

    ++menu_selection;
}

static void disk_browse_long_press(void) {
    // The save/load filename is the one selected.

    disk_browse_read_sorted_filename(s_file_index,
            filename_buffer, FNAME_BUFFER_LEN, menu_selection);

    // We have a concrete file, so no concept of "save next in series".

    nextname_buffer[0] = 0;

    disk_browse_finish();
}

static void disk_browse_PollADC(int32_t data) {
    static int last_index = -10 * DISK_BROWSE_PAGE_SIZE;

    int index = read_scaled_param(10, disk_browse_num_files);

    if (disk_browse_force_render) {
        last_index = -10 * DISK_BROWSE_PAGE_SIZE;
        disk_browse_force_render = false;
    }

    if (index != last_index) {
        disk_browse_navigate(menu_selection, index);

        menu_selection = index;
        last_index = index;
    }
}

static void diskmenu_browse_init(char *filename,
                                 char *nextname,
                                 int preset)
{
    // Set event handlers
    short_press_action = &disk_browse_short_press;
    button_timeout_action = &disk_browse_button_timeout;
    long_press_action = &disk_browse_long_press;
    app_event_handlers[kEventPollADC] = &disk_browse_PollADC;

    button_counter = 0;
    long_press = false;

    // clear screen
    for (size_t i = 0; i < 8; i++) {
        diskmenu_display_line(i, NULL);
    }

    diskmenu_filelist_init(&disk_browse_num_files);

    // render browser
    char text_buffer[DISPLAY_BUFFER_LEN];
    itoa(disk_browse_num_files, text_buffer, 10);
    strcat(text_buffer, " files total");
    diskmenu_render_line(text_buffer, 0, kBlank);

    // Create file index
    // We borrow the copy buffer for temporary storage of the file index.
    s_file_index = (uint8_t *) copy_buffer;

    mergesort_accessor_t filename_accessor = {
                        .data = 0,
                        .get_value = disk_browse_read_filename
                    };

    uint8_t temp_index[256];
    mergesort(s_file_index, temp_index,
              ((char *)copy_buffer) + 256, sizeof(copy_buffer) - 256,
              disk_browse_num_files, FNAME_BUFFER_LEN,
              &filename_accessor);

    // Force rendering of current page.
    disk_browse_force_render = true;
}
