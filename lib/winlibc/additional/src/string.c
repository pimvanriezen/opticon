#include <stddef.h> // For size_t
#include <stdint.h>
#include <stdbool.h>

#include <string.h>
#include <stdlib.h> // For memset

void bzero(void *string, size_t count) {
	memset(string, '\0', count);
}

char *strsep(char **stringPointer, const char *delimiter) {
	char *movingString = *stringPointer;
	if (movingString == NULL) return NULL;
	
	char *originalString = movingString;
	
	while (true) {
		int stringChar = *movingString;
		++movingString;
		
		const char *movingDelimiter = delimiter;
		int delimiterChar;
		do {
			delimiterChar = *movingDelimiter;
			++movingDelimiter;
			if (stringChar == delimiterChar) {
				if (stringChar == '\0') movingString = NULL;
				else movingString[-1] = '\0';
				*stringPointer = movingString;
				
				return originalString;
			}
		} while (delimiterChar != 0);
	}
}