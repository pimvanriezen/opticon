#include <libopticon/var_parse.h>
#include <libopticon/log.h>
#include <libopticon/popen.h>
#include <libopticon/glob.h>
#include "opticon-agent.h"
#include "probes.h"
#include "version.h"
#include <sys/select.h>

bool matchlist (const char *val, var *defarr) {
    for (int i=0; i<var_get_count (defarr); ++i) {
        const char *m = var_get_str_atindex (defarr, i);
        if (globcmp (val, m)) return true;
    }
    return false;
}

/** Allocate and initialize a probe object in memory */
probe *probe_alloc (void) {
    probe *res = (probe *) malloc (sizeof (probe));
    conditional_init (&res->pulse);
    pthread_mutex_init (&res->vlock, NULL);
    res->type = PROBE_NONE;
    res->call = NULL;
    res->id = NULL;
    res->prev = res->next = NULL;
    res->vcurrent = res->vold = NULL;
    res->lastpulse = 0;
    res->lastdispatch = time (NULL);
    res->lastreply = res->lastdispatch - 1;
    res->interval = 0;
    res->options = var_alloc();
    return res;
}

var *runprobe_version (probe *self) {
    var *res = var_alloc();
    var *res_agent = var_get_dict_forkey (res, "agent");
    var_set_str_forkey (res_agent, "v", VERSION);
    return res;
}

/** Implementation of the exec probe type. Reads JSON from the
  * stdout of the called program. */
var *runprobe_exec (probe *self) {
    char buffer[4096];
    buffer[0] = 0;
    FILE *proc = popen_safe (self->call, "r");
    int procfd = fileno (proc);
    fd_set fds;
    struct timeval tv;
    
    if (! proc) return NULL;
    while (!feof (proc)) {
        tv.tv_sec = 30;
        tv.tv_usec = 0;
        FD_ZERO (&fds);
        FD_SET (procfd, &fds);
// @todo Windows does not have timeouts for synchronous file operations (select() is for sockets), need to use overlapped io instead)
#if !defined (OS_WINDOWS)
        if (select (procfd+1, &fds, NULL, NULL, &tv) > 0) {
#endif
            size_t res = fread (buffer, 1, 4096, proc);
            if (res && res < 4096) {
                buffer[res] = 0;
                break;
            }
#if !defined (OS_WINDOWS)
        }
        else {
            log_error ("probe_exec timeout on '%s'", self->call);
            break;
        }
#endif
    }
    int pret = pclose_safe (proc);
    int status = WEXITSTATUS (pret);
    var *res = var_alloc();
    
    /* nagios probes don't return json, and have an exit code */
    if (self->type == PROBE_NAGIOS) {
        char *c = strchr (buffer, '\n');
        *c = 0;
        log_info ("Check result: %i %s", status, buffer);
        var_set_int_forkey (res, "status", status);
    }
    else {
        if (! var_parse_json (res, buffer)) {
            log_error ("Error parsing output from <%s>: %s", self->call,
                        parse_error());
            var_free (res);
            return NULL;
        }
    }
    return res;
}

/** Main thread loop for all probes. Waits for a pulse, then gets to
  * work.
  */
void probe_run (thread *t) {
    probe *self = (probe *) t;
    self->vold = self->vcurrent = NULL;
    log_info ("Starting probe <%s>", self->call);
    
    while (1) {
        var *nvar = self->func (self);
        if (nvar) {
            pthread_mutex_lock (&self->vlock);
            if (self->vold) {
                var_free ((var *) self->vold);
                self->vold = NULL;
            }
            self->vold = self->vcurrent;
            pthread_mutex_unlock (&self->vlock);
            self->vcurrent = nvar;
            self->lastreply = time (NULL);
            if (nvar->type != VAR_DICT) {
                log_warn ("Probe <%s> returned non-dict-type %x",
                          self->call, nvar->type);
            }
            else {
                log_debug ("Probe <%s> returned tree with %i branches",
                           self->call, nvar->value.arr.count);
            }
        }
        else {
            log_warn ("Probe <%s> received no data", self->call);
        }

        conditional_wait_fresh (&self->pulse);
        log_debug ("Probe <%s> pulse received", self->call);
    }
}

/** Look up a built-in probe by name */
probefunc_f probe_find_builtin (const char *id) {
    int i = 0;
    while (BUILTINS[i].name) {
        if (strcmp (BUILTINS[i].name, id) == 0) {
            return BUILTINS[i].func;
        }
        i++;
    }
    return NULL;
}

/** Initialize a probelist */
void probelist_init (probelist *self) {
    self->first = self->last = NULL;
}

/** Add a probe to a list */
int probelist_add (probelist *self, probetype t, const char *call, 
                   const char *id, int iv, var *opt) {
    probefunc_f func;
    if (t == PROBE_BUILTIN) func = probe_find_builtin (call);
    else func = runprobe_exec;
    if (! func) return 0;
    
    probe *p = probe_alloc();
    p->type = t;
    p->call = strdup (call);
    p->id = strdup (id);
    p->func = func;
    p->interval = iv;
    var_copy (p->options, opt);
    
    if (self->last) {
        p->prev = self->last;
        self->last->next = p;
        self->last = p;
    }
    else {
        self->first = self->last = p;
    }
    return 1;
}

/** Start all probes in a list */
void probelist_start (probelist *self) {
    probe *p = self->first;
    while (p) {
        thread_init (&p->thr, probe_run, NULL);
        p = p->next;
    }
}

/** Cancels all probes in a list */
void probelist_cancel (probelist *self) {
    probe *p = self->first;
    while (p) {
        thread_cancel (&p->thr);
    }
}
