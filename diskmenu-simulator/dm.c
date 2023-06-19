#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "diskmenu.h"
#include "util.h"
#include "events.h"

#define dbg printf("at: %s %s %d\n", __FILE__, __FUNCTION__, __LINE__);

// ============================================================================
//                        APPLICATION INFRASTRUCTURE
// ----------------------------------------------------------------------------

void handler_None(int32_t data) {}

// defined in globals.h
void empty_event_handlers() {
    for (size_t i = 0; i < kNumEventTypes; i++) {
        app_event_handlers[i] = &handler_None;
    }
}

// global array of pointers to handlers
void (*app_event_handlers[kNumEventTypes])(s32 data) = { handler_None };

char scratch[1024];
int dm_lastPoll = -1;
extern char display_lines[8][43];

enum {
    kQuit,
    kShort,
    kLong,
    kTimeout,
    kParam
};

int parse_dm(const char *in, int *arg) {
    if (0 == strcmp(in, "Q\n")) {
        return kQuit;
    }
    if (0 == strcmp(in, "S\n")) {
        return kShort;
    }
    if (0 == strcmp(in, "L\n")) {
        return kLong;
    }
    if (0 == strcmp(in, "T\n")) {
        return kTimeout;
    }
    if (in[0] == 'P' && in[1] == ' ') {
        *arg = atoi(in + 2);
        return kParam;
    }

    return -1;
}

int simulator_active = 1;

void done() {
    printf("Exiting diskmenu subsystem\n");
    simulator_active = 0;
}

int main() {
    char *in;
    time_t t;
    int i;

    srand((unsigned)time(&t));

    in = malloc(256);

    printf("teletype diskmenu. ('q' quits)\n\n");

    tele_usb_disk();
    // diskmenu_display_init();
    // diskmenu_set_exit_handler(&done);
    // diskmenu_set_scratch_buffer(scratch, sizeof(scratch));

    // diskmenu_init();
    (*app_event_handlers[kEventPollADC])(0);

    while (simulator_active) {

        (*app_event_handlers[kEventPollADC])(0);
        (*app_event_handlers[kEventScreenRefresh])(0);

        printf("> ");
        fgets(in, 256, stdin);

        i = 0;
        while (in[i]) {
            in[i] = toupper(in[i]);
            i++;
        }

        int arg = 0;
        int opcode = parse_dm(in, &arg);

        switch (opcode) {
            case kShort:
                {
                    //printf("short\n");
                    (*app_event_handlers[kEventFront])(0);
                    (*app_event_handlers[kEventFront])(1);
                } break;
            case kLong:
                {
                    //printf("long\n");
                    (*app_event_handlers[kEventFront])(0);
                    for (int i = 0; i < 8; ++i) {
                        (*app_event_handlers[kEventKeyTimer])(0);
                    }
                    (*app_event_handlers[kEventFront])(1);
                } break;
            // case kTimeout:
            //     {
            //         //printf("timeout\n");
            //         diskmenu_handle_button_timeout();
            //     } break;
            case kParam:
                {
                    //printf("param: %d\n", arg);
                    dm_lastPoll = arg;
                    (*app_event_handlers[kEventPollADC])(0);
                } break;
            case kQuit:
                {
                    simulator_active = 0;
                } break;
            default: {
                printf("Unrecognized: %s\n", in);
                }
        };

        printf("\n");
    }

    free(in);

    printf("(teletype diskmenu exit.)\n");
}
