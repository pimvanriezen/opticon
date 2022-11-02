#pragma once
#pragma GCC system_header

// @note We're extending this system header
#include_next <string.h>

void bzero(void *string, size_t count);

char *strsep(char **stringp, const char *delim);