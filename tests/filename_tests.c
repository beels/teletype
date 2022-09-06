#include "filename_tests.h"

#include "greatest/greatest.h"

#include "../src/filename.h"

#include <string.h>

TEST filename_find_wildcard_range_test() {
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
        rc = filename_find_wildcard_range(&wc_start, filename_buffer);

        ASSERT_EQm(data[i].fname, rc, data[i].rc);
        ASSERT_EQm(data[i].fname, wc_start, data[i].start);
    }

    PASS();
}

TEST filename_increment_test() {
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
        filename_increment_version(filename_buffer, data[i].start);

        ASSERT_STR_EQ(data[i].out, filename_buffer);
    }

    PASS();
}

TEST filename_ellipsis_test() {
    char filename_buffer[FNAME_BUFFER_LEN];

    struct data_type {
        char *in;
        int   maxchars;
        char *out;
    };

    struct data_type data[] = {
        { "abstraction.txt",    6, "...txt" },
        { "abstraction.txt",    7, "a...txt" },
        { "abstraction.txt",    8, "ab...txt" },
        { "abstraction.txt",    9, "abs...txt" },
        { "abstraction.txt",   10, "abst...txt" },
        { "abstraction.txt",   14, "abstract...txt" },
        { "abstraction.txt",   15, "abstraction.txt" },
        { "abstraction.txt",   30, "abstraction.txt" },
        { "abstract-text",      2, ".." },
        { "abstract-text",      3, "a.." },
        { "abstract-text",      8, "abstra.." },
        { "abstract-text",     12, "abstract-t.." },
        { "abstract-text",     13, "abstract-text" },
        { "abstract-text",     14, "abstract-text" },
        { "POLYRHYTHIC CHORD PROGRESSION.100",
                               28, "POLYRHYTHIC CHORD PROG...100" },
    };

    for (int i = 0; i < sizeof(data) / sizeof(*data); ++i) {
        strncpy(filename_buffer, data[i].in, FNAME_BUFFER_LEN);
        filename_ellipsis(filename_buffer, data[i].maxchars);

        ASSERT_STR_EQ(data[i].out, filename_buffer);
    }

    PASS();
}

SUITE(filename_suite) {
    RUN_TEST(filename_find_wildcard_range_test);
    RUN_TEST(filename_increment_test);
    RUN_TEST(filename_ellipsis_test);
}
