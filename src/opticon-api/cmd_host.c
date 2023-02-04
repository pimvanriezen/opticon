#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <libopticon/util.h>
#include <libopticon/datatypes.h>
#include <libopticon/codec_json.h>
#include <libopticon/var_dump.h>
#include <libopticon/hash.h>
#include <libopticon/timestr.h>
#include <microhttpd.h>
#include "req_context.h"
#include "cmd.h"
#include "options.h"
#include "externaldata.h"

static codec *JSONCODEC = NULL;

/** GET /$TENANT/host/overview */
int cmd_host_overview (req_context *ctx, req_arg *a,
                       var *env, int *status) {
    
    db *DB = localdb_create (OPTIONS.dbpath);
    if (! db_open (DB, ctx->tenantid, NULL)) {
        db_free (DB);
        return err_not_found (ctx, a, env, status);
    }
    
    log_info ("[overview] external_querytool: %s", OPTIONS.external_querytool);
    
    var *res = db_get_overview (DB);
    if (! res) res = var_alloc();
    else if (OPTIONS.external_querytool) {
        var *ov = var_find_key (res, "overview");
        if (! ov) log_info ("[overview] no overview in res");
        var *crsr = NULL;
        if (ov) crsr = var_first (ov);
        var *outenv = var_alloc();
        if (ctx->external_token) {
            var_set_str_forkey (outenv, "token", ctx->external_token);
        }
        while (crsr) {
            uuid hostid = mkuuid (crsr->id);
            var *extra = extdata_get (ctx->tenantid, hostid, outenv);
            if (extra) {
                var_link_as (extra, crsr, "external");
            }
            crsr = crsr->next;
        }
        var_free (outenv);
    }
    var_copy (env, res);
    var_free (res);
    db_free (DB);
    *status = 200;
    return 1;
}

uuid cmd_host_any_resolve (req_context *ctx, const char *hostuuidstr) {
    db *DB = localdb_create (OPTIONS.dbpath);
    ctx->hostid = mkuuid (hostuuidstr);
    
    var *cache = db_get_global (DB, "hosts.tenants");
    if (cache) {
        var *entry = var_find_key (cache, hostuuidstr);
        if (entry) {
            uuid res = ctx->tenantid = var_get_uuid (entry);
            var_free (cache);
            db_free (DB);
            return res;
        }
    }
    else cache = var_alloc();
    
    int uuid_cnt = 0;
    uuid *uuid_list = db_list_tenants (DB, &uuid_cnt);
    for (int i=0; i<uuid_cnt; ++i) {
        if (db_open (DB, uuid_list[i], NULL)) {
            if (db_host_exists (DB, ctx->hostid)) {
                uuid res = ctx->tenantid = uuid_list[i];
                var_set_uuid_forkey (cache, hostuuidstr, ctx->tenantid);
                db_set_global (DB, "hosts.tenants", cache);
                var_free (cache);
                db_free (DB);
                free (uuid_list);
                return res;
            }
        }
    }
    var_free (cache);
    db_free (DB);
    free (uuid_list);
    return uuidnil();
}

/** GET /any/host/overview */
int cmd_host_any_overview (req_context *ctx, req_arg *a,
                           var *env, int *status) {
    int uuidcount = 0;
    db *DB = localdb_create (OPTIONS.dbpath);
    uuid *uuids = db_list_tenants (DB, &uuidcount);
    
    var *v_overview = var_get_dict_forkey (env, "overview");
    
    for (int c = 0; c<uuidcount; ++c) {
        if (db_open (DB, uuids[c], NULL)) {
            var *res = db_get_overview (DB);
            if (! res) continue;
            
            var *ov = var_get_dict_forkey (res, "overview");
            var *crsr = NULL;
            
            if (ov->type == VAR_DICT) {
                crsr = ov->value.arr.first;
            }
            
            while (crsr) {
                var_set_uuid_forkey (crsr, "tenant", uuids[c]);
                crsr = crsr->next;
            }
            
            var_merge (v_overview, ov);
            var_free (res);
        }
    }
    
    db_free (DB);
    *status = 200;
    return 1;
}

/** GET /any/host/$HOST */
int cmd_host_any_get (req_context *ctx, req_arg *a,
                      ioport *outio, int *status) {
    cmd_host_any_resolve (ctx, a->argv[0]);
    return cmd_host_get (ctx, a, outio, status);
}

/** GET /any/host/$HOST/tenant */
int cmd_host_any_get_tenant (req_context *ctx, req_arg *a,
                             var *env, int *status) {
    uuid tenant = cmd_host_any_resolve (ctx, a->argv[0]);
    var_set_uuid_forkey (env, "tenant", tenant);
    *status = 200;
    return 1;
}

/** GET /$TENANT/host/$HOST */
int cmd_host_get (req_context *ctx, req_arg *a, ioport *outio, int *status) {
    db *DB = localdb_create (OPTIONS.dbpath);
    var *err;
    
    if (! db_open (DB, ctx->tenantid, NULL)) {
        db_free (DB);
        *status = 404;
        err = var_alloc();
        var_set_str_forkey (err, "error", "Tenant not found");
        var_write (err, outio);
        var_free (err);
        *status = 404;
        return 1;
    }
    if (!JSONCODEC) JSONCODEC= codec_create_json();
    host *h = host_alloc();
    
    h->uuid = ctx->hostid;
    if (db_get_current (DB, h)) {
        codec_encode_host (JSONCODEC, outio, h);
        *status = 200;
        db_free (DB);
        host_delete (h);
        return 1;
    }
    
    host_delete (h);
    db_free (DB);
    
    err = var_alloc();
    var_set_str_forkey (err, "error", "No current record found for host");
    var_write (err, outio);
    var_free (err);
    *status = 404;
    return 1;
}

/** DELETE /$TENANT/host/$HOST */
int cmd_host_remove (req_context *ctx, req_arg *a, var *env, int *status) {
    db *DB = localdb_create (OPTIONS.dbpath);
    if (! db_open (DB, ctx->tenantid, NULL)) {
        db_free (DB);
        return 0;
    }
    
    if (db_remove_host (DB, ctx->hostid)) {
        var *env_host = var_get_dict_forkey (env, "host");
        var_set_uuid_forkey (env_host, "deleted", ctx->hostid);
        db_free (DB);
        *status = 200;
        return 1;
    }
    
    *status = 400;
    return err_generic (env, "Could not delete host");
}

/** GET /$TENANT/host/$HOST/log */
int cmd_host_get_log (req_context *ctx, req_arg *a, var *env, int *status) {
    db *DB = localdb_create (OPTIONS.dbpath);
    if (! db_open (DB, ctx->tenantid, NULL)) {
        db_free (DB);
        return err_not_found (ctx, a, env, status);
    }
    
    var *log = db_get_log (DB, ctx->hostid);
    var *env_log = var_get_array_forkey (env, "log");
    if (log) {
        var_copy (env_log, log);
        var_free (log);
    }
    db_free (DB);

    *status = 200;
    return 1;
}

/** GET /$TENANT/host/$HOST/watcher */
int cmd_host_list_watchers (req_context *ctx, req_arg *a, 
                            var *env, int *status) {
    var *res = collect_meterdefs (ctx->tenantid, ctx->hostid, 1);
    if (! res) { return err_not_found (ctx, a, env, status); }
    
    var_copy (env, res);
    var_free (res);
    *status = 200;
    return 1;
}

/** POST /$TENANT/host/$HOST/watcher/$METERID 
  * watcher {
  *     warning { val: x, weight: 1.0 } # optional
  *     alert { val: x, weight: 1.0 } # optional
  *     critical { val: x, weight: 1.0 } #optional
  * }
  */
int cmd_host_set_watcher (req_context *ctx, req_arg *a, 
                          var *env, int *status) {
    if (a->argc < 3) return err_server_error (ctx, a, env, status);

    char meterid[16];
    if (! set_meterid (meterid, a, 2)) err_bad_request (ctx, a, env, status);

    db *DB = localdb_create (OPTIONS.dbpath);
    if (! db_open (DB, ctx->tenantid, NULL)) {
        db_free (DB);
        return 0;
    }
    
    var *def = collect_meterdefs (ctx->tenantid, uuidnil(), 0);
    var *def_meters = var_get_dict_forkey (def, "meter");
    var *def_meter = var_get_dict_forkey (def_meters, meterid);
    var *hmeta = db_get_hostmeta (DB, ctx->hostid);
    var *hmeta_meters = var_get_dict_forkey (hmeta, "meter");
    var *hmeta_meter = var_get_dict_forkey (hmeta_meters, meterid);
    
    /* Copy the type from parent information */
    const char *tp = var_get_str_forkey (def_meter, "type");
    if (! tp) tp = "integer";
    else if (strcmp (tp, "table") == 0) {
        db_free (DB);
        var_free (def);
        var_free (hmeta);
        *status = 400;
        return err_generic (env, "Cannot set watcher for table root");
    }
    
    var_set_str_forkey (hmeta_meter, "type", tp);
    
    var *in_watcher = var_get_dict_forkey (ctx->bodyjson, "watcher");
    
    const char *triggers[3] = {"warning","alert","critical"};
    
    /* Go over each trigger level, only copy what's actually there.
       Also only copy values relevant to a watcher */
    for (int i=0;i<3;++i) {
        var *tvar;
        tvar = var_find_key (in_watcher, triggers[i]);
        if (tvar) {
            var *w = var_get_dict_forkey (hmeta_meter, triggers[i]);
            var *nval = var_find_key (tvar, "val");
            if (! nval) {
                var_free (def);
                var_free (hmeta);
                db_free (DB);
                return err_bad_request (ctx, a, env, status);
            }
            var *wval = var_find_key (w, "val");
            if (! wval) {
                wval = var_alloc();
                strcpy (wval->id, "val");
                wval->hashcode = hash_token ("val");
                var_copy (wval, nval);
                var_link (wval, w);
            }
            else var_copy (wval, nval);
            
            /* Copy weight or set default */
            double weight = var_get_double_forkey (tvar, "weight");
            if (weight < 0.0001) weight = 1.0;
            var_set_double_forkey (wval, "weight", weight);
        }
    }
    
    var_dump (hmeta, stdout);
    
    db_set_hostmeta (DB, ctx->hostid, hmeta);
    var *envwatcher = var_get_dict_forkey (env, "watcher");
    var_copy (envwatcher, hmeta_meter);
    db_free (DB);
    var_free (def);
    var_free (hmeta);
    *status = 200;
    return 1;
}

/** DELETE /$TENANT/host/$HOST/watcher/$METERID */
int cmd_host_delete_watcher (req_context *ctx, req_arg *a, 
                             var *env, int *status) {
    if (a->argc < 3) return err_server_error (ctx, a, env, status);

    char meterid[16];
    if (! set_meterid (meterid, a, 2)) err_bad_request (ctx, a, env, status);

    db *DB = localdb_create (OPTIONS.dbpath);
    if (! db_open (DB, ctx->tenantid, NULL)) {
        db_free (DB);
        return 0;
    }

    var *hmeta = db_get_hostmeta (DB, ctx->hostid);
    var *hmeta_meters = var_get_dict_forkey (hmeta, "meter");
    var *hmeta_meter = var_get_dict_forkey (hmeta_meters, meterid);

    var_delete_key (hmeta_meter, "warning");
    var_delete_key (hmeta_meter, "alert");
    var_delete_key (hmeta_meter, "critical");
    
    db_set_hostmeta (DB, ctx->hostid, hmeta);
    var_free (hmeta);
    db_free (DB);
    var *env_meter = var_get_dict_forkey (env, "meter");
    var_set_str_forkey (env_meter, "deleted", meterid);
    *status = 200;
    return 1;
}

int cmd_host_get_range (req_context *ctx, req_arg *a, 
                        var *env, int *status) {
    return err_server_error (ctx, a, env, status);
}

int cmd_host_get_time (req_context *ctx, req_arg *a, 
                        ioport *outio, int *status) {

    db *DB = localdb_create (OPTIONS.dbpath);
    if (! db_open (DB, ctx->tenantid, NULL)) {
        db_free (DB);
        return 0;
    }
    if (!JSONCODEC) JSONCODEC = codec_create_json();
    host *h = host_alloc();
    time_t tnow = parse_timestr (a->argv[2]);
    
    h->uuid = ctx->hostid;
    if (db_get_record (DB, tnow, h)) {
        codec_encode_host (JSONCODEC, outio, h);
        *status = 200;
        db_free (DB);
        host_delete (h);
        return 1;
    }
    
    host_delete (h);
    db_free (DB);
    var *err = var_alloc();
    var_set_str_forkey (err, "error", "Record not found");
    var_write (err, outio);
    var_free (err);
    *status = 404;
    return 1;
}

int cmd_list_sessions (req_context *ctx, req_arg *a, var *env, int *status) {
    db *DB = localdb_create (OPTIONS.dbpath);
    var *list = db_get_global (DB, "sessions");
    var *list_s = var_get_array_forkey (list, "session");
    var *crsr = list_s->value.arr.first;
    while (crsr) {
        /* Don't export the AES key */
        var_delete_key (crsr, "key");
        crsr = crsr->next;
    }
    var_copy (env, list);
    var_free (list);
    *status = 200;
    return 1;
}

void add_graphdefs (var *into, var *from) {
    if (from->type != VAR_DICT) return;
    var *fcrsr = from->value.arr.first;
    while (fcrsr) {
        const char *graph_id = var_get_str_forkey (fcrsr, "graph");
        const char *datum_id = var_get_str_forkey (fcrsr, "datum");
        if (graph_id && datum_id) {
            var *crsr = var_get_dict_forkey (into, graph_id);
            crsr = var_get_dict_forkey (crsr, datum_id);
            var_copy (crsr, fcrsr);
            var_set_str_forkey (crsr, "meter", fcrsr->id);
        }
        fcrsr = fcrsr->next;
    }
}

/** Gets graph definitions out of (default) config and host metadata */
var *collect_graphdefs (uuid tenant, uuid host) {
    var *res = var_alloc();
    add_graphdefs (res, OPTIONS.gconf);
    /* FIXME: Add host meters */
    return res;
}

/** GET /$TENANT/host/$HOST/graph */
int cmd_host_list_graphs (req_context *ctx, req_arg *a, var *env,
                          int *status) {
    var *res = collect_graphdefs (ctx->tenantid, ctx->hostid);
    if (! res) {
        *status = 500;
        return err_generic (env, "Could not collect graphs");
    }
    var_link_as (res, env, "graph");
    *status = 200;
    return 1;    
}

/** GET /$TENANT/host/$HOST/graph/$graph/$datum/$timespan/$numsamples */
int cmd_host_get_graph (req_context *ctx, req_arg *a, var *env,
                        int *status) {
    if (a->argc < 6) {
        *status = 500;
        return err_generic (env, "Invalid argument list");
    }
    
    var *def = collect_graphdefs (ctx->tenantid, ctx->hostid);
    
    const char *arg_graph_id = a->argv[2];
    const char *arg_datum_id = a->argv[3];
    int timespan = atoi (a->argv[4]);
    int numsamples = atoi (a->argv[5]);
    
    if ( (! numsamples) ||
         (! timespan) ||
         (timespan/numsamples < 300) ) {
        *status = 400;
        var_free (def);
        return err_generic (env, "Invalid specification");
    }

    var *graph = var_find_key (def, arg_graph_id);
    if (graph == NULL || graph->type != VAR_DICT) {
        *status = 404;
        var_free (def);
        return err_generic (env, "Graph not found");
    }
    var *datum = var_find_key (graph, arg_datum_id);
    if (datum == NULL || datum->type != VAR_DICT) {
        *status = 404;
        var_free (def);
        return err_generic (env, "Datum not found");
    }
    
    double max = var_get_double_forkey (datum, "max");
    const char *title = var_get_str_forkey (datum, "title");
    const char *unit = var_get_str_forkey (datum, "unit");
    const char *color = var_get_str_forkey (datum, "color");
    const char *meter = var_get_str_forkey (datum, "meter");
    
    db *DB = localdb_create (OPTIONS.dbpath);
    if (! db_open (DB, ctx->tenantid, NULL)) {
        db_free (DB);
        var_free (def);
        *status = 500;
        return err_generic (env, "Could not open database for tenant");
    }
    
    double *data = db_get_graph (DB, ctx->hostid, arg_graph_id,
                                 arg_datum_id, timespan, numsamples);
    if (! data) {
        db_free (DB);
        var_free (def);
        *status = 500;
        return err_generic (env, "Error getting graph data");
    }
    
    var *arr = var_get_array_forkey (env, "data");
    for (int i=0; i<numsamples;++i) {
        double d = data[i];
        if (d > max) max = d;
        var_add_double (arr, d);
    }
    
    var_set_double_forkey (env, "max", max);
    if (title) var_set_str_forkey (env, "title", title);
    if (unit) var_set_str_forkey (env, "unit", unit);
    if (color) var_set_str_forkey (env, "color", color);
    if (meter) var_set_str_forkey (env, "meter", meter);
    
    free (data);
    db_free (DB);
    var_free (def);
    
    *status = 200;
    return 1;
}

int cmd_dancing_bears (req_context *ctx, req_arg *a, var *env, int *status) {
    const char *B = "    _--_     _--_    _--_     _--_     "
                    "_--_     _--_     _--_     _--_\n"
                    "   (    )~~~(    )  (    )~~~(    )   ("
                    "    )~~~(    )   (    )~~~(    )\n"
                    "    \\           /    \\           /   "
                    "  \\           /     \\           /\n"
                    "     (  ' _ `  )      (  ' _ `  )      "
                    " (  ' _ `  )       (  ' _ `  )\n"
                    "      \\       /        \\       /     "
                    "    \\       /         \\       /\n"
                    "    .__( `-' )          ( `-' )        "
                    "   ( `-' )        .__( `-' )  ___\n"
                    "   / !  `---' \\      _--'`---_        "
                    "  .--`---'\\       /   /`---'`-'   \\\n"
                    "  /  \\         !    /         \\___   "
                    "  /        _>\\    /   /          ._/   __\n"
                    " !   /\\        )   /   /       !  \\  "
                    " /  /-___-'   ) /'   /.-----\\___/     /  )\n"
                    " !   !_\\       ). (   <        !__/ /'"
                    "  (        _/  \\___//          `----'   !\n"
                    "  \\    \\       ! \\ \\   \\      /\\ "
                    "   \\___/`------' )       \\            ______/\n"
                    "   \\___/   )  /__/  \\--/   \\ /  \\  "
                    "._    \\      `<         `--_____----'\n"
                    "     \\    /   !       `.    )-   \\/  "
                    ") ___>-_     \\   /-\\    \\    /\n"
                    "     /   !   /         !   !  `.    / /"
                    "      `-_   `-/  /    !   !\n"
                    "    !   /__ /___       /  /__   \\__/ ("
                    "  \\---__/ `-_    /     /  /__\n"
                    "    (______)____)     (______)        \\"
                    "__)         `-_/     (______)\n";
    
    var_set_str_forkey (env, "bear", B);
    *status = 200;
    return 1;
}

