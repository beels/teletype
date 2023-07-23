#ifndef _DISKMENU_H_
#define _DISKMENU_H_

#include <stdint.h>
#include <stdbool.h>

// ============================================================================
//                              SUBSYSTEM LOGIC
// ----------------------------------------------------------------------------

// Main entry point
void tele_usb_disk(void);

// Top-level save/load menu handlers
void handler_usb_PollADC(int32_t data);
void handler_usb_Front(int32_t data);
void handler_usb_ScreenRefresh(int32_t data);

// ============================================================================
//                           USB DISK MINIMAL MENU
// ----------------------------------------------------------------------------

bool tele_usb_disk_write_operation(void);
void tele_usb_disk_read_operation(void);

#endif
