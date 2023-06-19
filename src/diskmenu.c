#include <ctype.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>  // for debugging

#include "util.h"

#include "diskmenu.h"
#include "filename.h"
#include "mergesort.h"
#include "scene_serialization.h"

#define dbg printf("at: %s %s %d\n", __FILE__, __FUNCTION__, __LINE__);

// Local functions for usb filesystem serialization
// ... temporarily exposed to support dewb minimal menu operations
// static void tele_usb_putc(void* self_data, uint8_t c);
// static void tele_usb_write_buf(void* self_data, uint8_t* buffer, uint16_t size);
// static uint16_t tele_usb_getc(void* self_data);
// static bool tele_usb_eof(void* self_data);

static void main_menu_short_press(void);
static void main_menu_button_timeout(void);
static void main_menu_long_press(void);
static void main_menu_handle_PollADC(void);
static void disk_browse_handle_PollADC(void);

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
                                 int preset);


            // ARB: Need to audit and declare the disk_browse* functions.

static bool diskmenu_discover_filenames(void);
static void disk_browse_render_item(int index, int selection);
static void disk_browse_render_page(int index);
static void disk_browse_navigate(int old_index, int new_index);

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

static int menu_selection = 4;
static int param_knob_scaling = MAIN_MENU_PAGE_SIZE;
static int param_last_index = -1;


            // ARB:
            // These buffers can be offsets into `copy_buffer`.

static char filename_buffer[FNAME_BUFFER_LEN];
static char nextname_buffer[FNAME_BUFFER_LEN];
static char *diskmenu_copy_buffer;
static size_t diskmenu_copy_buffer_length;

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
static void (*short_press_action)(void);
static void (*button_timeout_action)(void);
static void (*long_press_action)(void);

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

static void disk_browse_handle_PollADC(void) {
    int index = diskmenu_param_scaled(10, param_knob_scaling);
    if (0 <= param_last_index && index != param_last_index) {
        disk_browse_navigate(menu_selection, index);
        menu_selection = index;
    }

    param_last_index = index;
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

void diskmenu_set_exit_handler(void (*exit_handler)(void)) {
    diskmenu_exit_handler = exit_handler;
}

bool diskmenu_set_scratch_buffer(char *buffer, uint32_t length) {
    if (1024 <= length) {
        diskmenu_copy_buffer = buffer;
        diskmenu_copy_buffer_length = length;

        return true;
    }

    return false;
}

void diskmenu_init(void) {

    // This just keeps the PARAM knob quiet while we go through the disk access
    // operations a the start of diskmenu_init.

    pollADC_handler = NULL;

            // ARB:
            // Why is this being done before anything else?
            // This used to be the LUN scanning code.

    if (!diskmenu_copy_buffer || !diskmenu_discover_filenames()) {
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

void diskmenu_handle_long_press() {
    (*long_press_action)();
}

void diskmenu_handle_button_timeout() {
    (*button_timeout_action)();
}

void diskmenu_handle_PollADC() {
    if (pollADC_handler) {
        (*pollADC_handler)();
    }
}

void diskmenu_main_menu_init() {
    // initial button handlers (main menu)
    short_press_action = &main_menu_short_press;
    button_timeout_action = &main_menu_button_timeout;
    long_press_action = &main_menu_long_press;
    pollADC_handler = &main_menu_handle_PollADC;
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

static void disk_browse_button_timeout(void) {
    int selected_entry = menu_selection % DISK_BROWSE_PAGE_SIZE;

    char filename[FNAME_BUFFER_LEN];
    disk_browse_read_sorted_filename(
            s_file_index, filename, FNAME_BUFFER_LEN, menu_selection);
    filename_ellipsis(filename, filename, 28);
    diskmenu_render_line(filename, selected_entry + 1, kSelected);
}

static void disk_browse_finish(void) {
    diskmenu_filelist_close();

    diskmenu_main_menu_init();
}

static void disk_browse_render_item(int index, int selection) {
    int page = index / DISK_BROWSE_PAGE_SIZE;
    int first_entry = page * DISK_BROWSE_PAGE_SIZE;
    int current_entry = index - first_entry;

    if (disk_browse_num_files <= index) {
        diskmenu_display_line(current_entry + 1, NULL);
        return;
    }

    char filename[FNAME_BUFFER_LEN];
    disk_browse_read_sorted_filename(s_file_index,
                                     filename,
                                     FNAME_BUFFER_LEN,
                                     index);
    filename_ellipsis(filename, filename, 28);
    diskmenu_render_line(filename,
                         current_entry + 1,
                         (index == selection) ? kCurrent
                                              : kBlank);

    if (index == selection) {
        // Display title on line 0

        diskmenu_display_clear(0, 0x2);

        if (diskmenu_io_open(NULL, kModeR)) {
            uint8_t title[DISPLAY_BUFFER_LEN];

    // ARB: remove after refactor: too slow in loop.

            // This works because we set the "current file" as a side effect of
            // looking up the item's filename in
            // `disk_browse_read_sorted_filename` above.

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

static void disk_browse_render_page(int index) {
    int page = index / DISK_BROWSE_PAGE_SIZE;

    // Render current page

    int first_entry = page * DISK_BROWSE_PAGE_SIZE;

    for (int i = first_entry; i < first_entry + DISK_BROWSE_PAGE_SIZE; ++i) {
        disk_browse_render_item(i, index);
    }
}

static void disk_browse_navigate(int old_index, int new_index) {
    int page = new_index / DISK_BROWSE_PAGE_SIZE;
    bool update_page = (page != old_index / DISK_BROWSE_PAGE_SIZE);

    // Render current page

    int first_entry = page * DISK_BROWSE_PAGE_SIZE;

    for (int i = first_entry; i < first_entry + DISK_BROWSE_PAGE_SIZE; ++i) {
        if (update_page || i == new_index || i == old_index) {
            disk_browse_render_item(i, new_index);
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

static void diskmenu_browse_init(char *filename,
                                 char *nextname,
                                 int preset)
{
    // Set event handlers
    short_press_action = &disk_browse_short_press;
    button_timeout_action = &disk_browse_button_timeout;
    long_press_action = &disk_browse_long_press;
    pollADC_handler = &disk_browse_handle_PollADC;

    // clear screen
    for (size_t i = 0; i < 8; i++) {
        diskmenu_display_line(i, NULL);
    }

    diskmenu_filelist_init(&disk_browse_num_files);
    param_knob_scaling = disk_browse_num_files;
    param_last_index = -1;

    // render browser
    char text_buffer[DISPLAY_BUFFER_LEN];
    itoa(disk_browse_num_files, text_buffer, 10);
    strcat(text_buffer, " files total");
    diskmenu_render_line(text_buffer, 0, kBlank);

    // Create file index
    // We borrow the copy buffer for temporary storage of the file index.
    s_file_index = (uint8_t *) diskmenu_copy_buffer;

    mergesort_accessor_t filename_accessor = {
                        .data = 0,
                        .get_value = disk_browse_read_filename
                    };

    uint8_t temp_index[256];
    mergesort(s_file_index, temp_index,
              diskmenu_copy_buffer + 256,
              diskmenu_copy_buffer_length - 256,
              disk_browse_num_files, FNAME_BUFFER_LEN,
              &filename_accessor);

    // Render current page.
    menu_selection = diskmenu_param_scaled(10, param_knob_scaling);
    disk_browse_render_page(menu_selection);
}
