#include "sort.h"

#include <string.h>
#include <stdlib.h>

void sort_clear_slot(sort_index_t *index, uint8_t slot)
{
    if (!sort_validate_slot(index, slot)) {
        // This slot is already cleared, so there is nothing to do.

        return;
    }

    // Get an arbitrary slot and an arbitrary value distinct from the current
    // (valid) slot/value pair.

    uint8_t value = index->values[slot];

    uint8_t v = value + 1;
    uint8_t i = slot + 1;

    if (index->index[v] != slot) {
        // The new value is listed as not being stored in this slot.  Using
        // that value will invalidate this slot, with no influence on whatever
        // location might legitimately hold that value.

        index->values[slot] = v;
        return;
    }

    if (index->values[i] != value) {
        // The new slot is listed as not holding this value.  Using that slot
        // will invalidate this value, with no influence on whatever value
        // might legitimately be stored in that location.

        index->index[value] = i;
        return;
    }

    // If we get here, then
    //
    // ```
    // index->index[v] == slot && index->values[i] == value
    // ```
    //
    // We have discovered an invalid value and an invalid slot.  We can make
    // the current slot invalid by cross-referencing the two values and two
    // slots.

    // index->index[v] = slot;  // Not necessary; this value is aready set.
    index->values[i] = v;
    index->index[value] = i;
    index->values[slot] = value;
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

bool sort_validate_value(sort_index_t *index, uint8_t value)
{
    return index->values[index->index[value]] == value;
}

void sort_initialize(sort_index_t *index)
{
#if 0
    // Either of these obvious approaches would also work, but at greater
    // cost.

#if 0
    for (int i = 0; i < SORT_BUFFER_LEN; ++i) {
        sort_clear_slot(index, i);
    }
#else
    for (int i = 0; i < SORT_BUFFER_LEN; ++i) {
        index->values[i] = i;
        index->index[i] = i + 1;
    }
#endif
#else
    memset(index->values, 0, SORT_BUFFER_LEN);
    memset(index->index, 0, SORT_BUFFER_LEN);
    index->index[0] = 1;
    index->values[1] = 1;
#endif
}

void sort_insert_string(char         (*buffer)[SORT_STRING_BUFFER_SIZE],
                        int           buffer_len,
                        sort_index_t *index,
                        int           page,
                        int           num_entries,
                        uint8_t       string_index,
                        char         *string)
{
    int first_index_slot = page * buffer_len;
    int offset = 0;

    // Find a slot that `string` is less than.

    while (   offset < buffer_len
           && sort_validate_slot(index, first_index_slot + offset)
           && 0 < strncmp(string, buffer[offset], SORT_STRING_BUFFER_SIZE))
    {
        ++offset;
    }

    if (buffer_len <= offset) {
        // This item is sorted beyond our range

        return;
    }

    if (sort_validate_slot(index, first_index_slot + offset)) {
        // The target slot is occupied.  Shift everything down.

        // First figure out how many slots need shifting.  It's from the
        // current position to the end of the page.

        for (int i = buffer_len - 1; offset < i; --i) {
            int destination_slot = first_index_slot + i;
            if (num_entries <= destination_slot) {
                continue;
            }

            if (sort_validate_slot(index, destination_slot - 1)) {
                sort_set_slot(index,
                              index->values[destination_slot - 1],
                              destination_slot);
            }

            // These calls to strncpy might be able to be pulled out of the
            // loop and replaced with a single call to memcpy or memmove.

            strncpy(buffer[i], buffer[i - 1], SORT_STRING_BUFFER_SIZE);
        }
    }

    // The target slot is empty.  Fill it.

    strncpy(buffer[offset], string, SORT_STRING_BUFFER_SIZE);
    buffer[offset][SORT_MAX_STRING_LEN] = 0;

    sort_set_slot(index, string_index, first_index_slot + offset);
}

#define SORT_BATCH_SIZE 32

void sort_build_index(sort_index_t    *index,
                      int              num_entries,
                      sort_accessor_t *accessor)
{
    char work_buffer[SORT_BATCH_SIZE][SORT_STRING_BUFFER_SIZE];

    sort_initialize(index);

    for (int page = 0; page * SORT_BATCH_SIZE < num_entries; ++page) {
        for (int i = 0; i < num_entries; ++i) {
            if (sort_validate_value(index, i)) {
                continue;
            }

            char temp[SORT_STRING_BUFFER_SIZE];
            if (accessor->get_string(accessor,
                                     temp,
                                     SORT_STRING_BUFFER_SIZE,
                                     i))
            {
                sort_insert_string(work_buffer,
                                   SORT_BATCH_SIZE,
                                   index,
                                   page,
                                   num_entries,
                                   i,
                                   temp);
            }
        }
    }
}
