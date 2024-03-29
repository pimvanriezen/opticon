#ifndef _REQ_CONTEXT_H
#define _REQ_CONTEXT_H 1

#include <libopticon/var.h>
#include <microhttpd.h>
#include <arpa/inet.h>
#include <libopticon/timer.h>
#include <libopticon/ioport.h>
#include <time.h>

typedef uint8_t req_method;
#define REQ_GET 0x01
#define REQ_POST 0x02
#define REQ_PUT 0x04
#define REQ_DELETE 0x08
#define REQ_OPTIONS 0x10
#define REQ_OTHER 0x20
#define REQ_UPDATE (REQ_POST|REQ_PUT)
#define REQ_ANY (REQ_UPDATE|REQ_GET|REQ_DELETE|REQ_OPTIONS|REQ_OTHER)

typedef enum {
    AUTH_GUEST,
    AUTH_USER,
    AUTH_PROV,
    AUTH_ADMIN
} auth_level;

/** Request context for HTTP handling */
typedef struct req_context_s {
    var         *headers; /**< HTTP headers */
    var         *bodyjson; /**< Parsed JSON */
    var         *response; /**< Prepared JSON response */
    var         *outhdr; /**< Response headers */
    int          status; /**< HTTP response code */
    req_method   method; /**< HTTP request method */
    char         remote[INET6_ADDRSTRLEN]; /**< Remote address */
    char        *url; /**< Requested URL */
    char        *ctype; /**< Content type */
    char        *body; /**< Received body data */
    size_t       bodysz; /**< Size of body data */
    size_t       bodyalloc; /**< Allocated size of body buffer */
    uuid         tenantid; /**< URL-parsed tenantid, if any */
    uuid         hostid; /**< URL-parsed hostid, if any */
    auth_level   userlevel; /**< 1 if user is admin */
    uuid         opticon_token; /**< Header-parsed opticon token */
    char        *external_token; /**< Header-parsed openstack token */
    char        *original_ip; /**< Header-parsed X-Forwarded-For */
    uuid        *auth_tenants; /**< parsed tenant uuids */
    int          auth_tenantcount; /**< Number of authorized tenants */
    var         *auth_data; /**< Authentication data */
    timer        ti;
} req_context;

/** Arguments matched out of a URL */
typedef struct req_arg_s {
    int argc; /**< number of allocated elements */
    char **argv; /**< Element array (owned) */
} req_arg;

/** Handler function for a specified URL path */
typedef int (*path_f)(req_context *, req_arg *, var *resp, int *st);
typedef int (*path_text_f)(req_context *, req_arg *, ioport *resp, int *st);

/** Path match definition*/
typedef struct req_match_s {
    struct req_match_s  *next; /**< Link neighbour */
    struct req_match_s  *prev; /**< Link neighbour */
    const char          *matchstr; /**< Match definition. */
    req_method           method_mask; /**< Methods affected */
    path_f               func; /**< Handler function */
    path_text_f          textfunc; /**< Alternative handler function */
} req_match;

/** List header */
typedef struct req_matchlist_s {
    req_match   *first;
    req_match   *last;
} req_matchlist;

extern req_matchlist REQ_MATCHES;

req_arg     *req_arg_alloc (void);
void         req_arg_clear (req_arg *);
void         req_arg_free (req_arg *);
void         req_arg_add (req_arg *, const char *);
void         req_arg_remove_top (req_arg *);

void         req_matchlist_init (req_matchlist *);
void         req_matchlist_add (req_matchlist *, const char *,
                                req_method, path_f);
void         req_matchlist_add_text (req_matchlist *, const char *,
                                     req_method, path_text_f);
void         req_matchlist_dispatch (req_matchlist *, const char *url,
                                     req_context *ctx,
                                     struct MHD_Connection *conn);

req_context *req_context_alloc (void);
void         req_context_free (req_context *);
void         req_context_set_header (req_context *self, const char *hdr,
                                     const char *val);
int          req_context_parse_body (req_context *self);
void         req_context_append_body (req_context *, const char *, size_t);
void         req_context_set_url (req_context *, const char *);
void         req_context_set_method (req_context *, const char *);

#endif
