#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <libopticon/util.h>
#include <libopticon/datatypes.h>
#include <libopticon/defaultmeters.h>
#include <libopticon/aes.h>
#include <libopticon/var_dump.h>
#include <libopticon/var_parse.h>
#include <libopticon/pwcrypt.h>
#include <microhttpd.h>
#include "req_context.h"
#include "cmd.h"
#include "options.h"
#include "req_context.h"
#include "authsession.h"

var *check_password (const char *u, const char *p) {
    char pwfile[1024];
    sprintf (pwfile, "%s/user.db", OPTIONS.dbpath);
    var *pwdb = var_alloc();
    if (var_load_json (pwdb, pwfile)) {
        var *user = var_get_dict_forkey (pwdb, u);
        const char *pwhash = var_get_str_forkey (user, "passwd");
        if (pwhash) {
            char *cr = pwcrypt (p, pwhash);
            if (! strcmp (cr, pwhash)) {
                var *res = var_clone (user);
                var_free (pwdb);
                free (cr);
                return res;
            }
            free (cr);
        }
    }
    var_free (pwdb);
    return NULL;
}

int cmd_login (req_context *ctx, req_arg *a, var *env, int *status) {
    if (OPTIONS.auth != AUTH_INTERNAL) {
        var_set_str_forkey (env, "error", "External authentication required");
        *status = 400;
        return 1;
    }
    
    if (! var_find_key (ctx->bodyjson, "username")) {
        return err_bad_request (ctx, a, env, status);
    }
    if (! var_find_key (ctx->bodyjson, "password")) {
        return err_bad_request (ctx, a, env, status);
    }
    
    const char *username = var_get_str_forkey (ctx->bodyjson, "username");
    const char *password = var_get_str_forkey (ctx->bodyjson, "password");
    
    var *user = check_password (username, password);
    if (user) {
        uuid tenantid = var_get_uuid_forkey (user, "tenant");
        const char *level = var_get_str_forkey (user, "level");
        auth_level userlevel = AUTH_GUEST;
        if (! level) level = "USER";
        if (strcmp (level, "USER") == 0) userlevel = AUTH_USER;
        else if (strcmp (level, "ADMIN") == 0) userlevel = AUTH_ADMIN;
        
        uuid sessionid = authsession_create (tenantid, userlevel);
        var_set_uuid_forkey (env, "token", sessionid);
        *status = 200;
        return 1;
    }
    
    var_set_str_forkey (env, "error", "Login failed");
    *status = 403;
    return 1;
}

int cmd_set_user (req_context *ctx, req_arg *a, var *env, int *status) {
    if (OPTIONS.auth != AUTH_INTERNAL) {
        var_set_str_forkey (env, "error", "External authentication configured");
        *status = 400;
        return 1;
    }
    
    if (a->argc < 1) {
        return err_server_error (ctx, a, env, status);
    }
    
    
    char pwfile[1024];
    sprintf (pwfile, "%s/user.db", OPTIONS.dbpath);
    
    const char *user = a->argv[0];
    uuid tenantid = var_get_uuid_forkey (ctx->bodyjson, "tenant");
    const char *level = var_get_str_forkey (ctx->bodyjson, "level");
    const char *plainpw = var_get_str_forkey (ctx->bodyjson, "password");

    if (ctx->userlevel != AUTH_ADMIN) {
        var_set_str_forkey (env, "error", "Admin required");
        *status = 403;
        return 1;
    }
    
    char salt[16];
    salt[0] = rand() & 255;
    salt[1] = rand() & 255;
    salt[2] = 0;
    
    char *passwd = pwcrypt (plainpw, salt);
    
    var *pwdb = var_alloc();
    if (var_load_json (pwdb, pwfile)) {
        var *outpw = var_get_dict_forkey (pwdb, user);
        var_set_uuid_forkey (outpw, "tenant", tenantid);
        var_set_str_forkey (outpw, "passwd", passwd);
        if (level) var_set_str_forkey (outpw, "level", level);
        char tmpfn[1024];
        sprintf (tmpfn, "%s.new", pwfile);
        FILE *fout = fopen (tmpfn, "w");
        if (! fout) {
            var_free (pwdb);
            return err_server_error (ctx, a, env, status);
        }
        var_dump (pwdb, fout);
        fclose (fout);
        rename (tmpfn, pwfile);
        var_free (pwdb);
        free (passwd);
        
        var_set_str_forkey (env, "result", "ok");
        *status = 200;
        return 1;
    }
    
    free (passwd);
    var_free (pwdb);
    var_set_str_forkey (env, "error", "Could not load user.db");
    *status = 500;
    return 1;
}
