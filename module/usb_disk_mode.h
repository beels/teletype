#ifndef _USB_DISK_MODE_H_
#define _USB_DISK_MODE_H_

#include <stdint.h>

////////////////////////////////////////////////////////////////////////////////
// usb disk

void tele_usb_disk(void);
void tele_usb_disk_handler_Front(int32_t);
void tele_usb_disk_handler_KeyTimer(int32_t data);

#endif
