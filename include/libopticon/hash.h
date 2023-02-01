#ifndef _HASH_H
#define _HASH_H 1

#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <assert.h>
#include <fcntl.h>
#include <inttypes.h>
#include <libopticon/uuid.h>

uint32_t hash_token (const char *);
uint32_t hash_uuid (uuid);

#endif
