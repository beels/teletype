#include "filename.h"

#include <string.h>
#include <stdlib.h>

void filename_increment_version(char *filename, int wildcard_start)
{
    int n = atoi(filename + wildcard_start) + 1;

    int i = wildcard_start;
    while (i < FNAME_BUFFER_LEN && '0' <= filename[i] && filename[i] <= '9') {
        ++i;
    }

    while (wildcard_start <= --i) {
        filename[i] = '0' + (n % 10);
        n /= 10;
    }
}

bool filename_find_wildcard_range(int *wildcard_start, char *filename)
{
    for (int i = 0; i < FNAME_BUFFER_LEN; ++i) {
        if ('*' == filename[i]) {
            *wildcard_start = i;

            return true;
        }
    }

    return false;
}

