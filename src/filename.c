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

void filename_ellipsis(char *filename, int maxchars) {
    // If maxchars is less than 6, we segfault.  Actual use cases will always
    // be at least 20.

    // split filename into name + extension
    int length = strlen(filename);
    if (length <= maxchars) {
        return;
    }

    int i = length;
    while ('.' != filename[i]  && 0 < i) {
        --i;
    }

    if ('.' != filename[i]) {
        i = length;
    }

    // trim `length - (maxchars + 3)` characters
    int j = maxchars - (length - i) - 2;
    while (j < i - (length - maxchars)) {
        filename[j] = '.';
        ++j;
    }
    while (j < maxchars) {
        filename[j] = filename[j + (length - maxchars)];
        ++j;
    }
    filename[maxchars] = 0;
}

