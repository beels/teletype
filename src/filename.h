#ifndef _FILENAME_H_
#define _FILENAME_H_
#include <stdbool.h>

bool filename_find_wildcard_range(int *wildcard_start, char *filename);
void filename_increment_version(char *filename, int wildcard_start);
void filename_ellipsis(char *filename, int maxchars);

#define FNAME_BUFFER_LEN 37

#endif
