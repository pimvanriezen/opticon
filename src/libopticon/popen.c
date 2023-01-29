#include <libopticon/popen.h>
#include <libopticon/var.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/errno.h>

extern char **environ;

typedef struct popen_item {
    struct popen_item *next;
    FILE *f;
    pid_t pid;
} popen_item;


static pthread_mutex_t popen_mutex;
static popen_item *popen_list = NULL;

/*/ ======================================================================= /*/
/** Initializes internal data. */
/*/ ======================================================================= /*/
void popen_init (void) {
    pthread_mutex_init (&popen_mutex, NULL);
    popen_list = NULL;
}

/*/ ======================================================================= /*/
/** Read the current environment into a var dictionary. */
/*/ ======================================================================= /*/
var *env_to_var (void) {
    var *v = var_alloc();
    char **c = environ;
    while (*c) {
        char *copy = strdup (*c);
        char *splt = strchr (copy, '=');
        if (splt) {
            *splt = 0;
            var_set_str_forkey (v, copy, splt+1);
        }
        free (copy);
        c++;
    }
    return v;
}

/*/ ======================================================================= /*/
/** Convert a var dictionary into an environment pointer. */
/*/ ======================================================================= /*/
char **var_to_env (var *v) {
    char **res = (char **) malloc ((1+var_get_count (v))*sizeof(char *));
    int respos = 0;
    var *c = var_first (v);
    while (c) {
        const char *str = var_get_str (c);
        if (str) {
            int len = 4 + strlen (c->id) + strlen (str) + 2;
            char *tmp = (char *) malloc (len * sizeof (char));
            strcpy (tmp, "OPT_");
            char *cc = c->id;
            char *oc = tmp+4;
            while (*cc) {
                *oc = toupper (*cc);
                cc++;
                oc++;
            }
            sprintf (oc, "=%s", str);
            res[respos++] = tmp;
        }            
        c = c->next;
    }
    res[respos] = NULL;
    return res;
}

/*/ ======================================================================= /*/
/** Free memory for an environment array */
/*/ ======================================================================= /*/
void free_env (char **e) {
    char **crsr = e;
    while (*crsr) {
        free (*crsr);
        crsr++;
    }
    free (e);
}
/*/ ======================================================================= /*/
/** Implement a thread safe popen() which allows extra environment
  * variables to be merged in from a var object. */
/*/ ======================================================================= /*/
FILE *popen_safe_env (const char *command, const char *mode, var *opt) {
    FILE *fp = NULL;
    pthread_mutex_lock (&popen_mutex);
    
    popen_item *pi;
    popen_item *po;
    int pipe_fd[2];
    int parent_fd;
    int child_fd;
    int child_writing;
    pid_t pid;
    
    child_writing = 0;
    if (mode[0] != 'w') {
        child_writing = 1;
    }
    
    pi = malloc (sizeof (popen_item));
    if (pi) {
        pi->next = NULL;
        if (pipe (pipe_fd) == 0) {
            child_fd = pipe_fd[child_writing];
            parent_fd = pipe_fd[1-child_writing];
            
            if ((fp = fdopen (parent_fd, mode))) {
                if ((pid = fork()) == 0) {
                    close (0);
                    close (1);
                    close (2);
                    
                    open ("/dev/null",O_RDONLY);
                    
                    dup2 (child_fd, 1);
                    close (child_fd);
                    
                    open ("/dev/null",O_WRONLY);

                    for (po = popen_list; po; po = po->next) {
                        close (fileno (po->f));
                    }
                    
                    const char **argv = malloc (5 * sizeof (char *));
                    argv[0] = "/bin/sh";
                    argv[1] = "-c";
                    argv[2] = command;
                    argv[3] = NULL;
                    
                    if (! opt) {
                        execv (argv[0], (char *const *) argv);
                        exit(127);
                    }

                    var *env = env_to_var();
                    var_merge (env, opt);
                    char **e = var_to_env (env);
                    
                    execve (argv[0], (char *const *) argv, e);
                    exit(127);
                }
                close (child_fd);
                pi->pid = pid;
                pi->f = fp;
                pi->next = popen_list;
                popen_list = pi;
                pthread_mutex_unlock (&popen_mutex);
                return fp;
            }
            else {
                close (parent_fd);
                close (child_fd);
            }
        }
        
        free (pi);
    }
    
    pthread_mutex_unlock (&popen_mutex);
    return fp;
}

/*/ ======================================================================= /*/
/** Regular popen, but threadsafe. */
/*/ ======================================================================= /*/
FILE *popen_safe (const char *command, const char *mode) {
    return popen_safe_env (command, mode, NULL);
}

/*/ ======================================================================= /*/
/** Accompanying pclose equivalent. */
/*/ ======================================================================= /*/
int pclose_safe (FILE *fd) {
    int status;
    pid_t pid;
    pthread_mutex_lock (&popen_mutex);
    
    popen_item *crsr, *p;
    crsr = popen_list;
    if (crsr) p = crsr->next;
    
    if (crsr->f == fd) {
        popen_list = crsr->next;
        p = crsr;
    }
    else while (p) {
        if (p->f == fd) {
            crsr->next = p->next;
            break;
        }
        crsr = p;
        p = p->next;
    }
    
    if (p) {
        pid = p->pid;
        free (p);
        fclose (fd);
        
        pthread_mutex_unlock (&popen_mutex);
        
        while (1) {
            if (waitpid (pid, &status, 0) >= 0) return status;
            if (errno != EINTR) break;
        }
        return -1;
    }
    
    status = fclose (fd);
    pthread_mutex_unlock (&popen_mutex);
    return status;
}
