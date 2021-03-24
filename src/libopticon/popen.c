#include <libopticon/popen.h>
#include <pthread.h>

static pthread_mutex_t popen_mutex;

void popen_init (void) {
    pthread_mutex_init (&popen_mutex, NULL);
}

FILE *popen_safe (const char *command, const char *mode) {
    FILE *res = NULL;
    pthread_mutex_lock (&popen_mutex);
    res = popen (command, mode);
    pthread_mutex_unlock (&popen_mutex);
    return res;
}

int pclose_safe (FILE *stream) {
    int res = -1;
    pthread_mutex_lock (&popen_mutex);
    res = pclose (stream);
    pthread_mutex_unlock (&popen_mutex);
    return res;
}
