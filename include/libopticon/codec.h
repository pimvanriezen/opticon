#ifndef _CODEC_H
#define _CODEC_H 1

#include <libopticon/datatypes.h>
#include <libopticon/ioport.h>

/* =============================== TYPES =============================== */

typedef bool (*encode_host_func)(ioport *, host *);
typedef bool (*decode_host_func)(ioport *, host *);

/** Instance (bunch of function pointers) */
typedef struct codec_s {
    encode_host_func    encode_host;
    decode_host_func    decode_host;
} codec;

/* ============================= FUNCTIONS ============================= */

void         codec_release (codec *);
bool         codec_encode_host (codec *, ioport *, host *);
bool         codec_decode_host (codec *, ioport *, host *);

#endif
