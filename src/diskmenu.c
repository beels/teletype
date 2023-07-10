#include <ctype.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>  // for debugging

#include "util.h"

#include "diskmenu.h"
#include "diskmenu_api.h"
#include "filename.h"
#include "mergesort.h"
#include "scene_serialization.h"

#define dbg printf("at: %s %s %d\n", __FILE__, __FUNCTION__, __LINE__);

// Local functions for usb filesystem serialization
static void tele_usb_putc(void* self_data, uint8_t c);
static void tele_usb_write_buf(void* self_data, uint8_t* buffer, uint16_t size);
static uint16_t tele_usb_getc(void* self_data);
static bool tele_usb_eof(void* self_data);

static void main_menu_short_press(void);
static void main_menu_handle_PollADC(void);
static void main_menu_handle_screenRefresh(void);

static void diskmenu_main_menu_init(void);
static void diskmenu_finish(void);
static void diskmenu_exec(void);
static bool diskmenu_parse_target_filename(char *buffer, uint8_t preset);
static bool diskmenu_iterate_filename(char *output, char *pattern);
static void diskmenu_render_line(char *text, int line_no, int marker);
static void diskmenu_render_menu_line(int item, int line_no, int marker);
static void diskmenu_write_file(char *filename, int preset);
static void diskmenu_read_file(char *filename, int preset);
static void diskmenu_browse_init(char *filename,
                                 char *nextname,
                                 int   preset);
static bool diskmenu_append_dir(char *base, uint8_t length, char *leaf);

static void page_select_init(char *filename,
                             char *nextname,
                             int   preset);
static void page_select_handle_PollADC(void);
static void page_select_handle_screenRefresh(void);
static void page_select_short_press(void);
static void page_select_render_page(int index);

static void item_select_init(void);
static void item_select_handle_PollADC(void);
static void item_select_handle_screenRefresh(void);
static void item_select_render_page(int page, int item);
static void item_select_short_press(void);

            // ARB: Need to audit and declare the disk_browse* functions.

static void disk_browse_render_line(int      line,
                                    char    *filename,
                                    bool     selected,
                                    uint8_t  fg);
static void disk_browse_render_page(int     first_file_index,
                                    int     page_size,
                                    int     item,
                                    uint8_t fg);

static bool diskmenu_discover_filenames(void);

static void tele_usb_disk_exec(void);

// ============================================================================
//                                 Utilities
// ----------------------------------------------------------------------------

static int button_counter = 0;

void tele_usb_disk_init() {
    button_counter = 0;

    // disable event handlers while doing USB write
    diskmenu_assign_advanced_menu_event_handlers();

    // disable timers
    diskmenu_set_default_timers_enabled(false);

            // ARB:
            // Include these configurations as arguments to diskmenu_init.
            // Also let diskmenu_init return bool so that we short-circuit
            // gracefully here???

    diskmenu_set_exit_handler(tele_usb_disk_finish);

    diskmenu_init();
}

// ============================================================================
//                              EVENT HANDLERS
// ----------------------------------------------------------------------------

void tele_usb_disk_handler_Front(int32_t data) {
    if (0 == data) {
        // Exec only on button up.
        return;
    }

    diskmenu_handle_short_press();
}

void tele_usb_disk_PollADC(int32_t data) {
    diskmenu_handle_PollADC();
}

void tele_usb_disk_handler_ScreenRefresh(int32_t data) {
    diskmenu_handle_ScreenRefresh();
}

// ============================================================================
//                           SERIALIZATION SUPPORT
// ----------------------------------------------------------------------------

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


// ============================================================================
//                              SUBSYSTEM STATE
// ----------------------------------------------------------------------------

#define DISPLAY_MAX_LEN 32
#define DISPLAY_BUFFER_LEN (DISPLAY_MAX_LEN + 1)

#define MAIN_MENU_PAGE_SIZE 5
#define DISK_BROWSE_PAGE_SIZE 7


            // ARB:
            // menu_selection *should* have the value of the currently-selected
            // item among the *currently-selectable* items.  In general, that
            // means it should be the index value returned by PollADC.  The
            // reason to have a global variable for this value is so that it
            // can be conveyed to the Front handler.

static int menu_selection = 4;
static int page_selection = -1;
static int param_knob_scaling = MAIN_MENU_PAGE_SIZE;
static int param_last_index = -1;


            // ARB:
            // - These buffers can be offsets into `copy_buffer`.
            //
            // - browse_directory can probably just steal from
            // 'filename_buffer'.

static char browse_directory[FNAME_BUFFER_LEN];
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

static void (*diskmenu_exit_handler)(void);
static void (*pollADC_handler)(void);
static void (*screenRefresh_handler)(void);
static void (*short_press_action)(void);

static uint8_t *s_file_index;
static int disk_browse_num_files;

// ============================================================================
//                              EVENT HANDLERS
// ----------------------------------------------------------------------------

static void main_menu_handle_PollADC(void) {
    int index = diskmenu_param_scaled(10, param_knob_scaling);

    if (0 <= param_last_index && index != param_last_index) {
        if (index == kWriteNextInSeries && !nextname_buffer[0]) {
            return;
        }

        // Update selected items
        diskmenu_render_menu_line(menu_selection, menu_selection + 1,
                                       kBlank);

        diskmenu_render_menu_line(index, index + 1,
                                       kCurrent);

        menu_selection = index;
    }

    param_last_index = index;
}

static void main_menu_handle_screenRefresh(void) {
    diskmenu_display_print();
}

static
void main_menu_short_press() {
    diskmenu_exec();
}

void diskmenu_set_exit_handler(void (*exit_handler)(void)) {
    diskmenu_exit_handler = exit_handler;
}

void diskmenu_init(void) {

    // This just keeps the PARAM knob quiet while we go through the disk access
    // operations a the start of diskmenu_init.

    pollADC_handler = NULL;
    screenRefresh_handler = NULL;

            // ARB:
            // Why is this being done before anything else?
            // This used to be the LUN scanning code.

    if (!diskmenu_discover_filenames()) {
        // Exit usb disk mode


            // ARB: need to add failure feedback on display

        // Clear screen; show error message
        // Pause for a second, or prompt for key press

        diskmenu_finish();

        return;
    }

    diskmenu_main_menu_init();
}

void diskmenu_handle_short_press() {
    (*short_press_action)();
}

void diskmenu_handle_PollADC() {
    if (pollADC_handler) {
        (*pollADC_handler)();
    }
}

void diskmenu_handle_ScreenRefresh() {
    if (screenRefresh_handler) {
        (*screenRefresh_handler)();
    }
}

void diskmenu_main_menu_init() {
    // initial button handlers (main menu)
    short_press_action = &main_menu_short_press;
    pollADC_handler = &main_menu_handle_PollADC;
    screenRefresh_handler = &main_menu_handle_screenRefresh;
    param_knob_scaling = MAIN_MENU_PAGE_SIZE;
    param_last_index = -1;

    // clear screen
    for (size_t i = 0; i < 8; i++) {
        diskmenu_display_line(i, NULL);
    }

    // Parse selected preset number
    uint8_t preset = diskmenu_flash_scene_id();

    // Print selected preset title
    {
        char preset_title[DISPLAY_BUFFER_LEN];
        strcpy(preset_title, diskmenu_flash_scene_text(preset));


            // ARB:
            // Can we just use diskmenu_flash_scene_text directly here?

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

void diskmenu_finish(void) {
        if (diskmenu_exit_handler) {
            (*diskmenu_exit_handler)();
        }
}

void diskmenu_exec() {
    uint8_t preset = diskmenu_flash_scene_id();

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
            //xxx print_dbg("\r\ninvalid action");
            return;
        } break;
    }

    diskmenu_filelist_close();

    diskmenu_finish();
}

bool diskmenu_parse_target_filename(char *buffer, uint8_t preset) {
    char temp_filename[FNAME_BUFFER_LEN];

    // Parse selected preset title
    strcpy(temp_filename, diskmenu_flash_scene_text(preset));

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
    u8 gutter = display_font_string_position(">", 1) + 2;

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
            uint8_t preset = diskmenu_flash_scene_id();
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
    uint8_t preset = diskmenu_flash_scene_id();
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

void diskmenu_write_file(char *filename, int preset) {
    scene_state_t scene;
    ss_init(&scene);

    char text[SCENE_TEXT_LINES][SCENE_TEXT_CHARS];
    memset(text, 0, SCENE_TEXT_LINES * SCENE_TEXT_CHARS);

    diskmenu_flash_read(preset, &scene, &text);

    uint8_t status = 0;
    if (!diskmenu_io_create(&status, filename)) {
        // We still write the file if it already exists.
        if (status != kErrFileExists) {
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

    if (!diskmenu_io_open(&status, kModeW)) {
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
    tele_usb_writer.print_dbg = &diskmenu_dbg;
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

    if (diskmenu_filelist_find(NULL, 0, filename)) {
        // print_dbg("\r\nfound: ");
        // print_dbg(filename_buffer);
        if (!diskmenu_io_open(NULL, kModeR)) {
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
            tele_usb_reader.print_dbg = &diskmenu_dbg;
            tele_usb_reader.data =
                NULL;  // asf disk i/o holds state, no handles needed
            deserialize_scene(&tele_usb_reader, &scene, &text);

            diskmenu_io_close();
            diskmenu_flash_write(preset, &scene, &text);
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

static void disk_browse_finish(void) {
    diskmenu_filelist_close();

    diskmenu_main_menu_init();
}

static void diskmenu_browse_init(char *filename,
                                 char *nextname,
                                 int   preset)
{
    strncpy(browse_directory, "/", sizeof(browse_directory));
    page_select_init(filename, nextname, preset);
}

static void page_select_init(char *filename,
                             char *nextname,
                             int   preset)
{
    // Set event handlers
    short_press_action = &page_select_short_press;
    pollADC_handler = &page_select_handle_PollADC;
    screenRefresh_handler = &page_select_handle_screenRefresh;

    // clear screen
    for (size_t i = 0; i < 8; i++) {
        diskmenu_display_line(i, NULL);
    }

    diskmenu_filelist_init(&disk_browse_num_files);

    if (0 == strcmp(browse_directory, "/")) {
        param_knob_scaling = 1 + disk_browse_num_files / 7;
    }
    else {
        param_knob_scaling = 2 + disk_browse_num_files / 7;
    }
    param_last_index = -1;

    // render wait screen display while sorting file liet
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
              ((char *) copy_buffer) + 256,
              sizeof(copy_buffer) - 256,
              disk_browse_num_files, FNAME_BUFFER_LEN,
              &filename_accessor);

    // Render current page.
    page_selection = diskmenu_param_scaled(8, param_knob_scaling);

            // ARB: the parameter here is not necessary

    page_select_render_page(page_selection);
}

static void page_select_handle_PollADC(void) {
    int index = diskmenu_param_scaled(8, param_knob_scaling);
    if (0 <= param_last_index && index != param_last_index) {
        page_selection = index;

            // ARB: the parameter here is not necessary

        page_select_render_page(index);
    }

    param_last_index = index;
}

static void page_select_handle_screenRefresh(void) {
    // Conventional call for simulator compatibility.
    diskmenu_display_print();
}

static void page_select_render_page(int index) {
    // Render current directory
    diskmenu_display_clear(0, 0);
    diskmenu_display_set(0, 0, browse_directory, 0xa, 0);
    diskmenu_display_draw(0);

    if (0 == strcmp(browse_directory, "/")) {
        int first_entry = index * DISK_BROWSE_PAGE_SIZE;
        disk_browse_render_page(first_entry, DISK_BROWSE_PAGE_SIZE, -1, 0x4);
    }
    else if (0 < index) {
        int first_entry = index * DISK_BROWSE_PAGE_SIZE - 1;
        disk_browse_render_page(first_entry, DISK_BROWSE_PAGE_SIZE, -1, 0x4);
    }
    else {
        // First page (0) of a non-root directory
        disk_browse_render_page(0, DISK_BROWSE_PAGE_SIZE - 1, -1, 0x4);
    }
}

static void page_select_short_press(void) {
    item_select_init();
}

static void item_select_init() {
    // Set event handlers
    short_press_action = &item_select_short_press;
    pollADC_handler = &item_select_handle_PollADC;
    screenRefresh_handler = &item_select_handle_screenRefresh;

    // Calibrate param knob
    param_knob_scaling = 7;
    param_last_index = -1;

            // ARB: Can we just call `item_select_handle_PollADC` here?

    // Render current page.
    int index = diskmenu_param_scaled(5, param_knob_scaling);
    menu_selection = index;

            // ARB: the parameters here are not necessary

    item_select_render_page(page_selection, index);
}

static void item_select_handle_PollADC(void) {
    int index = diskmenu_param_scaled(5, param_knob_scaling);
    menu_selection = index;
    if (0 <= param_last_index && index != param_last_index) {
        item_select_render_page(page_selection, index);
    }

    param_last_index = index;
}

static void item_select_handle_screenRefresh(void) {
    // Conventional call for simulator compatibility.
    diskmenu_display_print();
}

static void item_select_render_page(int page, int index) {
    // Render current directory
    diskmenu_display_clear(0, 0);
    diskmenu_display_set(0, 0, browse_directory, 0xa, 0);
    diskmenu_display_draw(0);

    if (0 == strcmp(browse_directory, "/")) {
        disk_browse_render_page(page * DISK_BROWSE_PAGE_SIZE,
                                DISK_BROWSE_PAGE_SIZE,
                                index,
                                0xa);
    }
    else if (0 < page) {
        disk_browse_render_page(page * DISK_BROWSE_PAGE_SIZE - 1,
                                DISK_BROWSE_PAGE_SIZE,
                                index,
                                0xa);
    }
    else {
        // First page (0) of a non-root directory
        disk_browse_render_page(0,
                                DISK_BROWSE_PAGE_SIZE - 1,
                                index,
                                0xa);
    }
}

static bool diskmenu_append_dir(char *base, uint8_t length, char *leaf) {
    int expected = strlen(base) + strlen(leaf) + 1;
    strncat(base, leaf, length);
    strncat(base, "/", length);
    return expected == strlen(base);
}

static void item_select_short_press(void) {
    // The save/load filename is the one selected.

    if (0 == menu_selection && 0 != strcmp(browse_directory, "/")) {
        // This is the parent directory entry.
        diskmenu_filelist_gotoparent();
        for (int i = strlen(browse_directory) - 2;
             browse_directory[i] != '/';
             --i) 
        {
            browse_directory[i] = 0;
        }
        page_select_init(filename_buffer, nextname_buffer, 0);
        return;
    }

    int file_index = menu_selection + page_selection * DISK_BROWSE_PAGE_SIZE;
    if (0 != strcmp(browse_directory, "/")) {
        --file_index;
    }

    disk_browse_read_sorted_filename(s_file_index,
                                     filename_buffer,
                                     FNAME_BUFFER_LEN,
                                     file_index);

    if (diskmenu_filelist_isdir()) {
        // We have a directory, navigate in.

        if (diskmenu_filelist_cd()) {
            if (!diskmenu_append_dir(browse_directory,
                                     FNAME_BUFFER_LEN,
                                     filename_buffer))
            {
                strcpy(browse_directory, "!!");
            }

            page_select_init(filename_buffer, nextname_buffer, 0);
        }
    }
    else {
        // We have a concrete file, so no concept of "save next in series".

        nextname_buffer[0] = 0;

        disk_browse_finish();
    }
}

static void disk_browse_render_line(int      line,
                                    char    *filename,
                                    bool     selected,
                                    uint8_t  fg)
{
    if (selected) {
        diskmenu_display_clear(line, fg);
        diskmenu_display_set(line, 0, filename, 0, fg);
        diskmenu_display_draw(line);
    }
    else {
        diskmenu_display_clear(line, 0);
        diskmenu_display_set(line, 0, filename, fg, 0);
        diskmenu_display_draw(line);
    }
}

static void disk_browse_render_page(int     first_file_index,
                                    int     page_size,
                                    int     item,
                                    uint8_t fg)
{
    char filename[FNAME_BUFFER_LEN];

    if (page_size < DISK_BROWSE_PAGE_SIZE) {
        // Render parent directory entry
        strcpy(filename, "../");

        disk_browse_render_line(1, filename, 0 == item, fg);
    }

    // Render items on current page
    int line = 1 + DISK_BROWSE_PAGE_SIZE - page_size;
    for (int i = first_file_index; i < first_file_index + page_size; ++i) {
        if (i < disk_browse_num_files) {
            disk_browse_read_sorted_filename(s_file_index,
                                             filename,
                                             FNAME_BUFFER_LEN,
                                             i);
            if (diskmenu_filelist_isdir()) {
                filename_ellipsis(filename, filename, 27);
                strncat(filename, "/", FNAME_BUFFER_LEN);
                disk_browse_render_line(line, filename, line == item + 1, fg);
            }
            else {
                filename_ellipsis(filename, filename, 28);
                disk_browse_render_line(line, filename, line == item + 1, fg);
            }
        }
        else {
            // render blank line.
            memset(filename, 0, FNAME_BUFFER_LEN);
            disk_browse_render_line(line, filename, 0, fg);
        }
        ++line;
    }
}

// ============================================================================
//                           USB DISK MINIMAL MENU
// ----------------------------------------------------------------------------

// ARB: from "flash.h"; change to function argument.
#define SCENE_SLOTS 32

bool tele_usb_disk_write_operation(uint8_t* plun_state, uint8_t* plun) {
    // WRITE SCENES
    diskmenu_dbg("\r\nwriting scenes");

    char filename[13];
    strcpy(filename, "tt00s.txt");

    char text_buffer[40];
    strcpy(text_buffer, "WRITE");
    diskmenu_display_line(0, text_buffer);

    for (int i = 0; i < SCENE_SLOTS; i++) {
        scene_state_t scene;
        ss_init(&scene);

        char text[SCENE_TEXT_LINES][SCENE_TEXT_CHARS];
        memset(text, 0, SCENE_TEXT_LINES * SCENE_TEXT_CHARS);

        strcat(text_buffer, ".");  // strcat is dangerous, make sure the
                                   // buffer is large enough!
        diskmenu_display_line(0, text_buffer);

        diskmenu_flash_read(i, &scene, &text);

        // ARB: bool diskmenu_io_create(uint8_t *status, char *filename);
        uint8_t status = 0;
        if (!diskmenu_io_create(&status, filename)) {
            if (status != kErrFileExists) {
#if USB_DISK_TEST == 1
                if (status == FS_LUN_WP) {
                    // Test can be done only on no write protected
                    // device
                    return false;
                }
#endif
                *plun_state |= (1 << *plun);  // LUN test is done.
                diskmenu_dbg("\r\ncreate fail");
                return false;
            }
        }
        diskmenu_dbg("\r\ncreate ok");

        if (!diskmenu_io_open(&status, kModeW)) {
#if USB_DISK_TEST == 1
            if (status == FS_LUN_WP) {
                // Test can be done only on no write protected
                // device
                return false;
            }
#endif
            *plun_state |= (1 << *plun);  // LUN test is done.
            diskmenu_dbg("\r\nopen fail");
            return false;
        }
        diskmenu_dbg("\r\nopen ok");

        tt_serializer_t tele_usb_writer;
        tele_usb_writer.write_char = &tele_usb_putc;
        tele_usb_writer.write_buffer = &tele_usb_write_buf;
        tele_usb_writer.print_dbg = &diskmenu_dbg;
        tele_usb_writer.data =
            NULL;  // asf disk i/o holds state, no handles needed
        serialize_scene(&tele_usb_writer, &scene, &text);

        diskmenu_io_close();
        *plun_state |= (1 << *plun);  // LUN test is done.

        if (filename[3] == '9') {
            filename[3] = '0';
            filename[2]++;
        }
        else
            filename[3]++;

        diskmenu_dbg(".");
        if (SCENE_SLOTS - 1 == i) {
            diskmenu_dbg("Done.");
        }
    }

    diskmenu_filelist_close();

    return true;
}

void tele_usb_disk_read_operation() {
    // READ SCENES
    diskmenu_dbg("\r\nreading scenes...");

    char filename[13];
    strcpy(filename, "tt00.txt");

    char text_buffer[40];
    strcpy(text_buffer, "READ");
    diskmenu_display_line(1, text_buffer);

    diskmenu_filelist_init(NULL);

    for (int i = 0; i < SCENE_SLOTS; i++) {
        scene_state_t scene;
        ss_init(&scene);
        char text[SCENE_TEXT_LINES][SCENE_TEXT_CHARS];
        memset(text, 0, SCENE_TEXT_LINES * SCENE_TEXT_CHARS);

        strcat(text_buffer, ".");  // strcat is dangerous, make sure the
                                   // buffer is large enough!
        diskmenu_display_line(1, text_buffer);
        if (diskmenu_filelist_find(NULL, 0, filename)) {
            diskmenu_dbg("\r\nfound: ");
            diskmenu_dbg(filename);
            if (!diskmenu_io_open(NULL, kModeR))
                diskmenu_dbg("\r\ncan't open");
            else {
                tt_deserializer_t tele_usb_reader;
                tele_usb_reader.read_char = &tele_usb_getc;
                tele_usb_reader.eof = &tele_usb_eof;
                tele_usb_reader.print_dbg = &diskmenu_dbg;
                tele_usb_reader.data =
                    NULL;  // asf disk i/o holds state, no handles needed
                deserialize_scene(&tele_usb_reader, &scene, &text);

                diskmenu_io_close();
                diskmenu_flash_write(i, &scene, &text);
            }
        }
        else {
            diskmenu_dbg("\r\nnot found: ");
            diskmenu_dbg(filename);
        }

        diskmenu_filelist_goto(NULL, 0, 0);

        if (filename[3] == '9') {
            filename[3] = '0';
            filename[2]++;
        }
        else
            filename[3]++;
    }

    diskmenu_filelist_close();
}

// ============================================================================
//                           USB DISK MINIMAL MENU
// ----------------------------------------------------------------------------

// usb disk mode entry point
void tele_usb_disk() {
    // disable event handlers while doing USB write
    diskmenu_assign_msc_event_handlers();

    // clear screen
    for (size_t i = 0; i < 8; i++) {
        diskmenu_display_clear(i, 0);
        diskmenu_display_draw(i);
    }
}

static void draw_usb_menu_item(uint8_t item_num, const char* text);

// *very* basic USB operations menu

typedef enum {
    USB_MENU_COMMAND_WRITE,
    USB_MENU_COMMAND_READ,
    USB_MENU_COMMAND_BOTH,
    USB_MENU_COMMAND_ADVANCED,
    USB_MENU_COMMAND_EXIT,

    USB_MENU_COMMAND_COUNT
} usb_menu_command_t;

usb_menu_command_t usb_menu_command;

// usb disk mode execution

void tele_usb_disk_exec() {
    //print_dbg("\r\nusb");
    uint8_t lun_state = 0;  // unused
    uint8_t lun = 0;        // unused

    if (usb_menu_command == USB_MENU_COMMAND_ADVANCED) {
        // ARB: rename to something having to do with advanced menu.
        tele_usb_disk_init();
    }
    else if (diskmenu_device_open()) {

        if (usb_menu_command == USB_MENU_COMMAND_WRITE ||
            usb_menu_command == USB_MENU_COMMAND_BOTH) {
            tele_usb_disk_write_operation(&lun_state, &lun);
        }
        if (usb_menu_command == USB_MENU_COMMAND_READ ||
            usb_menu_command == USB_MENU_COMMAND_BOTH) {
            tele_usb_disk_read_operation();
        }

        diskmenu_filelist_close();
    }
}

void draw_usb_menu_item(uint8_t item_num, const char* text) {
    uint8_t line_num = 8 - USB_MENU_COMMAND_COUNT + item_num;
    uint8_t fg = usb_menu_command == item_num ? 0 : 0xa;
    uint8_t bg = usb_menu_command == item_num ? 0xa : 0;
    diskmenu_display_clear(line_num, bg);
    diskmenu_display_set(line_num, 0, text, fg, bg);
    diskmenu_display_draw(line_num);
}

void handler_usb_PollADC(int32_t data) {
    usb_menu_command = diskmenu_param(usb_menu_command);

    if (usb_menu_command >= USB_MENU_COMMAND_COUNT) {
        usb_menu_command = USB_MENU_COMMAND_COUNT - 1;
    }
}

void handler_usb_Front(int32_t data) {
    if (0 == data) {
        // Exec only on button up.
        return;
    }

    // disable timers
    u8 flags = diskmenu_irqs_pause();

    if (usb_menu_command != USB_MENU_COMMAND_EXIT) {
        tele_usb_disk_exec();
    }

    if (usb_menu_command != USB_MENU_COMMAND_ADVANCED) {
        // renable teletype
        tele_usb_disk_finish();
    }

    diskmenu_irqs_resume(flags);
}

void handler_usb_ScreenRefresh(int32_t data) {
    draw_usb_menu_item(USB_MENU_COMMAND_WRITE,    "WRITE TO USB");
    draw_usb_menu_item(USB_MENU_COMMAND_READ,     "READ FROM USB");
    draw_usb_menu_item(USB_MENU_COMMAND_BOTH,     "DO BOTH");
    draw_usb_menu_item(USB_MENU_COMMAND_ADVANCED, "ADVANCED");
    draw_usb_menu_item(USB_MENU_COMMAND_EXIT,     "EXIT");

    // No-op on hardware; render page in simulation.
    //
    // This approach can be replaced with simply moving the 
    // `diskmenu_display_print` code to the simulator and calling it
    // immediately after the once-per-iteration call to the refresh handler.

    diskmenu_display_print();
}

