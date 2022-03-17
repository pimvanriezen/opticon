#ifndef _PRETTYPRINT_H
#define _PRETTYPRINT_H 1

#include <libopticon/var.h>
#include "rsrc.h"

/** Table alignment indicator */
typedef enum {
    CA_NULL,
    CA_L,
    CA_R
} columnalign;

#define VT_GRN "\033[38;5;28m"
#define VT_RST "\033[0m"
#define VT_BLD "\033[1m"
#define VT_YLW "\033[38;5;185m"

void clear_pending_header (void);
const char *decorate_status (const char *);
void print_line (void);
void print_bar (int, double, double);
void print_graph (int, int, int, double, double *);
void print_hdr (const char *, resource *);
void print_value (const char *, const char *, ...);
void print_gauge_value (const char *, const char *, double, double);
void print_array (const char *key, var *arr);
void print_values (var *apires, const char *pfx);
void print_table (var *arr, const char **hdr, const char **fld,
                  columnalign *align, vartype *typ, int *wid,
                  const char **suffx, int *div);
void print_generic_table (var *);
void print_tables (var *);
void print_done (void);

#endif
