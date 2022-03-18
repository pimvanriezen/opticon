#include <libopticon/var.h>
#include <libopticon/var_parse.h>
#include <libopticon/log.h>
#include <assert.h>
#include "rsrc.h"

static var *PARSED_DEFMETERS = NULL;

/*/ ======================================================================= /*/
/** Get default meter data as var object */
/*/ ======================================================================= /*/
var *get_default_meterdef (void) {
    if (! PARSED_DEFMETERS) {
        PARSED_DEFMETERS = var_alloc();
        if (! var_parse_json (PARSED_DEFMETERS, librsrc.text.defmeters.data)) {
            log_error ("Parse error: %s", parse_error());
        }
    }
    return PARSED_DEFMETERS;
}

static var *PARSED_DEFSUMMARY = NULL;

/*/ ======================================================================= /*/
/** Get default summary data as var object */
/*/ ======================================================================= /*/
var *get_default_summarydef (void) {
    if (! PARSED_DEFSUMMARY) {
        PARSED_DEFSUMMARY = var_alloc();
        if (! var_parse_json (PARSED_DEFSUMMARY,
                              librsrc.text.defsummary.data)) {
            log_error ("Parse error (defsum): %s", parse_error());
        }
    }
    return PARSED_DEFSUMMARY;
}

static var *PARSED_DEFGRAPHS = NULL;

/*/ ======================================================================= /*/
/** Get default graph data as var object */
/*/ ======================================================================= /*/
var *get_default_graphs (void) {
    if (! PARSED_DEFGRAPHS) {
        PARSED_DEFGRAPHS = var_alloc();
        if (! var_parse_json (PARSED_DEFGRAPHS, librsrc.text.defgraphs.data )) {
            log_error ("Parse error (defgraphs): %s", parse_error());
        }
    }
    return PARSED_DEFGRAPHS;
}
