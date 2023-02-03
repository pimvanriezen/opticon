#ifndef _OPTICONDB_CMD_H
#define _OPTICONDB_CMD_H 1

#include <time.h>
#include <libopticon/var.h>

/* =============================== TYPES =============================== */

typedef enum {
    AUTH_UNSET,
    AUTH_INTERNAL,
    AUTH_OPENSTACK,
    AUTH_UNITHOST
} authtype;

/** Configuration and flags obtained from the configuration file and
  * command line.
  */
typedef struct optinfo {
    char        *tenant;
    char        *key;
    char        *path;
    char        *name;
    char        *host;
    char        *hostname;
    char        *meter;
    char        *type;
    char        *description;
    char        *unit;
    char        *level;
    char        *match;
    char        *value;
    char        *weight;
    char        *username;
    char        *password;
    bool         iterm;
    time_t       time;
    int          json;
    int          watch;
    int          flip;
    int          showgraphs;
    int          admin;
    char        *api_url;
    char        *keystone_url;
    char        *unithost_url;
    char        *unithost_identity_url;
    char        *external_token;
    char        *opticon_token;
    char        *config_file;
    var         *conf;
    authtype     auth;
} optinfo;

/* ============================== GLOBALS ============================== */

extern optinfo OPTIONS;

/* ============================= FUNCTIONS ============================= */

int keystone_login (void);
int unithost_login (void);
void cmd_print_graph (const char *, const char *, int, int);
int cmd_user_create (int argc, const char *argv[]);
int cmd_user_set_tenant (int argc, const char *argv[]);
int cmd_user_set_password (int argc, const char *argv[]);
int cmd_user_delete (int argc, const char *argv[]);
int cmd_tenant_list (int argc, const char *argv[]);
int cmd_tenant_get_metadata (int argc, const char *argv[]);
int cmd_tenant_set_metadata (int argc, const char *argv[]);
int cmd_tenant_set_quota (int argc, const char *argv[]);
int cmd_tenant_get_quota (int argc, const char *argv[]);
int cmd_tenant_get_summary (int argc, const char *argv[]);
int cmd_watcher_set (int argc, const char *argv[]);
int cmd_watcher_delete (int argc, const char *argv[]);
int cmd_watcher_list (int argc, const char *argv[]);
int cmd_meter_create (int argc, const char *argv[]);
int cmd_meter_delete (int argc, const char *argv[]);
int cmd_meter_list (int argc, const char *argv[]);
int cmd_tenant_delete (int argc, const char *argv[]);
int cmd_tenant_create (int argc, const char *argv[]);
int cmd_host_overview (int argc, const char *argv[]);
int cmd_host_list (int argc, const char *argv[]);
int cmd_get_record (int argc, const char *argv[]);
int cmd_remove_host (int argc, const char *argv[]);
int cmd_session_list (int argc, const char *argv[]);
int cmd_bears (int argc, const char *argv[]);

#endif
