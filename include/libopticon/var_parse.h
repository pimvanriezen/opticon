#ifndef _PARSE_H
#define _PARSE_H 1

#include <libopticon/var.h>
#include <stdio.h>

/* ============================= FUNCTIONS ============================= */

int          var_read_json (var *, FILE *);
int          var_load_json (var *, const char *);
int          var_parse_json (var *, const char *);
const char  *parse_error (void);

#endif
