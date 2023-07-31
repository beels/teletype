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

uint8_t diskmenu_foreground = 0xa;

// ============================================================================
//                        APPLICATION INFRASTRUCTURE
// ----------------------------------------------------------------------------

void diskmenu_assign_handlers(void (*frontHandler)(int32_t data),
                              void (*pollHandler)(int32_t data),
                              void (*refreshHandler)(int32_t data)) {
    app_event_handlers[kEventFront] = frontHandler;
    app_event_handlers[kEventPollADC] = pollHandler;
    app_event_handlers[kEventScreenRefresh] = refreshHandler;
}

// ============================================================================
//                             Subsystem Control
// ----------------------------------------------------------------------------

void tele_usb_disk_finish() {
    // renable teletype
    set_mode(M_LIVE);
    assign_main_event_handlers();
    default_timers_enabled = true;
}

// ============================================================================
//                           HARDWARE ABSTRACTION
// ----------------------------------------------------------------------------

void diskmenu_display_print(void) {
    // Do nothing.  All rendering is immediate on hardware.
}

void diskmenu_display_line(int line_no, const char *text, bool selected)
{
    int fg = selected ? 0 : diskmenu_foreground;
    int bg = selected ? diskmenu_foreground : 0;

    region_fill(&line[line_no], bg);

    if (text && text[0]) {
        font_string_region_clip(&line[line_no], text, 2, 0, fg, bg);
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

    if (!deadzone || abs(value - last_knob) > 1) {
        last_knob = value;
    }
    else {
        value = last_knob;
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

