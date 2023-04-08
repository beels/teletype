#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "diskmenu.h"
#include "util.h"

#define dbg printf("at: %s %s %d\n", __FILE__, __FUNCTION__, __LINE__);

char scratch[1024];
int dm_lastPoll = -1;
extern char display_lines[8][43];
void diskmenu_display_init(void);
void diskmenu_display_print(void);

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

    diskmenu_display_init();
    diskmenu_set_exit_handler(&done);
    diskmenu_set_scratch_buffer(scratch, sizeof(scratch));

    diskmenu_init();
    diskmenu_handle_PollADC();

    while (simulator_active) {

        diskmenu_handle_PollADC();
        diskmenu_display_print();

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
                    diskmenu_handle_short_press();
                } break;
            case kLong:
                {
                    //printf("long\n");
                    diskmenu_handle_long_press();
                } break;
            case kTimeout:
                {
                    //printf("timeout\n");
                    diskmenu_handle_button_timeout();
                } break;
            case kParam:
                {
                    //printf("param: %d\n", arg);
                    dm_lastPoll = arg;
                    diskmenu_handle_PollADC();
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
