#include "mergesort_tests.h"

#include "greatest/greatest.h"

#include "../src/mergesort.h"

#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct data_type {
    int   index;
    char *string;
};

bool get_value_from_array(struct mergesort_accessor_struct *self,
                          char *buffer, int len, uint8_t index)
{
    struct data_type *input = (struct data_type *)self->data;
    strncpy(buffer, input[index].string, len);

    return true;
}

static char test_item[4];

TEST mergesort_one_element_test() {
    uint8_t out[7];
    uint8_t temp[7];
    char    work[3 * (4 + 2)];

    int num_items = 1;

    struct data_type input[] = {
        {  0, "eee" },
    };

    mergesort_accessor_t accessor = { .data = input,
                                      .get_value = get_value_from_array };

    mergesort(out, temp, work, sizeof(work),
              num_items, 4, &accessor);

    struct data_type result[] = {
        {  0, "eee" },
    };

    for (int i = 0; i < num_items; ++i) {
        itoa(i, test_item, 10);

        ASSERT_EQm(test_item, result[i].index, out[i]);
        ASSERT_STR_EQm(test_item, result[i].string, input[out[i]].string);
    }

    PASS();
}

TEST mergesort_two_sorted_elements_test() {
    uint8_t out[7];
    uint8_t temp[7];
    char    work[3 * (4 + 2)];

    int num_items = 2;

    struct data_type input[] = {
        {  0, "eee" },
        {  1, "fff" },
    };

    mergesort_accessor_t accessor = { .data = input,
                                      .get_value = get_value_from_array };

    mergesort(out, temp, work, sizeof(work),
              num_items, 4, &accessor);

    struct data_type result[] = {
        {  0, "eee" },
        {  1, "fff" },
    };

    for (int i = 0; i < num_items; ++i) {
        itoa(i, test_item, 10);

        ASSERT_EQm(test_item, result[i].index, out[i]);
        ASSERT_STR_EQm(test_item, result[i].string, input[out[i]].string);
    }

    PASS();
}

TEST mergesort_two_unsorted_elements_test() {
    uint8_t out[7];
    uint8_t temp[7];
    char    work[3 * (4 + 2)];

    int num_items = 2;

    struct data_type input[] = {
        {  0, "fff" },
        {  1, "eee" },
    };

    mergesort_accessor_t accessor = { .data = input,
                                      .get_value = get_value_from_array };

    mergesort(out, temp, work, sizeof(work),
              num_items, 4, &accessor);

    struct data_type result[] = {
        {  1, "eee" },
        {  0, "fff" },
    };

    for (int i = 0; i < num_items; ++i) {
        itoa(i, test_item, 10);

        ASSERT_EQm(test_item, result[i].index, out[i]);
        ASSERT_STR_EQm(test_item, result[i].string, input[out[i]].string);
    }

    PASS();
}

TEST mergesort_three_sorted_elements_test() {
    uint8_t out[7];
    uint8_t temp[7];
    char    work[3 * (4 + 2)];

    int num_items = 3;

    struct data_type input[] = {
        {  0, "eee" },
        {  1, "fff" },
        {  2, "ggg" },
    };

    mergesort_accessor_t accessor = { .data = input,
                                      .get_value = get_value_from_array };

    mergesort(out, temp, work, sizeof(work),
              num_items, 4, &accessor);

    struct data_type result[] = {
        {  0, "eee" },
        {  1, "fff" },
        {  2, "ggg" },
    };

    for (int i = 0; i < num_items; ++i) {
        itoa(i, test_item, 10);

        ASSERT_EQm(test_item, result[i].index, out[i]);
        ASSERT_STR_EQm(test_item, result[i].string, input[out[i]].string);
    }

    PASS();
}

TEST mergesort_three_unsorted_elements_test() {
    uint8_t out[7];
    uint8_t temp[7];
    char    work[3 * (4 + 2)];

    int num_items = 3;

    struct data_type input[] = {
        {  0, "fff" },
        {  1, "ggg" },
        {  2, "eee" },
    };

    mergesort_accessor_t accessor = { .data = input,
                                      .get_value = get_value_from_array };

    mergesort(out, temp, work, sizeof(work),
              num_items, 4, &accessor);

    struct data_type result[] = {
        {  2, "eee" },
        {  0, "fff" },
        {  1, "ggg" },
    };

    for (int i = 0; i < num_items; ++i) {
        itoa(i, test_item, 10);

        ASSERT_EQm(test_item, result[i].index, out[i]);
        ASSERT_STR_EQm(test_item, result[i].string, input[out[i]].string);
    }

    PASS();
}

TEST mergesort_three_reversed_elements_test() {
    uint8_t out[7];
    uint8_t temp[7];
    char    work[3 * (4 + 2)];

    int num_items = 3;

    struct data_type input[] = {
        {  0, "ggg" },
        {  1, "fff" },
        {  2, "eee" },
    };

    mergesort_accessor_t accessor = { .data = input,
                                      .get_value = get_value_from_array };

    mergesort(out, temp, work, sizeof(work),
              num_items, 4, &accessor);

    struct data_type result[] = {
        {  2, "eee" },
        {  1, "fff" },
        {  0, "ggg" },
    };

    for (int i = 0; i < num_items; ++i) {
        itoa(i, test_item, 10);

        ASSERT_EQm(test_item, result[i].index, out[i]);
        ASSERT_STR_EQm(test_item, result[i].string, input[out[i]].string);
    }

    PASS();
}

TEST mergesort_four_reversed_elements_test() {
    uint8_t out[7];
    uint8_t temp[7];
    char    work[3 * (4 + 2)];

    int num_items = 4;

    struct data_type input[] = {
        {  0, "ggg" },
        {  1, "fff" },
        {  2, "eee" },
        {  3, "ddd" },
    };

    mergesort_accessor_t accessor = { .data = input,
                                      .get_value = get_value_from_array };

    mergesort(out, temp, work, sizeof(work),
              num_items, 4, &accessor);

    struct data_type result[] = {
        {  3, "ddd" },
        {  2, "eee" },
        {  1, "fff" },
        {  0, "ggg" },
    };

    for (int i = 0; i < num_items; ++i) {
        itoa(i, test_item, 10);

        ASSERT_EQm(test_item, result[i].index, out[i]);
        ASSERT_STR_EQm(test_item, result[i].string, input[out[i]].string);
    }

    PASS();
}

TEST mergesort_five_unsorted_elements_test() {
    uint8_t out[7];
    uint8_t temp[7];
    char    work[3 * (4 + 2)];

    int num_items = 5;

    struct data_type input[] = {
        {  0, "ggg" },
        {  1, "fff" },
        {  2, "eee" },
        {  3, "ddd" },
        {  4, "hhh" },
    };

    mergesort_accessor_t accessor = { .data = input,
                                      .get_value = get_value_from_array };

    mergesort(out, temp, work, sizeof(work),
              num_items, 4, &accessor);

    struct data_type result[] = {
        {  3, "ddd" },
        {  2, "eee" },
        {  1, "fff" },
        {  0, "ggg" },
        {  4, "hhh" },
    };

    for (int i = 0; i < num_items; ++i) {
        itoa(i, test_item, 10);

        ASSERT_EQm(test_item, result[i].index, out[i]);
        ASSERT_STR_EQm(test_item, result[i].string, input[out[i]].string);
    }

    PASS();
}

TEST mergesort_five_unsorted_elements_test_2() {
    uint8_t out[7];
    uint8_t temp[7];
    char    work[3 * (4 + 2)];

    int num_items = 5;

    struct data_type input[] = {
        {  0, "fff" },
        {  1, "eee" },
        {  2, "ddd" },
        {  3, "hhh" },
        {  4, "ggg" },
    };

    mergesort_accessor_t accessor = { .data = input,
                                      .get_value = get_value_from_array };

    mergesort(out, temp, work, sizeof(work),
              num_items, 4, &accessor);

    struct data_type result[] = {
        {  2, "ddd" },
        {  1, "eee" },
        {  0, "fff" },
        {  4, "ggg" },
        {  3, "hhh" },
    };

    for (int i = 0; i < num_items; ++i) {
        itoa(i, test_item, 10);

        ASSERT_EQm(test_item, result[i].index, out[i]);
        ASSERT_STR_EQm(test_item, result[i].string, input[out[i]].string);
    }

    PASS();
}

TEST mergesort_many_unsorted_elements_test() {
    uint8_t out[256];
    uint8_t temp[256];
    char    work[16 * (6 + 2)];

    int num_items = 217;

    struct data_type input[] = {
        { 0, "7630" },
        { 1, "17954" },
        { 2, "30283" },
        { 3, "17726" },
        { 4, "1382" },
        { 5, "7486" },
        { 6, "999" },
        { 7, "28550" },
        { 8, "21477" },
        { 9, "5838" },
        { 10, "15727" },
        { 11, "17297" },
        { 12, "26953" },
        { 13, "20859" },
        { 14, "2180" },
        { 15, "5174" },
        { 16, "26897" },
        { 17, "3993" },
        { 18, "14669" },
        { 19, "4867" },
        { 20, "16682" },
        { 21, "16723" },
        { 22, "15819" },
        { 23, "4430" },
        { 24, "9684" },
        { 25, "15570" },
        { 26, "9830" },
        { 27, "9733" },
        { 28, "13557" },
        { 29, "22925" },
        { 30, "25837" },
        { 31, "14007" },
        { 32, "26601" },
        { 33, "27323" },
        { 34, "18787" },
        { 35, "8470" },
        { 36, "29048" },
        { 37, "24486" },
        { 38, "20045" },
        { 39, "28228" },
        { 40, "20499" },
        { 41, "27518" },
        { 42, "20281" },
        { 43, "31343" },
        { 44, "16113" },
        { 45, "2101" },
        { 46, "31909" },
        { 47, "4050" },
        { 48, "16810" },
        { 49, "11287" },
        { 50, "24817" },
        { 51, "9878" },
        { 52, "7167" },
        { 53, "10562" },
        { 54, "32010" },
        { 55, "21077" },
        { 56, "4844" },
        { 57, "7089" },
        { 58, "22544" },
        { 59, "12759" },
        { 60, "28879" },
        { 61, "30776" },
        { 62, "21079" },
        { 63, "1437" },
        { 64, "23417" },
        { 65, "14094" },
        { 66, "10672" },
        { 67, "11036" },
        { 68, "4539" },
        { 69, "22521" },
        { 70, "29739" },
        { 71, "51" },
        { 72, "18035" },
        { 73, "23316" },
        { 74, "9123" },
        { 75, "22907" },
        { 76, "25529" },
        { 77, "20422" },
        { 78, "10397" },
        { 79, "3312" },
        { 80, "2015" },
        { 81, "25213" },
        { 82, "10917" },
        { 83, "29928" },
        { 84, "28100" },
        { 85, "2463" },
        { 86, "29965" },
        { 87, "539" },
        { 88, "25930" },
        { 89, "6867" },
        { 90, "25804" },
        { 91, "25949" },
        { 92, "32242" },
        { 93, "23780" },
        { 94, "17878" },
        { 95, "16701" },
        { 96, "10709" },
        { 97, "10823" },
        { 98, "22020" },
        { 99, "16656" },
        { 100, "21280" },
        { 101, "14215" },
        { 102, "13218" },
        { 103, "10314" },
        { 104, "17939" },
        { 105, "20183" },
        { 106, "23303" },
        { 107, "23200" },
        { 108, "3119" },
        { 109, "32735" },
        { 110, "21774" },
        { 111, "18551" },
        { 112, "15891" },
        { 113, "5416" },
        { 114, "8596" },
        { 115, "13583" },
        { 116, "17476" },
        { 117, "6339" },
        { 118, "21053" },
        { 119, "29069" },
        { 120, "1252" },
        { 121, "22265" },
        { 122, "9937" },
        { 123, "14961" },
        { 124, "348" },
        { 125, "28732" },
        { 126, "10829" },
        { 127, "23604" },
        { 128, "2693" },
        { 129, "21483" },
        { 130, "9314" },
        { 131, "21105" },
        { 132, "8551" },
        { 133, "9317" },
        { 134, "12667" },
        { 135, "20425" },
        { 136, "25785" },
        { 137, "126" },
        { 138, "3856" },
        { 139, "5699" },
        { 140, "9813" },
        { 141, "20690" },
        { 142, "26263" },
        { 143, "30605" },
        { 144, "27360" },
        { 145, "25528" },
        { 146, "1456" },
        { 147, "5803" },
        { 148, "26157" },
        { 149, "22075" },
        { 150, "29585" },
        { 151, "25186" },
        { 152, "20796" },
        { 153, "1955" },
        { 154, "11498" },
        { 155, "27262" },
        { 156, "11577" },
        { 157, "10312" },
        { 158, "16563" },
        { 159, "30733" },
        { 160, "30466" },
        { 161, "31023" },
        { 162, "17390" },
        { 163, "6909" },
        { 164, "7811" },
        { 165, "28687" },
        { 166, "10488" },
        { 167, "737" },
        { 168, "18440" },
        { 169, "11717" },
        { 170, "15371" },
        { 171, "10872" },
        { 172, "28421" },
        { 173, "21717" },
        { 174, "18072" },
        { 175, "24850" },
        { 176, "3616" },
        { 177, "7037" },
        { 178, "25000" },
        { 179, "10861" },
        { 180, "5184" },
        { 181, "8726" },
        { 182, "3759" },
        { 183, "13501" },
        { 184, "8605" },
        { 185, "5940" },
        { 186, "30755" },
        { 187, "5603" },
        { 188, "3010" },
        { 189, "3300" },
        { 190, "10493" },
        { 191, "22067" },
        { 192, "21225" },
        { 193, "23773" },
        { 194, "2157" },
        { 195, "21369" },
        { 196, "2164" },
        { 197, "15533" },
        { 198, "18737" },
        { 199, "32737" },
        { 200, "15859" },
        { 201, "29096" },
        { 202, "30089" },
        { 203, "13782" },
        { 204, "10478" },
        { 205, "20674" },
        { 206, "18845" },
        { 207, "3865" },
        { 208, "25203" },
        { 209, "19013" },
        { 210, "17719" },
        { 211, "25159" },
        { 212, "22435" },
        { 213, "16458" },
        { 214, "32061" },
        { 215, "25692" },
        { 216, "12533" },
    };

    mergesort_accessor_t accessor = { .data = input,
                                      .get_value = get_value_from_array };

    mergesort(out, temp, work, sizeof(work),
              num_items, 6, &accessor);

    struct data_type result[] = {
        { 157, "10312" },
        { 103, "10314" },
        { 78, "10397" },
        { 204, "10478" },
        { 166, "10488" },
        { 190, "10493" },
        { 53, "10562" },
        { 66, "10672" },
        { 96, "10709" },
        { 97, "10823" },
        { 126, "10829" },
        { 179, "10861" },
        { 171, "10872" },
        { 82, "10917" },
        { 67, "11036" },
        { 49, "11287" },
        { 154, "11498" },
        { 156, "11577" },
        { 169, "11717" },
        { 120, "1252" },
        { 216, "12533" },
        { 137, "126" },
        { 134, "12667" },
        { 59, "12759" },
        { 102, "13218" },
        { 183, "13501" },
        { 28, "13557" },
        { 115, "13583" },
        { 203, "13782" },
        { 4, "1382" },
        { 31, "14007" },
        { 65, "14094" },
        { 101, "14215" },
        { 63, "1437" },
        { 146, "1456" },
        { 18, "14669" },
        { 123, "14961" },
        { 170, "15371" },
        { 197, "15533" },
        { 25, "15570" },
        { 10, "15727" },
        { 22, "15819" },
        { 200, "15859" },
        { 112, "15891" },
        { 44, "16113" },
        { 213, "16458" },
        { 158, "16563" },
        { 99, "16656" },
        { 20, "16682" },
        { 95, "16701" },
        { 21, "16723" },
        { 48, "16810" },
        { 11, "17297" },
        { 162, "17390" },
        { 116, "17476" },
        { 210, "17719" },
        { 3, "17726" },
        { 94, "17878" },
        { 104, "17939" },
        { 1, "17954" },
        { 72, "18035" },
        { 174, "18072" },
        { 168, "18440" },
        { 111, "18551" },
        { 198, "18737" },
        { 34, "18787" },
        { 206, "18845" },
        { 209, "19013" },
        { 153, "1955" },
        { 38, "20045" },
        { 80, "2015" },
        { 105, "20183" },
        { 42, "20281" },
        { 77, "20422" },
        { 135, "20425" },
        { 40, "20499" },
        { 205, "20674" },
        { 141, "20690" },
        { 152, "20796" },
        { 13, "20859" },
        { 45, "2101" },
        { 118, "21053" },
        { 55, "21077" },
        { 62, "21079" },
        { 131, "21105" },
        { 192, "21225" },
        { 100, "21280" },
        { 195, "21369" },
        { 8, "21477" },
        { 129, "21483" },
        { 194, "2157" },
        { 196, "2164" },
        { 173, "21717" },
        { 110, "21774" },
        { 14, "2180" },
        { 98, "22020" },
        { 191, "22067" },
        { 149, "22075" },
        { 121, "22265" },
        { 212, "22435" },
        { 69, "22521" },
        { 58, "22544" },
        { 75, "22907" },
        { 29, "22925" },
        { 107, "23200" },
        { 106, "23303" },
        { 73, "23316" },
        { 64, "23417" },
        { 127, "23604" },
        { 193, "23773" },
        { 93, "23780" },
        { 37, "24486" },
        { 85, "2463" },
        { 50, "24817" },
        { 175, "24850" },
        { 178, "25000" },
        { 211, "25159" },
        { 151, "25186" },
        { 208, "25203" },
        { 81, "25213" },
        { 145, "25528" },
        { 76, "25529" },
        { 215, "25692" },
        { 136, "25785" },
        { 90, "25804" },
        { 30, "25837" },
        { 88, "25930" },
        { 91, "25949" },
        { 148, "26157" },
        { 142, "26263" },
        { 32, "26601" },
        { 16, "26897" },
        { 128, "2693" },
        { 12, "26953" },
        { 155, "27262" },
        { 33, "27323" },
        { 144, "27360" },
        { 41, "27518" },
        { 84, "28100" },
        { 39, "28228" },
        { 172, "28421" },
        { 7, "28550" },
        { 165, "28687" },
        { 125, "28732" },
        { 60, "28879" },
        { 36, "29048" },
        { 119, "29069" },
        { 201, "29096" },
        { 150, "29585" },
        { 70, "29739" },
        { 83, "29928" },
        { 86, "29965" },
        { 202, "30089" },
        { 188, "3010" },
        { 2, "30283" },
        { 160, "30466" },
        { 143, "30605" },
        { 159, "30733" },
        { 186, "30755" },
        { 61, "30776" },
        { 161, "31023" },
        { 108, "3119" },
        { 43, "31343" },
        { 46, "31909" },
        { 54, "32010" },
        { 214, "32061" },
        { 92, "32242" },
        { 109, "32735" },
        { 199, "32737" },
        { 189, "3300" },
        { 79, "3312" },
        { 124, "348" },
        { 176, "3616" },
        { 182, "3759" },
        { 138, "3856" },
        { 207, "3865" },
        { 17, "3993" },
        { 47, "4050" },
        { 23, "4430" },
        { 68, "4539" },
        { 56, "4844" },
        { 19, "4867" },
        { 71, "51" },
        { 15, "5174" },
        { 180, "5184" },
        { 87, "539" },
        { 113, "5416" },
        { 187, "5603" },
        { 139, "5699" },
        { 147, "5803" },
        { 9, "5838" },
        { 185, "5940" },
        { 117, "6339" },
        { 89, "6867" },
        { 163, "6909" },
        { 177, "7037" },
        { 57, "7089" },
        { 52, "7167" },
        { 167, "737" },
        { 5, "7486" },
        { 0, "7630" },
        { 164, "7811" },
        { 35, "8470" },
        { 132, "8551" },
        { 114, "8596" },
        { 184, "8605" },
        { 181, "8726" },
        { 74, "9123" },
        { 130, "9314" },
        { 133, "9317" },
        { 24, "9684" },
        { 27, "9733" },
        { 140, "9813" },
        { 26, "9830" },
        { 51, "9878" },
        { 122, "9937" },
        { 6, "999" },
    };

    for (int i = 0; i < num_items; ++i) {
        itoa(i, test_item, 10);

        ASSERT_EQm(test_item, result[i].index, out[i]);
        ASSERT_STR_EQm(test_item, result[i].string, input[out[i]].string);
    }

    PASS();
}

SUITE(mergesort_suite) {
    RUN_TEST(mergesort_one_element_test);
    RUN_TEST(mergesort_two_sorted_elements_test);
    RUN_TEST(mergesort_two_unsorted_elements_test);
    RUN_TEST(mergesort_three_sorted_elements_test);
    RUN_TEST(mergesort_three_unsorted_elements_test);
    RUN_TEST(mergesort_three_reversed_elements_test);
    RUN_TEST(mergesort_four_reversed_elements_test);
    RUN_TEST(mergesort_five_unsorted_elements_test);
    RUN_TEST(mergesort_five_unsorted_elements_test_2);
    RUN_TEST(mergesort_many_unsorted_elements_test);
}
