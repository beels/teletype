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

void filename_ellipsis(char *output, char *filename, int maxchars) {
    // Re-write 'filename' into 'output', trimming the file extension off the
    // end and rendering the remainder as 'a...xxx' where 'xxx' are the last
    // three characters of the non-extension part and 'a' is as much of the
    // initial substring as will fit if the total length of the result is
    // 'maxchars'.  If the entire non-extension part will fit in 'maxchars', no
    // ellipsis is added.
    //
    // If maxchars is less than 6, we segfault.  Actual use cases will always
    // be at least 20.

    int length = strlen(filename);

    int i = length;
    while ('.' != filename[i]  && 0 < i) {
        --i;
    }

    if ('.' == filename[i]) {
        length = i;
    }

    if (length <= maxchars) {
        strncpy(output, filename, length);
        output[length] = 0;
    }
    else {
        int tail = length - 3;
        int ellipsis = maxchars - 5;
        for (int i = 0; i < ellipsis; ++i) {
            output[i] = filename[i];
        }
        for (int i = 0; i < 2; ++i) {
            output[ellipsis + i] = '.';
        }
        for (int i = 0; i < 3; ++i) {
            output[maxchars - 3 + i] = filename[tail + i];
        }
        output[maxchars] = 0;
    }
}

