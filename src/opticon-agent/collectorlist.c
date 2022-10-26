#include <stdlib.h>
#include <libopticon/var.h>
#include <libopticon/util.h>
#include <libopticon/transport_udp.h>
#include <libopticon/log.h>
#include "opticon-agent.h"

collector *collector_new (collectorlist *parent) {
    collector *self = (collector *) malloc (sizeof (collector));
    self->next = self->prev = NULL;
    self->port = 0;
    self->addr = NULL;
    self->transport = NULL;
    self->resender = NULL;
    self->lastkeyrotate = (time_t) 0;
    if (parent->last) {
        self->prev = parent->last;
        self->prev->next = self;
        parent->last = self;
    }
    else {
        parent->first = parent->last = self;
    }
    return self;
}

void collectorlist_init (collectorlist *self) {
    self->first = self->last = NULL;
}

void collectorlist_add_host (collectorlist *self, var *data) {
    const char *addr = var_get_str_forkey (data, "address");
    const char *tenantid = var_get_str_forkey (data, "tenant");
    const char *tenantkey = var_get_str_forkey (data, "key");
    
    if (! addr) {
        log_error ("No address specification for collector");
        return;
    }
    
    if (! tenantid) {
        log_error ("No tenant specification for collector");
        return;
    }
    
    if (! tenantkey) {
        log_error ("No tenant key provided for collector");
        return;
    }
        
    collector *c = collector_new (self);
    
    c->addr = strdup (addr);
    c->auth.sessionid = 0;
    c->auth.serial = 0;
    c->auth.tenantid = mkuuid (tenantid);
    c->auth.tenantkey = aeskey_from_base64 (tenantkey);
    c->auth.hostid = APP.hostid;
    c->port = var_get_int_forkey (data, "port");
    c->transport = outtransport_create_udp();
}

void collectorlist_start (collectorlist *self) {
    collector *c = self->first;
    while (c) {
        if (! outtransport_setremote (c->transport, c->addr, c->port)) {
            log_error ("Error setting remote address '%s'", c->addr);
            exit (1);
        }
        else {
            c->resender = authresender_create (c->transport);
        }
        c = c->next;
    }
}
