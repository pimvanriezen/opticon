#ifndef _EXTERNALDATA_H
#define _EXTERNALDATA_H 1

#include <libopticon/uuid.h>
#include <libopticon/var.h>

typedef struct extdata_s {
    struct extdata_s    *prev;
    struct extdata_s    *next;
    uint32_t             hash;
    uuid                 hostid;
    var                 *data;
} extdata;

typedef struct extdatalist_s {
    extdata *first;
    extdata *last;
} extdatalist;

var *extdata_get (uuid hostid);

#endif
