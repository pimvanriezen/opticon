#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <assert.h>
#include <libopticon/util.h>
#include <libopticon/var_parse.h>
#include <libopticon/ioport_buffer.h>
#include <libopticon/var_dump.h>
#include <libopticon/log.h>
#include "req_context.h"

/*/ ======================================================================= /*/
/** Allocate a request argument list */
/*/ ======================================================================= /*/
req_arg *req_arg_alloc (void) {
    req_arg *self = malloc (sizeof (req_arg));
    self->argc = 0;
    self->argv = NULL;
    return self;
}

/*/ ======================================================================= /*/
/** Add a preallocated string to an argument list.
  * \param self The list
  * \param a The string to add */
/*/ ======================================================================= /*/
void req_arg_add_nocopy (req_arg *self, const char *a) {
    if (self->argc) {
        self->argv = (char **)
            realloc (self->argv, sizeof (char *) * (self->argc+1));
    }
    else self->argv = malloc (sizeof (char *));
    self->argv[self->argc] = (char *) a;
    self->argc++;
}

/*/ ======================================================================= /*/
/** Allocate and copy a string, adding it to an argument list.
  * \param self The list
  * \param a The string to add */
/*/ ======================================================================= /*/
void req_arg_add (req_arg *self, const char *a) {
    req_arg_add_nocopy (self, strdup (a));
}

/*/ ======================================================================= /*/
/** Remove the top element of an argument list.
  * \param self The list */
/*/ ======================================================================= /*/
void req_arg_remove_top (req_arg *self) {
    if (self->argc < 1) return;
    if (self->argc == 1) {
        free (self->argv[0]);
        self->argc = 0;
        self->argv = NULL;
        return;
    }
    
    size_t sz = (self->argc-1) * sizeof (char *);
    memmove (self->argv+1, self->argv, sz);
    self->argc--;
}

/*/ ======================================================================= /*/
/** Clear an argument list */
/*/ ======================================================================= /*/
void req_arg_clear (req_arg *self) {
    for (int i=0; i<self->argc; ++i) free (self->argv[i]);
    if (self->argv) free (self->argv);
    self->argc = 0;
    self->argv = NULL;
}

/*/ ======================================================================= /*/
/** Clear and deallocate an argument list */
/*/ ======================================================================= /*/
void req_arg_free (req_arg *self) {
    req_arg_clear (self);
    free (self);
}

/*/ ======================================================================= /*/
/** Initialize a matchlist header */
/*/ ======================================================================= /*/
void req_matchlist_init (req_matchlist *self) {
    self->first = self->last = NULL;
}

/*/ ======================================================================= /*/
/** Add a match definition to a matchlist.
  * \param self The list
  * \param s The match definition
  * \param mmask The mask of methods caught
  * \param f The function to call */
/*/ ======================================================================= /*/
void req_matchlist_add (req_matchlist *self, const char *s,
                        req_method mmask, path_f f) {
    req_match *m = malloc (sizeof (req_match));
    m->next = m->prev = NULL;
    m->matchstr = s;
    m->method_mask = mmask;
    m->func = f;
    m->textfunc = NULL;
    
    if (self->first) {
        m->prev = self->last;
        self->last->next = m;
        self->last = m;
    }
    else {
        self->first = self->last = m;
    }
}

/*/ ======================================================================= /*/
/** Add a match definition to a matchlist with a text-based
  * handler function.
  * \param self The list
  * \param s The match definition
  * \param mmask The mask of methods caught
  * \param f The function to call */
/*/ ======================================================================= /*/
void req_matchlist_add_text (req_matchlist *self, const char *s,
                             req_method mmask, path_text_f f) {
    req_match *m = malloc (sizeof (req_match));
    m->next = m->prev = NULL;
    m->matchstr = s;
    m->method_mask = mmask;
    m->func = NULL;
    m->textfunc = f;
    
    if (self->first) {
        m->prev = self->last;
        self->last->next = m;
        self->last = m;
    }
    else {
        self->first = self->last = m;
    }
}

/*/ ======================================================================= /*/
/** Check a url against a req_match definition.
  * \param self The match definition
  * \param url The url string
  * \param arg Caught arguments are stored here.
  * \return 1 On match, 0 on fail. */
/*/ ======================================================================= /*/
int req_match_check (req_match *self, const char *url, req_arg *arg) {
    req_arg_clear (arg);
    
    const char *curl = url;
    const char *curl_start = url;
    const char *cmatch = self->matchstr;
    while (*curl) {
        /* complete wildcard, always matches */
        if (*cmatch == '*' && cmatch[1] == 0) return 1;
        /* '*' wildcard, just skip over to the next matching char */
        if (*cmatch == '*') {
            while (*curl && *curl != cmatch[1]) curl++;
            if (! *curl) {
                /* end of url, end of match? */
                if (cmatch[1]) return 0;
                return 1;
            }
            cmatch++;
        }
        /* '%' argument, add argument */
        else if (*cmatch == '%') {
            char matchtp = cmatch[1];
            int validch = 0;
            assert (strchr ("sSUTi", matchtp));
            curl_start = curl;
            cmatch++;
            
            if (*curl == 0) return 0;
            
            /* Skip over the url until the next slash or the end */
            while (*curl && *curl != '/') {
                /* Make sure we're getting valid data for the type */
                switch (matchtp) {
                    case 's': 
                        validch++;
                        break;
                    
                    case 'S':
                        if (! strchr ("0123456789_-."
                                      "abcdefghijklmnopqrstuvwxyz"
                                      "ABCDEFGHIJKLMNOPQRSTUVWXYZ",
                                      *curl)) {
                            return 0;
                        }
                        validch++;
                        break;
                    
                    case 'U':
                        if (!strchr ("0123456789abcdef-", *curl)) {
                            return 0;
                        }
                        if (*curl != '-') validch++;
                        break;
                    
                    case 'T':
                        if (!strchr ("0123456789-:TZ", *curl)) {
                            return 0;
                        }
                        if (*curl>='0' && *curl<='9') validch++;
                        break;
                    
                    case 'i':
                        if (! strchr ("0123456789", *curl)) {
                            return 0;
                        }
                        validch++;
                        break;
                }
                curl++;
            }
            
            /* Reject invalid UUID / timespec */
            if (matchtp == 'U' && validch != 32) return 0;
            
            /* 123456789 1234 */
            /* 20210401113800 */
            if (matchtp == 'T' && (validch < 12 || validch > 14)) return 0;
            
            /* Don't permit empty url fields */
            if (validch < 1) return 0;
            
            /* Copy the url-part to the argument list */
            size_t elmsize = curl-curl_start;
            if (! elmsize) return 0;
            char *elm = malloc (elmsize+1);
            memcpy (elm, curl_start, elmsize);
            elm[elmsize] = 0;
            req_arg_add_nocopy (arg, elm); /*req_arg will free() elm*/
            cmatch++;
        }
        
        if (*cmatch == '*') {
            if (! *curl) return 1;
            continue;
        }
        if (*cmatch != *curl) {
            return 0;
        }
        if (! *curl) return 1;
        curl++;
        cmatch++;
    }
    if (*cmatch) {
        return 0;
    }
    return 1;
}

/*/ ======================================================================= /*/
/** Generates a response for a server error */
/*/ ======================================================================= /*/
int err_server_error (req_context *ctx, req_arg *arg,
                      var *out, int *status) {
    var_set_str_forkey (out, "error", "Internal Server Error");
    *status = 500;
    return 1;
}

/*/ ======================================================================= /*/
/** Utility function for sending either JSON-encoded or straight
  * data to the client.
  * \param conn The MHD connection handler.
  * \param res JSON data, if any.
  * \param status HTTP status code to use
  * \param out The ioport to use (may already contain data)
  * \param txt If 1, skip JSON encoding, send out ioport as-is. */
/*/ ======================================================================= /*/
void req_write_response (struct MHD_Connection *conn, var *res, int status,
                         var *outhdr, ioport *out, int txt) {
    /* if no text data was sent into the ioport, encode the
     * response */
    if (! txt) {
        ioport_write (out, "{\n", 2);
        var_write_indented (res, out, 4);
        ioport_write (out, "}\n", 2);
    }
    void *buf = (void *) ioport_get_buffer (out);
    size_t buflen = ioport_read_available (out);
    struct MHD_Response *response;
    response = MHD_create_response_from_buffer (buflen, buf,
                                                MHD_RESPMEM_MUST_COPY);
    
    var *hc = outhdr->value.arr.first;
    while (hc) {
        MHD_add_response_header (response, hc->id, var_get_str(hc));
        hc = hc->next;
    }
    MHD_queue_response (conn, status, response);
    MHD_destroy_response (response);
}

/*/ ======================================================================= /*/
/** Take a request context and connection and shop it around inside
  * a request matchlist.
  * \param self The matchlist
  * \param url The URL string
  * \param ctx The request context
  * \param connection The microhttp connection */
/*/ ======================================================================= /*/
void req_matchlist_dispatch (req_matchlist *self, const char *url,
                             req_context *ctx,
                             struct MHD_Connection *conn) {
                             
    ioport *out = ioport_create_buffer (NULL, 4096);                         
    req_arg *targ = req_arg_alloc();

    /* Iterate over the list */
    req_match *crsr = self->first;
    while (crsr) {
        if (ctx->method & crsr->method_mask) {
            if (req_match_check (crsr, url, targ)) {
                if (crsr->func) {
                    if (crsr->func (ctx, targ, ctx->response, &ctx->status)) {
                        req_write_response (conn, ctx->response, ctx->status,
                                            ctx->outhdr, out, 0);
                        req_arg_free (targ);
                        ioport_close (out);
                        return;
                    }
                }
                else if (crsr->textfunc) {
                    if (crsr->textfunc (ctx, targ, out, &ctx->status)) {
                        req_write_response (conn, ctx->response, ctx->status,
                                            ctx->outhdr, out, 1);
                        req_arg_free (targ);
                        ioport_close (out);
                        return;
                    }
                }
            }
        }
        crsr = crsr->next;
    }

    /* No matches, bail out */
    log_error ("No matches");
    err_server_error (ctx, targ, ctx->response, &ctx->status);
    req_write_response (conn, ctx->response, ctx->status, ctx->outhdr, out, 0);
    req_arg_free (targ);
    ioport_close (out);
}

/*/ ======================================================================= /*/
/** Allocate a request context object */
/*/ ======================================================================= /*/
req_context *req_context_alloc (void) {
    req_context *self = malloc (sizeof (req_context));
    if (self) {
        self->headers = var_alloc();
        self->bodyjson = var_alloc();
        self->response = var_alloc();
        self->outhdr = var_alloc();
        self->auth_data = var_alloc();
        self->auth_tenants = NULL;
        self->auth_tenantcount = 0;
        self->status = 500;
        self->url = NULL;
        self->ctype = NULL;
        self->body = NULL;
        self->bodysz = self->bodyalloc = 0;
        self->userlevel = AUTH_GUEST;
        memset (&self->tenantid, 0, sizeof (uuid));
        memset (&self->hostid, 0, sizeof (uuid));
        memset (&self->opticon_token, 0, sizeof (uuid));
        self->external_token = NULL;
        self->original_ip = NULL;
        timer_start (&self->ti);
    }
    return self;
}

/*/ ======================================================================= /*/
/** Deallocate a request context object and all its child structures */
/*/ ======================================================================= /*/
void req_context_free (req_context *self) {
    var_free (self->headers);
    var_free (self->bodyjson);
    var_free (self->response);
    var_free (self->outhdr);
    var_free (self->auth_data);
    if (self->url) {
        free (self->url);
    }
    if (self->ctype) free (self->ctype);
    if (self->auth_tenants) free (self->auth_tenants);
    if (self->body) free (self->body);
    if (self->external_token) free (self->external_token);
    if (self->original_ip) free (self->original_ip);
    free (self);
}

/*/ ======================================================================= /*/
/** Set a header inside the request context */
/*/ ======================================================================= /*/
void req_context_set_header (req_context *self, const char *hdrname,
                             const char *hdrval) {
    char *canon = strdup (hdrname);
    char *c = canon;
    int needcap=1;
    while (*c) {
        if (needcap) { *c = toupper(*c); needcap = 0; }
        else if (*c == '-') needcap = 1;
        else *c = tolower (*c);
        c++;
    }
    var_set_str_forkey (self->headers, canon, hdrval);
    
    /** Handle Openstack Token */
    if (strcmp (canon, "X-Auth-Token") == 0) {
        self->external_token = strdup (hdrval);
    }
    else if (strcmp (canon, "X-Opticon-Token") == 0) {
        self->opticon_token = mkuuid (hdrval);
    }
    else if (strcmp (canon, "Content-Type") == 0) {
        self->ctype = strdup (hdrval);
    }
    else if (strcmp (canon, "X-Forwarded-For") == 0) {
        self->original_ip = strdup (hdrval);
    }
    
    free (canon);
}

/*/ ======================================================================= /*/
/** Parse the loaded body data inside the request context as JSON */
/*/ ======================================================================= /*/
int req_context_parse_body (req_context *self) {
    if (! self->body) return 1;
    if (strcasecmp (self->ctype, "application/json") != 0) return 0;
    return var_parse_json (self->bodyjson, self->body);
}

/*/ ======================================================================= /*/
/** Internal function for sizing the body data buffer */
/*/ ======================================================================= /*/
static size_t mkallocsz (size_t target) {
    if (target < 4096) return 4096;
    if (target < 8192) return 8192;
    if (target < 16384) return 16384;
    if (target < 65536) return 65536;
    return target + 4096;
}

/*/ ======================================================================= /*/
/** Handle uploaded data */
/*/ ======================================================================= /*/
void req_context_append_body (req_context *self, const char *dt, size_t sz) {
    if (! self->body) {
        self->bodyalloc = mkallocsz (sz);
        self->body = malloc (self->bodyalloc);
        self->bodysz = 0;
    }
    if ((self->bodysz + sz + 1) > self->bodyalloc) {
        self->bodyalloc = mkallocsz (self->bodysz + sz + 1);
        self->body = (char *) realloc (self->body, self->bodyalloc);
    }
    memcpy (self->body + self->bodysz, dt, sz);
    self->bodysz += sz;
    self->body[self->bodysz] = 0;
}

/*/ ======================================================================= /*/
/** Set the url */
/*/ ======================================================================= /*/
void req_context_set_url (req_context *self, const char *url) {
    if (self->url) free (self->url);
    self->url = strdup (url);
}

/*/ ======================================================================= /*/
/** Set the request method */
/*/ ======================================================================= /*/
void req_context_set_method (req_context *self, const char *mth) {
    req_method m = REQ_OTHER;
    if (strcasecmp (mth, "get") == 0) m = REQ_GET;
    else if (strcasecmp (mth, "post") == 0) m = REQ_POST;
    else if (strcasecmp (mth, "put") == 0) m = REQ_PUT;
    else if (strcasecmp (mth, "delete") == 0) m = REQ_DELETE;
    else if (strcasecmp (mth, "options") == 0) m = REQ_OPTIONS;
    self->method = m;
}
