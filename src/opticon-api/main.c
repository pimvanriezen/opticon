#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <libopticon/cliopt.h>
#include <libopticon/util.h>
#include <libopticon/datatypes.h>
#include <libopticon/ioport.h>
#include <libopticon/ioport_buffer.h>
#include <libopticon/var_parse.h>
#include <libopticon/var_dump.h>
#include <libopticon/defaultmeters.h>
#include <libopticon/react.h>
#include <libopticon/daemon.h>
#include <libhttp/http.h>
#include <libopticon/log.h>
#include <microhttpd.h>
#include <syslog.h>
#include "req_context.h"
#include "cmd.h"
#include "options.h"
#include "tokencache.h"
#include "authsession.h"

req_matchlist REQ_MATCHES;

#if MHD_VERSION < 0x00097000
    typedef int MHDResult;
#else
    typedef enum MHD_Result MHDResult;
#endif

/*/ ======================================================================= /*/
/** MHD callback function for setting the context headers */
/*/ ======================================================================= /*/
MHDResult enumerate_header (void *cls, enum MHD_ValueKind kind,
                            const char *key, const char *value) {
    req_context *ctx = (req_context *) cls;
    req_context_set_header (ctx, key, value);
    return MHD_YES;
}

/*/ ======================================================================= /*/
/** MHD callback function for handling a connection */
/*/ ======================================================================= /*/
MHDResult answer_to_connection (void *cls, struct MHD_Connection *connection,
                                const char *url, const char *method,
                                const char *version, const char *upload_data,
                                size_t *upload_data_size, void **con_cls) {
    /* Set up a context if we're at the first callback */
    req_context *ctx = *con_cls;
    if (ctx == NULL) {
        ctx = req_context_alloc();
        req_context_set_url (ctx, url);
        req_context_set_method (ctx, method);
        *con_cls = ctx;
        return MHD_YES;
    }
    
    /* Handle post data */
    if (*upload_data_size) {
        req_context_append_body (ctx, upload_data, *upload_data_size);
        *upload_data_size = 0;
        return MHD_YES;
    }
    
    MHD_get_connection_values (connection, MHD_HEADER_KIND,
                               enumerate_header, ctx);
    
    struct sockaddr *so;
    so = MHD_get_connection_info (connection,
            MHD_CONNECTION_INFO_CLIENT_ADDRESS)->client_addr;
    
    if (so->sa_family == AF_INET) {
        struct sockaddr_in *si = (struct sockaddr_in *) so;        
        inet_ntop (AF_INET, &si->sin_addr, ctx->remote, INET6_ADDRSTRLEN);
    }
    else if (so->sa_family == AF_INET6) {
        struct sockaddr_in6 *si = (struct sockaddr_in6 *) so;
        inet_ntop (AF_INET6, &si->sin6_addr, ctx->remote, INET6_ADDRSTRLEN);
    }
    
    /* If request comes from localhost, parse X-Forwarded-For */
    if (strcmp (ctx->remote, "127.0.0.1") == 0) {
        if (ctx->original_ip) {
            char *comma = strchr (ctx->original_ip, ',');
            if (comma) *comma = 0;
            strncpy (ctx->remote, ctx->original_ip, INET6_ADDRSTRLEN-1);
            ctx->remote[INET6_ADDRSTRLEN-1] = 0;
        }
    }
    
    /* Parse post data */
    req_context_parse_body (ctx);
    
    req_matchlist_dispatch (&REQ_MATCHES, url, ctx, connection);
    
    const char *lvl = "AUTH_GUEST";
    if (ctx->userlevel == AUTH_USER) lvl = "AUTH_USER ";
    else if (ctx->userlevel == AUTH_ADMIN) lvl = "AUTH_ADMIN";
    else if (ctx->userlevel == AUTH_PROV) lvl = "AUTH_PROV";
    
    timer_end (&ctx->ti);
    
    log_info ("%s [%s] %i %s %s (%.1f ms)", ctx->remote, lvl, ctx->status,
              method, url, ctx->ti.diff);

    req_context_free (ctx);
    *con_cls = NULL;
    return MHD_YES;
}

/*/ ======================================================================= /*/
/** Handle an X-Auth-Token header that should be passed to keystone */
/*/ ======================================================================= /*/
int handle_external_token_openstack (req_context *ctx) {
    if (! OPTIONS.keystone_url[0]) return 0;
    char *url = (char *) malloc (strlen(OPTIONS.keystone_url)+40);
    char l = OPTIONS.keystone_url[strlen(OPTIONS.keystone_url)-1];
    sprintf (url, "%s%sv2.0/tenants", OPTIONS.keystone_url,
                  (l == '/') ? "" : "/");  
    int retcode = 0;
    int i = 0;
    var *hdr = var_alloc();
    var_set_str_forkey (hdr, "X-Auth-Token", ctx->external_token);
    var *data = var_alloc();
    var *res = http_call ("GET", url, hdr, data, NULL, NULL);

    if (ctx->auth_tenants) {
        free (ctx->auth_tenants);
        ctx->auth_tenants = NULL;
    }

    if (res) {
        var *res_tenants = var_get_array_forkey (res, "tenants");
        ctx->auth_tenantcount = var_get_count (res_tenants);
        if (ctx->auth_tenantcount) {
            ctx->auth_tenants = malloc (ctx->auth_tenantcount * sizeof (uuid));
            i = 0;
            var *crsr = res_tenants->value.arr.first;
            while (crsr) {
                ctx->auth_tenants[i++] = var_get_uuid_forkey (crsr, "id");
                const char *strid = var_get_str_forkey (crsr, "id");
                const char *tname = var_get_str_forkey (crsr, "name");
                if (strid && tname) {
                    var_set_str_forkey (ctx->auth_data, strid, tname);
                }
                crsr = crsr->next;
            }
            if (i) {
                retcode = 1;
                ctx->userlevel = AUTH_USER;
            }
        }
        var_free (res);
    }
                   
    log_info ("%s [AUTH_TOKEN] %03i FWD %s %s <%c%c...>",
              ctx->remote, retcode ? 200 : 403, url,
              retcode?"ACCEPT":"REJECT",
              ctx->external_token[0], ctx->external_token[1]);
    
    free (url);
    var_free (hdr);
    var_free (data);
    return retcode;;
}

/*/ ======================================================================= /*/
/** Cache for looking up services in the unithost registry */
/*/ ======================================================================= /*/
static var *UNITHOST_CACHE = NULL;

/*/ ======================================================================= /*/
/** Resolves base url for a unithost service by name from unithost-registry.
  * \param registry_url Base URL for the unithost registry
  * \param svcname      The service name to look up
  * \return             Copied string with the resolved base URL, caller
  *                     should free() when done. */
/*/ ======================================================================= /*/
char *resolve_unithost_service (const char *registry_url, const char *svcname) {
    if (UNITHOST_CACHE) {
        const char *v = var_get_str_forkey (UNITHOST_CACHE, svcname);
        if (v) return strdup (v);
    }
    else {
        UNITHOST_CACHE = var_alloc();
    }
    
    char *url = (char *) malloc (strlen(registry_url)+48);
    sprintf (url, "%s/service", registry_url);
    var *hdr = var_alloc();
    var *data = var_alloc();
    var *res = http_call ("GET", url, hdr, data, NULL, NULL);
    if (! res) {
        log_error ("Error resolving service '%s' from unithost-registry at %s",
                   svcname, registry_url);
        var_free (hdr);
        var_free (data);
        return NULL;
    }
    
    var *svc = var_find_key (res, svcname);
    if (! svc) {
        log_error ("Could not find service '%s' in unithost-registry at %s",
                   svcname, registry_url);
        var_free (hdr);
        var_free (data);
        var_free (res);
        return NULL;
    }
    
    char *retval = strdup (var_get_str_forkey (svc, "external_url"));
    var_free (hdr);
    var_free (data);
    var_free (res);
    
    var_set_str_forkey (UNITHOST_CACHE, svcname, retval);
    return retval;
}

/*/ ======================================================================= /*/
/** Handle an X-Auth-Token header that should be passed to unithost */
/*/ ======================================================================= /*/
int handle_external_token_unithost (req_context *ctx) {
    if (! OPTIONS.unithost_url[0]) return 0;
    
    char *accurl;
    if (OPTIONS.unithost_account_url) {
        accurl = strdup (OPTIONS.unithost_account_url);
    }
    else {
        accurl = resolve_unithost_service (OPTIONS.unithost_url, "account");
    }
    if (! accurl) return 0;
    
    char *url = (char *) malloc (strlen(accurl)+40);
    sprintf (url, "%s/token", accurl);
    free (accurl);
    
    int retcode = 0;
    int i = 0;
    var *hdr = var_alloc();
    var_set_str_forkey (hdr, "X-Auth-Token", ctx->external_token);
    var *data = var_alloc();
    var *res = http_call ("GET", url, hdr, data, NULL, NULL);

    ctx->userlevel = AUTH_GUEST;

    if (ctx->auth_tenants) {
        free (ctx->auth_tenants);
        ctx->auth_tenants = NULL;
        ctx->auth_tenantcount = 0;
    }

    if (res) {
        if (var_find_key (res, "tenant")) {
            uuid u = var_get_uuid_forkey (res, "tenant");
            ctx->auth_tenantcount = 1;
            ctx->auth_tenants = malloc (sizeof (uuid));
            ctx->auth_tenants[0] = u;
            
            const char *strid = var_get_str_forkey (res, "tenant");
            var *acc = var_find_key (res, "account");
            if (acc) {
                const char *idname = var_get_str_forkey (acc, "company");
                if (! idname || (! idname[0])) {
                    idname = var_get_str_forkey (acc, "contact");
                }
                var_set_str_forkey (ctx->auth_data, strid, idname);
                retcode = 1;
                ctx->userlevel = AUTH_USER;
            }
        }
        else {
            ctx->auth_tenantcount = 0;
        }
            
        var *roles = var_get_array_forkey (res, "roles");
        if (roles) {
            if (var_contains_str (roles, "ADMIN")) {
                retcode = 1;
                ctx->userlevel = AUTH_ADMIN;
            }
            else if (var_contains_str (roles, "PROVISIONING")) {
                retcode = 1;
                ctx->userlevel = AUTH_PROV;
            }
        }

        var_free (res);
    }
    
    log_info ("%s [AUTH_TOKEN] %03i FWD %s %s (Token %c%c...)",
              ctx->remote, retcode ? 200 : 403, url,
              retcode?"ACCEPT":"REJECT",
              ctx->external_token[0], ctx->external_token[1]);
    
    free (url);
    var_free (hdr);
    var_free (data);
    return retcode;;
}

/*/ ======================================================================= /*/
/** Handle X-Auth-Token header */
/*/ ======================================================================= /*/
int handle_external_token (req_context *ctx) {
    if (! ctx->external_token) return 0;
    if (! ctx->external_token[0]) return 0;
    if (! ctx->external_token[1]) return 0;

    switch (OPTIONS.auth) {
        case AUTH_OPENSTACK:
            return handle_external_token_openstack (ctx);
        
        case AUTH_UNITHOST:
            return handle_external_token_unithost (ctx);
            
        default:
            return 0;
    }
}

/*/ ======================================================================= /*/
/** Add CORS headers. FIXME: be better about this. */
/*/ ======================================================================= /*/
int flt_add_cors (req_context *ctx, req_arg *a, var *out, int *status) {
    var_set_str_forkey (ctx->outhdr, "Content-type", "application/json");
    var_set_str_forkey (ctx->outhdr, "Access-Control-Allow-Origin","*");
    var_set_str_forkey (ctx->outhdr, "Access-Control-Allow-Methods",
                        "GET, POST, PUT, DELETE, OPTIONS, PATCH");
    var_set_str_forkey (ctx->outhdr, "Access-Control-Allow-Headers",
                        "Content-type, X-Auth-Token");
    if (ctx->method == REQ_OPTIONS) {
        *status = 200;
        return 1;
    }
    return 0;                  
}

/*/ ======================================================================= /*/
/** Filter that croaks when no valid token was set */
/*/ ======================================================================= /*/
int flt_check_validuser (req_context *ctx, req_arg *a,
                         var *out, int *status) {
    
    char uuidstr[40];
    
    if (uuidvalid (ctx->opticon_token)) {
        authsession *s = authsession_find (ctx->opticon_token);
        if (s) {
            ctx->userlevel = s->userlevel;
            ctx->auth_tenants = malloc (sizeof (uuid));
            ctx->auth_tenants[0] = s->tenant;
            ctx->auth_tenantcount = 1;
            return 0;
        }
        if (strcmp (OPTIONS.adminhost, ctx->remote) != 0) {
            log_info ("%s [AUTH_LOCAL] 403 LOCALDB REJECT (IP)", ctx->remote); 
            ctx->userlevel = AUTH_GUEST;
            return 0;
        }
        else if (uuidcmp (ctx->opticon_token, OPTIONS.admintoken)) {
            ctx->userlevel = AUTH_ADMIN;
        }
        else {
            uuid2str (ctx->opticon_token, uuidstr);
            log_info ("%s [AUTH_LOCAL] 403 LOCALDB REJECT (Token %c%c...)",
                      ctx->remote, uuidstr[0], uuidstr[1]);
            ctx->userlevel = AUTH_GUEST;
        }
    }
    else if (ctx->external_token) {
        tcache_node *cache = tokencache_lookup (ctx->external_token);
        if (cache) {
            if (ctx->auth_tenants) {
                free (ctx->auth_tenants);
                ctx->auth_tenants = NULL;
            }

            if (cache->tenantcount) {            
                ctx->auth_tenants = malloc (cache->tenantcount * sizeof(uuid));
                memcpy (ctx->auth_tenants, cache->tenantlist,
                        cache->tenantcount * sizeof(uuid));
            }
            ctx->auth_tenantcount = cache->tenantcount;
            ctx->userlevel = cache->userlevel;
            free (cache);
            if (ctx->userlevel == AUTH_GUEST) {
                return err_unauthorized (ctx, a, out, status);
            }
            return 0;
        }
        if (! handle_external_token (ctx)) {
            tokencache_store_invalid (ctx->external_token);
            return err_unauthorized (ctx, a, out, status);
        }
        tokencache_store_valid (ctx->external_token, ctx->auth_tenants,
                                ctx->auth_tenantcount, ctx->userlevel);
        
    }
    else { 
        return err_unauthorized (ctx, a, out, status);
    }
    return 0;
}

/*/ ======================================================================= /*/
/** Filter that croaks when no valid token was set, or the user behind
  * said token is a filthy peasant. */
/*/ ======================================================================= /*/
int flt_check_admin (req_context *ctx, req_arg *a, var *out, int *status) {
    if (ctx->userlevel != AUTH_ADMIN) {
        return err_not_allowed (ctx, a, out, status);
    }
    return 0;
}

/*/ ======================================================================= /*/
/** Filter that requires ADMIN or PROVISIONING role */
/*/ ======================================================================= /*/
int flt_check_prov (req_context *ctx, req_arg *a, var *out, int *status) {
    if ((ctx->userlevel != AUTH_ADMIN) && (ctx->userlevel != AUTH_PROV)) {
        return err_not_allowed (ctx, a, out, status);
    }
    return 0;
}

/*/ ======================================================================= /*/
/** Filter that extracts the tenantid argumnt from the url, and croaks
  * when it is invalid. */
/*/ ======================================================================= /*/
int flt_check_tenant (req_context *ctx, req_arg *a, var *out, int *status) {
    if (a->argc<1) return err_server_error (ctx, a, out, status);
    ctx->tenantid = mkuuid (a->argv[0]);
    if (! uuidvalid (ctx->tenantid)) { 
        return err_server_error (ctx, a, out, status);
    }
    
    /* Admin user is ok now */
    if (ctx->userlevel == AUTH_ADMIN) return 0;
    if (ctx->userlevel == AUTH_PROV) return 0;
    
    /* Other users need to match the tenant */
    for (int i=0; i<ctx->auth_tenantcount; ++i) {
        if (uuidcmp (ctx->auth_tenants[i], ctx->tenantid)) return 0;
    }
    
    return err_not_allowed (ctx, a, out, status);
}

/*/ ======================================================================= /*/
/** Filter that extracts the host uuid argument from a url. */
/*/ ======================================================================= /*/
int flt_check_host (req_context *ctx, req_arg *a, var *out, int *status) {
    if (a->argc<2) return err_server_error (ctx, a, out, status);
    ctx->hostid = mkuuid (a->argv[1]);
    if (! uuidvalid (ctx->hostid)) { 
        return err_server_error (ctx, a, out, status);
    }
    return 0;
}

/*/ ======================================================================= /*/
/** Set up all the url routing */
/*/ ======================================================================= /*/
void setup_matches (void) {
    #define _P_(xx,yy,zz) req_matchlist_add(&REQ_MATCHES,xx,yy,zz)
    #define _T_(xx,yy,zz) req_matchlist_add_text(&REQ_MATCHES,xx,yy,zz)

    _P_ ("/obligatory-dancing-bears", REQ_GET,    cmd_dancing_bears);
    _P_ ("*",                         REQ_ANY,    flt_add_cors);
    _P_ ("/login",                    REQ_POST,   cmd_login);
    _P_ ("*",                         REQ_ANY,    flt_check_validuser);
    _P_ ("/",                         REQ_GET,    cmd_list_tenants);
    _P_ ("/",                         REQ_ANY,    err_method_not_allowed);
    _P_ ("/token",                    REQ_GET,    cmd_token);
    _P_ ("/session",                  REQ_GET,    flt_check_admin);
    _P_ ("/session",                  REQ_GET,    cmd_list_sessions);
    _P_ ("/user/%s",                  REQ_UPDATE, cmd_user_set);
    _P_ ("/user/%s",                  REQ_DELETE, cmd_user_delete);
    _P_ ("/any*",                     REQ_ANY,    flt_check_admin);
    _P_ ("/any/host/overview",        REQ_GET,    cmd_host_any_overview);
    _T_ ("/any/host/%U",              REQ_GET,    cmd_host_any_get);
    _P_ ("/any/host/%U/tenant",       REQ_GET,    cmd_host_any_get_tenant);
    _P_ ("/%U*",                      REQ_ANY,    flt_check_tenant);
    _P_ ("/%U",                       REQ_GET,    cmd_tenant_get);
    _P_ ("/%U",                       REQ_POST,   cmd_tenant_create);
    _P_ ("/%U",                       REQ_PUT,    cmd_tenant_update);
    _P_ ("/%U",                       REQ_DELETE, flt_check_admin);
    _P_ ("/%U",                       REQ_DELETE, cmd_tenant_delete);
    _P_ ("/%U/summary",               REQ_GET,    cmd_tenant_get_summary);
    _P_ ("/%U/quota",                 REQ_GET,    cmd_tenant_get_quota);
    _P_ ("/%U/meta",                  REQ_GET,    cmd_tenant_get_meta);
    _P_ ("/%U/meta",                  REQ_UPDATE, cmd_tenant_set_meta);
    _P_ ("/%U/meta",                  REQ_ANY,    err_method_not_allowed);
    _P_ ("/%U/meter",                 REQ_GET,    cmd_tenant_list_meters);
    _P_ ("/%U/meter/%S",              REQ_UPDATE, cmd_tenant_set_meter);
    _P_ ("/%U/meter/%S/%S",           REQ_UPDATE, cmd_tenant_set_meter);
    _P_ ("/%U/meter/%S",              REQ_DELETE, cmd_tenant_delete_meter);
    _P_ ("/%U/meter/%S/%S",           REQ_DELETE, cmd_tenant_delete_meter);
    _P_ ("/%U/watcher",               REQ_GET,    cmd_tenant_list_watchers);
    _P_ ("/%U/watcher/%S",            REQ_UPDATE, cmd_tenant_set_watcher);
    _P_ ("/%U/watcher/%S/%S",         REQ_UPDATE, cmd_tenant_set_watcher);
    _P_ ("/%U/watcher/%S",            REQ_DELETE, cmd_tenant_delete_watcher);
    _P_ ("/%U/watcher/%S/%S",         REQ_DELETE, cmd_tenant_delete_watcher);
    _P_ ("/%U/host",                  REQ_GET,    cmd_tenant_list_hosts);
    _P_ ("/%U/host",                  REQ_ANY,    err_method_not_allowed);
    _P_ ("/%U/host/overview",         REQ_GET,    cmd_host_overview);
    _P_ ("/%U/host/%U*",              REQ_ANY,    flt_check_host);
    _T_ ("/%U/host/%U",               REQ_GET,    cmd_host_get);
    _P_ ("/%U/host/%U",               REQ_DELETE, cmd_host_remove);
    _P_ ("/%U/host/%U",               REQ_ANY,    err_method_not_allowed);
    _P_ ("/%U/host/%U/log",           REQ_GET,    cmd_host_get_log);
    _P_ ("/%U/host/%U/externaldata",  REQ_GET,    cmd_host_get_external);
    _P_ ("/%U/host/%U/watcher",       REQ_GET,    cmd_host_list_watchers);
    _P_ ("/%U/host/%U/watcher",       REQ_ANY,    err_method_not_allowed);
    _P_ ("/%U/host/%U/watcher/%S",    REQ_UPDATE, cmd_host_set_watcher);
    _P_ ("/%U/host/%U/watcher/%S/%S", REQ_UPDATE, cmd_host_set_watcher);
    _P_ ("/%U/host/%U/watcher/%S",    REQ_DELETE, cmd_host_delete_watcher);
    _P_ ("/%U/host/%U/watcher/%S/%S", REQ_DELETE, cmd_host_delete_watcher);
    _P_ ("/%U/host/%U/range/%T/%T",   REQ_GET,    cmd_host_get_range);
    _P_ ("/%U/host/%U/graph",         REQ_GET,    cmd_host_list_graphs);
    _P_ ("/%U/host/%U/graph/%S/%S/%i/%i",
                                      REQ_GET,    cmd_host_get_graph);
    _T_ ("/%U/host/%U/time/%T",       REQ_GET,    cmd_host_get_time);
    _P_ ("*",                         REQ_GET,    err_not_found);
    _P_ ("*",                         REQ_ANY,    err_method_not_allowed);
    #undef _P_
    #undef _T_
}

/*/ ======================================================================= /*/
/** Panic handler for microhttpd */
/*/ ======================================================================= /*/
static void panic_func (void *cls, const char *file, unsigned int line,
                        const char *reason) {
    log_error ("[httpd] Error %s:%u: %s", file, line, reason);
}

/*/ ======================================================================= /*/
/** Daemon runner. Basically kicks microhttpd in action, then sits
  * back and enjoys the show. */
/*/ ======================================================================= /*/
int daemon_main (int argc, const char *argv[]) {
    if (strcmp (OPTIONS.logpath, "@syslog") == 0) {
        log_open_syslog ("opticon-api", OPTIONS.loglevel);
    }
    else {
        log_open_file (OPTIONS.logpath, OPTIONS.loglevel);
    }
    
    log_info ("--- Opticon API ready for action ---");

    struct MHD_Daemon *daemon;
    unsigned int flags = MHD_USE_THREAD_PER_CONNECTION;
    MHD_set_panic_func (&panic_func, NULL);
    daemon = MHD_start_daemon (flags, OPTIONS.port, NULL, NULL,
                               &answer_to_connection, NULL,
                               MHD_OPTION_CONNECTION_LIMIT,
                               (unsigned int) 256,
                               MHD_OPTION_PER_IP_CONNECTION_LIMIT, 
                               (unsigned int) 64,
                               MHD_OPTION_END);
    if (OPTIONS.autoexit) {
        sleep (60);
        exit (0);
    }
    while (1) sleep (60);
}

/*/ ======================================================================= /*/
/** Set up foreground flag */
/*/ ======================================================================= /*/
int set_foreground (const char *i, const char *v) {
    OPTIONS.foreground = 1;
    return 1;
}

/*/ ======================================================================= /*/
/** Set up configuration file path */
/*/ ======================================================================= /*/
int set_confpath (const char *i, const char *v) {
    OPTIONS.confpath = v;
    return 1;
}

int set_mconfpath (const char *i, const char *v) {
    OPTIONS.mconfpath = v;
    return 1;
}

int set_autoexit (const char *i, const char *v) {
    OPTIONS.autoexit = 1;
    return 1;
}

int set_gconfpath (const char *i, const char *v) {
    OPTIONS.gconfpath = v;
    return 1;
}

/*/ ======================================================================= /*/
/** Set up pidfile path */
/*/ ======================================================================= /*/
int set_pidfile (const char *i, const char *v) {
    OPTIONS.pidfile = v;
    return 1;
}

/*/ ======================================================================= /*/
/** Set the logfile path, @syslog for logging to syslog */
/*/ ======================================================================= /*/
int set_logpath (const char *i, const char *v) {
    OPTIONS.logpath = v;
    return 1;
}

/*/ ======================================================================= /*/
/** Handle --loglevel */
/*/ ======================================================================= /*/
int set_loglevel (const char *i, const char *v) {
    if (strcmp (v, "CRIT") == 0) OPTIONS.loglevel = LOG_CRIT;
    else if (strcmp (v, "ERR") == 0) OPTIONS.loglevel = LOG_ERR;
    else if (strcmp (v, "WARNING") == 0) OPTIONS.loglevel = LOG_WARNING;
    else if (strcmp (v, "INFO") == 0) OPTIONS.loglevel = LOG_INFO;
    else if (strcmp (v, "DEBUG") == 0) OPTIONS.loglevel = LOG_DEBUG;
    else OPTIONS.loglevel = LOG_WARNING;
    return 1;
}

/*/ ======================================================================= /*/
/** Handle --admin-token */
/*/ ======================================================================= /*/
int conf_admin_token (const char *id, var *v, updatetype tp) {
    OPTIONS.admintoken = mkuuid (var_get_str (v));
    if (! uuidvalid (OPTIONS.admintoken)) {
        log_error ("Invalid admintoken");
        exit (1);
    }
    return 1;
}

/*/ ======================================================================= /*/
/** Handle the auth_method configuration */
/*/ ======================================================================= /*/
int conf_auth_method (const char *id, var *v, updatetype tp) {
    const char *mth = var_get_str (v);
    if (strcmp (mth, "internal") == 0) OPTIONS.auth = AUTH_INTERNAL;
    else if (strcmp (mth, "openstack") == 0) OPTIONS.auth = AUTH_OPENSTACK;
    else if (strcmp (mth, "unithost") == 0) OPTIONS.auth = AUTH_UNITHOST;
    else {
        log_error ("Invalid auth_method");
        exit (1);
    }
    return 1;
}

/*/ ======================================================================= /*/
/** Handle auth/admin_host config */
/*/ ======================================================================= /*/
int conf_admin_host (const char *id, var *v, updatetype tp) {
    OPTIONS.adminhost = var_get_str (v);
    return 1;
}

/*/ ======================================================================= /*/
/** Handle auth/keystone_url config */
/*/ ======================================================================= /*/
int conf_keystone_url (const char *id, var *v, updatetype tp) {
    OPTIONS.keystone_url = var_get_str (v);
    if (OPTIONS.auth == AUTH_UNSET) OPTIONS.auth = AUTH_OPENSTACK;
    return 1;
}

/*/ ======================================================================= /*/
/** Handle auth/unithost_url config */
/*/ ======================================================================= /*/
int conf_unithost_url (const char *id, var *v, updatetype tp) {
    char *url = strdup (var_get_str (v));
    if (url[0]) {
        char *e = &url[strlen(url)-1];
        if (*e == '/') *e = 0;
    }
    OPTIONS.unithost_url = url;
    if (OPTIONS.auth == AUTH_UNSET) OPTIONS.auth = AUTH_UNITHOST;
    return 1;
}

/*/ ======================================================================= /*/
/** Handle auth/unithost_account_url config */
/*/ ======================================================================= /*/
int conf_unithost_account_url (const char *id, var *v, updatetype tp) {
    char *url = strdup (var_get_str (v));
    if (url[0]) {
        char *e = &url[strlen(url)-1];
        if (*e == '/') *e = 0;
    }
    OPTIONS.unithost_account_url = url;
    return 1;
}

/*/ ======================================================================= /*/
/** Handle network/port config */
/*/ ======================================================================= /*/
int conf_port (const char *id, var *v, updatetype tp) {
    OPTIONS.port = var_get_int (v);
    return 1;
}

/*/ ======================================================================= /*/
/** Handle database/path config */
/*/ ======================================================================= /*/
int conf_dbpath (const char *id, var *v, updatetype tp) {
    OPTIONS.dbpath = var_get_str (v);
    return 1;
}

/*/ ======================================================================= /*/
/** Handle plugin/querytool config */
/*/ ======================================================================= /*/
int conf_querytool (const char *id, var *v, updatetype tp) {
    OPTIONS.external_querytool = var_get_str (v);
    return 1;
}

apioptions OPTIONS;

/*/ ======================================================================= /*/
/** Command line flags */
/*/ ======================================================================= /*/
cliopt CLIOPT[] = {
    {"--foreground","-f",OPT_FLAG,NULL,set_foreground},
    {"--autoexit","-x",OPT_FLAG,NULL,set_autoexit},
    {"--pidfile","-p",OPT_VALUE,
        "/var/run/opticon-api.pid", set_pidfile},
    {"--logfile","-l",OPT_VALUE, "@syslog", set_logpath},
    {"--loglevel","-L",OPT_VALUE, "INFO", set_loglevel},
    {"--config-path","-c",OPT_VALUE,
        "/etc/opticon/opticon-api.conf", set_confpath},
    {"--meter-config-path","-m",OPT_VALUE,
        "/etc/opticon/opticon-meter.conf", set_mconfpath},
    {"--graph-config-path","-g",OPT_VALUE,
        "/etc/opticon/opticon-graph.conf", set_gconfpath},
    {NULL,NULL,0,NULL,NULL}
};

/*/ ======================================================================= /*/
/** Application main. Handle command line flags, load configurations,
  * initialize, then kick off into daemon mode. */
/*/ ======================================================================= /*/
int main (int _argc, const char *_argv[]) {
    int argc = _argc;
    const char **argv = cliopt_dispatch (CLIOPT, _argv, &argc);
    if (! argv) return 1;
    
    OPTIONS.auth = AUTH_UNSET;
    OPTIONS.keystone_url = NULL;
    OPTIONS.unithost_url = NULL;
    OPTIONS.unithost_account_url = NULL;
    OPTIONS.external_querytool = NULL;

    opticonf_add_reaction ("network/port", conf_port);
    opticonf_add_reaction ("auth/admin_token", conf_admin_token);
    opticonf_add_reaction ("auth/admin_host", conf_admin_host);
    opticonf_add_reaction ("auth/auth_method", conf_auth_method);
    opticonf_add_reaction ("auth/keystone_url", conf_keystone_url);
    opticonf_add_reaction ("auth/unithost_url", conf_unithost_url);
    opticonf_add_reaction ("auth/unithost_account_url",
                           conf_unithost_account_url);
    opticonf_add_reaction ("database/path", conf_dbpath);
    opticonf_add_reaction ("plugin/querytool", conf_querytool);
    
    OPTIONS.conf = var_alloc();
    if (! var_load_json (OPTIONS.conf, OPTIONS.confpath)) {
        log_error ("Error loading %s: %s\n",
                   OPTIONS.confpath, parse_error());
        return 1;
    }
    
    OPTIONS.gconf = get_default_graphs();
    if (! var_load_json (OPTIONS.gconf, OPTIONS.gconfpath)) {
        log_warn ("Error loading graph config %s: %s",
                  OPTIONS.gconfpath, parse_error());
    }

    OPTIONS.mconf = get_default_meterdef();
    if (! var_load_json (OPTIONS.mconf, OPTIONS.mconfpath)) {
        log_error ("Error loading %s: %s\n",
                   OPTIONS.mconfpath, parse_error());
        return 1;
    }
    
    opticonf_handle_config (OPTIONS.conf);
    tokencache_init();
    setup_matches();
    authsession_init();

    if (! daemonize (OPTIONS.pidfile, argc, argv, daemon_main,
                     OPTIONS.foreground)) {
        log_error ("Error spawning");
        return 1;
    }
}
