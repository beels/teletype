#include "sort.h"

#include <string.h>
#include <stdlib.h>

void sort_clear_slot(sort_index_t *index, uint8_t slot)
{
    index->values[slot] = slot;
    index->index[slot] = slot + 1;
}

void sort_set_slot(sort_index_t *index, uint8_t value, uint8_t slot)
{
    index->values[slot] = value;
    index->index[value] = slot;
}

bool sort_validate_slot(sort_index_t *index, uint8_t slot)
{
    return index->index[index->values[slot]] == slot;
}

void sort_initialize(sort_index_t *index)
{
    for (int i = 0; i < SORT_BUFFER_LEN; ++i) {
        sort_clear_slot(index, i);
    }
}

#define BATCH_SIZE 7

void sort_insert_string(char         (*buffer)[SORT_STRING_BUFFER_SIZE],
                        int           buffer_len,
                        sort_index_t *index,
                        int           page,
                        int           num_entries,
                        uint8_t       string_index,
                        char         *string)
{
    uint8_t first_index_slot = page * buffer_len;
    uint8_t target_index_slot = first_index_slot;

    // Find a slot that `string` is less than.

    while (target_index_slot < first_index_slot + buffer_len
           && sort_validate_slot(index, target_index_slot)
           && 0 < strncmp(string,
                          buffer[target_index_slot - first_index_slot],
                          SORT_STRING_BUFFER_SIZE))
    {
        ++target_index_slot;
    }

    if (target_index_slot >= first_index_slot + buffer_len) {
        // This item is sorted beyond our range

        return;
    }

    int target_buffer_slot = target_index_slot - first_index_slot;

    if (sort_validate_slot(index, target_index_slot)) {
        // The target slot is occupied.  Shift everything down.

        // First figure out how many slots need shifting.  It's from the
        // current position to the end of the page.

        for (int i = buffer_len - 1;
             target_index_slot - first_index_slot < i;
             --i)
        {
            if (sort_validate_slot(index, first_index_slot + i - 1)) {
                sort_set_slot(index,
                              index->values[first_index_slot + i - 1],
                              first_index_slot + i);
            }
            else {
                sort_clear_slot(index, first_index_slot + i);
            }

            // ARB:
            // These calls to strncpy might be able to be pulled out of the
            // loop and replaced with a single call to memcpy or memmove.

            strncpy(buffer[i], buffer[i - 1], SORT_STRING_BUFFER_SIZE);
        }
    }

    // The target slot is empty.  Fill it.

    strncpy(buffer[target_buffer_slot], string, SORT_STRING_BUFFER_SIZE);
    buffer[target_buffer_slot][SORT_MAX_STRING_LEN] = 0;

    sort_set_slot(index, string_index, target_index_slot);
}

void sort_build_index(sort_index_t    *index,
                      int              num_entries,
                      sort_accessor_t *accessor)
{
    char work_buffer[BATCH_SIZE][SORT_STRING_BUFFER_SIZE];

    sort_initialize(index);

    for (int page = 0; page < num_entries; page += BATCH_SIZE) {
        for (int i = 0; i < num_entries; ++i) {
            char temp[SORT_STRING_BUFFER_SIZE];
            accessor->get_string(temp, SORT_STRING_BUFFER_SIZE, i);
            sort_insert_string(work_buffer,
                               BATCH_SIZE,
                               index,
                               page,
                               num_entries,
                               i,
                               temp);
        }
    }
}
