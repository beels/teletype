#include "filename_tests.h"

#include "greatest/greatest.h"

#include <string.h>

#define FNAME_BUFFER_LEN 13
#define true 1
#define false 0
#define bool int

void tele_usb_disk_increment_filename(char *filename, int wildcard_start)
{
    int n = atoi(filename + wildcard_start) + 1;

    // Write the increment number.

    int i = wildcard_start;
    while (i < FNAME_BUFFER_LEN && '0' <= filename[i] && filename[i] <= '9') {
        ++i;
    }

    while (wildcard_start <= --i) {
        filename[i] = '0' + (n % 10);
        n /= 10;
    }
}

bool tele_usb_find_wildcard_range(int *wildcard_start, char *filename)
{
    for (int i = 0; i < FNAME_BUFFER_LEN; ++i) {
        if ('*' == filename[i]) {
            *wildcard_start = i;

            return true;
        }
    }

    return false;
}

TEST filename_find_wildcard_range() {
    int wc_start, wc_len, wc_tail;
    char filename_buffer[FNAME_BUFFER_LEN];
    bool rc;

    struct data_type {
        char *fname; bool rc; int start;
    };

    struct data_type data[] = {
        { "abc*efg.txt", true,  3, },
        { "abc*.txt",    true,  3, },
        { "abc.txt",    false, -1, },
    };

    for (int i = 0; i < sizeof(data) / sizeof(*data); ++i) {
        wc_start = wc_len = wc_tail = -1;
        strncpy(filename_buffer, data[i].fname, FNAME_BUFFER_LEN);
        rc = tele_usb_find_wildcard_range(&wc_start, filename_buffer);

        ASSERT_EQm(data[i].fname, rc, data[i].rc);
        ASSERT_EQm(data[i].fname, wc_start, data[i].start);
    }

    PASS();
}

TEST filename_increment() {
    char filename_buffer[FNAME_BUFFER_LEN];

    struct data_type {
        char *in;
        int   start;
        char *out;
    };

    struct data_type data[] = {
        { "087efg.txt",    0, "088efg.txt" },
        { "abc087.txt",    3, "abc088.txt" },
        { "abcdefgh.001", 10, "abcdefgh.002" },
        { "ab333fgh.001", 10, "ab333fgh.002" },
        { "abc08efg.txt",  3, "abc09efg.txt" },
        { "abc09efg.txt",  3, "abc10efg.txt" },
        { "abc99efg.txt",  3, "abc00efg.txt" },
        { "abc08.txt"   ,  3, "abc09.txt"    },
        { "abc19.txt"   ,  3, "abc20.txt"    }
    };

    for (int i = 0; i < sizeof(data) / sizeof(*data); ++i) {
        strncpy(filename_buffer, data[i].in, FNAME_BUFFER_LEN);
        tele_usb_disk_increment_filename(filename_buffer, data[i].start);

        ASSERT_STR_EQ(data[i].out, filename_buffer);
    }

    PASS();
}

SUITE(filename_suite) {
    RUN_TEST(filename_find_wildcard_range);
    RUN_TEST(filename_increment);
}
