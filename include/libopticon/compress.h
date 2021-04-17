#ifndef _COMPRESS_H
#define _COMPRESS_H 1

#include <libopticon/datatypes.h>
#include <libopticon/ioport_buffer.h>
#include <zlib.h>
#include <stdbool.h>

/* ============================= FUNCTIONS ============================= */

bool compress_data (ioport *in, ioport *out);
bool decompress_data (ioport *in, ioport *out);

#endif
