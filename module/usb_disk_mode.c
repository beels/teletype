#include "usb_disk_mode.h"

#include <ctype.h>
#include <stdint.h>
#include <string.h>

#include "flash.h"
#include "globals.h"
#include "scene_serialization.h"

// libavr32
#include "font.h"
#include "interrupts.h"
#include "region.h"
#include "util.h"

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

// Local functions for usb filesystem serialization
void tele_usb_putc(void* self_data, uint8_t c);
void tele_usb_write_buf(void* self_data, uint8_t* buffer, uint16_t size);
uint16_t tele_usb_getc(void* self_data);
bool tele_usb_eof(void* self_data);
void tele_usb_disk_init(void);
void tele_usb_disk_finish(void);

static u8 flags;

void tele_usb_putc(void* self_data, uint8_t c) {
    file_putc(c);
}

void tele_usb_write_buf(void* self_data, uint8_t* buffer, uint16_t size) {
    file_write_buf(buffer, size);
}

uint16_t tele_usb_getc(void* self_data) {
    return file_getc();
}

bool tele_usb_eof(void* self_data) {
    return file_eof() != 0;
}

void tele_usb_disk_handler_Front(int32_t data) {
    tele_usb_disk_write_and_save();
    tele_usb_disk_finish();
}

void tele_usb_disk_init() {
    // disable event handlers while doing USB write
    assign_msc_event_handlers();

    // disable timers
    // flags = irqs_pause();
    default_timers_enabled = false;

    // clear screen
    for (size_t i = 0; i < 8; i++) {
        region_fill(&line[i], 0);
        region_draw(&line[i]);
    }
}

void tele_usb_disk_finish() {
    // renable teletype
    set_mode(M_LIVE);
    assign_main_event_handlers();
    // irqs_resume(flags);
    default_timers_enabled = true;
}

// usb disk mode entry point
void tele_usb_disk() {
    char text_buffer[40];
    print_dbg("\r\nusb");

    tele_usb_disk_init();

    // We assume that there is one and only one available LUN, otherwise it is
    // not safe to iterate through all possible LUN even after we finish
    // writing saving and loading scenes.

    for (uint8_t lun = 0; (lun < uhi_msc_mem_get_lun()) && (lun < 8); lun++) {
        // print_dbg("\r\nlun: ");
        // print_dbg_ulong(lun);

        // Mount drive
        nav_drive_set(lun);
        if (!nav_partition_mount()) {
            if (fs_g_status == FS_ERR_HW_NO_PRESENT) {
                continue;
            }
            print_dbg("\r\nfail");
            continue;
        }

        // Print current scene number and name
        //
        // Bug: it seems that the scene number is not recorded anywhere.  We
        // have only the current preset (`preset_select`), which can be chenged
        // in the preset read screen without actually changing the current
        // scene.

        {
            itoa(preset_select, text_buffer, 10);

            region_fill(&line[0], 0);
            font_string_region_clip(&line[0], text_buffer, 2, 0, 0xa, 0);
            u8 pos = font_string_position(text_buffer, strlen(text_buffer));
            pos += font_string_position(" ", 1);
            font_string_region_clip(&line[0],
                                    flash_scene_text(preset_select, 0),
                                    pos + 2, 0, 0xa, 0);
            region_draw(&line[0]);
        }

        {
            strcpy(text_buffer, "PRESS TO CONFIRM");

            region_fill(&line[1], 0);
            font_string_region_clip(&line[1], text_buffer, 2, 0, 0xa, 0);
            region_draw(&line[1]);
        }

        break;
    }
}

void tele_usb_disk_write_and_save() {
    char text_buffer[40];

    // WRITE SCENES
    char filename[13];
    strcpy(filename, "tt00s.txt");

    print_dbg("\r\nwriting scenes");
    strcpy(text_buffer, "WRITE");
    region_fill(&line[2], 0);
    // The `..._tab` variant shifts to column 48 when it encounters a `|`
    // character.  Is this desired behavior?
    font_string_region_clip_tab(&line[2], text_buffer, 2, 0, 0xa, 0);
    region_draw(&line[2]);

    for (int i = 0; i < SCENE_SLOTS; i++) {
        scene_state_t scene;
        ss_init(&scene);

        char text[SCENE_TEXT_LINES][SCENE_TEXT_CHARS];
        memset(text, 0, SCENE_TEXT_LINES * SCENE_TEXT_CHARS);

        strcat(text_buffer, ".");  // strcat is dangerous, make sure the
                                   // buffer is large enough!
        region_fill(&line[2], 0);
        font_string_region_clip_tab(&line[2], text_buffer, 2, 0, 0xa, 0);
        region_draw(&line[2]);

        flash_read(i, &scene, &text, 1, 1, 1);

        if (!nav_file_create((FS_STRING)filename)) {
            if (fs_g_status != FS_ERR_FILE_EXIST) {
                if (fs_g_status == FS_LUN_WP) {
                    // Test can be done only on no write protected
                    // device
                    continue;
                }
                print_dbg("\r\nfail");
                continue;
            }
        }

        if (!file_open(FOPEN_MODE_W)) {
            if (fs_g_status == FS_LUN_WP) {
                // Test can be done only on no write protected
                // device
                continue;
            }
            print_dbg("\r\nfail");
            continue;
        }

        tt_serializer_t tele_usb_writer;
        tele_usb_writer.write_char = &tele_usb_putc;
        tele_usb_writer.write_buffer = &tele_usb_write_buf;
        tele_usb_writer.print_dbg = &print_dbg;
        tele_usb_writer.data =
            NULL;  // asf disk i/o holds state, no handles needed
        serialize_scene(&tele_usb_writer, &scene, &text);

        file_close();

        if (filename[3] == '9') {
            filename[3] = '0';
            filename[2]++;
        }
        else
            filename[3]++;

        print_dbg(".");
    }

    nav_filelist_reset();


    // READ SCENES
    strcpy(filename, "tt00.txt");
    print_dbg("\r\nreading scenes...");

    strcpy(text_buffer, "READ");
    region_fill(&line[3], 0);
    font_string_region_clip_tab(&line[3], text_buffer, 2, 0, 0xa, 0);
    region_draw(&line[3]);

    for (int i = 0; i < SCENE_SLOTS; i++) {
        scene_state_t scene;
        ss_init(&scene);
        char text[SCENE_TEXT_LINES][SCENE_TEXT_CHARS];
        memset(text, 0, SCENE_TEXT_LINES * SCENE_TEXT_CHARS);

        strcat(text_buffer, ".");  // strcat is dangerous, make sure the
                                   // buffer is large enough!
        region_fill(&line[3], 0);
        font_string_region_clip_tab(&line[3], text_buffer, 2, 0, 0xa, 0);
        region_draw(&line[3]);
        if (nav_filelist_findname(filename, 0)) {
            print_dbg("\r\nfound: ");
            print_dbg(filename);
            if (!file_open(FOPEN_MODE_R))
                print_dbg("\r\ncan't open");
            else {
                tt_deserializer_t tele_usb_reader;
                tele_usb_reader.read_char = &tele_usb_getc;
                tele_usb_reader.eof = &tele_usb_eof;
                tele_usb_reader.print_dbg = &print_dbg;
                tele_usb_reader.data =
                    NULL;  // asf disk i/o holds state, no handles needed
                deserialize_scene(&tele_usb_reader, &scene, &text);

                file_close();
                flash_write(i, &scene, &text);
            }
        }

        nav_filelist_reset();

        if (filename[3] == '9') {
            filename[3] = '0';
            filename[2]++;
        }
        else
            filename[3]++;
    }

    nav_exit();
}
