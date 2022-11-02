#include <stddef.h> // For size_t
#include <stdint.h>
#include <stdbool.h>

#include <stdlib.h>
#include <windows.h>

void *memset(void *destination, int byte, size_t count) {
	//char *mem = (char *)destination;
	//while (count != 0) {
	//	*mem = byte;
	//	++mem;
	//	--count;
	//}
	//return destination;
	
	__stosb((unsigned char *)destination, (unsigned char)byte, count);
	return destination;
}