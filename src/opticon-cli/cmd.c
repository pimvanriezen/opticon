#include <stdio.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include <libopticon/datatypes.h>
#include <libopticon/aes.h>
#include <libopticon/codec_json.h>
#include <libopticon/ioport_file.h>
#include <libopticon/util.h>
#include <libopticon/var.h>
#include <libopticon/var_dump.h>
#include <libopticon/defaultmeters.h>
#include <libhttp/http.h>

#include "cmd.h"
#include "api.h"
#include "prettyprint.h"
#include "term.h"
#include "readpass.h"

/*/ ======================================================================= /*/
/** The tenant-list command */
/*/ ======================================================================= /*/
int cmd_tenant_list (int argc, const char *argv[]) {
    var *res = api_get ("/");
    if (OPTIONS.json) {
        var_dump (res, stdout);
    }
    else {
        print_hdr("Tenants", rsrc(icns.people));
        term_printf (" UUID                                 Hosts  Name\n");

        var *res_tenant = var_get_array_forkey (res, "tenant");
        if (var_get_count (res_tenant)) {
            var *crsr = res_tenant->value.arr.first;
            while (crsr) {
                term_printf ("<g> %s</> %5" PRIu64 "<y>  %s</>\n",
                             var_get_str_forkey (crsr, "id"),
                             var_get_int_forkey (crsr, "hostcount"),
                             var_get_str_forkey (crsr, "name"));
                crsr = crsr->next;
            }
        }
        print_line();
    }
    var_free (res);
    return 0;
}

/*/ ======================================================================= /*/
/** The tenant-get-metadata command */
/*/ ======================================================================= /*/
int cmd_tenant_get_metadata (int argc, const char *argv[]) {
    if (OPTIONS.tenant[0] == 0) {
        fprintf (stderr, "%% No tenantid provided\n");
        return 1;
    }

    var *meta = api_get ("/%s/meta", OPTIONS.tenant);
    var_dump (meta, stdout);
    var_free (meta);
    return 0;
}

/*/ ======================================================================= /*/
/** The tenant-set-metadata command */
/*/ ======================================================================= /*/
int cmd_tenant_set_metadata (int argc, const char *argv[]) {
    if (argc < 4) {
        fprintf (stderr, "%% Missing key and value arguments\n");
        return 1;
    }
    
    const char *key = argv[2];
    const char *val = argv[3];
    
    if (OPTIONS.tenant[0] == 0) {
        fprintf (stderr, "%% No tenantid provided\n");
        return 1;
    }

    var *api_meta = api_get ("/%s/meta", OPTIONS.tenant);
    var *meta = var_get_dict_forkey (api_meta, "metadata");
    var_set_str_forkey (meta, key, val);
    
    var *res = api_call ("POST", api_meta, "/%s/meta", OPTIONS.tenant);
    var_free (api_meta);
    var_free (res);
    return 0;
}

/*/ ======================================================================= /*/
/** The tenant-set-quota command */
/*/ ======================================================================= /*/
int cmd_tenant_set_quota (int argc, const char *argv[]) {
    if (argc < 3) {
        fprintf (stderr, "%% Missing quota value\n");
        return 1;
    }
    
    uint64_t nquota = strtoull (argv[2], NULL, 10);
    if (! nquota) {
        fprintf (stderr, "%% Illegal quota value\n");
        return 1;
    }
    
    var *api_root = var_alloc();
    var *api_meta = var_get_dict_forkey (api_root, "tenant");
    var_set_int_forkey (api_meta, "quota", nquota);
    var *res = api_call ("PUT", api_root, "/%s", OPTIONS.tenant);
    var_free (api_root);
    var_free (res);
    return 0;
}

/*/ ======================================================================= /*/
/** The tenant-get-quota command */
/*/ ======================================================================= /*/
int cmd_tenant_get_quota (int argc, const char *argv[]) {
    if (argc < 2) {
        fprintf (stderr, "%% Syntax error");
        return 1;
    }
    
    print_hdr ("Quota", rsrc(icns.disks));
    var *meta = api_get ("/%s/quota", OPTIONS.tenant);
    if (! meta) return 1;
    term_printf ("Tenant storage quota: <b>%" PRIu64 "</> MB\n",
                 var_get_int_forkey (meta, "quota"));
    term_printf ("Current usage: <b>%.2f</> %%\n",
                 var_get_double_forkey (meta, "usage"));
    var_free (meta);
    return 0;
}

/*/ ======================================================================= /*/
/** The tenant-get-summary command */
/*/ ======================================================================= /*/
int cmd_tenant_get_summary (int argc, const char *argv[]) {
    if (OPTIONS.tenant[0] == 0) {
        fprintf (stderr, "%% No tenantid provided\n");
        return 1;
    }

    var *summ = api_get ("/%s/summary", OPTIONS.tenant);
    var_dump (summ, stdout);
    var_free (summ);
    return 0;
}

/*/ ======================================================================= /*/
/** The host-overview command */
/*/ ======================================================================= /*/
int cmd_host_overview (int argc, const char *argv[]) {
    if (OPTIONS.tenant[0] == 0) {
        fprintf (stderr, "%% No tenantid provided\n");
        return 1;
    }

    var *ov = api_get ("/%s/host/overview", OPTIONS.tenant);
    if (! var_get_count (ov)) return 0;

    const char *diskiofield = NULL;
    const char *addrfield = NULL;
    
    print_hdr ("Overview", rsrc(icns.overview));
    int namewidth=28;
    if (TERMINFO.cols > 80) {
        namewidth += (TERMINFO.cols-81);
        if (TERMINFO.cols > 100) {
            diskiofield = "DISK I/O";
            namewidth -= 9;
            
            if (TERMINFO.cols > 120) {
                addrfield = "IP";
                namewidth -= 31;
            }
        }
    }
    
    char namefmt[16];
    sprintf (namefmt, "%%-%is", namewidth);
    term_printf (" ");
    term_printf (namefmt, "NAME");
    term_printf (" ");
    
    if (addrfield) {
        term_printf ("%-30s ", addrfield);
    }
    
    term_printf (" STATUS   LOAD ");
    if (diskiofield) {
        term_printf ("%-9s", diskiofield);
    }
    
    term_printf ("  NET I/O      CPU      GRAPH\n");

    var *ov_dict = var_get_dict_forkey (ov, "overview");
    if (! var_get_count (ov_dict)) return 0;
    var *crsr = ov_dict->value.arr.first;
    while (crsr) {
        const char *hname = var_get_str_forkey (crsr, "hostname");
        if ((! hname) || (! *hname)) hname = crsr->id;
        char shortname[32];
        strncpy (shortname, hname, 31);
        shortname[31] = 0;
        const char *hstat = var_get_str_forkey (crsr, "status");
        if (! hstat ) hstat = "UNSET";
        double load = var_get_double_forkey (crsr, "loadavg");
        uint64_t netio = var_get_int_forkey (crsr, "net/in_kbs");
        netio += var_get_int_forkey (crsr, "net/out_kbs");
        
        uint64_t diskio = var_get_int_forkey (crsr, "io/rdops");
        diskio += var_get_int_forkey (crsr, "io/wrops");
        
        const char *ip = var_get_str_forkey (crsr, "agent/ip");
        double cpu = var_get_double_forkey (crsr, "pcpu");
        int rcpu = (cpu+5.0) / 10;
        term_printf ("<y> ");
        term_printf (namefmt, shortname);
        term_printf ("</> ");
        if (addrfield) term_printf ("%-30s ", ip);
        term_printf ("%s %6.2f ", decorate_status(hstat), load);
        if (diskiofield) {
            term_printf ("%8" PRIu64 " ", diskio);
        }
        term_printf (" %8" PRIu64 " <b>%6.2f</> %% -|", netio, cpu);
        print_bar (12, 100, cpu);
        term_printf ("|+\n");
        crsr = crsr->next;
    }
    var_free (ov);
    print_line();
    return 0;
}    
    
    
/*/ ======================================================================= /*/
/** The meter-create command */
/*/ ======================================================================= /*/
int cmd_meter_create (int argc, const char *argv[]) {
    if (OPTIONS.tenant[0] == 0) {
        fprintf (stderr, "%% No tenantid provided\n");
        return 1;
    }
    if (OPTIONS.meter[0] == 0) {
        fprintf (stderr, "%% No meter provided\n");
        return 1;
    }

    var *req = var_alloc();
    var *req_meter = var_get_dict_forkey (req, "meter");
    var_set_str_forkey (req_meter, "type", OPTIONS.type);
    if (OPTIONS.description[0]) {
        var_set_str_forkey (req_meter, "description", OPTIONS.description);
    }
    if (OPTIONS.unit[0]) {
        var_set_str_forkey (req_meter, "unit", OPTIONS.unit);
    }
    
    var *apires = api_call ("POST",req,"/%s/meter/%s",
                            OPTIONS.tenant, OPTIONS.meter);

    var_free (req);
    var_free (apires);
    return 0;
}

/*/ ======================================================================= /*/
/** The meter-delete command */
/*/ ======================================================================= /*/
int cmd_meter_delete (int argc, const char *argv[]) {
    if (OPTIONS.tenant[0] == 0) {
        fprintf (stderr, "%% No tenantid provided\n");
        return 1;
    }
    if (OPTIONS.meter[0] == 0) {
        fprintf (stderr, "%% No meter provided\n");
        return 1;
    }
    
    var *p = var_alloc();
    var *apires = api_call ("DELETE", p, "/%s/meter/%s",
                            OPTIONS.tenant, OPTIONS.meter);
    var_free (p);
    var_free (apires);
    return 0;
}

/*/ ======================================================================= /*/
/** The meter-list command */
/*/ ======================================================================= /*/
int cmd_meter_list (int argc, const char *argv[]) {
    if (OPTIONS.tenant[0] == 0) {
        fprintf (stderr, "%% No tenantid provided\n");
        return 1;
    }
    
    var *apires = api_get ("/%s/meter", OPTIONS.tenant);
    if (OPTIONS.json) {
        var_dump (apires, stdout);
    }
    else {
        var *res_meter = var_get_dict_forkey (apires, "meter");
        if (var_get_count (res_meter)) {
            term_printf ("From     Meter        Type      "
                         "Unit    Description\n");
            print_line();
            var *crsr = res_meter->value.arr.first;
            while (crsr) {
                const char *desc = var_get_str_forkey (crsr, "description");
                const char *type = var_get_str_forkey (crsr, "type");
                const char *unit = var_get_str_forkey (crsr, "unit");
                const char *org = var_get_str_forkey (crsr, "origin");
                double max = var_get_double_forkey (crsr, "max");
            
                if (!desc) desc = "-";
                if (!unit) unit = "";
                if (!org) org = "default";
            
                term_printf ("%-8s %-12s %-8s  %-7s %s\n", org, crsr->id,
                        type, unit, desc);
                crsr = crsr->next;
            }
            print_line();
        }
    }
    var_free (apires);
    return 0;
}

/*/ ======================================================================= /*/
/** The watcher-set command */
/*/ ======================================================================= /*/
int cmd_watcher_set (int argc, const char *argv[]) {
    if (OPTIONS.tenant[0] == 0) {
        fprintf (stderr, "%% No tenantid provided\n");
        return 1;
    }
    if (OPTIONS.meter[0] == 0) {
        fprintf (stderr, "%% No meter provided\n");
        return 1;
    }
    if (OPTIONS.match[0] == 0) {
        fprintf (stderr, "%% No match provided\n");
        return 1;
    }
    if (OPTIONS.value[0] == 0) {
        fprintf (stderr, "%% No value provided\n");
        return 1;
    }
    
    var *mdef = api_get ("/%s/meter", OPTIONS.tenant);
    if (! mdef) return 1;
    
    var *mdef_m = var_get_dict_forkey (mdef, "meter");
    var *mde_meter = var_get_dict_forkey (mdef_m, OPTIONS.meter);
    const char *inftype = var_get_str_forkey (mde_meter, "type");
    
    if (! inftype) {
        fprintf (stderr, "%% Meter '%s' not found in tenant\n", OPTIONS.meter);
        return 1;
    }
    
    var *req = var_alloc();
    var *req_watcher = var_get_dict_forkey (req, "watcher");
    var *reql = var_get_dict_forkey (req_watcher, OPTIONS.level);
    
    if (OPTIONS.host[0] == 0) {
        var_set_str_forkey (reql, "cmp", OPTIONS.match);
    }
    
    if (strcmp (inftype, "integer") == 0) {
        var_set_int_forkey (reql, "val", strtoull (OPTIONS.value, NULL, 10));
    }
    else if (strcmp (inftype, "frac") == 0) {
        var_set_double_forkey (reql, "val", atof (OPTIONS.value));
    }
    else {
        var_set_str_forkey (reql, "val", OPTIONS.value);
    }
    
    var_set_double_forkey (reql, "weight", atof (OPTIONS.weight));
    
    var *apires;
    if (OPTIONS.host[0]) {
        apires = api_call ("POST", req, "/%s/host/%s/watcher/%s",
                           OPTIONS.tenant, OPTIONS.host, OPTIONS.meter);
    }
    else {
        apires = api_call ("POST", req, "/%s/watcher/%s",
                            OPTIONS.tenant, OPTIONS.meter);
    }
    var_free (mdef);
    var_free (req);
    var_free (apires);
    return 0;
}

/*/ ======================================================================= /*/
/** The watcher-delete command */
/*/ ======================================================================= /*/
int cmd_watcher_delete (int argc, const char *argv[]) {
    if (OPTIONS.tenant[0] == 0) {
        fprintf (stderr, "%% No tenantid provided\n");
        return 1;
    }
    if (OPTIONS.meter[0] == 0) {
        fprintf (stderr, "%% No meter provided\n");
        return 1;
    }
    
    var *req = var_alloc();
    var *apires;
    
    if (OPTIONS.host[0]) {
        apires = api_call ("DELETE", req, "/%s/host/%s/watcher/%s",
                           OPTIONS.tenant, OPTIONS.host, OPTIONS.meter);
    }
    else {
        apires = api_call ("DELETE", req, "/%s/watcher/%s",
                            OPTIONS.tenant, OPTIONS.meter);
    }
    var_free (req);
    var_free (apires);
    return 0;    
}

/*/ ======================================================================= /*/
/** Screen display function for watcher-related data */
/*/ ======================================================================= /*/
void print_data (const char *meterid, const char *trig, var *v) {
    const char *origin = var_get_str_forkey (v, "origin");
    if (! origin) origin = "default";
    char valst[64];
    var *val = var_find_key (v, "val");
    if (! val) return;
    
    switch (val->type) {
        case VAR_INT:
            sprintf (valst, "%" PRIu64, var_get_int (val));
            break;
        
        case VAR_DOUBLE:
            sprintf (valst, "%.2f", var_get_double (val));
            break;
        
        case VAR_STR:
            strncpy (valst, var_get_str (val), 63);
            valst[63] = 0;
            break;
        
        default:
            strcpy (valst, "<error %i>");
            break;
    }
    
    term_printf ("%-8s %-12s %-9s %-6s %21s %18.1f\n", origin, meterid,
                 trig, var_get_str_forkey (v, "cmp"), valst,
                 var_get_double_forkey (v, "weight"));
}

/*/ ======================================================================= /*/
/** The watcher-list command */
/*/ ======================================================================= /*/
int cmd_watcher_list (int argc, const char *argv[]) {
    if (OPTIONS.tenant[0] == 0) {
        fprintf (stderr, "%% No tenantid provided\n");
        return 1;
    }
    
    var *apires;
    
    if (OPTIONS.host[0]) {
        apires = api_get ("/%s/host/%s/watcher", OPTIONS.tenant, OPTIONS.host);
    }
    else {
        apires = api_get ("/%s/watcher", OPTIONS.tenant);
    }
    if (OPTIONS.json) {
        var_dump (apires, stdout);
        var_free (apires);
        return 0;
    }

    term_printf ("From     Meter        Trigger   Match                  "
                 "Value             Weight\n");
    print_line();

    var *apiwatch = var_get_dict_forkey (apires, "watcher");
    if (var_get_count (apiwatch)) {
        var *crsr = apiwatch->value.arr.first;
        while (crsr) {
            print_data (crsr->id, "warning",
                        var_get_dict_forkey (crsr, "warning"));
            print_data (crsr->id, "alert",
                        var_get_dict_forkey (crsr, "alert"));
            print_data (crsr->id, "critical",
                        var_get_dict_forkey (crsr, "critical"));
            crsr = crsr->next;
        }
        print_line();
    }
    var_free (apires);
    return 0;
}

/*/ ======================================================================= /*/
/** If OPTIONS.tenant is the default, unset it */
/*/ ======================================================================= /*/
void disregard_default_tenant (void) {
    var *conf_defaults = var_get_dict_forkey (OPTIONS.conf, "defaults");
    const char *deftenant = var_get_str_forkey (conf_defaults, "tenant");
    if (! deftenant) return;
    if (strcmp (deftenant, OPTIONS.tenant) == 0) OPTIONS.tenant = "";
}

/*/ ======================================================================= /*/
/** The tenant-delete command */
/*/ ======================================================================= /*/
int cmd_tenant_delete (int argc, const char *argv[]) {
    /* Avoid using the default tenant in this case */
    disregard_default_tenant();

    if (OPTIONS.tenant[0] == 0) {
        fprintf (stderr, "%% No tenantid provided\n");
        return 1;
    }
    
    var *req = var_alloc();
    var *apires = api_call ("DELETE", req, "/%s", OPTIONS.tenant);
    var_free (req);
    var_free (apires);
    return 0;
}

/*/ ======================================================================= /*/
/** The tenant-create command */
/*/ ======================================================================= /*/
int cmd_tenant_create (int argc, const char *argv[]) {
    uuid tenant;
    
    /* Avoid using the default tenant in this case */
    disregard_default_tenant();
    
    if (OPTIONS.tenant[0] == 0) {
        tenant = uuidgen();
        OPTIONS.tenant = (char *) malloc (40);
        uuid2str (tenant, OPTIONS.tenant);
    }
    else {
        tenant = mkuuid (OPTIONS.tenant);
    }
    
    var *req = var_alloc();
    var *req_tenant = var_get_dict_forkey (req, "tenant");
    if (OPTIONS.key[0]) {
        var_set_str_forkey (req_tenant, "key", OPTIONS.key);
    }
    if (OPTIONS.name[0]) {
        var_set_str_forkey (req_tenant, "name", OPTIONS.name);
    }
    
    var *apires = api_call ("POST", req, "/%s", OPTIONS.tenant);
    if (apires) {
        var *r = var_get_dict_forkey (apires, "tenant");
        term_printf ("Tenant created:\n");
        print_line();
        term_printf ("     Name: %s\n"
                     "     UUID: %s\n"
                     "  AES Key: %s\n",
                     var_get_str_forkey (r, "name"),
                     OPTIONS.tenant,
                     var_get_str_forkey (r, "key"));
        print_line();
    }
    
    var_free (req);
    var_free (apires);
    return 0;
}

/*/ ======================================================================= /*/
/** The host-list command */
/*/ ======================================================================= /*/
int cmd_host_list (int argc, const char *argv[]) {
    if (OPTIONS.tenant[0] == 0) {
        fprintf (stderr, "%% No tenantid provided\n");
        return 1;
    }

    const char *unit;
    
    var *apires = api_get ("/%s/host", OPTIONS.tenant);
    
    if (OPTIONS.json) {
        var_dump (apires, stdout);
        var_free (apires);
        return 0;
    }
    
    var *v_hosts = var_get_array_forkey (apires, "host");
    if (var_get_count (v_hosts)) {
        print_hdr ("Hosts", rsrc(icns.overview));
        term_printf ("UUID                                    SIZE "
                     "FIRST             LAST\n");
        var *crsr = v_hosts->value.arr.first;
        
        while (crsr) {
            uint64_t usage = var_get_int_forkey (crsr, "usage");
            unit = "KB";
            usage = usage / 1024;
            if (usage > 2048) {
                unit = "MB";
                usage = usage / 1024;
            }
            
            char start[24];
            char end[24];
            
            strncpy (start, var_get_str_forkey (crsr, "start"), 23);
            strncpy (end, var_get_str_forkey (crsr, "end"), 23);
            start[16] = 0;
            end[16] = 0;
            start[10] = ' ';
            end[10] = ' ';

            term_printf ("<g>%s</> <b>%4" PRIu64 "</> %s %s  %s\n",
                         var_get_str_forkey (crsr, "id"),
                         usage, unit, start, end);

            crsr = crsr->next;
        }
        print_line();
    }

    var_free (apires);
    return 0;
}

/*/ ======================================================================= /*/
/** The delete-host command */
/*/ ======================================================================= /*/
int cmd_remove_host (int argc, const char *argv[]) {
    if (OPTIONS.tenant[0] == 0) {
        fprintf (stderr, "%% No tenantid provided\n");
        return 1;
    }
    if (OPTIONS.host[0] == 0) {
        fprintf (stderr, "%% No hostid provided\n");
        return 1;
    }
    
    var *p = var_alloc();
    var *apires = api_call ("DELETE", p, "/%s/host/%s",
                            OPTIONS.tenant, OPTIONS.host);
    var_free (p);
    var_free (apires);
    return 0;
}

/*/ ======================================================================= /*/
/** The user-create command */
/*/ ======================================================================= /*/
int cmd_user_create (int argc, const char *argv[]) {
    char password[64];
    password[0] = 0;
    
    if ((OPTIONS.tenant[0] == 0) ||
        (strcmp (OPTIONS.tenant, "any") == 0)) {
        fprintf (stderr, "%% No tenantid provided\n");
        return 1;
    }
    
    if (OPTIONS.username[0] == 0) {
        fprintf (stderr, "%% No username provided\n");
        return 1;
    }
    
    if (OPTIONS.password[0] == 0) {
        printf ("Password: ");
        readpass (password, 64);
    }
    else {
        strncpy (password, OPTIONS.password, 63);
    }
    
    var *req = var_alloc();
    var_set_str_forkey (req, "password", password);
    var_set_str_forkey (req, "tenant", OPTIONS.tenant);
    if (OPTIONS.admin) {
        var_set_str_forkey (req, "level", "ADMIN");
    }
    else {
        var_set_str_forkey (req, "level", "USER");
    }
    var *apires = api_call ("PUT", req, "/user/%s", OPTIONS.username);
    var_free (req);
    var_free (apires);
    return 0;
}

/*/ ======================================================================= /*/
/** The user-set-tenant command */
/*/ ======================================================================= /*/
int cmd_user_set_tenant (int argc, const char *argv[]) {
    if ((OPTIONS.tenant[0] == 0) ||
        (strcmp (OPTIONS.tenant, "any") == 0)) {
        fprintf (stderr, "%% No tenantid provided\n");
        return 1;
    }
    
    if (OPTIONS.username[0] == 0) {
        fprintf (stderr, "%% No username provided\n");
        return 1;
    }
    
    var *req = var_alloc();
    var_set_str_forkey (req, "tenant", OPTIONS.tenant);
    var *apires = api_call ("POST", req, "/user/%s", OPTIONS.username);
    var_free (req);
    var_free (apires);
    return 0;
}

/*/ ======================================================================= /*/
/** The user-set-password command */
/*/ ======================================================================= /*/
int cmd_user_set_password (int argc, const char *argv[]) {
    char password[64];
    password[0] = 0;
    
    if (OPTIONS.username[0] == 0) {
        fprintf (stderr, "%% No username provided\n");
        return 1;
    }
    
    if (OPTIONS.password[0] == 0) {
        printf ("Password: ");
        readpass (password, 64);
    }
    else {
        strncpy (password, OPTIONS.password, 63);
    }
    
    var *req = var_alloc();
    var_set_str_forkey (req, "password", password);
    var *apires = api_call ("POST", req, "/user/%s", OPTIONS.username);
    var_free (req);
    var_free (apires);
    return 0;
}

/*/ ======================================================================= /*/
/** The user-delete command */
/*/ ======================================================================= /*/
int cmd_user_delete (int argc, const char *argv[]) {
    if (OPTIONS.username[0] == 0) {
        fprintf (stderr, "%% No username provided\n");
        return 1;
    }

    var *req = var_alloc();
    var *apires = api_call ("DELETE", req, "/user/%s", OPTIONS.username);
    var_free (req);
    var_free (apires);
    return 0;
}

/*/ ======================================================================= /*/
/** The get-record command */
/*/ ======================================================================= /*/
int cmd_get_record (int argc, const char *argv[]) {
    if (OPTIONS.tenant[0] == 0) {
        fprintf (stderr, "%% No tenantid provided\n");
        return 1;
    }
    if (OPTIONS.host[0] == 0) {
        fprintf (stderr, "%% No hostid provided\n");
        return 1;
    }
    
    if (strcmp (OPTIONS.tenant, "any") == 0) {
        var *res = api_get ("/any/host/%s/tenant", OPTIONS.host);
        if (res) {
            free (OPTIONS.tenant);
            OPTIONS.tenant = strdup (var_get_str_forkey (res, "tenant"));
            var_free (res);
        }
    }

    var *apires = api_get ("/%s/host/%s", OPTIONS.tenant, OPTIONS.host);
    
    if (OPTIONS.json) {
        var_dump (apires, stdout);
        var_free (apires);
        return 0;
    }
    
    #define $arr(x) var_get_array_forkey(apires,x)
    #define $int(x) var_get_int_forkey(apires,x)
    #define $str(x) var_get_str_forkey(apires,x)
    #define $frac(x) var_get_double_forkey(apires,x)
    #define $$int(x,y) var_get_int_forkey(var_get_dict_forkey(apires,x),y)
    #define $$str(x,y) var_get_str_forkey(var_get_dict_forkey(apires,x),y)
    #define $$frac(x,y) var_get_double_forkey(var_get_dict_forkey(apires,x),y)
    #define $ifrac(x,y) var_get_double_atindex(var_get_array_forkey(apires,x),y)
    #define $done(x) var_delete_key(apires,x)
    #define $exists(x) var_find_key(apires,x)
    /* ----------------------------------------------------------------------*/
    
    term_new_column();
    print_hdr ("Host", rsrc(icns.computer));
    print_value ("UUID", "<y>%s</>", OPTIONS.host);
    print_value ("Hostname", "<y>%s</>", $str("hostname"));
    print_value ("Address", "<b>%s</> (rtt: <b>%.2f</> ms, <b>%.0f</> %% loss)",
                            $$str("link","ip"),
                            $$frac("link","rtt"),
                            $$frac("link","loss"));
                            
    print_value ("Version", "<y>%s</>", $$str("agent","v"));
    print_value ("Status", "%s", decorate_status($str("status")));
    print_array ("Problems", $arr("problems"));
    
    $done("hostname");
    $done("link");
    $done("status");
    $done("problems");
    $done("version");
    
    /* ----------------------------------------------------------------------*/

    if (! OPTIONS.showgraphs) {
        char uptimestr[128];
        uint64_t uptime = $int("uptime"); $done("uptime");
        uint64_t u_days = uptime / 86400ULL;
        uint64_t u_hours = (uptime - (86400 * u_days)) / 3600ULL;
        uint64_t u_mins = (uptime - (86400 * u_days) -
                          (3600 * u_hours)) / 60ULL;
        uint64_t u_sec = uptime % 60;
    
        if (u_days) {
            sprintf (uptimestr, "%" PRIu64 " day%s, %" PRIu64 ":%02" PRIu64
                     ":%02" PRIu64 "", u_days, (u_days==1)?"":"s",
                     u_hours, u_mins, u_sec);
        }
        else if (u_hours) {
            sprintf (uptimestr, "%" PRIu64 ":%02" PRIu64 ":%02"
                     PRIu64, u_hours, u_mins, u_sec);
        }
        else {
            sprintf (uptimestr, "%" PRIu64 " minute%s, %" PRIu64 " second%s",
                     u_mins, (u_mins==1)?"":"s", u_sec, (u_sec==1)?"":"s");
        }
    

        char uptimeastr[128];
        uptimeastr[0] = 0;
        uint64_t uptimea = $$int("agent","up");
        uint64_t ua_days = uptimea / 86400ULL;
        uint64_t ua_hours = (uptimea - (86400 * ua_days)) / 3600ULL;
        uint64_t ua_mins = (uptimea - (86400 * ua_days) -
                           (3600 * ua_hours)) / 60ULL;
        uint64_t ua_sec = uptimea % 60;
        
        $done("agent");
        
        if (uptimea) {
            if (ua_days) {
                sprintf (uptimeastr, "%" PRIu64 " day%s, %" PRIu64 ":%02" PRIu64
                         ":%02" PRIu64 "", ua_days, (ua_days==1)?"":"s",
                         ua_hours, ua_mins, ua_sec);
            }
            else if (ua_hours) {
                sprintf (uptimeastr, "%" PRIu64 ":%02" PRIu64 ":%02"
                         PRIu64, ua_hours, ua_mins, ua_sec);
            }
            else {
                sprintf (uptimeastr, "%" PRIu64 " minute%s, %" PRIu64 
                                     " second%s", ua_mins, (u_mins==1)?"":"s", 
                                     ua_sec, (ua_sec==1)?"":"s");
            }
            
            print_value ("Uptime","<g>%s</> (Agent: %s)",uptimestr, uptimeastr);
        }
        else {
            print_value ("Uptime","<g>%s</>", uptimestr);
        }

        const char *hw = $$str("os","hw");
        if (hw) print_value ("Hardware", "%s", hw);

        var *cpus = $arr("cpu");
        if (cpus && var_get_count (cpus)) {
            var *vcpu = var_get_dict_atindex (cpus, 0);
            print_value ("CPU","<y>%ix</> %s",
                         var_get_int_forkey (vcpu, "count"),
                         var_get_str_forkey (vcpu, "model"));
        }
        $done("cpu");

        print_value ("OS/Arch", "<y>%s %s</> (%s)",
                     $$str("os","kernel"), $$str("os","version"),
                     $$str("os","arch"));
                     
        const char *dist = $$str("os","distro");
        if (dist) print_value ("Distribution", "<g>%s</>", dist);
        $done("os");
        
        /* ------------------------------------------------------------------*/
        term_new_column();
        print_hdr ("Resources", rsrc(icns.gauge));
        print_value ("Processes", "<b>%" PRIu64 "</> (<b>%" PRIu64 "</> "
                                  "running, <b>%" PRIu64 "</> stuck)",
                                  $$int("proc","total"),
                                  $$int("proc","run"),
                                  $$int("proc","stuck"));
        $done("proc");
 
        print_value ("Load Average","<b>%6.2f</> / <b>%6.2f</> / <b>%6.2f</>",
                     $ifrac ("loadavg",0), $ifrac ("loadavg", 1),
                     $ifrac ("loadavg",2));
        $done ("loadavg");

        char cpubuf[128];
        sprintf (cpubuf, VT_BLD "%6.2f " VT_RST "%%", $frac("pcpu"));
    
        char meter[32];
        strcpy (meter, "-[                      ]+");
    
        double iowait = $$frac("io","pwait");
        double pcpu = $frac("pcpu"); $done("pcpu");
        double level = 4.5;
    
        int pos = 2;
        while (level < 100.0 && pos < 22) {
            if (level < pcpu) meter[pos++] = '#';
            else meter[pos++] = ' ';
            level += 4.5;
        }
    
    
        print_gauge_value ("CPU", "%", pcpu, 100);
        if (iowait>0.001) {
            print_value ("CPU iowait", "<b>%6.2f</>", iowait);
        }
        print_value ("Available RAM", "<b>%.2f</> MB",
                     ((double)$$int("mem","total"))/1024.0);
        
        print_value ("Free RAM","<b>%.2f</> MB",
                     ((double)$$int("mem","free"))/1024.0);
    
        print_value ("Network in/out", "<b>%i</> Kb/s (<b>%i</> pps) / "
                                       "<b>%i</> Kb/s (<b>%i</> pps)",
                                       $$int("net","in_kbs"),
                                       $$int("net","in_pps"),
                                       $$int("net","out_kbs"),
                                       $$int("net","out_pps"));
    
        print_value ("Disk i/o", "<b>%i</> rdops / <b>%i</> wrops",
                     $$int("io","rdops"), $$int("io","wrops"));
    
        $done("mem");
        $done("net");
        $done("io");
        $done("badness");

        /* ------------------------------------------------------------------*/
        if ($exists ("temp")) {
            term_new_column();
            print_hdr ("System Temperature", rsrc(icns.temp));
            
            const char *temp_hdr[] = {"PROBE","TEMP",NULL};
            const char *temp_fld[] = {"id","v",NULL};
            columnalign temp_align[] = {CA_L, CA_R, CA_NULL};
            vartype temp_tp[] = {VAR_STR,VAR_INT,VAR_NULL};
            int temp_wid[] = {16, 8, 0};
            int temp_div[] = {0, 0, 0};
            const char *temp_suf[] = {"","°C",NULL};
            
            var *v_temp = $arr ("temp");
            print_table (v_temp, temp_hdr, temp_fld, temp_align, temp_tp,
                         temp_wid, temp_suf, temp_div);
            $done ("temp");
        }
        
        /* ------------------------------------------------------------------*/
        if ($exists ("fan")) {
            term_new_column();
            print_hdr ("System Fans", rsrc(icns.fan));
            
            const char *fan_hdr[] = {"PROBE","SPEED",NULL};
            const char *fan_fld[] = {"id","v",NULL};
            columnalign fan_align[] = {CA_L, CA_R, CA_NULL};
            vartype fan_tp[] = {VAR_STR,VAR_INT,VAR_NULL};
            int fan_wid[] = {16, 8, 0};
            int fan_div[] = {0, 0, 0};
            const char *fan_suf[] = {"","%",NULL};
            
            var *v_fan = $arr("fan");
            print_table (v_fan, fan_hdr, fan_fld, fan_align, fan_tp,
                         fan_wid, fan_suf, fan_div);
            $done ("fan");
        }
        
        /* ------------------------------------------------------------------*/
        if ($exists ("pkgl") &&
            ($$int("pkgm","inq") || $$int("pkgm","reboot"))) {
            term_new_column();
            print_hdr ("System Updates", rsrc(icns.pkg));
            
            const char *pkg_hdr[] = {"PACKAGE","VERSION",NULL};
            const char *pkg_fld[] = {"id","v",NULL};
            columnalign pkg_align[] = {CA_L, CA_R, CA_NULL};
            vartype pkg_tp[] = {VAR_STR, VAR_STR, VAR_NULL};
            int pkg_wid[] = {48,16,0};
            int pkg_div[] = {0,0,0};
            const char *pkg_suf[] = {"","",NULL};
            
            var *v_pkg = $arr ("pkgl");
            print_table (v_pkg, pkg_hdr, pkg_fld, pkg_align, pkg_tp,
                         pkg_wid, pkg_suf, pkg_div);
                         
        }

        $done ("pkgl");
        $done ("pkgm");
        
        /* ------------------------------------------------------------------*/
        if ($exists ("cntr")) {

            term_new_column();
            print_hdr ("Containers", rsrc(icns.container));
            
            const char *cntr_hdr[] = {"NAME","IMAGE","SIZE",NULL};
            const char *cntr_fld[] = {"name","image","size",NULL};
            columnalign cntr_align[] = {CA_L, CA_L, CA_R, CA_NULL};
            vartype cntr_tp[] = {VAR_STR, VAR_STR, VAR_INT, VAR_NULL};
            int cntr_wid[] = {16,32,10};
            int cntr_div[] = {0,0,0,0};
            const char *cntr_suf[] = {"","","MB",NULL};
            
            var *v_cntr = $arr ("cntr");
            print_table (v_cntr, cntr_hdr, cntr_fld, cntr_align, cntr_tp,
                         cntr_wid, cntr_suf, cntr_div);
            
            $done ("cntr");
        }
 
         /* ------------------------------------------------------------------*/
        if ($exists ("bgp")) {

            term_new_column();
            print_hdr ("BGP Peers", rsrc(icns.router));
            
            const char *bgp_hdr[] = {"PEER ADDRESS","MSG SENT",
                                      "MSG RECV","UPTIME","REMOTE AS",NULL};
            const char *bgp_fld[] = {"peer","sent","recv","up","as",NULL};
            columnalign bgp_align[] = {CA_L, CA_R, CA_R, CA_R, CA_R, CA_NULL};
            vartype bgp_tp[] = {VAR_STR, VAR_INT, VAR_INT, VAR_STR,
                                 VAR_INT,VAR_NULL};
            int bgp_wid[] = {20,10,10,12,10,0};
            int bgp_div[] = {0,0,0,0,0,0};
            const char *bgp_suf[] = {"","","","","",NULL};
            
            var *v_bgp = $arr ("bgp");
            print_table (v_bgp, bgp_hdr, bgp_fld, bgp_align, bgp_tp,
                         bgp_wid, bgp_suf, bgp_div);
            
            $done ("bgp");
        }

       /* ------------------------------------------------------------------*/
        if ($exists ("phdns")) {
            term_new_column();
            print_hdr ("Pi-Hole DNS",rsrc(icns.dns));
            print_value ("Block table size", "<b>%i</>", $$int("phdns","sz"));
            print_value ("Queries today", "%i", $$int("phdns","qnow"));
            print_value ("Blocked today", "%i", $$int("phdns","bnow"));
            print_value ("Blocked", "<b>%.1f</>", $$frac("phdns","pblk"));
            $done ("phdns");
        }
        
       /* ------------------------------------------------------------------*/
        if ($exists ("mysql")) {
            term_new_column();
            print_hdr ("MySQL Server",rsrc(icns.db));
            print_value ("Configuration", "<b>%s</>", $$str("mysql","conf"));
            print_value ("Queries/second", "%.2f", $$frac("mysql","qps"));
            print_value ("Errors/second", "%.2f", $$frac("mysql","erps"));
            print_value ("Connections/second", "%.2f", $$frac("mysql","cps"));
            $done ("mysql");
        }
        
        /* ------------------------------------------------------------------*/
        if ($exists ("mssql")) {
            term_new_column();
            print_hdr ("MS SQL Server",rsrc(icns.db));
            print_value ("User Connections","%i",$$frac("mssql","uc"));
            print_value ("Requests/second","%i",$$frac("mssql","bps"));
            print_value ("Errors/second","%i",$$frac("mssql","erps"));
        }

        /* ------------------------------------------------------------------*/
        print_values (apires, NULL);
    
        /* ------------------------------------------------------------------*/
        term_new_column();
        print_hdr ("Process List", rsrc(icns.procs));
    
        const char *top_hdr[] = {"USER","PID","CPU","MEM","NAME",NULL};
        const char *top_fld[] = {"user","pid","pcpu","pmem","name",NULL};
        columnalign top_align[] = {CA_L, CA_R, CA_R, CA_R, CA_L, CA_NULL};
        vartype top_tp[] =
            {VAR_STR,VAR_INT,VAR_DOUBLE,VAR_DOUBLE,VAR_STR,VAR_NULL};
        int top_wid[] = {15, 7, 9, 9, 34, 0};
        int top_div[] = {0, 0, 0, 0, 0, 0};
        const char *top_suf[] = {"",""," %", " %", "", NULL};
    
        var *v_top = $arr("top");
        print_table (v_top, top_hdr, top_fld, 
                     top_align, top_tp, top_wid, top_suf, top_div);
    
        $done("top");
        /* ------------------------------------------------------------------*/
        var *v_df = $arr("df");
        if (var_get_count (v_df) > 0) {
            term_new_column();
            print_hdr ("Storage", rsrc(icns.disks));
    
            const char *df_hdr[] = {
                "DEVICE","SIZE","FS","USED","MOUNTPOINT",NULL
            };
            const char *df_fld[] = {
                "device","size","fs","pused","mount",NULL
            };
            columnalign df_aln[] = {
                CA_L,CA_R,CA_L,CA_R,CA_L,CA_NULL
            };
            vartype df_tp[] = {
                VAR_STR,VAR_INT,VAR_STR,VAR_DOUBLE,VAR_STR,VAR_NULL
            };
            int df_wid[] = {28, 12, 6, 8, 0};
            int df_div[] = {0, (1024), 0, 0, 0, 0};
            const char *df_suf[] = {""," GB", "", " %", "", ""};
    
            /*print_generic_table (v_df);*/
    
            print_table (v_df, df_hdr, df_fld,
                         df_aln, df_tp, df_wid, df_suf, df_div);
        }    
        $done("df");
        
        /* ------------------------------------------------------------------*/
        if ($exists("raid")) {
            var *v_raid = $arr("raid");
            if (var_get_count (v_raid) > 0) {
                term_new_column();
                print_hdr ("RAID", rsrc(icns.raid));
    
                const char *raid_hdr[] = {
                    "DEVICE","LEVEL","STATE","DISKS","FAILED",NULL
                };
                const char *raid_fld[] = {
                    "dev","level","state","count","fail",NULL
                };
                columnalign raid_aln[] = {
                    CA_L,CA_L,CA_L,CA_R,CA_R,CA_NULL
                };
                vartype raid_tp[] = {
                    VAR_STR,VAR_STR,VAR_STR,VAR_INT,VAR_INT,VAR_NULL
                };
                int raid_wid[] = {10, 10, 16, 8, 8};
                int raid_div[] = {0, 0, 0, 0, 0, 0};
                const char *raid_suf[] = {""," ", "", "", "", ""};
    
                /*print_generic_table (v_raid);*/
    
                print_table (v_raid, raid_hdr, raid_fld,
                             raid_aln, raid_tp, raid_wid, raid_suf, raid_div);
            }
        }
        $done("raid");
        
        /* ------------------------------------------------------------------*/
        var *v_vm = $arr("vm");
        if (var_get_count (v_vm) > 0) {
            term_new_column();
            print_hdr ("Virtual Machines", rsrc(icns.vm));
            
            const char *vm_hdr[] = {"MACHINE NAME","CPU CORES","MEMORY",
                                    "CPU USAGE",NULL};
            const char *vm_fld[] = {"name","cores","ram","cpu",NULL};
            columnalign vm_aln[] = {CA_L,CA_R,CA_R,CA_R,CA_NULL};
            vartype vm_tp[] = {VAR_STR,VAR_INT,VAR_INT,VAR_DOUBLE,VAR_NULL};
            int vm_wid[] = {28,10,10,20,0};
            int vm_div[] = {0,0,0,0,0};
            const char *vm_suf[] = {"","","MB","%",""};
            print_table (v_vm, vm_hdr, vm_fld, vm_aln, vm_tp,
                         vm_wid, vm_suf, vm_div);
        }
        $done("vm");
    
        /* ------------------------------------------------------------------*/
        var *v_http = $arr("http");
        if (var_get_count (v_http) > 0) {
            term_new_column();
            print_hdr ("Hosted Websites", rsrc(icns.www));
            
            const char *http_hdr[] = {"SITE","COUNT",NULL};
            const char *http_fld[] = {"site","count",NULL};
            columnalign http_aln[] = {CA_L,CA_R,CA_NULL};
            vartype http_tp[] = {VAR_STR,VAR_INT,VAR_NULL};
            int http_wid[] = {50,10,0};
            int http_div[] = {0,0,0};
            const char *http_suf[] = {"","",""};
            print_table (v_http, http_hdr, http_fld, http_aln, http_tp,
                         http_wid, http_suf, http_div);
        }
        $done("http");

        /** Print any remaining table data */
        print_tables (apires);
        term_end_column();
    }
    else {
        term_end_column();
        print_hdr("Graphs", rsrc(icns.graph));
        term_new_column();
        cmd_print_graph ("cpu","usage", 76, 2);
        term_printf ("<l>  12    ^     ^      ^     ^     ^      6     ^     "
                  "^      ^     ^     ^     0</>\n");
        if (TERMINFO.cols >= 160) term_printf ("\n");

        term_new_column();
        cmd_print_graph ("link","rtt", 76, 2);
        term_printf ("<l>  12    ^     ^      ^     ^     ^      6     ^     "
                  "^      ^     ^     ^     0</>\n");
        if (TERMINFO.cols >= 160) term_printf ("\n");

        if ((TERMINFO.cols >= 160) && (TERMINFO.rows >= 50)) {
            term_new_column();
            cmd_print_graph ("net","input", 76, 2);
            term_printf ("<l>  12    ^     ^      ^     ^     ^      6     ^"
                          "     ^      ^     ^     ^     0</>\n\n");
            term_new_column();
            cmd_print_graph ("net","output", 76, 2);
            term_printf ("<l>  12    ^     ^      ^     ^     ^      6     ^"
                          "     ^      ^     ^     ^     0</>\n\n");
            term_new_column();
            cmd_print_graph ("io","read", 76, 2);
            term_printf ("<l>  12    ^     ^      ^     ^     ^      6     ^"
                          "     ^      ^     ^     ^     0</>\n\n");
            term_new_column();
            cmd_print_graph ("io","write", 76, 2);
            term_printf ("<l>  12    ^     ^      ^     ^     ^      6     ^"
                          "     ^      ^     ^     ^     0</>\n\n");
            term_end_column();
        }
        else {
            term_new_column();
            cmd_print_graph ("net","input", 37, 2);
            term_crsr_up (7);
            cmd_print_graph ("net","output", 37, 41);
            term_printf ("<l>"
                         "  12 ^  ^  ^  ^  ^  6  ^  ^  ^  ^  ^  0"
                         "  12 ^  ^  ^  ^  ^  6  ^  ^  ^  ^  ^  0  </>\n");

            if (TERMINFO.cols >= 160) term_printf ("\n");
            term_new_column();
            cmd_print_graph ("io","read", 37, 2);
            term_crsr_up (7);
            cmd_print_graph ("io","write", 37, 41);
            term_printf ("<l>"
                         "  12 ^  ^  ^  ^  ^  6  ^  ^  ^  ^  ^  0"
                         "  12 ^  ^  ^  ^  ^  6  ^  ^  ^  ^  ^  0  </>\n");
            if (TERMINFO.cols >= 160) term_printf ("\n");
            term_end_column();
        }
    }
    print_line();

    var_free (apires);
    print_done();
    return 0;    
}

/*/ ======================================================================= /*/
/** The session-list command [admin] */
/*/ ======================================================================= /*/
int cmd_session_list (int argc, const char *argv[]) {
    var *v = api_get ("/session");

    if (OPTIONS.json) {
        var_dump (v, stdout);
        var_free (v);
        return 0;
    }

    print_hdr ("Sessions", rsrc(icns.sessions));
    const char *spc = "                            ";
    
    term_printf (" ID                SENDER             TENANT    ");
    if (TERMINFO.cols>135) term_printf (spc);
    term_printf ("HOST      ");
    if (TERMINFO.cols>107) term_printf (spc);
    term_printf ("SERIAL      HEARTBEAT\n");
    
    var *v_session = var_get_array_forkey (v, "session");
    var *crsr = v_session->value.arr.first;
    while (crsr) {
        char *dt = strdup (var_get_str_forkey (crsr, "lastcycle"));
        if (*dt) dt[strlen(dt)-1] = 0;
        
        char *shorthost = strdup (var_get_str_forkey (crsr, "hostid"));
        char *shorttenant = strdup (var_get_str_forkey (crsr, "tenantid"));
        
        if (TERMINFO.cols < 108) shorthost[8] = 0;
        if (TERMINFO.cols < 136) shorttenant[8] = 0;
        
        term_printf (" <g>%08x-%08x</> <b>%-18s</> %s  %s  <y>%-12" PRIu64
                     "</> <b>%s</>\n",
                     (uint32_t) (var_get_int_forkey (crsr, "sessid")),
                     (uint32_t) (var_get_int_forkey (crsr, "addr")),
                     var_get_str_forkey (crsr, "remote"),
                     shorttenant, shorthost,
                     var_get_int_forkey (crsr, "lastserial"),
                     dt+11);

        crsr = crsr->next;
        
        free (dt);
        free (shorthost);
        free (shorttenant);
    }
    print_line();

    var_free (v);
    return 0;
}

/*/ ======================================================================= /*/
/** Prints out a graph */
/*/ ======================================================================= /*/
void cmd_print_graph (const char *graph_id, const char *datum_id, int width,
                      int indent) {
    var *apires = api_get ("/%s/host/%s/graph/%s/%s/43200/%i",
                           OPTIONS.tenant, OPTIONS.host,
                           graph_id, datum_id, width);
    if (! apires) {
        term_crsr_setx (indent);
        term_printf ("%s: no graph\n\n\n\n\n\n\n", datum_id);
        return;
    }
    
    double max = var_get_double_forkey (apires, "max");
    var *arr = var_get_array_forkey (apires, "data");
    if (! arr) {
        term_crsr_setx (indent);
        term_printf ("%s: no graph\n\n\n\n\n\n\n", datum_id);
        var_free (apires);
        return;
    }
    
    const char *title = var_get_str_forkey (apires, "title");
    const char *unit = var_get_str_forkey (apires, "unit");
    if (!title) title = "Untitled";
    
    term_crsr_setx (indent);
    term_printf ("%s", title);

    if (unit && unit[0]!='%') {
        term_printf (" (max %.1f %s)", max, unit);
    }
    term_printf ("\n");

    double *dat = (double *) malloc (width*sizeof(double));
    for (int i=0; i<width; ++i) {
        dat[i] = var_get_double_atindex (arr, i);
    }
    
    int height = 6;
    if (width > 40) height = 8;
    
    var_free (apires);
    print_graph (width, height, indent, max, dat);
}

/*/ ======================================================================= /*/
/** The dancing-bears command */
/*/ ======================================================================= /*/
int cmd_bears (int argc, const char *argv[]) {
    var *v = api_get ("/obligatory-dancing-bears");
    puts (var_get_str_forkey (v,"bear"));
    var_free (v);
    
    return 0;
}
