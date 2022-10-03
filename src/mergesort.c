#include "mergesort.h"

#include <stdlib.h>
#include <string.h>
#include <strings.h>

static char *work_buffer = 0;
static int work_len = 0;

static int mergesort_compare(const void *a_ptr, const void *b_ptr)
{
    uint8_t a = *(const uint8_t *) a_ptr;
    uint8_t b = *(const uint8_t *) b_ptr;

    return strncasecmp(work_buffer + (a * work_len),
                       work_buffer + (b * work_len),
                       work_len);
}

static void reduce_partitions(uint8_t *output_index, uint8_t *temp_index,
                       int num_items, uint8_t item_maxlen,
                       int num_buckets, int partition_size,
                       uint8_t *buckets, uint8_t *string_locations,
                       char *sort_buffer,
                       mergesort_accessor_t *accessor)
{
    // calculate number of buckets (already calculated)

    // fill work buffer with first item in each bucket
    for (int i = 0; i < num_buckets; ++i) {
        char *item = sort_buffer + i * item_maxlen;
        accessor->get_value(accessor, item, item_maxlen,
                            output_index[i * partition_size]);
        string_locations[i] = i;
    }

    // sort work buffer
    qsort(string_locations, num_buckets, sizeof(*string_locations),
          mergesort_compare);
    for (int i = 0; i < num_buckets; ++i) {
        buckets[i] = string_locations[i] * partition_size;
    }

    // free up output_index, now that we don't need the temp space any more
    memcpy(temp_index, output_index, num_items);

    // while work buffer has items:
    for (int k = 0; k < num_items && 0 < num_buckets; ++k) {
        //   write first item to output
        output_index[k] = temp_index[buckets[0]];
        //   read following item from that bucket
        int next = buckets[0] + 1;
        if (next < num_items && next % partition_size) {
            buckets[0] = next;
            char *item = sort_buffer + string_locations[0] * item_maxlen;
            accessor->get_value(accessor, item, item_maxlen, temp_index[next]);

            //   insert new item into work buffer (insert sort or insert and
            //   sort)
            int num_shift = 0;
            for (int i = 1; i < num_buckets; ++i) {
                int a_offset = string_locations[i] * item_maxlen;
                int b_offset = string_locations[0] * item_maxlen;
                if (0 > strncasecmp(sort_buffer + a_offset,
                                    sort_buffer + b_offset,
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
            memmove(string_locations, string_locations + 1, num_buckets - 1);
            memmove(buckets, buckets + 1, num_buckets - 1);
            num_buckets--;
        }
    }
}

void mergesort(uint8_t *output_index, uint8_t *temp_index,
               char *sort_buffer, int buffer_size,
               int num_items, uint8_t item_maxlen,
               mergesort_accessor_t *accessor)
{
    int max_buckets = buffer_size / (item_maxlen + sizeof(*output_index) + 1);

    // Pass 1: fill buckets

    int j = 0;
    while (j * max_buckets < num_items) {
        int batch_size = max_buckets;

        if (num_items < (j + 1) * max_buckets) {
            batch_size = num_items - j * max_buckets;
        }

        work_buffer = sort_buffer;
        work_len = item_maxlen;

        for (int i = 0; i < batch_size; ++i) {
            char *item = work_buffer + i * item_maxlen;
            accessor->get_value(accessor, item, item_maxlen,
                                j * max_buckets + i);
            temp_index[i] = i;
        }

        qsort(temp_index, batch_size, sizeof(*temp_index), mergesort_compare);

        for (int i = 0; i < batch_size; ++i) {
            output_index[j * max_buckets + i] =
                temp_index[i] + j * max_buckets;
        }

        ++j;
    }

    // Pass 2: merge buckets
    uint8_t *buckets = ((uint8_t *) sort_buffer) + max_buckets * item_maxlen;
    uint8_t *string_locations = buckets + max_buckets;

    int num_partitions = j;
    int partition_size = max_buckets;

    while (1 < num_partitions) {
        int batch_size = num_partitions < max_buckets
                              ? num_partitions
                              : max_buckets;

        int n = num_items;
        for (int i = 0; i * batch_size < num_partitions; ++i) {
            int items_this_batch =
                              (n < batch_size * partition_size
                                    ? n
                                    : batch_size * partition_size);
            int buckets_this_batch = items_this_batch / partition_size
                                       + !!(items_this_batch % partition_size);
            reduce_partitions(output_index + i * batch_size * partition_size,
                              temp_index,
                              items_this_batch, item_maxlen,
                              buckets_this_batch, partition_size,
                              buckets, string_locations, sort_buffer,
                              accessor);
            n -= items_this_batch;
        }

        partition_size *= batch_size;
        num_partitions = num_items / partition_size
                             + !!(num_items % partition_size);
    }
}

