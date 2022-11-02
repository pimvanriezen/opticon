#ifndef _UUID_H
#define _UUID_H 1

#include <inttypes.h>
#include <stdbool.h>

/** UUIDs are normally passed by value */
typedef struct { uint64_t msb; uint64_t lsb; } uuid;

bool         uuidcmp (uuid first, uuid second);
uuid         mkuuid (const char *str);
bool         isuuid (const char *str);
uuid         uuidgen (void);
uuid         uuidnil (void);
bool         uuidvalid (uuid);
void         uuid2str (uuid u, char *into);
uuid         bytes2uuid (uint8_t *bytes);

#endif
