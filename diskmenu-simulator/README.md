# Simulator Architecture

This diskmenu simulator emulates the teletype display and USB disk access by
overriding at link time the functions that are defined in the "HARDWARE
ABSTRACTION" seciton of `src/diskmenu.h`.  These functions are given their
default (hardware) implementations in `module/usb_disk_mode.c`.

