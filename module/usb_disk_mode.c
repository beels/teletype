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

// We implement this API
#include "diskmenu_api.h"

// Change to 1 to enable debug "fail" output from early test code.
#define USB_DISK_TEST 0

// ============================================================================
//                        APPLICATION INFRASTRUCTURE
// ----------------------------------------------------------------------------

void diskmenu_assign_msc_event_handlers(void) {
    assign_msc_event_handlers();
}

void diskmenu_assign_advanced_menu_event_handlers(void) {
    empty_event_handlers();

    app_event_handlers[kEventPollADC] = &tele_usb_disk_PollADC;

    // one day this could be used to map the front button and pot to be used as
    // a UI with a memory stick

    app_event_handlers[kEventFront] = &tele_usb_disk_handler_Front;
    app_event_handlers[kEventKeyTimer] = &tele_usb_disk_handler_KeyTimer;
    app_event_handlers[kEventScreenRefresh] =
                                          &tele_usb_disk_handler_ScreenRefresh;
}

uint8_t diskmenu_irqs_pause() {
    return irqs_pause();
}

void diskmenu_irqs_resume(uint8_t flags) {
    irqs_resume(flags);
}

void diskmenu_set_default_timers_enabled(bool value) {
    default_timers_enabled = value;
}

// ============================================================================
//                             Subsystem Control
// ----------------------------------------------------------------------------

void tele_usb_disk_finish() {
    // renable teletype
    set_mode(M_LIVE);
    assign_main_event_handlers();
    diskmenu_set_default_timers_enabled(true);
}

// ============================================================================
//                           HARDWARE ABSTRACTION
// ----------------------------------------------------------------------------

// Default (hardware) implementations of functions that are overridden in
// diskmenu-simulator.

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
        return true;
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


            // ARB: It looks like `num_entries` does not account for files that
            // do not match the supported filename pattern.

bool diskmenu_filelist_init(int *num_entries) {
    if (nav_filelist_single_disable()) {
        if (num_entries) {
            *num_entries = nav_filelist_nb(FS_FILE) + nav_filelist_nb(FS_DIR);
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

bool diskmenu_filelist_isdir(void) {
    return nav_file_isdir();
}

bool diskmenu_filelist_cd() {
    return nav_dir_cd();
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

void diskmenu_display_print(void) {
    // Do nothing.  All rendering is immediate on hardware.
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

int diskmenu_param(int last_value) {
    uint16_t adc[4];
    adc_convert(&adc);
    uint8_t cursor = adc[1] >> 8;
    uint8_t deadzone = cursor & 1;
    cursor >>= 1;
    if (!deadzone || abs(cursor - last_value) > 1) {
        return cursor;
    }
    else {
        return last_value;
    }
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

void diskmenu_dbg(const char *str) {
    print_dbg(str);
}

