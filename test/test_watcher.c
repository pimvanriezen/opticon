#include <libopticon/meter.h>
#include <libopticon/watchlist.h>
#include <libopticon/host.h>
#include <libopticon/util.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

int main (int argc, const char *argv[]) {
    meterid_t mi_load = makeid ("load",MTYPE_FRAC,0);
    
    watchlist *wl = (watchlist *) malloc (sizeof (watchlist));
    adjustlist *al = (adjustlist *) malloc (sizeof (adjustlist));
    watchlist_init (wl);
    adjustlist_init (al);
    
    watchlist_add_frac (wl, mi_load, WATCH_FRAC_GT, 5.0, 5.0, WATCH_WARN);
    watchlist_add_frac (wl, mi_load, WATCH_FRAC_GT, 10.0, 8.0, WATCH_ALERT);
    watchlist_add_frac (wl, mi_load, WATCH_FRAC_GT, 20.0, 12.0, WATCH_CRIT);
    
    watchadjust *adj = adjustlist_get (al, mi_load);
    adj->type = WATCHADJUST_FRAC;
    adj->adjust[WATCH_WARN].data.frac = 10.0;
    adj->adjust[WATCH_WARN].weight = 1.0;
    adj->adjust[WATCH_ALERT].data.frac = 20.0;
    adj->adjust[WATCH_ALERT].weight = 1.0;
    adj->adjust[WATCH_CRIT].data.frac = 50.0;
    adj->adjust[WATCH_CRIT].weight = 1.0;
    
    host *h = host_alloc();
    meter *m_load = host_get_meter (h, mi_load);
    meter_setcount (m_load, 0);
    meter_set_frac (m_load, 0, 12.0);
    
    meterwatch *mw = wl->first;
    double badness = 0.0;
    watchtrigger maxtrigger = WATCH_NONE;
    
    while (mw) {
        double nb = calculate_badness (m_load, mw, NULL, &maxtrigger);
        if (nb > badness) badness = nb;
        mw = mw->next;
    }
    
    assert (badness == 8.0);
    assert (maxtrigger == WATCH_ALERT);
    printf ("base badness %.2f mt %i\n", badness, maxtrigger);
    
    maxtrigger = WATCH_NONE;
    badness = 0.0;
    mw = wl->first;

    while (mw) {
        double nb = calculate_badness (m_load, mw, adj, &maxtrigger);
        if (nb > badness) badness = nb;
        mw = mw->next;
    }
    
    assert (badness == 5.0);
    assert (maxtrigger == WATCH_WARN);
    return 0;
}
