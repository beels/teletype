#include "usb_disk_mode.h"

#include <ctype.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "flash.h"
#include "globals.h"
#include "adc.h"

// libavr32
#include "events.h"
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

