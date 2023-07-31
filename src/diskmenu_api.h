#ifndef _DISKMENU_API_H_
#define _DISKMENU_API_H_

#include <stdint.h>

#include "state.h"
#include "scene_serialization.h"

#if defined(SIM)

#define FOPEN_MODE_R      1
#define FOPEN_MODE_W      2
#define FS_ERR_FILE_EXIST 3
#define FS_STRING         char *

#define _MEM_TYPE_SLOW_

extern uint8_t fs_g_status;

#else

#include "fs_com.h"
#include "navigation.h"
#include "uhi_msc_mem.h"

#endif

// ============================================================================
//                        APPLICATION INFRASTRUCTURE
// ----------------------------------------------------------------------------

extern char copy_buffer[SCENE_TEXT_LINES][SCENE_TEXT_CHARS];
extern uint8_t copy_buffer_len;
extern uint8_t diskmenu_foreground;
extern bool default_timers_enabled;

void empty_event_handlers(void);
void diskmenu_assign_handlers(void (*frontHandler)(int32_t data),
                              void (*pollHandler)(int32_t data),
                              void (*refreshHandler)(int32_t data));


// Subsystem control
void tele_usb_disk_finish(void);

// ============================================================================
//                           HARDWARE ABSTRACTION
// ----------------------------------------------------------------------------

#if 0
// Local functions for test/simulator abstraction
// file IO
void diskmenu_io_close(void);
bool diskmenu_io_open(uint8_t *status, uint8_t fopen_mode);

            // ARB: simple redirects for tele_usb variants.  Can these be
            // deleted?

void diskmenu_io_putc(uint8_t c);
void diskmenu_io_write_buf(uint8_t* buffer, uint16_t size);
uint16_t diskmenu_io_getc(void);
bool diskmenu_io_eof(void);

bool diskmenu_io_create(uint8_t *status, char *filename);
// filesystem navigation
bool diskmenu_device_open(void);
bool diskmenu_filelist_init(int *num_entries);
bool diskmenu_filelist_find(char *output, uint8_t length, char *pattern);
bool diskmenu_filelist_goto(char *output, int len, uint8_t index);
void diskmenu_filelist_close(void);
bool diskmenu_filelist_isdir(void);
bool diskmenu_filelist_cd(void);
bool diskmenu_filelist_gotoparent(void);
#else
// Default (hardware) implementations of functions that are overridden in
// diskmenu-simulator.

uint16_t file_write_buf(uint8_t _MEM_TYPE_SLOW_ *, uint16_t);
uint16_t nav_filelist_nb(bool);
bool nav_filelist_reset(void);
bool nav_filelist_single_disable(void);
bool nav_filelist_validpos(void);
bool nav_partition_mount(void);
void file_close(void);
uint8_t file_eof(void);
uint16_t file_getc(void);
bool file_open(uint8_t);
bool file_putc(uint8_t);
bool nav_dir_cd(void);
bool nav_dir_gotoparent(void);
bool nav_drive_set(uint8_t);
void nav_exit(void);
bool nav_file_create(FS_STRING);
bool nav_file_create(const FS_STRING);
bool nav_file_isdir(void);
bool nav_filelist_findname(const FS_STRING, bool);
bool nav_filelist_goto(uint16_t);
uint8_t uhi_msc_mem_get_lun(void);

uint8_t irqs_pause(void);
void irqs_resume(uint8_t flags);

#endif

// display
void diskmenu_display_line(int line_no, const char *text, bool selected);
uint8_t display_font_string_position(const char* str, uint8_t pos);
// flash
uint8_t diskmenu_flash_scene_id(void);
void diskmenu_flash_read(
                      uint8_t scene_id,
                      scene_state_t *scene,
                      char (*text)[SCENE_TEXT_LINES][SCENE_TEXT_CHARS]);
const char *diskmenu_flash_scene_text(uint8_t scene_id);
void diskmenu_flash_write(
                     uint8_t scene_id,
                     scene_state_t *scene,
                     char (*text)[SCENE_TEXT_LINES][SCENE_TEXT_CHARS]);

int diskmenu_param_scaled(uint8_t resolution, uint8_t scale);

void diskmenu_dbg(const char *str);
#endif
