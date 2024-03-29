#include "wordlist.h"
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#define isspace(q) ((q==' ')||(q=='\t'))

static char *findspace (char *src) {
    register char *t1;
    register char *t2;
    
    t1 = strchr (src, ' ');
    t2 = strchr (src, '\t');
    if (t1 && t2) {
        if (t1<t2) t2 = NULL;
        else t1 = NULL;
    }
    
    if (t1) return t1;
    return t2;
}

int wordcount (const char *string) {
    int ln;
    int cnt;
    int i;
    
    cnt = 1;
    ln = strlen (string);
    i = 0;
    
    while (i < ln) {
        if (isspace(string[i])) {
            while (isspace(string[i])) ++i;
            if (string[i]) ++cnt;
        }
        ++i;
    }
    
    return cnt;
}

int wordsepcount (const char *string, char sep) {
    int ln;
    int cnt;
    int i;
    
    cnt = 1;
    ln = strlen (string);
    i = 0;
    
    while (i < ln) {
        if (string[i] == sep) ++cnt;
        ++i;
    }
    
    return cnt;
}

wordlist *wordlist_create (void) {
    wordlist *res;
    
    res = malloc (sizeof (wordlist));
    res->argc = 0;
    res->argv = NULL;
    return res;
}

void wordlist_add (wordlist *arg, const char *elm) {
    if (arg->argc) {
        arg->argv = (char **) realloc (arg->argv, (arg->argc + 1) * sizeof (char *));
        arg->argv[arg->argc++] = strdup (elm);
    }
    else {
        arg->argc = 1;
        arg->argv = malloc (sizeof (char *));
        arg->argv[0] = strdup (elm);
    }
}

wordlist *wordlist_make (const char *string) {
    wordlist    *result;
    char         *rightbound;
    char         *word;
    char         *crsr;
    int           count;
    int           pos;
    
    crsr = (char *) string;
    while ((*crsr == ' ')||(*crsr == '\t')) ++crsr;
    
    result = malloc (sizeof (wordlist));
    count = wordcount (crsr);
    result->argc = count;
    result->argv = malloc (count * sizeof (char *));
    
    pos = 0;
    
    while ((rightbound = findspace (crsr))) {
        word = malloc ((rightbound-crsr+3) * sizeof (char));
        memcpy (word, crsr, rightbound-crsr);
        word[rightbound-crsr] = 0;
        result->argv[pos++] = word;
        crsr = rightbound;
        while (isspace(*crsr)) ++crsr;
    }
    if (*crsr) {
        word = strdup (crsr);
        result->argv[pos++] = word;
    }
    
    return result;
}

wordlist *wordlist_split (const char *string, char sep) {
    wordlist    *result;
    char         *rightbound;
    char         *word;
    char         *crsr;
    int           count;
    int           pos = 0;
    
    crsr = (char *) string;

    result = malloc (sizeof (wordlist));
    count = wordsepcount (crsr, sep);
    result->argc = count;
    result->argv = (char **) calloc (count, sizeof (char *));

    while ((rightbound = strchr (crsr, sep))) {
        word = malloc ((rightbound-crsr+3) * sizeof (char));
        memcpy (word, crsr, rightbound-crsr);
        word[rightbound-crsr] = 0;
        result->argv[pos++] = word;
        crsr = rightbound;
        crsr++;
    }
    if (*crsr) {
        word = strdup (crsr);
        if (*word) {
            int ln = strlen(word);
            if (ln && word[ln-1] == '\n') word[ln-1] = 0;
        }
        result->argv[pos++] = word;
    }
    if (pos < result->argc) result->argc = pos;
    return result;
}

void wordlist_free (wordlist *lst) {
    int i;
    for (i=0;i<lst->argc;++i) {
        free (lst->argv[i]);
        lst->argv[i] = NULL;
    }
    free (lst->argv);
    lst->argv = NULL;
    free (lst);
}
