#ifndef _DISKMENU_API_H_
#define _DISKMENU_API_H_

#include <stdint.h>

#include "state.h"
#include "scene_serialization.h"

// ============================================================================
//                        APPLICATION INFRASTRUCTURE
// ----------------------------------------------------------------------------

void diskmenu_assign_msc_event_handlers(void);

uint8_t diskmenu_irqs_pause(void);
void diskmenu_irqs_resume(uint8_t flags);

// Subsystem control
void tele_usb_disk_finish(void);

// ============================================================================
//                           HARDWARE ABSTRACTION
// ----------------------------------------------------------------------------

// Local functions for test/simulator abstraction
// file IO
void diskmenu_io_close(void);
bool diskmenu_io_open(uint8_t *status, uint8_t fopen_mode);
     // ARB: remove after refactor
uint16_t diskmenu_io_read_buf(uint8_t *buffer,
                                   uint16_t u16_buf_size);
void diskmenu_io_putc(uint8_t c);
void diskmenu_io_write_buf(uint8_t* buffer, uint16_t size);
uint16_t diskmenu_io_getc(void);
bool diskmenu_io_eof(void);
bool diskmenu_io_create(uint8_t *status, char *filename);
// filesystem navigation
bool diskmenu_device_open(void);
bool diskmenu_device_close(void);
bool diskmenu_filelist_init(int *num_entries);
bool diskmenu_filelist_find(char *output, uint8_t length, char *pattern);
bool diskmenu_filelist_goto(char *output, int len, uint8_t index);
void diskmenu_filelist_close(void);
// display
void diskmenu_display_clear(int line_no, uint8_t bg);
void diskmenu_display_set(int line_no,
                          uint8_t offset,
                          const char *text,
                          uint8_t fg,
                          uint8_t bg);
void diskmenu_display_draw(int line_no);
void diskmenu_display_print(void);
void diskmenu_display_line(int line_no, const char *text);
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

int diskmenu_param(int last_value);
int diskmenu_param_scaled(uint8_t resolution, uint8_t scale);

void diskmenu_dbg(const char *str);
#endif

