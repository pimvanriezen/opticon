#include <stdio.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>

#include <libopticon/datatypes.h>
#include <libopticon/aes.h>
#include <libopticon/codec_json.h>
#include <libopticon/ioport_file.h>
#include <libopticon/util.h>
#include <libopticon/var.h>
#include <libopticon/dump.h>
#include <libopticon/defaultmeters.h>
#include <libopticondb/db_local.h>
#include <libhttp/http.h>
#include "api.h"
#include "cmd.h"

var *api_call (const char *mth, var *data, const char *fmt, ...)
{
    char path[1024];
    path[0] = 0;
    va_list ap;
    va_start (ap, fmt);
    vsnprintf (path, 1023, fmt, ap);
    va_end (ap);
    
    char tmpurl[1024];
    var *outhdr = var_alloc();
    var *errinfo = var_alloc();
    if (OPTIONS.keystone_token[0]) {
        var_set_str_forkey (outhdr, "X-Auth-Token", OPTIONS.keystone_token);
    }
    else if (OPTIONS.opticon_token[0]) {
        var_set_str_forkey (outhdr, "X-Opticon-Token", OPTIONS.opticon_token);
    }
    strcpy (tmpurl, OPTIONS.api_url);
    int len = strlen (tmpurl);
    if (! len) { var_free (outhdr); return NULL; }
    
    if (tmpurl[len-1] == '/') tmpurl[len-1] = 0;
    strncat (tmpurl, path, 1023);
    tmpurl[1023] = 0;
    var *res = http_call (mth, tmpurl, outhdr, data, errinfo, NULL);
    if (! res) {
        const char *errstr = var_get_str_forkey (errinfo, "error");
        if (! errstr) errstr = "Unknown error";
        fprintf (stderr, "%% %s\n", errstr);
        exit (1);
    }
    var_free (outhdr);
    var_free (errinfo);
    return res;
}

var *api_get (const char *fmt, ...) {
    char path[1024];
    path[0] = 0;
    va_list ap;
    va_start (ap, fmt);
    vsnprintf (path, 1023, fmt, ap);
    va_end (ap);
    
    char tmpurl[1024];
    var *outhdr = var_alloc();
    var *errinfo = var_alloc();
    var *data = var_alloc();
    if (OPTIONS.keystone_token[0]) {
        var_set_str_forkey (outhdr, "X-Auth-Token", OPTIONS.keystone_token);
    }
    else if (OPTIONS.opticon_token[0]) {
        var_set_str_forkey (outhdr, "X-Opticon-Token", OPTIONS.opticon_token);
    }
    strcpy (tmpurl, OPTIONS.api_url);
    int len = strlen (tmpurl);
    if (! len) {
        var_free (outhdr);
        var_free (data);
        return NULL;
    }
    
    if (tmpurl[len-1] == '/') tmpurl[len-1] = 0;
    strncat (tmpurl, path, 1023);
    tmpurl[1023] = 0;
    var *res = http_call ("GET", tmpurl, outhdr, data, errinfo, NULL);
    if (! res) {
        const char *errstr = var_get_str_forkey (errinfo, "error");
        if (! errstr) errstr = "Unknown error";
        fprintf (stderr, "%% %s\n", errstr);
        exit (1);
    }
    var_free (outhdr);
    var_free (errinfo);
    var_free (data);
    return res;
}

