#ifndef _GLOBALS_H_
#define _GLOBALS_H_

#include <stdbool.h>
#include <stdint.h>

#include "line_editor.h"
#include "region.h"
#include "scene_serialization_constants.h"
#include "teletype.h"

// global variables (defined in main.c)

// holds the current scene
extern scene_state_t scene_state;
extern char scene_text[SCENE_TEXT_LINES][SCENE_TEXT_CHARS];

// the current preset
extern uint8_t preset_select;

// holds screen data
extern region line[8];

// timer control for irq support in usb disk mode
extern bool default_timers_enabled;

// mode handling
typedef enum {
    M_LIVE,
    M_EDIT,
    M_PATTERN,
    M_PRESET_W,
    M_PRESET_R,
    M_HELP
} tele_mode_t;

// event queue
// void empty_event_handlers(void);       // local to main.c
void assign_main_event_handlers(void);
void assign_msc_event_handlers(void);
// void check_events(void);               // local to main.c

// device config
typedef struct {
    uint8_t flip;
} device_config_t;

void set_mode(tele_mode_t mode);
void set_last_mode(void);
void clear_delays_and_slews(scene_state_t *ss);

void assign_main_event_handlers(void);

// global copy buffer
extern char copy_buffer[SCENE_TEXT_LINES][SCENE_TEXT_CHARS];
extern uint8_t copy_buffer_len;

#endif
