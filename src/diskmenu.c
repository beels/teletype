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

static void main_menu_init(void);
static void main_menu_short_press(int32_t data);
static void main_menu_handle_PollADC(int32_t data);
static void main_menu_handle_screenRefresh(int32_t data);

static void diskmenu_main_menu_init(void);

            // ARB: Can this be deleted?

static void diskmenu_exec(void);
static bool diskmenu_parse_target_filename(char *buffer, uint8_t preset);
static void diskmenu_render_menu_line(int item, int line_no, bool selected);
static int diskmenu_write_file(char *filename, int preset);
static int diskmenu_read_file(char *filename, int preset);
static void diskmenu_browse_init(char *filename,
                                 char *nextname,
                                 int   preset);
static bool diskmenu_append_dir(char *base, uint8_t length, char *leaf);

static void page_select_init(char *filename,
                             char *nextname,
                             int   preset);
static void page_select_short_press(int32_t data);
static void page_select_handle_PollADC(int32_t data);
static void page_select_handle_screenRefresh(int32_t data);
static void page_select_render_page(int index);

static void item_select_init(void);
static void item_select_short_press(int32_t data);
static void item_select_handle_PollADC(int32_t data);
static void item_select_handle_screenRefresh(int32_t data);
static void item_select_render_page(int page, int item);

static void disk_browse_render_page(char *filenames,
                                    int   page_size,
                                    int   item);

static bool diskmenu_discover_filenames(void);

static void tele_usb_disk_exec(void);

// ============================================================================
//                                 Utilities
// ----------------------------------------------------------------------------

static inline bool diskmenu_device_open(void) {
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

            // ARB: It looks like `num_entries` does not account for files that
            // do not match the supported filename pattern.

static inline bool diskmenu_filelist_init(int *num_entries) {
    if (nav_filelist_single_disable()) {
        if (num_entries) {
            *num_entries = nav_filelist_nb(FS_FILE) + nav_filelist_nb(FS_DIR);
        }
        return true;
    }

    return false;
}

static inline bool diskmenu_filelist_find(char *output,
                                          uint8_t length,
                                          char *pattern)
{
    if (nav_filelist_findname((FS_STRING)pattern, false)
        && nav_filelist_validpos())
    {
        if (output) {
            return nav_file_getname(output, length);
        }
        return true;
    }

    return false;
}

static inline bool diskmenu_filelist_goto(char *output,
                                          int length,
                                          uint8_t index)
{
    if (nav_filelist_goto(index) && nav_filelist_validpos()) {
        if (output) {
            return nav_file_getname(output, length);
        }
        return true;
    }

    return false;
}

static inline void diskmenu_filelist_close(void) {
    nav_filelist_reset();
    nav_exit();
}

// ============================================================================
//                           SERIALIZATION SUPPORT
// ----------------------------------------------------------------------------

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


static int browse_depth = 0;
static char browse_directory[FNAME_BUFFER_LEN];

            // ARB:
            // - These temp buffers can be offsets into `copy_buffer`.
            //
            // - browse_directory has to persist across directory navigation
            //   so it requires storage outside of the copy_buffer.

static char filename_buffer[FNAME_BUFFER_LEN];
static char nextname_buffer[FNAME_BUFFER_LEN];

enum {
    kHelpText = -1,
    kReadFile = 0,
    kWriteFile,
    kWriteNextInSeries,
    kBrowse,
    kExit
};

static uint8_t *s_file_index;
static char *s_page_text;
static int disk_browse_num_files;

// ============================================================================
//                              EVENT HANDLERS
// ----------------------------------------------------------------------------

static void main_menu_handle_PollADC(int32_t data) {
    int index = diskmenu_param_scaled(10, param_knob_scaling);

    if (0 <= param_last_index && index != param_last_index) {
        if (index == kWriteNextInSeries && !nextname_buffer[0]) {
            return;
        }

        // Update selected items
        diskmenu_render_menu_line(menu_selection, menu_selection + 1, false);

        diskmenu_render_menu_line(index, index + 1, true);

        menu_selection = index;
    }

    param_last_index = index;
}

static void main_menu_handle_screenRefresh(int32_t data) {
    diskmenu_display_print();
}

static void main_menu_short_press(int32_t data) {
    if (data) {
        diskmenu_exec();
    }
}

void main_menu_init() {
    // disable event handlers while doing USB write

    // This just keeps the PARAM knob quiet while we go through the disk access
    // operations setting up the usb disk mode menu.

    empty_event_handlers();

    // disable timers
    default_timers_enabled = false;

            // ARB:
            // Why is this being done before anything else?
            // This used to be the LUN scanning code.

    if (!diskmenu_discover_filenames()) {
        // Exit usb disk mode

            // ARB: need to add failure feedback on display

        // Clear screen; show error message
        // Pause for a second, or prompt for key press

        tele_usb_disk_finish();

        return;
    }

    diskmenu_main_menu_init();
}

void diskmenu_main_menu_init() {
    // initial button handlers (main menu)
    diskmenu_assign_handlers(&main_menu_short_press,
                             &main_menu_handle_PollADC,
                             &main_menu_handle_screenRefresh);
    param_knob_scaling = MAIN_MENU_PAGE_SIZE;
    param_last_index = -1;

    // clear screen
    for (size_t i = 0; i < 8; i++) {
        diskmenu_display_line(i, NULL, false);
    }

    // Parse selected preset number
    uint8_t preset = diskmenu_flash_scene_id();

    // Print selected preset title
    {
        char preset_title[DISPLAY_BUFFER_LEN];
        strcpy(preset_title, diskmenu_flash_scene_text(preset));


            // ARB:
            // Can we just use diskmenu_flash_scene_text directly here?

        diskmenu_display_line(0, preset_title, false);
    }

    // Menu items
    diskmenu_render_menu_line(kReadFile,          1, false);
    diskmenu_render_menu_line(kWriteFile,         2, false);
    diskmenu_render_menu_line(kWriteNextInSeries, 3, false);
    diskmenu_render_menu_line(kBrowse,            4, false);
    diskmenu_render_menu_line(kExit,              5, false);

    menu_selection = 4;
    // force sync with param knob position
    param_last_index = 9000;
    main_menu_handle_PollADC(0);
}

void diskmenu_exec() {
    uint8_t preset = diskmenu_flash_scene_id();
    char text_buffer[DISPLAY_BUFFER_LEN];

    switch (menu_selection) {
        case kReadFile: {
            // Read from file
            int rc = diskmenu_read_file(filename_buffer, preset);
            if (0 != rc) {
                strcpy(text_buffer, rc == -1 ? "fail: " : "no file: ");
                strncat(text_buffer,
                        filename_buffer,
                        DISPLAY_BUFFER_LEN - strlen(text_buffer));
                diskmenu_display_line(0, text_buffer, false);
            }
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

    tele_usb_disk_finish();
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

void diskmenu_render_menu_line(int item, int line_no, bool selected) {
    char text_buffer[DISPLAY_BUFFER_LEN];

    switch (item) {
        case kReadFile: { // Menu line 0: Read from file 'abcd.123'
            strcpy(text_buffer, "Read '");
            filename_ellipsis(text_buffer + 6, filename_buffer, 22);
            strcat(text_buffer, "'");

            diskmenu_display_line(line_no, text_buffer, selected);
        } break;

        case kWriteFile: { // Menu line 1: Write to file 'abcd.123'
            strcpy(text_buffer, "Write '");
            filename_ellipsis(text_buffer + 7, filename_buffer, 21);
            strcat(text_buffer, "'");

            diskmenu_display_line(line_no, text_buffer, selected);
        } break;

        case kWriteNextInSeries: { // Menu line 2: filename iterator
            if (nextname_buffer[0]) {
                strcpy(text_buffer, "Write '");
                filename_ellipsis(text_buffer + 7, nextname_buffer, 21);
                strcat(text_buffer, "'");

                diskmenu_display_line(line_no, text_buffer, selected);
            }
        } break;

        case kBrowse: { // Menu line 3: Browse filesystem
            diskmenu_display_line(line_no, "Browse USB disk", selected);
        } break;

        case kExit: { // Menu line 4: Exit USB disk mode
            uint8_t preset = diskmenu_flash_scene_id();
            char preset_buffer[3];
            itoa(preset, preset_buffer, 10);

            strcpy(text_buffer, "Exit to scene ");
            strcat(text_buffer, preset_buffer);

            diskmenu_display_line(line_no, text_buffer, selected);
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

        while(diskmenu_filelist_find(nextname_buffer,
                                     FNAME_BUFFER_LEN,
                                     filename_buffer))
        {
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

int diskmenu_write_file(char *filename, int preset) {
    scene_state_t scene;
    ss_init(&scene);

    char text[SCENE_TEXT_LINES][SCENE_TEXT_CHARS];
    memset(text, 0, SCENE_TEXT_LINES * SCENE_TEXT_CHARS);

    diskmenu_flash_read(preset, &scene, &text);

    if (!nav_file_create((FS_STRING) filename)) {
        // We still write the file if it already exists.
        if (fs_g_status != FS_ERR_FILE_EXIST) {
            return -1;
        }
    }

    if (!file_open(FOPEN_MODE_W)) {
        return -2;
    }

    tt_serializer_t tele_usb_writer;
    tele_usb_writer.write_char = &tele_usb_putc;
    tele_usb_writer.write_buffer = &tele_usb_write_buf;
    tele_usb_writer.print_dbg = &diskmenu_dbg;
    tele_usb_writer.data =
        NULL;  // asf disk i/o holds state, no handles needed
    serialize_scene(&tele_usb_writer, &scene, &text);

    file_close();

    return 0;
}

int diskmenu_read_file(char *filename, int preset) {
    scene_state_t scene;
    ss_init(&scene);
    char text[SCENE_TEXT_LINES][SCENE_TEXT_CHARS];
    memset(text, 0, SCENE_TEXT_LINES * SCENE_TEXT_CHARS);

    if (!diskmenu_filelist_find(NULL, 0, filename)) {
        return -2;
    }

    if (!file_open(FOPEN_MODE_R)) {
        return -1;
    }

    tt_deserializer_t tele_usb_reader;
    tele_usb_reader.read_char = &tele_usb_getc;
    tele_usb_reader.eof = &tele_usb_eof;
    tele_usb_reader.print_dbg = &diskmenu_dbg;
    tele_usb_reader.data =
        NULL;  // asf disk i/o holds state, no handles needed
    deserialize_scene(&tele_usb_reader, &scene, &text);

    file_close();
    diskmenu_flash_write(preset, &scene, &text);

    return 0;
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
    browse_directory[0] = '/';
    browse_directory[1] = 0;
    browse_depth = 0;
    page_select_init(filename, nextname, preset);
}

static void page_select_init(char *filename,
                             char *nextname,
                             int   preset)
{
    // Set event handlers
    diskmenu_assign_handlers(&page_select_short_press,
                             &page_select_handle_PollADC,
                             &page_select_handle_screenRefresh);

    // clear screen
    for (size_t i = 0; i < 8; i++) {
        diskmenu_display_line(i, NULL, false);
    }

    diskmenu_filelist_init(&disk_browse_num_files);

    if (0 == browse_depth) {
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
    diskmenu_display_line(0, text_buffer, false);

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

static void page_select_handle_PollADC(int32_t data) {
    int index = diskmenu_param_scaled(8, param_knob_scaling);
    if (0 <= param_last_index && index != param_last_index) {
        page_selection = index;

            // ARB: the parameter here is not necessary

        page_select_render_page(index);
    }

    param_last_index = index;
}

static void page_select_handle_screenRefresh(int32_t data) {
    // Conventional call for simulator compatibility.
    diskmenu_display_print();
}

static void page_select_render_page(int index) {
    // Store filenames in temp buffer.
    // We borrow more of the copy buffer for storage.
    s_page_text = copy_buffer + 256;

    int has_parent = 0 == index && browse_depth;

    char *filename = s_page_text;
    char *end = s_page_text + DISK_BROWSE_PAGE_SIZE * FNAME_BUFFER_LEN;
    if (has_parent) {
        strcpy(filename, "../");
        filename += FNAME_BUFFER_LEN;
    }

    int first_entry = index * DISK_BROWSE_PAGE_SIZE
                          - (browse_depth && !has_parent);
    while (filename < end) {
        memset(filename, 0, FNAME_BUFFER_LEN);
        if (first_entry < disk_browse_num_files) {
            disk_browse_read_sorted_filename(s_file_index,
                                             filename_buffer,
                                             FNAME_BUFFER_LEN,
                                             first_entry);
            filename_ellipsis(filename, filename_buffer, 27);
            if (nav_file_isdir()) {
                strncat(filename, "/", FNAME_BUFFER_LEN);
            }
        }

        filename += FNAME_BUFFER_LEN;
        ++first_entry;
    }

    // Render current directory
    diskmenu_display_line(0, browse_directory, false);

    // Ugly, but desperately trying to save space.
    diskmenu_foreground = 0x4;
    disk_browse_render_page(s_page_text,
                            DISK_BROWSE_PAGE_SIZE,
                            -1);
    diskmenu_foreground = 0xa;
}

static void page_select_short_press(int32_t data) {
    if (data) {
        item_select_init();
    }
}

static void item_select_init() {
    // Set event handlers
    diskmenu_assign_handlers(&item_select_short_press,
                             &item_select_handle_PollADC,
                             &item_select_handle_screenRefresh);

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

static void item_select_handle_PollADC(int32_t data) {
    int index = diskmenu_param_scaled(5, param_knob_scaling);
    menu_selection = index;
    if (0 <= param_last_index && index != param_last_index) {
        item_select_render_page(page_selection, index);
    }

    param_last_index = index;
}

static void item_select_handle_screenRefresh(int32_t data) {
    // Conventional call for simulator compatibility.
    diskmenu_display_print();
}

static void item_select_render_page(int page, int index) {
    // Render current directory
    diskmenu_display_line(0, browse_directory, false);

    disk_browse_render_page(s_page_text,
                            DISK_BROWSE_PAGE_SIZE,
                            index);
}

static bool diskmenu_append_dir(char *base, uint8_t length, char *leaf) {
    int expected = strlen(base) + strlen(leaf) + 1;
    strncat(base, leaf, length);
    strncat(base, "/", length);
    return expected == strlen(base);
}

static void item_select_short_press(int32_t data) {
    // The save/load filename is the one selected.

    if (!data) {
        return;
    }

    const bool is_non_root = 0 < browse_depth;
    if (0 == menu_selection && is_non_root) {
        // This is the parent directory entry.
        nav_dir_gotoparent();
        --browse_depth;
        for (int i = strlen(browse_directory) - 2;
             browse_directory[i] != '/';
             --i) 
        {
            browse_directory[i] = 0;
        }
        page_select_init(filename_buffer, nextname_buffer, 0);
        return;
    }

    int file_index = menu_selection
                        + page_selection * DISK_BROWSE_PAGE_SIZE
                        - is_non_root;

    // Must re-read the filename because the display names have been truncated.

    disk_browse_read_sorted_filename(s_file_index,
                                     filename_buffer,
                                     FNAME_BUFFER_LEN,
                                     file_index);

    if (nav_file_isdir()) {
        // We have a directory, navigate in.

        if (nav_dir_cd()) {
            ++browse_depth;
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

static void disk_browse_render_page(char *filenames, int page_size, int item)
{
    // Render items on current page
    for (int i = 0; i < page_size; ++i) {
        diskmenu_display_line(i + 1, filenames, i == item);
        filenames += FNAME_BUFFER_LEN;
    }
}

// ============================================================================
//                           USB DISK MINIMAL MENU
// ----------------------------------------------------------------------------

// ARB: from "flash.h"; change to function argument.
#define SCENE_SLOTS 32

bool tele_usb_disk_write_operation() {
    // WRITE SCENES
    diskmenu_dbg("\r\nwriting scenes");

    strcpy(filename_buffer, "tt00s.txt");

    char text_buffer[40];
    strcpy(text_buffer, "WRITE");
    diskmenu_display_line(0, text_buffer, false);

    for (int i = 0; i < SCENE_SLOTS; i++) {
        strcat(text_buffer, ".");  // strcat is dangerous, make sure the
                                   // buffer is large enough!
        diskmenu_display_line(0, text_buffer, false);

        int rc = diskmenu_write_file(filename_buffer, i);
        if (rc == -1 ) {
            diskmenu_dbg("\r\ncreate fail");
            return false;
        }
        else if (rc == -2) {
            diskmenu_dbg("\r\nopen fail");
            return false;
        }

        if (filename_buffer[3] == '9') {
            filename_buffer[3] = '0';
            filename_buffer[2]++;
        }
        else
            filename_buffer[3]++;

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

    strcpy(filename_buffer, "tt00.txt");

    char text_buffer[40];
    strcpy(text_buffer, "READ");
    diskmenu_display_line(1, text_buffer, false);

    diskmenu_filelist_init(NULL);

    for (int i = 0; i < SCENE_SLOTS; i++) {
        strcat(text_buffer, ".");  // strcat is dangerous, make sure the
                                   // buffer is large enough!
        diskmenu_display_line(1, text_buffer, false);

#if 1
        diskmenu_read_file(filename_buffer, i);
#else
        int rc = diskmenu_read_file(filename_buffer, i);
        if (rc != -1) {
            diskmenu_dbg("\r\ncan't open");
        }
        else if (rc == -2) {
            diskmenu_dbg("\r\nnot found: ");
            diskmenu_dbg(filename_buffer);
        }
#endif

        diskmenu_filelist_goto(NULL, 0, 0);

        if (filename_buffer[3] == '9') {
            filename_buffer[3] = '0';
            filename_buffer[2]++;
        }
        else {
            filename_buffer[3]++;
        }
    }

    diskmenu_filelist_close();
}

// ============================================================================
//                           USB DISK MINIMAL MENU
// ----------------------------------------------------------------------------

// usb disk mode entry point
void tele_usb_disk() {
    // disable event handlers while doing USB write
    empty_event_handlers();

    diskmenu_assign_handlers(&handler_usb_Front,
                             &handler_usb_PollADC,
                             &handler_usb_ScreenRefresh);

    // clear screen
    for (size_t i = 0; i < 8; i++) {
        diskmenu_display_line(i, "", false);
    }
}

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

    if (usb_menu_command == USB_MENU_COMMAND_ADVANCED) {
        // ARB: rename to something having to do with advanced menu.
        main_menu_init();
    }
    else if (diskmenu_device_open()) {

        if (usb_menu_command == USB_MENU_COMMAND_WRITE ||
            usb_menu_command == USB_MENU_COMMAND_BOTH) {
            tele_usb_disk_write_operation();
        }
        if (usb_menu_command == USB_MENU_COMMAND_READ ||
            usb_menu_command == USB_MENU_COMMAND_BOTH) {
            tele_usb_disk_read_operation();
        }

        diskmenu_filelist_close();
    }
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
    u8 flags = irqs_pause();

    if (usb_menu_command != USB_MENU_COMMAND_EXIT) {
        tele_usb_disk_exec();
    }

    if (usb_menu_command != USB_MENU_COMMAND_ADVANCED) {
        // renable teletype
        tele_usb_disk_finish();
    }

    irqs_resume(flags);
}

void handler_usb_ScreenRefresh(int32_t data) {
    enum { kFirstLine = 8 - USB_MENU_COMMAND_COUNT };

    diskmenu_display_line(kFirstLine + USB_MENU_COMMAND_WRITE,
                            "WRITE TO USB",
                            usb_menu_command == USB_MENU_COMMAND_WRITE);
    diskmenu_display_line(kFirstLine + USB_MENU_COMMAND_READ,
                            "READ FROM USB",
                            usb_menu_command == USB_MENU_COMMAND_READ);
    diskmenu_display_line(kFirstLine + USB_MENU_COMMAND_BOTH,
                            "DO BOTH",
                            usb_menu_command == USB_MENU_COMMAND_BOTH);
    diskmenu_display_line(kFirstLine + USB_MENU_COMMAND_ADVANCED,
                            "ADVANCED",
                            usb_menu_command == USB_MENU_COMMAND_ADVANCED);
    diskmenu_display_line(kFirstLine + USB_MENU_COMMAND_EXIT,
                            "EXIT",
                            usb_menu_command == USB_MENU_COMMAND_EXIT);

    // No-op on hardware; render page in simulation.
    //
    // This approach can be replaced with simply moving the 
    // `diskmenu_display_print` code to the simulator and calling it
    // immediately after the once-per-iteration call to the refresh handler.

    diskmenu_display_print();
}

