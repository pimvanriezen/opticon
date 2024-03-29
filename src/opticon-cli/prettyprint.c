#include <stdio.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>

#include <libopticon/datatypes.h>
#include <libopticon/strnappend.h>
#include <libopticon/util.h>
#include <libopticon/var.h>

#include "prettyprint.h"
#include "api.h"
#include "cmd.h"
#include "term.h"

static const char *PENDING_HDR = NULL;
static resource *PENDING_RSRC = NULL;
static var *MDEF = NULL;
static enum termtype {
    TERM_UNSET,
    TERM_MODERN,
    TERM_PRIMITIVE } TERM = TERM_UNSET;

static int PRINTED_FIRST = 0;

enum termtype term (void) {
    if (TERM == TERM_UNSET) {
        const char *env_term = getenv("TERM");
        if (env_term) {
            if ((strcmp (env_term, "xterm-mono") == 0) ||
                (strcmp (env_term, "vt100") == 0)) {
                TERM = TERM_PRIMITIVE;
            }
            else {
                TERM = TERM_MODERN;
            }
        }
        else {
            TERM = TERM_PRIMITIVE;
        }
    }
    return TERM;
}

void print_line (void) {
    if (TERMINFO.attr & TERMATTR_INLINEGFX) {
        term_printf ("\n");
    }
    else {
        term_fill_line ('-');
    }
}

static char *statustxt = NULL;
static int statusline = 0;

void clear_pending_header (void) {
    PENDING_HDR = NULL;
    PENDING_RSRC = NULL;
    PRINTED_FIRST = 0;
    statusline = 0;
}

const char *decorate_status (const char *st) {
    if (! statustxt) statustxt = (char *) malloc (1024);
    int color = 245;
    if (strcmp (st, "OK") == 0) color = (statusline&1) ? 28 : 22;
    else if (strcmp (st, "WARN") == 0) color = (statusline&1) ? 166 : 202;
    else if (strcmp (st, "ALERT") == 0) color = (statusline&1) ? 89 : 88;
    else if (strcmp (st, "CRIT") == 0) color = (statusline&1) ? 25 : 24;
    
    statusline++;
    sprintf (statustxt, "\033[48;5;%im\033[1m %-5s \033[0m", color, st);
    return statustxt;
}

void print_bar (int width, double max, double v) {
    if (! width) width = 1;
    double prop = max/width;
    const char *bars[9] = {
        " ","▏","▎","▍","▌","▋","▊","▉","█"
    };
    const char *pbars[9] = {
        " ", " ", " ", " ", ">", ">", ">", ">", "#"
    };
    
    if (term() == TERM_PRIMITIVE) {
        bars[1] = " ";
        bars[2] = " ";
        bars[3] = " ";
        bars[4] = ">";
        bars[5] = ">";
        bars[6] = ">";
        bars[7] = ">";
        bars[8] = "#";
    }
    
    term_printf ("\033[38;5;45m\033[48;5;239m");
    for (int i=0; i<width; ++i) {
        if ((i+1) <= (v/prop)) {
            term_printf ("%s", (term()==TERM_MODERN) ? bars[8] : pbars[8]);
        }
        else if (i > (v/prop)) {
            term_printf (" ");
        }
        else {
            double diff = ((i+1) - (v/prop)) * 8;
            term_printf ("%s", (term()==TERM_MODERN) ? bars[(int) (8-diff)]
                                                   : pbars[(int) (8-diff)]);
        }
    }
    term_printf ("</>");
}

void print_graph (int width, int height, int ind, double minmax, double *data) {
    const char *bars[9] = {" ","▁","▂","▃","▄","▅","▆","▇","█"};
    const char *pbars[9] = {" "," "," "," ",".",".",".",".","#"};
    char *map = (char *) malloc (width*height);
    
    double max = minmax;
    int i=0;
    int x=0;
    int y=0;
    
    for (i=0; i<width; ++i) {
        if (data[i] > max) max = data[i];
    }
    
    for (x=0; x<width; ++x) {
        int scaled = (data[x] * height / max) * 8;
        int plotted = 0;
        for (y=height-1;y>=0;--y) {
            if ((scaled - plotted) >= 8) {
                map[x+(y*width)] = 8;
                plotted += 8;
            }
            else if (scaled > plotted) {
                map[x+(y*width)] = scaled-plotted;
                plotted += (scaled-plotted);
            }
            else {
                map[x+(y*width)] = 0;
            }
        }
    }
    
    double r = 0;
    double g = 215;
    double b = 255;
    
    double Tr = 80;
    double Tg = 200;
    double Tb = 80;
    
    double dr = (Tr-r) / (height);
    double dg = (Tg-g) / (height);
    double db = (Tb-b) / (height);
    
    double gr = 60;
    
    for (y=0; y<height; ++y) {
        term_crsr_setx (ind); // printf ("\033[%iC", ind);

        if (TERMINFO.attr & TERMATTR_RGBCOLOR) {
            term_printf ("\033[38;2;%.0f;%.0f;%.0fm"
                         "\033[48;2;%.0f;%.0f;%.0fm",
                         r, g, b, gr, gr, gr);
            for (x=0; x<width; ++x) {
                term_printf ("%s", bars[map[x+(y*width)]]);
            }
        
            r = r + dr;
            g = g + dg;
            b = b + db;
            gr = gr *0.96;
        }
        else {
            for (x=0; x<width; ++x) {
                term_printf ("%s", pbars[map[x+(y*width)]]);
            }
        }
        
        term_printf ("</>\n");
    }
    
    free (map);    
}

/** Display function for host-show section headers */
void print_hdr (const char *hdr, resource *rs) {
    term_print_hdr (hdr, rs);
}

/** Display function for host-show data */
void print_value (const char *key, const char *fmt, ...) {
    if (PENDING_HDR) {
        print_hdr (PENDING_HDR, PENDING_RSRC);
        PENDING_HDR = NULL;
        PENDING_RSRC = NULL;
    }
    char val[4096];
    val[0] = 0;
    va_list ap;
    va_start (ap, fmt);
    vsnprintf (val, 4095, fmt, ap);
    va_end (ap);
    val[4095] = 0;

    const char *dots = "......................";
    int dotspos = strlen(dots) - 18;
    term_printf (" %s<l>", key);
    dotspos += strlen (key);
    if (dotspos < strlen (dots)) term_printf ("%s", dots+dotspos);
    term_printf ("</>: %s\n", val);
}

void print_gauge_value (const char *key, const char *unit, double val,
                        double max) {
    const char *dots = "......................";
    int dotspos = strlen(dots) - 18;
    term_printf (" %s<l>", key);
    dotspos += strlen (key);
    if (dotspos < strlen (dots)) term_printf ("%s", dots+dotspos);
    term_printf ("</>: <b>%6.2f </>%-20s-|", val, unit);
    print_bar (25, max, val);
    term_printf ("|+\n");
}

/** Print out an array as a comma-separated list */
void print_array (const char *key, var *arr) {
    const char *str;
    char out[4096];
    out[0] = 0;
    int cnt=0;
    var *crsr = arr->value.arr.first;
    while (crsr) {
        if (cnt) strnappend (out, ",", 4095);
        switch (crsr->type) {
            case VAR_INT:
                snprintf (out+strlen(out), 4095-strlen(out),
                          "<b>%" PRIu64 "</>", var_get_int (crsr));
                break;
            
            case VAR_DOUBLE:
                snprintf (out+strlen(out), 4095-strlen(out), 
                         "<b>%.2f</>", var_get_double (crsr));
                break;
            
            case VAR_STR:
                str = var_get_str (crsr);
                if (! str) break;
                snprintf (out+strlen(out), 4095-strlen(out),
                          "<y>%s</>", var_get_str (crsr));
                break;
            
            default:
                strnappend (out, "?", 4095);
                break;
        }
        crsr = crsr->next;
        cnt++;
    }
    out[4095] = 0;
    
    print_value (key, "%s", out);   
}

/** Print out any non-grid values in a var.
  * \param apires The var
  * \param pfx Path prefix (used for recursion)
  * \param mdef Meter definitions (used for recursion)
  */
void print_values (var *apires, const char *pfx) {
    if (! MDEF) MDEF = api_get ("/%s/meter", OPTIONS.tenant);
    var *meters = var_get_dict_forkey (MDEF, "meter");
    var *crsr = apires->value.arr.first;
    PENDING_HDR = "Misc";
    PENDING_RSRC = rsrc(icns.overview);
    while (crsr) {
        char valbuf[1024];
        const char *name = NULL;
        const char *unit = NULL;
        var *meter;
        if (pfx) {
            sprintf (valbuf, "%s/%s", pfx, crsr->id);
            meter = var_get_dict_forkey (meters, valbuf);
        }
        else meter = var_get_dict_forkey (meters, crsr->id);
        name = var_get_str_forkey (meter, "description");
        if (! name) name = crsr->id;
        unit = var_get_str_forkey (meter, "unit");
        if (! unit) unit = "";
        switch (crsr->type) {
            case VAR_ARRAY:
                if (crsr->value.arr.first &&
                    crsr->value.arr.first->type != VAR_DICT) {
                    print_array (name, crsr);
                }
                break;
            
            case VAR_STR:
                sprintf (valbuf, "<y>%s</>", var_get_str(crsr));
                print_value (name, "%s", valbuf);
                break;
            
            case VAR_INT:
                sprintf (valbuf, "<b>%" PRIu64 "</> %s",
                         var_get_int(crsr), unit);
                print_value (name, "%s", valbuf);
                break;
            
            case VAR_DOUBLE:
                sprintf (valbuf, "<b>%.2f</> %s", var_get_double(crsr), unit);
                print_value (name, "%s", valbuf);
                break;
            
            case VAR_DICT:
                print_values (crsr, crsr->id);
                break;
                
            default:
                break;
        }
        crsr = crsr->next;
    }
}

/** Print out tabular data (like top, df) with headers, bells and
  * whistles.
  * \param arr The array-of-dicts to print
  * \param hdr Array of strings containing column headers. NULL terminated.
  * \param fld Array of strings representing column field ids.
  * \param align Array of columnalign for left/right alignment of columns.
  * \param typ Array of data types.
  * \param wid Array of integers representing column widths.
  * \param suffx Array of suffixes to print after specific column cells.
  * \param div Number to divide a VAR_INT value by prior to printing. The
  *            resulting number will be represented as a float with two
  *            decimals.
  */
void print_table (var *arr, const char **hdr, const char **fld,
                  columnalign *align, vartype *typ, int *wid,
                  const char **suffx, int *div) {
    char fmt[128];
    char buf[1024];
    char tsuffx[64];
    
    int col = 0;
    int colorsdone = 0;
    
    const char *coltbl[] = { "\033[38;5;185m",
                             "\033[38;5;28m" };
    
    term_printf (" ");
    while (hdr[col]) {
        strcpy (fmt, "%");
        if (align[col] == CA_L) strcat (fmt, "-");
        if (wid[col]) {
            sprintf (fmt+strlen(fmt), "%i", wid[col]);
        }
        strcat (fmt, "s ");
        term_printf (fmt, hdr[col]);
        col++;
    }
    term_printf ("\n ");
    
    var *node = arr->value.arr.first;
    const char *str;
    while (node) {
        col = 0;
        colorsdone = 0;
        while (hdr[col]) {
            int isbold = 0;
            int iscolor = 0;
            switch (typ[col]) {
                case VAR_STR:
                    str = var_get_str_forkey (node, fld[col]);
                    if (! str) buf[0] = 0;
                    else strncpy (buf, str, 512);
                    buf[512] = 0;
                    iscolor = 1;
                    break;
                
                case VAR_INT:
                    if (div[col]) {
                        double nval =
                            (double) var_get_int_forkey (node, fld[col]);
                        
                        nval = nval / ((double) div[col]);
                        sprintf (buf, "%.2f", nval);
                        isbold = 1;
                    }
                    else {
                        sprintf (buf, "%" PRIu64,
                                 var_get_int_forkey (node, fld[col]));
                        isbold = 1;
                    }
                    break;
                
                case VAR_DOUBLE:
                    sprintf (buf, "%.2f",
                             var_get_double_forkey (node, fld[col]));
                    isbold = 1;
                    break;
                
                default:
                    buf[0] = 0;
                    break;
            }
            int sufwidth = 0;
            if (suffx[col][0]) {
                strcpy (tsuffx, (suffx[col][0] != ' ') ? " " : "");
                int wpos = strlen (tsuffx);
                int i;
                sufwidth = wpos;
                for (i=0; i<62 && wpos<62 && suffx[col][i]; ++i) {
                    if (suffx[col][i] == '%') tsuffx[wpos++] = '%';
                    tsuffx[wpos++] = suffx[col][i];
                    if (suffx[col][i] > 0) sufwidth++;
                    else if (suffx[col][i] & 64) sufwidth++;
                }
                tsuffx[wpos] = 0;
            }
            else {
                tsuffx[0] = 0;
            }
            strcpy (fmt, isbold ? "\033[1m" : iscolor ? coltbl[colorsdone&1] : "");
            strcat (fmt, "%");
            if (align[col] == CA_L) strcat (fmt, "-");
            if (wid[col]) {
                sprintf (fmt+strlen(fmt),"%i",wid[col]-sufwidth);
            }
            strcat (fmt, "s");
            if (isbold || iscolor) strcat (fmt, "\033[0m");
            if (iscolor) colorsdone++;
            strcat (fmt, tsuffx);
            strcat (fmt, " ");
            term_printf (fmt, buf);
            col++;
        }
        term_printf ("\n");
        node = node->next;
        if (node) term_printf(" ");
    }
}

/** Prepare data for print_table on a generic table */
void print_generic_table (var *table) {
    char fullkey[192];
    sprintf (fullkey, "%s/", table->id);
    char *nodekey = fullkey+strlen(fullkey);

    if (! MDEF) MDEF = api_get ("/%s/meter", OPTIONS.tenant);
    var *meters = var_get_dict_forkey (MDEF, "meter");
    int rowcount = var_get_count (table);
    if (rowcount < 1) return;
    var *crsr = table->value.arr.first;
    int count = var_get_count (crsr);
    if (count < 1) return;
    int sz = count+1;
    int slen;
    int i;
    char *c;
    
    char **header = (char **) calloc (sz, sizeof(char *));
    char **field = (char **) calloc (sz, sizeof(char *));
    columnalign *align = (columnalign *) calloc (sz, sizeof(columnalign));
    vartype *type = (vartype *) calloc (sz, sizeof(vartype));
    int *width = (int *) calloc (sz, sizeof (int));
    int *div = (int *) calloc (sz, sizeof (int));
    char **suffix = (char **) calloc (sz, sizeof (char *));
    
    var *node = crsr->value.arr.first;
    for (i=0; node && (i<count); node=node->next, ++i) {
        type[i] = node->type;
        field[i] = (char *) node->id;
        width[i] = strlen (node->id);
        strcpy (nodekey, node->id);
        var *mdef = var_find_key (meters, fullkey);
        if (! mdef) {
            header[i] = strdup (node->id);
        }
        else {
            const char *d = var_get_str_forkey (mdef, "description");
            if (d && strlen(d) < 16) header[i] = strdup (d);
            else header[i] = strdup (node->id);
        }
        for (c = header[i]; *c; ++c) *c = toupper (*c);
        if (node->type == VAR_DOUBLE || node->type == VAR_INT) {
            align[i] = i ? CA_R : CA_L;
            if (node->type == VAR_DOUBLE) {
                if (width[i] < 8) width[i] = 8;
            }
            else if (width[i] < 10) width[i] = 10;
            
        }
        else {
            align[i] = CA_L;
            if (node->type == VAR_STR) {
                slen = strlen (var_get_str(node)) + 1;
                if (slen > width[i]) width[i] = slen;
            }
        }
        suffix[i] = "";
        if (mdef) {
            const char *unit = var_get_str_forkey (mdef, "unit");
            if (unit) suffix[i] = (char*) unit;
        }
    }
    
    crsr = crsr->next;
    while (crsr) {
        node = crsr->value.arr.first;
        for (i=0; node && (i<count); node=node->next, ++i) {
            if (node->type == VAR_STR) {
                slen = strlen (var_get_str(node)) +1;
                if (slen > width[i]) width[i] = slen;
            }
        }
    
        crsr = crsr->next;
    }
    
    int twidth = 0;
    for (i=0; i<count; ++i) twidth += width[i];
    if (twidth<40) {
        int add = (40 - twidth) / count;
        if (! add) add = 1;
        for (i=0; i<count; ++i) width[i] += add;
    }
    else if (twidth<60) {
        for (i=0; i<count; ++i) width[i]++;
    }
    
    const char *title = table->id;
    var *tdef = var_find_key (meters, table->id);
    if (tdef) {
        const char *c = var_get_str_forkey (tdef, "description");
        if (c) title = c;
    }
    
    resource *rs = rsrc(icns.overview);
    if (strcmp (table->id, "who") == 0) rs = rsrc(icns.remote);
    else if (strcmp (table->id, "vm") == 0) rs = rsrc(icns.vm);
    term_new_column();
    print_hdr (title, rs);
    print_table (table, (const char **) header, (const char **) field,
                 align, type, width, (const char **) suffix, div);
    
    for (i=0; i<count; ++i) free (header[i]);
    free (header);
    free (field);
    free (align);
    free (type);
    free (width);
    free (suffix);
    free (div);
}

/** Print out any tables found in a var dictionary. This will be called 
  * by cmd_get_record() for any table-like value not already printed.
  */
void print_tables (var *apires) {
    var *crsr = apires->value.arr.first;
    while (crsr) {
        if (crsr->type == VAR_ARRAY && var_get_count(crsr)) {
            if (crsr->value.arr.first->type == VAR_DICT) {
                print_generic_table (crsr);
            }
        }
        crsr = crsr->next;
    }
}

/** Free open memory */
void print_done (void) {
    if (MDEF) {
        var_free (MDEF);
        MDEF = NULL;
    }
}

