#ifndef _TERM_H
#define _TERM_H 1

#include "rsrc.h"
#include <stdbool.h>

#define TERMATTR_RGBCOLOR 0x01
#define TERMATTR_UNICODE 0x02
#define TERMATTR_INLINEGFX 0x04

typedef int termattr;

typedef struct {
    const char *name;
    termattr attr;
    int cols;
    int rows;
    int x, y;
    int curcolumn;
    int columnstart;
    int columnend;
    int columnwidth;
} terminfo;

extern terminfo TERMINFO;

void term_getwinsz (void);
void term_init (void);
void term_printf (const char *fmt, ...);
int  term_width (void);
void term_print_icns (int height, resource *icns);
void term_print_hdr (const char *title, resource *icns);
void term_new_column (void);
void term_end_column (void);
bool term_is_graphical (void);
void term_crsr_up (int);
void term_crsr_setx (int);
void term_advance (int);
void term_fill_line (char c);
int  term_is_iterm (void);
const char *term_write_escape (const char *);

#endif
