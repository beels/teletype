#include "sort_tests.h"

#include "greatest/greatest.h"

#include "../src/sort.h"

#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

TEST sort_initialize_test() {
    uint8_t ibuff[SORT_BUFFER_LEN];
    uint8_t vbuff[SORT_BUFFER_LEN];
    sort_index_t index = { .index = ibuff, .values = vbuff };

    sort_initialize(&index);

    for (int i = 0; i < SORT_BUFFER_LEN; ++i) {
        char item[4];
        itoa(i, item, 10);
        ASSERT_EQm(item, false, sort_validate_slot(&index, i));
    }

    PASS();
}

TEST sort_set_slot_test() {
    uint8_t ibuff[SORT_BUFFER_LEN];
    uint8_t vbuff[SORT_BUFFER_LEN];
    sort_index_t index = { .index = ibuff, .values = vbuff };

    sort_initialize(&index);

    sort_set_slot(&index, 17, 3);

    for (int i = 0; i < SORT_BUFFER_LEN; ++i) {
        char item[4];
        itoa(i, item, 10);
        if (3 == i) {
            ASSERT_EQm(item, 17, index.values[3]);
            ASSERT_EQm(item, 3, index.index[17]);
            ASSERT_EQm(item, true, sort_validate_slot(&index, i));
        }
        else {
            ASSERT_EQm(item, false, sort_validate_slot(&index, i));
        }
    }

    sort_set_slot(&index, 16, 3);

    for (int i = 0; i < SORT_BUFFER_LEN; ++i) {
        char item[4];
        itoa(i, item, 10);
        if (3 == i) {
            ASSERT_EQm(item, 16, index.values[3]);
            ASSERT_EQm(item, 3, index.index[16]);
            ASSERT_EQm(item, true, sort_validate_slot(&index, i));
        }
        else {
            ASSERT_EQm(item, false, sort_validate_slot(&index, i));
        }
    }

    sort_set_slot(&index, 17, 5);

    for (int i = 0; i < SORT_BUFFER_LEN; ++i) {
        char item[4];
        itoa(i, item, 10);
        if (5 == i) {
            ASSERT_EQm(item, 17, index.values[5]);
            ASSERT_EQm(item, 5, index.index[17]);
            ASSERT_EQm(item, true, sort_validate_slot(&index, i));
        }
        else if (3 == i) {
            ASSERT_EQm(item, 16, index.values[3]);
            ASSERT_EQm(item, 3, index.index[16]);
            ASSERT_EQm(item, true, sort_validate_slot(&index, i));
        }
        else {
            ASSERT_EQm(item, false, sort_validate_slot(&index, i));
        }
    }

    sort_initialize(&index);

    sort_set_slot(&index, 255, 255);

    for (int i = 0; i < SORT_BUFFER_LEN; ++i) {
        char item[4];
        itoa(i, item, 10);
        if (255 == i) {
            ASSERT_EQm(item, 255, index.values[255]);
            ASSERT_EQm(item, 255, index.index[255]);
            ASSERT_EQm(item, true, sort_validate_slot(&index, i));
        }
        else {
            ASSERT_EQm(item, false, sort_validate_slot(&index, i));
        }
    }

    sort_initialize(&index);

    sort_set_slot(&index, 0, 255);

    for (int i = 0; i < SORT_BUFFER_LEN; ++i) {
        char item[4];
        itoa(i, item, 10);
        if (255 == i) {
            ASSERT_EQm(item, 0, index.values[255]);
            ASSERT_EQm(item, 255, index.index[0]);
            ASSERT_EQm(item, true, sort_validate_slot(&index, i));
        }
        else {
            ASSERT_EQm(item, false, sort_validate_slot(&index, i));
        }
    }

    sort_initialize(&index);

    sort_set_slot(&index, 255, 0);

    for (int i = 0; i < SORT_BUFFER_LEN; ++i) {
        char item[4];
        itoa(i, item, 10);
        if (0 == i) {
            ASSERT_EQm(item, 255, index.values[0]);
            ASSERT_EQm(item, 0, index.index[255]);
            ASSERT_EQm(item, true, sort_validate_slot(&index, i));
        }
        else {
            ASSERT_EQm(item, false, sort_validate_slot(&index, i));
        }
    }

    PASS();
}

#define BATCH_SIZE 5

TEST sort_insert_string_test_0() {
    uint8_t ibuff[SORT_BUFFER_LEN];
    uint8_t vbuff[SORT_BUFFER_LEN];
    sort_index_t index = { .index = ibuff, .values = vbuff };

    sort_initialize(&index);

    char work_buffer[BATCH_SIZE][SORT_STRING_BUFFER_SIZE];

    for (int i = 0; i < BATCH_SIZE; ++i) {
        work_buffer[i][0] = 0;
    }

    sort_insert_string(work_buffer,
                       BATCH_SIZE,
                       &index,
                       0,
                       5,
                       7,
                       "hello");

    ASSERT_STR_EQ("hello", work_buffer[0]);

    for (int i = 1; i < BATCH_SIZE; ++i) {
        ASSERT_STR_EQ("", work_buffer[i]);
    }

    ASSERT_EQ(7, index.values[0]);
    ASSERT_EQ(0, index.index[7]);
    ASSERT_EQ(true, sort_validate_slot(&index, 0));
    ASSERT_EQ(false, sort_validate_slot(&index, 1));

    PASS();
}

TEST sort_insert_string_test_1() {
    uint8_t ibuff[SORT_BUFFER_LEN];
    uint8_t vbuff[SORT_BUFFER_LEN];
    sort_index_t index = { .index = ibuff, .values = vbuff };

    sort_initialize(&index);

    char work_buffer[BATCH_SIZE][SORT_STRING_BUFFER_SIZE];

    for (int i = 0; i < BATCH_SIZE; ++i) {
        work_buffer[i][0] = 0;
    }

    sort_insert_string(work_buffer,
                       BATCH_SIZE,
                       &index,
                       0,
                       5,
                       7,
                       "hello");

    sort_insert_string(work_buffer,
                       BATCH_SIZE,
                       &index,
                       0,
                       5,
                       11,
                       "oy");

    ASSERT_STR_EQ("hello", work_buffer[0]);
    ASSERT_STR_EQ("oy", work_buffer[1]);

    for (int i = 2; i < BATCH_SIZE; ++i) {
        ASSERT_STR_EQ("", work_buffer[i]);
    }

    ASSERT_EQ(7, index.values[0]);
    ASSERT_EQ(0, index.index[7]);
    ASSERT_EQ(true, sort_validate_slot(&index, 0));

    ASSERT_EQ(11, index.values[1]);
    ASSERT_EQ(1, index.index[11]);
    ASSERT_EQ(true, sort_validate_slot(&index, 1));

    ASSERT_EQ(false, sort_validate_slot(&index, 2));

    PASS();
}

TEST sort_insert_string_test_2() {
    uint8_t ibuff[SORT_BUFFER_LEN];
    uint8_t vbuff[SORT_BUFFER_LEN];
    sort_index_t index = { .index = ibuff, .values = vbuff };

    sort_initialize(&index);

    char work_buffer[BATCH_SIZE][SORT_STRING_BUFFER_SIZE];

    for (int i = 0; i < BATCH_SIZE; ++i) {
        work_buffer[i][0] = 0;
    }

    sort_insert_string(work_buffer,
                       BATCH_SIZE,
                       &index,
                       0,
                       5,
                       7,
                       "hello");

    sort_insert_string(work_buffer,
                       BATCH_SIZE,
                       &index,
                       0,
                       5,
                       11,
                       "oy");

    sort_insert_string(work_buffer,
                       BATCH_SIZE,
                       &index,
                       0,
                       5,
                       215,
                       "good-bye");

    ASSERT_STR_EQm(work_buffer[0], "good-bye", work_buffer[0]);
    ASSERT_STR_EQm(work_buffer[1], "hello", work_buffer[1]);
    ASSERT_STR_EQm(work_buffer[2], "oy", work_buffer[2]);

    for (int i = 3; i < BATCH_SIZE; ++i) {
        ASSERT_STR_EQ("", work_buffer[i]);
    }

    ASSERT_EQ(215, index.values[0]);
    ASSERT_EQ(0, index.index[215]);
    ASSERT_EQ(true, sort_validate_slot(&index, 0));

    ASSERT_EQ(7, index.values[1]);
    ASSERT_EQ(1, index.index[7]);
    ASSERT_EQ(true, sort_validate_slot(&index, 1));

    ASSERT_EQ(11, index.values[2]);
    ASSERT_EQ(2, index.index[11]);
    ASSERT_EQ(true, sort_validate_slot(&index, 2));

    ASSERT_EQ(false, sort_validate_slot(&index, 3));

    PASS();
}

TEST sort_insert_string_test_3() {
    uint8_t ibuff[SORT_BUFFER_LEN];
    uint8_t vbuff[SORT_BUFFER_LEN];
    sort_index_t index = { .index = ibuff, .values = vbuff };

    sort_initialize(&index);

    char work_buffer[BATCH_SIZE][SORT_STRING_BUFFER_SIZE];

    for (int i = 0; i < BATCH_SIZE; ++i) {
        work_buffer[i][0] = 0;
    }

    sort_insert_string(work_buffer,
                       BATCH_SIZE,
                       &index,
                       0,
                       5,
                       215,
                       "good-bye");

    sort_insert_string(work_buffer,
                       BATCH_SIZE,
                       &index,
                       0,
                       5,
                       11,
                       "oy");

    sort_insert_string(work_buffer,
                       BATCH_SIZE,
                       &index,
                       0,
                       5,
                       7,
                       "hello");

    ASSERT_STR_EQm(work_buffer[0], "good-bye", work_buffer[0]);
    ASSERT_STR_EQm(work_buffer[1], "hello", work_buffer[1]);
    ASSERT_STR_EQm(work_buffer[2], "oy", work_buffer[2]);

    for (int i = 3; i < BATCH_SIZE; ++i) {
        ASSERT_STR_EQ("", work_buffer[i]);
    }

    ASSERT_EQ(215, index.values[0]);
    ASSERT_EQ(0, index.index[215]);
    ASSERT_EQ(true, sort_validate_slot(&index, 0));

    ASSERT_EQ(7, index.values[1]);
    ASSERT_EQ(1, index.index[7]);
    ASSERT_EQ(true, sort_validate_slot(&index, 1));

    ASSERT_EQ(11, index.values[2]);
    ASSERT_EQ(2, index.index[11]);
    ASSERT_EQ(true, sort_validate_slot(&index, 2));

    ASSERT_EQ(false, sort_validate_slot(&index, 3));

    PASS();
}

struct data_type {
    int   index;
    char *string;
};

typedef struct {
    int               num_entries;
    struct data_type *entries;
} data_type_profile_t;

TEST sort_insert_string_test_4() {
    uint8_t ibuff[SORT_BUFFER_LEN];
    uint8_t vbuff[SORT_BUFFER_LEN];
    sort_index_t index = { .index = ibuff, .values = vbuff };

    sort_initialize(&index);

    char work_buffer[BATCH_SIZE][SORT_STRING_BUFFER_SIZE];

    for (int i = 0; i < BATCH_SIZE; ++i) {
        work_buffer[i][0] = 0;
    }

    struct data_type input[] = {
        {  0, "eeeeeeee" },
        {  1, "ffffffff" },
        {  2, "dddddddd" },
        {  3, "cccccccc" },
        {  4, "hhhhhhhh" },
        {  5, "aaaaaaaa" },
        {  6, "bbbbbbbb" },
        {  7, "gggggggg" },
    };

    for (int i = 0; i < sizeof(input) / sizeof(*input); ++i) {
        sort_insert_string(work_buffer,
                           BATCH_SIZE,
                           &index,
                           0,
                           8,
                           input[i].index,
                           input[i].string);
    }

    struct data_type result[] = {
        {  5, "aaaaaaaa" },
        {  6, "bbbbbbbb" },
        {  3, "cccccccc" },
        {  2, "dddddddd" },
        {  0, "eeeeeeee" },
    };

#if 0
    for (int i = 0; i < sizeof(result) / sizeof(*result); ++i) {
        printf("%d: %d -> %d: %s\n", i,
                                     index.index[index.values[i]],
                                     index.values[i],
                                     work_buffer[i]);
    }

    for (int i = sizeof(result) / sizeof(*result); i < 12; ++i) {
        printf("%d: %d -> %d: (%d)\n", i,
                                     index.index[index.values[i]],
                                     index.values[i],
                                     strlen(work_buffer[i]));
    }
#endif

    for (int i = 0; i < sizeof(result) / sizeof(*result); ++i) {
        char item[4];
        itoa(i, item, 10);

        ASSERT_STR_EQm(work_buffer[i], result[i].string, work_buffer[i]);
        ASSERT_EQm(item, result[i].index, index.values[i]);
        ASSERT_EQm(item, i, index.index[result[i].index]);
        ASSERT_EQm(item, true, sort_validate_slot(&index, i));
    }

    for (int i = sizeof(result) / sizeof(*result); i < BATCH_SIZE; ++i) {
        ASSERT_STR_EQ("", work_buffer[i]);
    }

    for (int i = sizeof(result) / sizeof(*result); i < 256; ++i) {
        char item[4];
        itoa(i, item, 10);
        ASSERT_EQm(item, false, sort_validate_slot(&index, i));
    }

    PASS();
}

TEST sort_insert_string_test_5() {
    uint8_t ibuff[SORT_BUFFER_LEN];
    uint8_t vbuff[SORT_BUFFER_LEN];
    sort_index_t index = { .index = ibuff, .values = vbuff };

    sort_initialize(&index);

    char work_buffer[BATCH_SIZE][SORT_STRING_BUFFER_SIZE];

    for (int i = 0; i < BATCH_SIZE; ++i) {
        work_buffer[i][0] = 0;
    }

    struct data_type input[] = {
        {  215, "good-bye" },
        {   11, "oy" },
        {    7, "hello" },
    };

    for (int i = 0; i < sizeof(input) / sizeof(*input); ++i) {
        sort_insert_string(work_buffer,
                           BATCH_SIZE,
                           &index,
                           0,
                           5,
                           input[i].index,
                           input[i].string);
    }

    struct data_type result[] = {
        {  215, "good-bye" },
        {    7, "hello" },
        {   11, "oy" },
    };

#if 0
    for (int i = 0; i < sizeof(result) / sizeof(*result); ++i) {
        printf("%d: %d -> %d: %s\n", i,
                                     index.index[index.values[i]],
                                     index.values[i],
                                     work_buffer[i]);
    }

    for (int i = sizeof(result) / sizeof(*result); i < 12; ++i) {
        printf("%d: %d -> %d: (%d)\n", i,
                                     index.index[index.values[i]],
                                     index.values[i],
                                     strlen(work_buffer[i]));
    }
#endif

    for (int i = 0; i < sizeof(result) / sizeof(*result); ++i) {
        char item[4];
        itoa(i, item, 10);

        ASSERT_STR_EQm(work_buffer[i], result[i].string, work_buffer[i]);
        ASSERT_EQm(item, result[i].index, index.values[i]);
        ASSERT_EQm(item, i, index.index[result[i].index]);
        ASSERT_EQm(item, true, sort_validate_slot(&index, i));
    }

    for (int i = sizeof(result) / sizeof(*result); i < BATCH_SIZE; ++i) {
        ASSERT_STR_EQ("", work_buffer[i]);
    }

    for (int i = sizeof(result) / sizeof(*result); i < 256; ++i) {
        char item[4];
        itoa(i, item, 10);
        ASSERT_EQm(item, false, sort_validate_slot(&index, i));
    }

    PASS();
}

TEST sort_insert_string_test_6() {
    uint8_t ibuff[SORT_BUFFER_LEN];
    uint8_t vbuff[SORT_BUFFER_LEN];
    sort_index_t index = { .index = ibuff, .values = vbuff };

    sort_initialize(&index);

    char work_buffer[BATCH_SIZE][SORT_STRING_BUFFER_SIZE];

    for (int i = 0; i < BATCH_SIZE; ++i) {
        work_buffer[i][0] = 0;
    }

    struct data_type input[] = {
        {  0, "eeeeeeee" },
        {  1, "ffffffff" },
        {  2, "dddddddd" },
        {  3, "cccccccc" },
        {  4, "hhhhhhhh" },
        {  5, "aaaaaaaa" },
        {  6, "bbbbbbbb" },
        {  7, "gggggggg" },
    };

    for (int i = 0; i < sizeof(input) / sizeof(*input); ++i) {
        sort_insert_string(work_buffer,
                           BATCH_SIZE,
                           &index,
                           2,
                           15,
                           input[i].index,
                           input[i].string);
    }

    struct data_type result[] = {
        {  5, "aaaaaaaa" },
        {  6, "bbbbbbbb" },
        {  3, "cccccccc" },
        {  2, "dddddddd" },
        {  0, "eeeeeeee" },
    };

#if 0
    for (int i = 0; i < sizeof(result) / sizeof(*result); ++i) {
        printf("%d: %d -> %d: %s\n", i,
                                     index.index[index.values[i]],
                                     index.values[i],
                                     work_buffer[i]);
    }

    for (int i = sizeof(result) / sizeof(*result); i < 12; ++i) {
        printf("%d: %d -> %d: (%d)\n", i,
                                     index.index[index.values[i]],
                                     index.values[i],
                                     strlen(work_buffer[i]));
    }
#endif

    for (int i = 0; i < sizeof(result) / sizeof(*result); ++i) {
        char item[4];
        itoa(i, item, 10);

        ASSERT_STR_EQm(work_buffer[i], result[i].string, work_buffer[i]);
        ASSERT_EQm(item, result[i].index, index.values[2 * BATCH_SIZE + i]);
        ASSERT_EQm(item, 2 * BATCH_SIZE + i, index.index[result[i].index]);
        ASSERT_EQm(item, true, sort_validate_slot(&index, 2 * BATCH_SIZE + i));
    }

    for (int i = sizeof(result) / sizeof(*result); i < BATCH_SIZE; ++i) {
        ASSERT_STR_EQ("", work_buffer[i]);
    }

    for (int i = sizeof(result) / sizeof(*result);
         2 * BATCH_SIZE + i < 256;
         ++i)
    {
        char item[4];
        itoa(i, item, 10);
        ASSERT_EQm(item,
                   false, sort_validate_slot(&index, 2 * BATCH_SIZE + i));
    }

    PASS();
}

bool get_string_from_data_type(sort_accessor_t *self,
                               char *buffer, int len, uint8_t index)
{
    data_type_profile_t *input_profile = (data_type_profile_t *) self->data;

    if (input_profile->num_entries <= index) {
        return false;
    }

    strncpy(buffer, input_profile->entries[index].string, len);

    return true;
}

TEST sort_build_index_test_0() {
    uint8_t ibuff[SORT_BUFFER_LEN];
    uint8_t vbuff[SORT_BUFFER_LEN];
    sort_index_t index = { .index = ibuff, .values = vbuff };

    sort_initialize(&index);

    struct data_type input[] = {
        {  0, "eeeeeeee" },
        {  1, "ffffffff" },
        {  2, "dddddddd" },
        {  3, "iiiiiiii" },
        {  4, "gggggggg" },
        {  5, "aaaaaaaa" },
        {  6, "bbbbbbbb" },
        {  7, "hhhhhhhh" },
        {  8, "cccccccc" },
    };

    int n = sizeof(input) / sizeof(*input);

    data_type_profile_t profile = {
                            .num_entries = n,
                            .entries = input
                        };

    sort_accessor_t accessor = { .data = (void *) &profile,
                                 .get_string = get_string_from_data_type };

    sort_build_index(&index, n, &accessor);

    struct data_type result[] = {
        {  5, "aaaaaaaa" },
        {  6, "bbbbbbbb" },
        {  8, "cccccccc" },
        {  2, "dddddddd" },
        {  0, "eeeeeeee" },
        {  1, "ffffffff" },
        {  4, "gggggggg" },
        {  7, "hhhhhhhh" },
        {  3, "iiiiiiii" },
    };

#if 1
    for (int i = 0; i < n; ++i) {
        printf("%d: %d -> %d: %s\n", i,
                                     index.index[index.values[i]],
                                     index.values[i],
                                     "" /* work_buffer[i] */);
    }

    for (int i = n; i < 12; ++i) {
        printf("%d: %d -> %d\n", i,
                                 index.index[index.values[i]],
                                 index.values[i]);
    }
#endif

    for (int i = 0; i < n; ++i) {
        char item[4];
        itoa(i, item, 10);

        ASSERT_EQm(item, result[i].index, index.values[i]);
        ASSERT_EQm(item, i, index.index[result[i].index]);
        ASSERT_EQm(item, true, sort_validate_slot(&index, i));
    }

    for (int i = n; i < 256; ++i)
    {
        char item[4];
        itoa(i, item, 10);
        ASSERT_EQm(item, false, sort_validate_slot(&index, i));
    }

    PASS();
}

TEST sort_build_index_test_1() {
    uint8_t ibuff[SORT_BUFFER_LEN];
    uint8_t vbuff[SORT_BUFFER_LEN];
    sort_index_t index = { .index = ibuff, .values = vbuff };

    sort_initialize(&index);

    struct data_type input[] = {
        {  0, "eeeeeeee" },
        {  1, "ffffffff" },
        {  2, "dddddddd" },
        {  3, "iiiiiiii" },
        {  4, "gggggggg" },
        {  5, "aaaaaaaa" },
        {  6, "bbbbbbbb" },
        {  7, "cccccccc" },
        {  8, "hhhhhhhh" },
    };

    int n = sizeof(input) / sizeof(*input);

    data_type_profile_t profile = {
                            .num_entries = n,
                            .entries = input
                        };

    sort_accessor_t accessor = { .data = (void *) &profile,
                                 .get_string = get_string_from_data_type };

    sort_build_index(&index, n, &accessor);

    struct data_type result[] = {
        {  5, "aaaaaaaa" },
        {  6, "bbbbbbbb" },
        {  7, "cccccccc" },
        {  2, "dddddddd" },
        {  0, "eeeeeeee" },
        {  1, "ffffffff" },
        {  4, "gggggggg" },
        {  8, "hhhhhhhh" },
        {  3, "iiiiiiii" },
    };

#if 1
    for (int i = 0; i < n; ++i) {
        printf("%d: %d -> %d: %s\n", i,
                                     index.index[index.values[i]],
                                     index.values[i],
                                     "" /* work_buffer[i] */);
    }

    for (int i = n; i < 12; ++i) {
        printf("%d: %d -> %d\n", i,
                                 index.index[index.values[i]],
                                 index.values[i]);
    }
#endif

    for (int i = 0; i < n; ++i) {
        char item[4];
        itoa(i, item, 10);

        ASSERT_EQm(item, result[i].index, index.values[i]);
        ASSERT_EQm(item, i, index.index[result[i].index]);
        ASSERT_EQm(item, true, sort_validate_slot(&index, i));
    }

    for (int i = n; i < 256; ++i)
    {
        char item[4];
        itoa(i, item, 10);
        ASSERT_EQm(item, false, sort_validate_slot(&index, i));
    }

    PASS();
}

TEST sort_build_index_test_2() {
    uint8_t ibuff[SORT_BUFFER_LEN];
    uint8_t vbuff[SORT_BUFFER_LEN];
    sort_index_t index = { .index = ibuff, .values = vbuff };

    sort_initialize(&index);

    struct data_type input[] = {
        {  0, "eeeeeeee" },
        {  1, "ffffffff" },
        {  2, "dddddddd" },
        {  3, "iiiiiiii" },
        {  4, "gggggggg" },
        {  5, "aaaaaaaa" },
        {  6, "bbbbbbbb" },
    };

    int n = sizeof(input) / sizeof(*input);

    data_type_profile_t profile = {
                            .num_entries = n,
                            .entries = input
                        };

    sort_accessor_t accessor = { .data = (void *) &profile,
                                 .get_string = get_string_from_data_type };

    sort_build_index(&index, n, &accessor);

    struct data_type result[] = {
        {  5, "aaaaaaaa" },
        {  6, "bbbbbbbb" },
        {  2, "dddddddd" },
        {  0, "eeeeeeee" },
        {  1, "ffffffff" },
        {  4, "gggggggg" },
        {  3, "iiiiiiii" },
    };

#if 1
    for (int i = 0; i < n; ++i) {
        printf("%d: %d -> %d: %s\n", i,
                                     index.index[index.values[i]],
                                     index.values[i],
                                     "" /* work_buffer[i] */);
    }

    for (int i = n; i < 12; ++i) {
        printf("%d: %d -> %d\n", i,
                                 index.index[index.values[i]],
                                 index.values[i]);
    }
#endif

    for (int i = 0; i < n; ++i) {
        char item[4];
        itoa(i, item, 10);

        ASSERT_EQm(item, result[i].index, index.values[i]);
        ASSERT_EQm(item, i, index.index[result[i].index]);
        ASSERT_EQm(item, true, sort_validate_slot(&index, i));
    }

    for (int i = n; i < 256; ++i)
    {
        char item[4];
        itoa(i, item, 10);
        ASSERT_EQm(item, false, sort_validate_slot(&index, i));
    }

    PASS();
}

TEST sort_build_index_test_3() {
    uint8_t ibuff[SORT_BUFFER_LEN];
    uint8_t vbuff[SORT_BUFFER_LEN];
    sort_index_t index = { .index = ibuff, .values = vbuff };

    sort_initialize(&index);

    struct data_type input[] = {
        {  0, "eeeeeeee" },
        {  1, "ffffffff" },
        {  2, "dddddddd" },
        {  3, "iiiiiiii" },
        {  4, "gggggggg" },
        {  5, "aaaaaaaa" },
        {  6, "bbbbbbbb" },
        {  7, "cccccccc" },
    };

    int n = sizeof(input) / sizeof(*input);

    data_type_profile_t profile = {
                            .num_entries = n,
                            .entries = input
                        };

    sort_accessor_t accessor = { .data = (void *) &profile,
                                 .get_string = get_string_from_data_type };

    sort_build_index(&index, n, &accessor);

    struct data_type result[] = {
        {  5, "aaaaaaaa" },
        {  6, "bbbbbbbb" },
        {  7, "cccccccc" },
        {  2, "dddddddd" },
        {  0, "eeeeeeee" },
        {  1, "ffffffff" },
        {  4, "gggggggg" },
        {  3, "iiiiiiii" },
    };

#if 1
    for (int i = 0; i < n; ++i) {
        printf("%d: %d -> %d: %s\n", i,
                                     index.index[index.values[i]],
                                     index.values[i],
                                     "" /* work_buffer[i] */);
    }

    for (int i = n; i < 12; ++i) {
        printf("%d: %d -> %d\n", i,
                                 index.index[index.values[i]],
                                 index.values[i]);
    }
#endif

    for (int i = 0; i < n; ++i) {
        char item[4];
        itoa(i, item, 10);

        ASSERT_EQm(item, result[i].index, index.values[i]);
        ASSERT_EQm(item, i, index.index[result[i].index]);
        ASSERT_EQm(item, true, sort_validate_slot(&index, i));
    }

    for (int i = n; i < 256; ++i)
    {
        char item[4];
        itoa(i, item, 10);
        ASSERT_EQm(item, false, sort_validate_slot(&index, i));
    }

    PASS();
}

bool make_string_from_index(sort_accessor_t *self,
                            char *buffer, int len, uint8_t index)
{
    uint8_t value = 255 - index;

    char temp[4];
    itoa(value, temp, 10);
    strncpy(buffer, temp, len);

    return true;
}

TEST sort_build_index_test_4() {
    uint8_t ibuff[SORT_BUFFER_LEN];
    uint8_t vbuff[SORT_BUFFER_LEN];
    sort_index_t index = { .index = ibuff, .values = vbuff };

    sort_initialize(&index);

    int n = 10;

    sort_accessor_t accessor = { .data = 0,
                                 .get_string = make_string_from_index };

    sort_build_index(&index, n, &accessor);

#if 1
    for (int i = 0; i < n; ++i) {
        char temp[4];
        make_string_from_index(0, temp, 4, index.values[i]);
        printf("%d: %d -> %d: %s\n", i,
                                     index.index[index.values[i]],
                                     index.values[i],
                                     temp);
    }
#endif

#if 1
    for (int i = 0; i < n; ++i) {
        char item[4];
        itoa(i, item, 10);

        ASSERT_EQm(item, n - 1 - i, index.values[i]);
        ASSERT_EQm(item, i, index.index[n - 1 - i]);
        ASSERT_EQm(item, true, sort_validate_slot(&index, i));
    }
#endif

    PASS();
}

SUITE(sort_suite) {
    RUN_TEST(sort_initialize_test);
    RUN_TEST(sort_set_slot_test);
    RUN_TEST(sort_insert_string_test_0);
    RUN_TEST(sort_insert_string_test_1);
    RUN_TEST(sort_insert_string_test_2);
    RUN_TEST(sort_insert_string_test_3);
    RUN_TEST(sort_insert_string_test_4);
    RUN_TEST(sort_insert_string_test_5);
    RUN_TEST(sort_insert_string_test_6);
    RUN_TEST(sort_build_index_test_0);
    RUN_TEST(sort_build_index_test_1);
    RUN_TEST(sort_build_index_test_2);
    RUN_TEST(sort_build_index_test_3);
    RUN_TEST(sort_build_index_test_4);
}
