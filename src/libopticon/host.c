#include <libopticon/host.h>
#include <libopticon/util.h>
#include <libopticon/defaults.h>
#include <assert.h>

/*/ ======================================================================= /*/
/** Allocate and initialize a host structure.
  * \return Pointer to the initialized struct. Caller is responsible for
  *         freeing allocated memory. */
/*/ ======================================================================= /*/
host *host_alloc (void) {
    host *res = malloc (sizeof (host));
    res->prev = NULL;
    res->next = NULL;
    res->first = NULL;
    res->last = NULL;
    res->trash = NULL;
    res->tenant = NULL;
    res->graphlist = NULL;
    res->status = 0;
    res->lastserial = 0;
    res->badness = 0.0;
    res->lastmetasync = 0;
    res->lastmodified = 0;
    res->mcount = 0;
    adjustlist_init (&res->adjust);
    pthread_rwlock_init (&res->lock, NULL);
    return res;
}

/*/ ======================================================================= /*/
/** Find or create a host structure for a specified tenant. 
  * \param tenantid The UUID for the tenant.
  * \param hostid The UUID for the host. */
/*/ ======================================================================= /*/
host *host_find (uuid tenantid, uuid hostid) {
    host *h = NULL;
    host *nh = NULL;
    tenant *t = tenant_find (tenantid, TENANT_LOCK_WRITE);
    if (! t) return NULL;
    
    h = t->first;
    if (! h) {
        h = host_alloc();
        h->uuid = hostid;
        h->tenant = t;
        t->first = t->last = h;
        tenant_done (t);
        return h;
    }
    
    while (h) {
        if (uuidcmp (h->uuid, hostid)) {
            tenant_done (t);
            return h;
        }
        if (h->next) h = h->next;
        else {
            nh = host_alloc();
            nh->uuid = hostid;
            nh->tenant = t;
            h->next = nh;
            nh->prev = h;
            t->last = nh;
            tenant_done (t);
            return nh;
        }
    }
    tenant_done (t);
    return NULL;
}

/*/ ======================================================================= /*/
/** Deallocate a host and its associated meters. Remove it from the
  * tenant, if it is associated with one. 
  * \param h The host to delete */
/*/ ======================================================================= /*/
void host_delete (host *h) {
    meter *m, *nm;
    if (h->tenant) {
        if (h->prev) {
            h->prev->next = h->next;
        }
        else h->tenant->first = h->next;
        if (h->next) {
            h->next->prev = h->prev;
        }
        else h->tenant->last = h->prev;
    }
    m = h->first;
    while (m) {
        nm = m->next;
        if (m->d.any) free (m->d.any);
        free (m);
        m = nm;
    }
    pthread_rwlock_destroy (&h->lock);
    adjustlist_clear (&h->adjust);
    free (h);
}

/*/ ======================================================================= /*/
/** Figure out if a meter currently exists for a host.
  * \param host The host object
  * \param id The id of the meter
  * \return 1 on success, 0 on failure */
/*/ ======================================================================= /*/
int host_has_meter (host *h, meterid_t id) {
    meterid_t rid = (id & (MMASK_TYPE | MMASK_NAME));
    meter *m = h->first;
    int res = 0;
    while (m) {
        if (m->id == rid) {
            res = 1;
            break;
        }
        m = m->next;
    }
    return res;
}

/*/ ======================================================================= /*/
/** Find a meter only by its name, regardless of type */
/*/ ======================================================================= /*/
meter *host_find_meter_name (host *h, meterid_t id) {
    meterid_t rid = (id & MMASK_NAME);
    meter *m = h->first;
    while (m) {
        if ((m->id & MMASK_NAME) == rid) return m;
        m = m->next;
    }
    return NULL;
}

/*/ ======================================================================= /*/
/** look up a meter */
/*/ ======================================================================= /*/
meter *host_find_meter (host *h, meterid_t id) {
    meterid_t rid = (id & (MMASK_TYPE | MMASK_NAME));
    meter *m = h->first;
    if (! m) return NULL;
    while (m) {
        if (m->id == rid) return m;
        m = m->next;
    }
    return NULL;
}

/*/ ======================================================================= /*/
/** Get (or create) a specific meter for a host.
  * \param host The host structure.
  * \param id The meterid (label and type).
  * \return Pointer to the meter structure, which will be
  *         linked into the host's meterlist. */
/*/ ======================================================================= /*/
meter *host_get_meter (host *h, meterid_t id) {
    meterid_t rid = (id & (MMASK_TYPE | MMASK_NAME));
    meter *m = h->first;
    meter *nm = NULL;
    if (! m) {
        nm = meter_alloc();
        h->first = h->last = nm;
        h->mcount = 1;
        nm->id = rid;
        nm->host = h;
        return nm;
    }
    while (m) {
        if (m->id == rid) return m;
        if ((m->id & MMASK_NAME) == (rid & MMASK_NAME)) {
            if (m->count == SZ_EMPTY_ARRAY) return m;
        }
        if (m->next) m = m->next;
        else {
            nm = meter_alloc();
            m->next = nm;
            nm->prev = m;
            h->last = nm;
            nm->id = rid;
            nm->host = h;
            h->mcount++;
            return nm;
        }
    }
    return NULL;
}

/*/ ======================================================================= /*/
/** Look up a meter by prefix. */
/*/ ======================================================================= /*/
meter *host_find_prefix (host *h, meterid_t prefix, meter *prev) {
    meter *crsr = prev ? prev : h->first;
    meterid_t mask = id2mask (prefix);
    if (! crsr) return NULL;
    
    crsr = crsr->next;
    while (crsr) {
        char meter[16];
        id2str (crsr->id &mask, meter);
        if ((crsr->id & mask) == (prefix & MMASK_NAME)) {
            if (idisprefix (prefix, crsr->id, mask)) return crsr;
        }
        crsr = crsr->next;
    }
    return NULL;
}

void host_empty_trash (host *self) {
    meter *m, *mnext;
    
    m = self->trash;
    while (m) {
        mnext = m->next;
        free (m);
        m = mnext;
    }
    self->trash = NULL;
}

/*/ ======================================================================= /*/
/** Remove a meter from a host list and add it to the trash */
/*/ ======================================================================= /*/
void host_delete_meter (host *h, meter *m) {
    if (m->prev) {
        if (m->next) {
            m->prev->next = m->next;
            m->next->prev = m->prev;
        }
        else {
            m->prev->next = NULL;
            h->last = m->prev;
        }
    }
    else {
        if (m->next) {
            m->next->prev = NULL;
            h->first = m->next;
        }
        else {
            h->first = NULL;
            h->last = NULL;
        }
    }

    h->mcount--;
    meter_set_empty (m);
    m->lastmodified = 0;
    m->prev = m->next = NULL;
    
    if (h->trash) {
        h->trash->prev = m;
        m->next = h->trash;
        h->trash = m;
    }
}

/*/ ======================================================================= /*/
/** Mark the beginning of a new update cycle. Saves the meters
  * from constantly asking the kernel for the current time. */
/*/ ======================================================================= /*/
void host_begin_update (host *h, time_t t) {
    h->lastmodified = t;
}

/*/ ======================================================================= /*/
/** End an update round. Reaps any dangling meters that have been
    inactive for more than five minutes. */
/*/ ======================================================================= /*/
void host_end_update (host *h) {
    bool is_stale = false;
    time_t last = h->lastmodified;
    meterid_t mid_status = makeid("status",MTYPE_STR,0);
    meter *m_status = host_find_meter (h, mid_status);
    meter *m = h->first;
    meter *nm;
    fstring status;
    
    /* Remove previously trashed nodes */
    host_empty_trash (h);
    
    /* Don't reap meters from STALE hosts */
    if (m_status) {
        status = meter_get_str (m_status, 0);
        if (strcmp (status.str, "STALE") == 0) is_stale = true;
    }
    
    while (m) {
        nm = m->next;
        if (is_stale) m->lastmodified = last;
        else if (m->lastmodified < last) {
            if ((last - m->lastmodified) > default_meter_timeout) {
                host_delete_meter (h, m);
            }
        }
        m = nm;
    }
}

/*/ ======================================================================= /*/
/** Fill up a meter with integer values */
/*/ ======================================================================= /*/
meter *host_set_meter_uint (host *h, meterid_t id,
                            unsigned int cnt,
                            uint64_t *data) {
    
    unsigned int count = cnt ? cnt : 1;
    meter *m = host_get_meter (h, id);
    m->count = cnt;
    m->lastmodified = h->lastmodified;
    
    if (m->d.any != NULL) {
        free (m->d.any);
        m->d.any = NULL;
    }
    
    m->d.u64 = malloc (count * sizeof (uint64_t));
    memcpy (m->d.u64, data, count * sizeof (uint64_t));
    return m;
}

/*/ ======================================================================= /*/
/** Fill up a meter with string values */
/*/ ======================================================================= /*/
meter *host_set_meter_str (host *h, meterid_t id,
                           unsigned int cnt,
                           const fstring *str) {
    unsigned int count = cnt ? cnt : 1;
    meter *m = host_get_meter (h, id);
    m->count = cnt;
    m->lastmodified = h->lastmodified;
    if (m->d.any != NULL) {
        free (m->d.any);
        m->d.any = NULL;
    }
    m->d.str = malloc (count * sizeof (fstring));
    memcpy (m->d.str, str, count * sizeof (fstring));
    return m;   
}

