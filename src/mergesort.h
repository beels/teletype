#ifndef _MERGESORT_H_
#define _MERGESORT_H_

#include <stdbool.h>
#include <ctype.h>
#include <stdint.h>

typedef struct mergesort_accessor_struct {
    void *data;
    bool (*get_value)(struct mergesort_accessor_struct *self,
                      char *buffer, int len, uint8_t index);
} mergesort_accessor_t;

void mergesort(uint8_t *output_index, uint8_t *temp_index,
               char *sort_buffer, uint8_t buffer_size,
               int num_items, uint8_t item_maxlen,
               mergesort_accessor_t *accessor);

#endif // _MERGESORT_H_
