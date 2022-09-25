#ifndef _SORT_H_
#define _SORT_H_

#include <stdbool.h>
#include <ctype.h>
#include <stdint.h>

#define SORT_BUFFER_LEN 256

#define SORT_MAX_STRING_LEN 40
#define SORT_STRING_BUFFER_SIZE (SORT_MAX_STRING_LEN + 1)

typedef struct {
    uint8_t *index;
    uint8_t *values;
} sort_index_t;

typedef struct sort_accessor_struct {
    void *data;
    bool (*get_string)(struct sort_accessor_struct *self,
                       char *buffer, int len, uint8_t index);
} sort_accessor_t;

void sort_clear_slot(sort_index_t *index, uint8_t slot);

void sort_set_slot(sort_index_t *index, uint8_t value, uint8_t slot);

bool sort_validate_slot(sort_index_t *index, uint8_t slot);

bool sort_validate_value(sort_index_t *index, uint8_t value);

void sort_initialize(sort_index_t *index);

void sort_insert_string(char         (*buffer)[SORT_STRING_BUFFER_SIZE],
                        int           buffer_len,
                        sort_index_t *index,
                        int           page,
                        int           num_entries,
                        uint8_t       string_index,
                        char         *string);

void sort_build_index(sort_index_t    *index,
                      int              num_entries,
                      sort_accessor_t *accessor);

#endif
