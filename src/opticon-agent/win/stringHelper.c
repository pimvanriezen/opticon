#include <stddef.h> // For size_t
#include <stdint.h>
#include <stdbool.h>

#include <windows.h>
#include <ctype.h> // For tolower

#include "stringHelper.h"

void stringToLowerCase(char *string) {
	while (*string) {
		*string = tolower(*string);
		++string;
	}
}