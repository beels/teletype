#include "mergesort.h"

#include <stdlib.h>
#include <string.h>

static char *work_buffer = 0;
static int work_len = 0;

int mergesort_compare(const void *a_ptr, const void *b_ptr)
{
    uint8_t a = *(const uint8_t *) a_ptr;
    uint8_t b = *(const uint8_t *) b_ptr;

    return strncmp(work_buffer + (a * work_len), work_buffer + (b * work_len),
                   work_len);
}

void mergesort(uint8_t *output_index, uint8_t *temp_index,
               char *sort_buffer, uint8_t buffer_size,
               int num_items, uint8_t item_maxlen,
               mergesort_accessor_t *accessor)
{
    int buffer_capacity = buffer_size / item_maxlen;

    // Pass 1: fill buckets

    int j = 0;
    while (j * buffer_capacity < num_items) {
        int batch_size = buffer_capacity;

        if (num_items < (j + 1) * buffer_capacity) {
            batch_size = num_items - j * buffer_capacity;
        }

        work_buffer = sort_buffer;
        work_len = item_maxlen;

        for (int i = 0; i < batch_size; ++i) {
            char *item = work_buffer + i * item_maxlen;
            accessor->get_value(accessor, item, item_maxlen,
                                j * buffer_capacity + i);
            temp_index[i] = i;
        }

        qsort(temp_index, batch_size, sizeof(*temp_index), mergesort_compare);

        for (int i = 0; i < batch_size; ++i) {
            output_index[j * buffer_capacity + i] =
                temp_index[i] + j * buffer_capacity;
        }

        ++j;
    }

    // Pass 2: merge buckets
    uint8_t buckets[20];
    uint8_t string_locations[20];

    // calculate number of buckets (can use final value of j)

    // fill work buffer with first item in each bucket
    for (int i = 0; i < j; ++i) {
        char *item = sort_buffer + i * item_maxlen;
        accessor->get_value(accessor, item, item_maxlen,
                            output_index[i * buffer_capacity]);
        string_locations[i] = i;
    }

    // sort work buffer
    qsort(string_locations, j, sizeof(*string_locations), mergesort_compare);
    for (int i = 0; i < j; ++i) {
        buckets[i] = string_locations[i] * buffer_capacity;
    }

    // free up output_index, now that we don't need the temp space any more
    memcpy(temp_index, output_index, num_items);

    // while work buffer has items:
    for (int k = 0; k < num_items && 0 < j; ++k) {
        //   write first item to output
        output_index[k] = temp_index[buckets[0]];
        //   read following item from that bucket
        int next = buckets[0] + 1;
        if (next < num_items && next % buffer_capacity) {
            buckets[0] = next;
            char *item = sort_buffer + string_locations[0] * item_maxlen;
            accessor->get_value(accessor, item, item_maxlen, temp_index[next]);

            //   insert new item into work buffer (insert sort or insert and
            //   sort)
            int num_shift = 0;
            for (int i = 1; i < j; ++i) {
                if (0 > strncmp(sort_buffer + string_locations[i] * item_maxlen,
                                sort_buffer + string_locations[0] * item_maxlen,
                                item_maxlen))
                {
                    num_shift++;
                } else {
                    break;
                }
            }
            for (int i = 0; i < num_shift; ++i) {
                uint8_t temp = string_locations[i];
                string_locations[i] = string_locations[i + 1];
                string_locations[i + 1] = temp;
                temp = buckets[i];
                buckets[i] = buckets[i + 1];
                buckets[i + 1] = temp;
            }
        }
        else {
            //     if no item left in bucket, shrink buffer
            memmove(string_locations, string_locations + 1, j - 1);
            memmove(buckets, buckets + 1, j - 1);
            j--;
        }
    }

}

