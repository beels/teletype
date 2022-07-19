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
// The menu consists of five entries:
//
// 1. Read from file 'abcde007.txt'
// 2. Write to file 'abcde007.txt'
// 3. Write to file 'abcde008.txt' (conditional)
// 4. Legacy WRITE/READ operation
// 5. Exit USB disk mode           (default)
//
// When the menu is first displayed, the fifth entry is selected, and a short
// press on the Teletype panel button will advance to the next entry, wrapping
// around to the beginning as needed.  A long press on the panel button will
// execute the selected menu entry and then exit USB disk mode.
//
// The "Exit USB disk mode" entry exits USB disk mode without taking any other
// action.
//
// The "Legacy WRITE/READ operation" entry writes all scenes to
// 'tt[0-3][0-9]s.txt' and then reads all scenes from 'tt[0-3][0-9].txt', just
// like as Teletype USB disk mode has always done.
//
// The other three entries read or write the currently-selected scene from or
// to the named file listed in the menu entry.  Note that the
// "currently-selected scene" is *not* the currently-running Teletype scene.
// It is the sceen that is displayed if you hit the ESC key on the keyboard.
// Often this is also the currently-running Teletype scene, but they can
// diverge.  The scene number and title of the currently-selected scene is
// displayed at the top of the menu for clarity.
//
// The file name listed in the menu entry is chosen as follows:
//
// - By default, the filename is the traditional 'tt[0-3[0-9].txt' name
//   associated with the currently-selected scene.  I.e., if scene 17 is
//   currently selected, the first menu entry will say "Read from file
//   'tt17.txt'".
//
// - If the description of the currently-selected scene contains a line
//   beginning with the token ":FNAME:" then the filename will be the text that
//   follows that token.  I.e., if the description contains a line that reads
//   ":FNAME:MYSCENE.TXT", then the first menu entry will say "Read from file
//   'myscene.txt'".
//
// - If the filename designated with the ":FNAME:" token contains an asterisk,
//   then the firmware will search through all files in the root of the USB
//   disk that start with the text before the asterisk and will choose the file
//   that comes last in alphanumeric order.  This name will be displayed in the
//   first and second menu entries.  In addition, the firmware will attempt to
//   read the part of the filename that starts at the asterisk as an integer,
//   and increment the integer by 1.  The result will be displayed in the third
//   menu entry.  I.e., if the disk contains the files 'abcde006.txt' and
//   'abcde007.txt' and the scene description contains the text
//   ":FNAME:ABCDE*.TXT" then the menu will appear as listed at the top of this
//   note.  This feature gives the user the ability to save a given scene
//   repeatedly and always have the option to (a) revert to the most recently
//   saved version, (b) update the most recently saved version, or (c) save to
//   a new file, which now becomes the most recently saved version, and
//   preserve the existing saved versions as an archive.
//
//   Note that the asterisk can appear anywhere in the file name, so names such
//   as 'abc.001' can be used if your operating system editor does not rely on
//   the '*.txt' extension to operate correctly.

void tele_usb_disk(void);
void tele_usb_disk_handler_Front(int32_t);
void tele_usb_disk_handler_KeyTimer(int32_t data);

#endif
