#ifndef _USB_DISK_MODE_H_
#define _USB_DISK_MODE_H_

#include <stdint.h>

////////////////////////////////////////////////////////////////////////////////
// usb disk

// Notes on updates to tele_usb_disk
//
// With the addition of tele_usb_disk_handler_Front and
// tele_usb_disk_handler_KeyTimer, tele_usb_disk now supports menu-driven
// reading and writing of Teletype scenes.
//
// The menu consists of a title display and five entries:
//
// 17 MY SCENE TITLE
//   Read 'MY SCENE TITLE.007'
//   Write ''MY SCENE TITLE.007'
//   Write ''MY SCENE TITLE.008'         (conditional)
//   Browse USB disk
// > Exit USB disk mode                   (default)
//
// When the menu is first displayed, the "exit" entry is selected, and a short
// press on the Teletype panel button will advance to the next entry ("read"),
// wrapping around to the beginning as needed.  A long press on the panel
// button will execute the selected menu entry and then exit USB disk mode.
//
// The "Exit USB disk mode" entry exits USB disk mode without taking any other
// action.  Because it is the most conservative option, it is the default entry
// selected when you enter USB disk mode.
//
// The other three entries read or write the currently-selected scene from or
// to the file listed in the menu entry.  These entries operate on the content
// of flash memory, not on the currently-running scene.  Therefore, the
// currently-runing scene will not immediately change when a file is read.  The
// scene must be re-loaded (if desired) from flash after you exit USB disk
// mode.  Note that the "currently-selected scene" is the most-recently-loaded
// Teletype scene in flash memory, which corresponds to the starting point for
// whatever scripts are currently running on the Teletype.  This is not
// necessarily the same scene number and title that is displayed when you hit
// the ESC key.
//
// The file name listed in the menu entry is the title of the selected scene,
// with a numeric file extension added.  The menu gives the option of reading
// and writing from the existing file on the USB disk that has the highest
// matching numeric extension, or writing to a new file on the USB disk with
// the next available numeric extension.  In the sample menu above, there is a
// file on the disk named "MY SCENE TITLE.007", but no file with a
// higher-numbered extension.  The numeric extension allows the user to easily
// save successive versions of a scene while it is being developed.
//
// The "browse" entry takes the user to a scrolling list of files on the USB
// disk, from which they can select an alternate file to read or write:
//
// MY LITTLE SCENE
//   A SEQUENCER.001
//   A SEQUENCER.002
//   CHORD PROGRESSION.001
// > CHORD PROGRESSION.002
//   CHORD PROGRESSION.003
//   MY LITTLE SCENE.001
//   MY LITTLE SCENE.002
//
// The file list can be navigated using the PARAM knob, and the file currently
// marked with a ">" on the left can be selected by a long press on the front
// button.  When a file is selected the user is returned to the main menu,
// where they can read/write the selected file.
//
// Anywhere in the USB disk mode, when the user presses the front button for
// long enough to be considered a "long" press, the ">" marker changes to a
// "*".

void handler_usb_PollADC(int32_t data);
void handler_usb_Front(int32_t data);
void handler_usb_ScreenRefresh(int32_t data);
void tele_usb_disk_dewb(void);

void tele_usb_disk(void);
void tele_usb_disk_handler_Front(int32_t);
void tele_usb_disk_handler_KeyTimer(int32_t data);
void tele_usb_disk_PollADC(int32_t);

#endif
