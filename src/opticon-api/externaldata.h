#ifndef _EXTERNALDATA_H
#define _EXTERNALDATA_H 1

#include <libopticon/uuid.h>
#include <libopticon/var.h>

void extdata_init (void);
var *extdata_get (uuid tenantid, uuid hostid, var *env);

#endif
