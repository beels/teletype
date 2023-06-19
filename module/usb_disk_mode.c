#include "usb_disk_mode.h"

#include <ctype.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "flash.h"
#include "globals.h"
#include "scene_serialization.h"

// libavr32
#include "events.h"
#include "adc.h"
#include "font.h"
#include "interrupts.h"
#include "region.h"
#include "util.h"

// this
#include "diskmenu.h"

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

void draw_usb_menu_item(uint8_t item_num, const char* text);
bool tele_usb_disk_write_operation(uint8_t* plun_state, uint8_t* plun);
void tele_usb_disk_read_operation(void);

// Subsystem control
static void tele_usb_disk_init(void);
static void tele_usb_disk_finish(void);

// ============================================================================
//                                 Utilities
// ----------------------------------------------------------------------------

static int button_counter = 0;
static bool long_press = false;

// ============================================================================
//                             Subsystem Control
// ----------------------------------------------------------------------------

// usb disk mode entry point
void tele_usb_disk() {
    print_dbg("\r\nusb");

    tele_usb_disk_init();
}

void tele_usb_disk_init() {
    button_counter = 0;
    long_press = false;

    // disable event handlers while doing USB write
    assign_msc_event_handlers();

    // disable timers
    default_timers_enabled = false;


            // ARB:
            // Include these configurations as arguments to diskmenu_init.
            // Also let diskmenu_init return bool so that we short-circuit
            // gracefully here???

    diskmenu_set_exit_handler(tele_usb_disk_finish);
    diskmenu_set_scratch_buffer((char *)copy_buffer, sizeof(copy_buffer));

    diskmenu_init();
}

void tele_usb_disk_finish() {
    // renable teletype
    set_mode(M_LIVE);
    assign_main_event_handlers();
    default_timers_enabled = true;
}

// ============================================================================
//                              EVENT HANDLERS
// ----------------------------------------------------------------------------

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
            diskmenu_handle_long_press();
        }
        else {
            diskmenu_handle_short_press();
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

        diskmenu_handle_button_timeout();
    }
}

void tele_usb_disk_PollADC(int32_t data) {
    diskmenu_handle_PollADC();
}

// ============================================================================
//                           HARDWARE ABSTRACTION
// ----------------------------------------------------------------------------

void diskmenu_io_close(void) {
    file_close();
}

bool diskmenu_io_open(uint8_t *status, uint8_t fopen_mode)
{
    int modes[2] = { FOPEN_MODE_R, FOPEN_MODE_W };

    if (file_open(modes[fopen_mode])) {
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
        *status = fs_g_status == FS_ERR_FILE_EXIST ? kErrFileExists
                                                   : kErrUnknown ;
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

bool diskmenu_filelist_goto(char *output, int length, uint8_t index) {
    if (nav_filelist_goto(index) && nav_filelist_validpos()) {
        if (output) {
            return nav_file_getname(output, length);
        }
        return true;
    }

    return false;
}

// *very* basic USB operations menu


typedef enum {
    USB_MENU_COMMAND_WRITE = 0,
    USB_MENU_COMMAND_READ = 1,
    USB_MENU_COMMAND_BOTH = 2,
    USB_MENU_COMMAND_EXIT = 3,
} usb_menu_command_t;

usb_menu_command_t usb_menu_command;

void draw_usb_menu_item(uint8_t item_num, const char* text) {
    uint8_t line_num = 4 + item_num;
    uint8_t fg = usb_menu_command == item_num ? 0 : 0xa;
    uint8_t bg = usb_menu_command == item_num ? 0xa : 0;
    region_fill(&line[line_num], bg);
    font_string_region_clip_tab(&line[line_num], text, 2, 0, fg, bg);
    region_draw(&line[line_num]);
}

void handler_usb_PollADC(int32_t data) {
    uint16_t adc[4];
    adc_convert(&adc);
    uint8_t cursor = adc[1] >> 9;
    uint8_t deadzone = cursor & 1;
    cursor >>= 1;
    if (!deadzone || abs(cursor - usb_menu_command) > 1) {
        usb_menu_command = cursor;
    }
}

void handler_usb_Front(int32_t data) {
    // disable timers
    u8 flags = irqs_pause();

    if (usb_menu_command != USB_MENU_COMMAND_EXIT) { tele_usb_disk_exec(); }

    // renable teletype
    set_mode(M_LIVE);
    assign_main_event_handlers();
    irqs_resume(flags);
}

void handler_usb_ScreenRefresh(int32_t data) {
    draw_usb_menu_item(0, "WRITE TO USB");
    draw_usb_menu_item(1, "READ FROM USB");
    draw_usb_menu_item(2, "DO BOTH");
    draw_usb_menu_item(3, "EXIT");
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

uint8_t display_font_string_position(const char* str, uint8_t pos) {
    return font_string_position(str, pos);
}

uint8_t diskmenu_flash_scene_id(void) {
    return flash_last_saved_scene();
}

void diskmenu_flash_read(uint8_t scene_id,
                         scene_state_t *scene,
                         char (*text)[SCENE_TEXT_LINES][SCENE_TEXT_CHARS])
{
    flash_read(scene_id, scene, text, 1, 1, 1);
}

const char *diskmenu_flash_scene_text(uint8_t scene_id) {
    return flash_scene_text(scene_id, 0);
}

void diskmenu_flash_write(uint8_t scene_id,
                          scene_state_t *scene,
                          char (*text)[SCENE_TEXT_LINES][SCENE_TEXT_CHARS])
{
    flash_write(scene_id, scene, text);
}

void diskmenu_dbg(const char *str) {
    print_dbg(str);
}

int diskmenu_param_scaled(uint8_t resolution, uint8_t scale) {

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

// usb disk mode execution
void tele_usb_disk_exec() {
    print_dbg("\r\nusb");
    uint8_t lun_state = 0;

    for (uint8_t lun = 0; (lun < uhi_msc_mem_get_lun()) && (lun < 8); lun++) {
        // print_dbg("\r\nlun: ");
        // print_dbg_ulong(lun);

        // Mount drive
        nav_drive_set(lun);
        if (!nav_partition_mount()) {
            if (fs_g_status == FS_ERR_HW_NO_PRESENT) {
                // The test can not be done, if LUN is not present
                lun_state &= ~(1 << lun);  // LUN test reseted
                continue;
            }
            lun_state |= (1 << lun);  // LUN test is done.
            print_dbg("\r\nfail");
            // ui_test_finish(false); // Test fail
            continue;
        }
        // Check if LUN has been already tested
        if (lun_state & (1 << lun)) { continue; }

        if (usb_menu_command == USB_MENU_COMMAND_WRITE ||
            usb_menu_command == USB_MENU_COMMAND_BOTH) {
            if (!tele_usb_disk_write_operation(&lun_state, &lun)) { continue; }
        }
        if (usb_menu_command == USB_MENU_COMMAND_READ ||
            usb_menu_command == USB_MENU_COMMAND_BOTH) {
            tele_usb_disk_read_operation();
        }

        nav_exit();
    }
}

bool tele_usb_disk_write_operation(uint8_t* plun_state, uint8_t* plun) {
    // WRITE SCENES
    print_dbg("\r\nwriting scenes");

    char filename[13];
    strcpy(filename, "tt00s.txt");

    char text_buffer[40];
    strcpy(text_buffer, "WRITE");
    region_fill(&line[0], 0);
    font_string_region_clip_tab(&line[0], text_buffer, 2, 0, 0xa, 0);
    region_draw(&line[0]);

    for (int i = 0; i < SCENE_SLOTS; i++) {
        scene_state_t scene;
        ss_init(&scene);

        char text[SCENE_TEXT_LINES][SCENE_TEXT_CHARS];
        memset(text, 0, SCENE_TEXT_LINES * SCENE_TEXT_CHARS);

        strcat(text_buffer, ".");  // strcat is dangerous, make sure the
                                   // buffer is large enough!
        region_fill(&line[0], 0);
        font_string_region_clip_tab(&line[0], text_buffer, 2, 0, 0xa, 0);
        region_draw(&line[0]);

        flash_read(i, &scene, &text, 1, 1, 1);

        if (!nav_file_create((FS_STRING)filename)) {
            if (fs_g_status != FS_ERR_FILE_EXIST) {
                if (fs_g_status == FS_LUN_WP) {
                    // Test can be done only on no write protected
                    // device
                    return false;
                }
                *plun_state |= (1 << *plun);  // LUN test is done.
                print_dbg("\r\nfail");
                return false;
            }
        }

        if (!file_open(FOPEN_MODE_W)) {
            if (fs_g_status == FS_LUN_WP) {
                // Test can be done only on no write protected
                // device
                return false;
            }
            *plun_state |= (1 << *plun);  // LUN test is done.
            print_dbg("\r\nfail");
            return false;
        }

        tt_serializer_t tele_usb_writer;
        tele_usb_writer.write_char = &tele_usb_putc;
        tele_usb_writer.write_buffer = &tele_usb_write_buf;
        tele_usb_writer.print_dbg = &print_dbg;
        tele_usb_writer.data =
            NULL;  // asf disk i/o holds state, no handles needed
        serialize_scene(&tele_usb_writer, &scene, &text);

        file_close();
        *plun_state |= (1 << *plun);  // LUN test is done.

        if (filename[3] == '9') {
            filename[3] = '0';
            filename[2]++;
        }
        else
            filename[3]++;

        print_dbg(".");
    }

    nav_filelist_reset();
    return true;
}

void tele_usb_disk_read_operation() {
    // READ SCENES
    print_dbg("\r\nreading scenes...");

    char filename[13];
    strcpy(filename, "tt00.txt");

    char text_buffer[40];
    strcpy(text_buffer, "READ");
    region_fill(&line[1], 0);
    font_string_region_clip_tab(&line[1], text_buffer, 2, 0, 0xa, 0);
    region_draw(&line[1]);

    for (int i = 0; i < SCENE_SLOTS; i++) {
        scene_state_t scene;
        ss_init(&scene);
        char text[SCENE_TEXT_LINES][SCENE_TEXT_CHARS];
        memset(text, 0, SCENE_TEXT_LINES * SCENE_TEXT_CHARS);

        strcat(text_buffer, ".");  // strcat is dangerous, make sure the
                                   // buffer is large enough!
        region_fill(&line[1], 0);
        font_string_region_clip_tab(&line[1], text_buffer, 2, 0, 0xa, 0);
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
}
