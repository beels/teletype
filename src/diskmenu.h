#ifndef _DISKMENU_H_
#define _DISKMENU_H_

#include <stdint.h>
#include <stdbool.h>

enum {
    kModeR,
    kModeW,
    kErrFileExists,
    kErrUnknown,
};

// ============================================================================
//                              SUBSYSTEM LOGIC
// ----------------------------------------------------------------------------

// Main entry point
void tele_usb_disk(void);

// Subsystem interface
void diskmenu_set_exit_handler(void (*exit_handler)(void));
bool diskmenu_set_scratch_buffer(char *buffer, uint32_t length);
void diskmenu_init(void);
void diskmenu_handle_short_press(void);
void diskmenu_handle_long_press(void);
void diskmenu_handle_button_timeout(void);
void diskmenu_handle_PollADC(void);

// ============================================================================
//                           USB DISK MINIMAL MENU
// ----------------------------------------------------------------------------

bool tele_usb_disk_write_operation(uint8_t* plun_state, uint8_t* plun);
void tele_usb_disk_read_operation(void);

#endif
