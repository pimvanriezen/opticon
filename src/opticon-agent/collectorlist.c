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
    collector *c = collector_new (self);
    c->addr = strdup (var_get_str_forkey (data, "address"));
    c->auth.sessionid = 0;
    c->auth.serial = 0;
    c->auth.tenantid = mkuuid (var_get_str_forkey (data, "tenant"));
    c->auth.tenantkey = aeskey_from_base64 (var_get_str_forkey (data, "key"));
    c->auth.hostid = APP.hostid;
    c->port = var_get_int_forkey (data, "port");
    c->transport = outtransport_create_udp();
}

void collectorlist_start (collectorlist *self) {
    collector *c = self->first;
    while (c) {
        if (! outtransport_setremote (c->transport, c->addr, c->port)) {
            log_error ("Error setting remote address '%s'", c->addr);
        }
        else {
            c->resender = authresender_create (c->transport);
        }
        c = c->next;
    }
}
