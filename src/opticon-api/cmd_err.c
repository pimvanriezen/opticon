#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <libopticon/util.h>
#include <libopticon/datatypes.h>
#include <microhttpd.h>
#include "req_context.h"
#include "cmd.h"
#include "options.h"

int err_generic (var *out, const char *fmt, ...) {
    char buffer[4096];
    buffer[0] = 0;
    va_list ap;
    va_start (ap, fmt);
    vsnprintf (buffer, 4096, fmt, ap);
    va_end (ap);

    var_set_str_forkey (out, "error", buffer);
    return 1;
}

/*/ ======================================================================= /*/
/** Generate a generic 403 error */
/*/ ======================================================================= /*/
int err_unauthorized (req_context *ctx, req_arg *a, var *out, int *status) {
    var_set_str_forkey (out, "error", "Unauthorized access");
    *status = 403;
    return 1;    
}

/*/ ======================================================================= /*/
/** Generate a generic 401 error */
/*/ ======================================================================= /*/
int err_not_allowed (req_context *ctx, req_arg *a, var *out, int *status) {
    var_set_str_forkey (out, "error", "Permission denied");
    *status = 401;
    return 1;    
}

/*/ ======================================================================= /*/
/** Generate a generic 404 error */
/*/ ======================================================================= /*/
int err_not_found (req_context *ctx, req_arg *a, var *out, int *status) {
    var_set_str_forkey (out, "error", "Resource not found");
    *status = 404;
    return 1;
}

/*/ ======================================================================= /*/
/** Generate a generic 405 error */
/*/ ======================================================================= /*/
int err_method_not_allowed (req_context *ctx, req_arg *a,
                            var *out, int *status) {
    var_set_str_forkey (out, "error", "Method not allowed");
    *status = 405;
    return 1;
}

/*/ ======================================================================= /*/
/** Generate a generic 400 error */
/*/ ======================================================================= /*/
int err_bad_request (req_context *ctx, req_arg *a, var *out, int *status) {
    var_set_str_forkey (out, "error", "Bad request");
    *status = 400;
    return 1;    
}

/*/ ======================================================================= /*/
/** Generate a generic 409 error */
/*/ ======================================================================= /*/
int err_conflict (req_context *ctx, req_arg *a, var *out, int *status) {
    var_set_str_forkey (out, "error", "Request conflicts with existing "
                        "resources");
    *status = 409;
    return 1;    
}

