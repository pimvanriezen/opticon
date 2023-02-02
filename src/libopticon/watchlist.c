#include <libopticon/watchlist.h>
#include <libopticon/util.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

/*/ ======================================================================= /*/
/** Initialize list header and thread lock for a watchlist.
  * \param self The watchlist ot initialize. */
/*/ ======================================================================= /*/
void watchlist_init (watchlist *self) {
    self->first = self->last = NULL;
    pthread_mutex_init (&self->mutex, NULL);
}

/*/ ======================================================================= /*/
/** Add a meterwatch to the list.
  * \param self The watchlist
  * \param m The watcher to add. */
/*/ ======================================================================= /*/
void watchlist_add (watchlist *self, meterwatch *m) {
    if (self->last) {
        m->prev = self->last;
        self->last->next = m;
        self->last = m;
    }
    else {
        self->first = self->last = m;
    }
}

/*/ ======================================================================= /*/
/** Create, and add, a new integer-based meterwatch to the watchlist.
  * \param self The watchlist
  * \param id The meterid to watch
  * \param tp The type of watcher
  * \param val The value to compare to
  * \param bad Weight/badness.
  * \param trig Alert tigger level. */
/*/ ======================================================================= /*/
void watchlist_add_uint (watchlist *self, meterid_t id, watchtype tp,
                         uint64_t val, double bad, watchtrigger trig) {
    meterwatch *w = malloc (sizeof (meterwatch));
    w-> next = w->prev = NULL;
    w->id = id;
    w->tp = tp;
    w->trigger = trig;
    w->badness = bad;
    w->dat.u64 = val;
    watchlist_add (self, w);
}

/*/ ======================================================================= /*/
/** Create, and add, a new fractional-based meterwatch to the watchlist.
  * \param self The watchlist
  * \param id The meterid to watch
  * \param tp The type of watcher
  * \param val The value to compare to
  * \param bad Weight/badness.
  * \param trig Alert tigger level. */
/*/ ======================================================================= /*/
void watchlist_add_frac (watchlist *self, meterid_t id, watchtype tp,
                         double val, double bad, watchtrigger trig) {
    meterwatch *w = malloc (sizeof (meterwatch));
    w-> next = w->prev = NULL;
    w->id = id;
    w->tp = tp;
    w->trigger = trig;
    w->badness = bad;
    w->dat.frac = val;
    watchlist_add (self, w);
}

/*/ ======================================================================= /*/
/** Create, and add, a new string-based meterwatch to the watchlist.
  * \param self The watchlist
  * \param id The meterid to watch
  * \param tp The type of watcher
  * \param val The value to compare to
  * \param bad Weight/badness.
  * \param trig Alert tigger level. */
/*/ ======================================================================= /*/
void watchlist_add_str (watchlist *self, meterid_t id, watchtype tp,
                        const char *val, double bad, watchtrigger trig) {
                        
    meterwatch *w = malloc (sizeof (meterwatch));
    w-> next = w->prev = NULL;
    w->id = id;
    w->tp = tp;
    w->trigger = trig;
    w->badness = bad;
    strncpy (w->dat.str.str, val, 127);
    w->dat.str.str[127] = 0;
    watchlist_add (self, w);
}

/*/ ======================================================================= /*/
/** Remove, and deallocate, all meters associated with a watchlist.
  * \param self The list to clear */
/*/ ======================================================================= /*/
void watchlist_clear (watchlist *self) {
    meterwatch *w;
    meterwatch *nw;
    w = self->first;
    while (w) {
        nw = w->next;
        free (w);
        w = nw;
    }
    self->first = self->last = NULL;
}

/*/ ======================================================================= /*/
/** Allocate and initialize a graphlist */
/*/ ======================================================================= /*/
graphlist *graphlist_create (void) {
    graphlist *self = malloc (sizeof (graphlist));
    self->first = self->last = NULL;
    pthread_mutex_init (&self->mutex, NULL);
    return self;
}

/*/ ======================================================================= /*/
/** Clear and deallocate a graphlist */
/*/ ======================================================================= /*/
void graphlist_free (graphlist *self) {
    graphlist_clear (self);
    free (self);
}

/*/ ======================================================================= /*/
/** Empty an existing graphlist */
/*/ ======================================================================= /*/
void graphlist_clear (graphlist *self) {
    graphtarget *crsr, *ncrsr;
    
    pthread_mutex_lock (&self->mutex);
    crsr = self->first;
    self->first = self->last = NULL;
    pthread_mutex_unlock (&self->mutex);
    while (crsr) {
        ncrsr = crsr->next;
        if (crsr->graph_id) free (crsr->graph_id);
        if (crsr->datum_id) free (crsr->datum_id);
        if (crsr->title) free (crsr->title);
        if (crsr->unit) free (crsr->unit);
        if (crsr->color) free (crsr->color);
        free (crsr);
        crsr = ncrsr;
    }
}

/*/ ======================================================================= /*/
/** Add a target to a graphlist */
/*/ ======================================================================= /*/
void graphlist_add (graphlist *self, meterid_t id, const char *graph,
                    const char *datum, const char *title, const char *unit,
                    const char *color, double max) {
    graphtarget *res = malloc (sizeof (graphtarget));
    res->next = res->prev = NULL;
    res->id = id;
    res->graph_id = graph ? strdup (graph) : NULL;
    res->datum_id = datum ? strdup (datum) : NULL;
    res->title = title ? strdup (title) : NULL;
    res->unit = unit ? strdup (unit) : NULL;
    res->color = color ? strdup (color) : NULL;
    res->max = max;
    
    pthread_mutex_lock (&self->mutex);
    if (self->last) {
        res->prev = self->last;
        self->last->next = res;
        self->last = res;
    }
    else {
        self->first = self->last = res;
    }
    pthread_mutex_unlock (&self->mutex);
}

/*/ ======================================================================= /*/
/** Build a graphlist from a var tree */
/*/ ======================================================================= /*/
void graphlist_make (graphlist *self, var *def) {
    graphlist_clear (self);
    var *crsr = def->value.arr.first;
    while (crsr) {
        meterid_t mid = makeid (crsr->id, MTYPE_INT, 0);
        graphlist_add (self, mid,
                       var_get_str_forkey (crsr, "graph"),
                       var_get_str_forkey (crsr, "datum"),
                       var_get_str_forkey (crsr, "title"),
                       var_get_str_forkey (crsr, "unit"),
                       var_get_str_forkey (crsr, "color"),
                       var_get_double_forkey (crsr, "max"));
        
        crsr = crsr->next;               
    }
}

/*/ ======================================================================= /*/
/** Iterator for graphlist. Returns the first element. */
/*/ ======================================================================= /*/
graphtarget *graphlist_begin (graphlist *self) {
    if (! self->first) return NULL;
    pthread_mutex_lock (&self->mutex);
    return self->first;
}

/*/ ======================================================================= /*/
/** Iterator for graphlist. Returns the next element, NULL if done */
/*/ ======================================================================= /*/
graphtarget *graphlist_next (graphlist *self, graphtarget *node) {
    if (! node->next) {
        pthread_mutex_unlock (&self->mutex);
        return NULL;
    }
    return node->next;
}

/*/ ======================================================================= /*/
/** Interrupt a running iterator */
/*/ ======================================================================= /*/
void graphlist_break (graphlist *self) {
    pthread_mutex_unlock (&self->mutex);
}
