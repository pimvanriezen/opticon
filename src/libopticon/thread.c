#include <libopticon/thread.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

static pthread_key_t *TKEY = NULL;

/*/ ======================================================================= /*/
/** Post-cancel/post-exit cleanup routing. Will call the thread-defined
  * cancel routine if there is any. */
/*/ ======================================================================= /*/
void thread_cleanup (void *dt) {
    thread *self = (thread *) dt;
    if (self->cancel) { self->cancel (self); }
    conditional_signal (self->cshutdown);
}

/*/ ======================================================================= /*/
/** Call a thread's run function */
/*/ ======================================================================= /*/
void *thread_spawn (void *dt) {
    if (!TKEY) {
        TKEY = malloc (sizeof (pthread_key_t));
        pthread_key_create (TKEY, NULL);
    }
    thread *self = (thread *) dt;
    self->isrunning = 1;
    pthread_cleanup_push (thread_cleanup, self);
    pthread_setspecific (*TKEY, self);
    self->run (self);
    pthread_cleanup_pop(0);
    self->isrunning = 0;
    thread_cleanup (self);
    return NULL;
}

/*/ ======================================================================= /*/
/** Set a thread's name field */
/*/ ======================================================================= /*/
void thread_setname (thread *self, const char *nm) {
    strncpy (self->name, nm, 15);
    self->name[15] = 0;
}

/*/ ======================================================================= /*/
/** Return pointer to the currently running thread, or NULL if not in a
    thread */
/*/ ======================================================================= /*/
thread *thread_current (void) {
    if (! TKEY) return NULL;
    return (thread *) pthread_getspecific (*TKEY);
}

/*/ ======================================================================= /*/
/** Allocate and spawn a thread */
/*/ ======================================================================= /*/
thread *thread_create (run_f run, cancel_f cancel) {
    if (!TKEY) {
        TKEY = malloc (sizeof (pthread_key_t));
        pthread_key_create (TKEY, NULL);
    }
    thread *self = (thread *) malloc (sizeof (thread));
    thread_init (self, run, cancel);
    return self;
}

/*/ ======================================================================= /*/
/** Allocate and initialize a conditional object */
/*/ ======================================================================= /*/
conditional *conditional_create (void) {
    conditional *c = (conditional *) malloc (sizeof (conditional));
    conditional_init (c);
    return c;
}

/*/ ======================================================================= /*/
/** Initialize a conditional */
/*/ ======================================================================= /*/
void conditional_init (conditional *self) {
    pthread_mutexattr_init (&self->mattr);
    pthread_mutex_init (&self->mutex, &self->mattr);
    pthread_cond_init (&self->cond, NULL);
    self->queue = 0;
}

/*/ ======================================================================= /*/
/** Add a signal to a conditional's queue and send it out */
/*/ ======================================================================= /*/
void conditional_signal (conditional *self) {
    pthread_mutex_lock (&self->mutex);
    self->queue++;
    pthread_cond_signal (&self->cond);
    pthread_mutex_unlock (&self->mutex);
}

/*/ ======================================================================= /*/
/** Wait for a signal (or pick a backlogged one off the queue) */
/*/ ======================================================================= /*/
void conditional_wait (conditional *self) {
    pthread_mutex_lock (&self->mutex);
    if (self->queue) {
        self->queue--;
        pthread_mutex_unlock (&self->mutex);
        return;
    }
    while (pthread_cond_wait (&self->cond, &self->mutex)!=0) {}
    self->queue--;
    pthread_mutex_unlock (&self->mutex);
}

/*/ ======================================================================= /*/
/** Wait for a new signal (queued doesn't count) */
/*/ ======================================================================= /*/
void conditional_wait_fresh (conditional *self) {
    pthread_mutex_lock (&self->mutex);
    self->queue = 0;
    while (pthread_cond_wait (&self->cond, &self->mutex)!=0) {}
    self->queue--;
    pthread_mutex_unlock (&self->mutex);
}

/*/ ======================================================================= /*/
/** Clean up pthread data inside a conditional */
/*/ ======================================================================= /*/
void conditional_cleanup (conditional *c) {
    pthread_mutex_destroy (&c->mutex);
    pthread_mutexattr_destroy (&c->mattr);
    pthread_cond_destroy (&c->cond);
}

/*/ ======================================================================= /*/
/** Demolish a conditional */
/*/ ======================================================================= /*/
void conditional_free (conditional *c) {
    conditional_cleanup (c);
    free (c);
}

/*/ ======================================================================= /*/
/** Initialize a thread structure and spawn it */
/*/ ======================================================================= /*/
void thread_init (thread *self, run_f run, cancel_f cancel) {
    self->run = run;
    self->cancel = cancel;
    self->isrunning = 0;
    self->cshutdown = conditional_create();
    sprintf (self->name, "thread-%06" PRIx64, ((uint64_t)self) & 0xffffff);
    pthread_attr_init (&self->tattr);
    pthread_create (&self->thread, &self->tattr, thread_spawn, self);
}

/*/ ======================================================================= /*/
/** Free up a thread's resources. Thread should already be
  * canceled or shut down at this point. */
/*/ ======================================================================= /*/
void thread_free (thread *self) {
    conditional_free (self->cshutdown);
    free (self); /* and your mind will follow */
}

/*/ ======================================================================= /*/
/** Cancel a thread and wait for it to exit */
/*/ ======================================================================= /*/
void thread_cancel (thread *self) {
    pthread_cancel (self->thread);
    conditional_wait (self->cshutdown);
}
