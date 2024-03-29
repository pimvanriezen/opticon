#include <libopticon/react.h>
#include <stdlib.h>
#include <string.h>

pathnode OPTICONF_ROOT;

/*/ ======================================================================= /*/
/** Allocate a pathnode */
/*/ ======================================================================= /*/
pathnode *pathnode_alloc (void) {
    pathnode *self = malloc (sizeof (pathnode));
    if (! self) return NULL;
    
    self->prev = self->next = self->parent = 
                 self->first = self->next = NULL;
    self->idmatch[0] = 0;
    self->rcount = 0;
    self->reactions = NULL;
    return self;
}

/*/ ======================================================================= /*/
/** Add a reaction to a pathnode */
/*/ ======================================================================= /*/
void pathnode_add_reaction (pathnode *self, reaction_f f) {
    if (self->rcount == 0) {
        self->reactions = malloc (sizeof (reaction_f));
        self->reactions[0] = f;
        self->rcount = 1;
    }
    else {
        self->rcount++;
        self->reactions = (reaction_f *) 
            realloc (self->reactions, self->rcount * sizeof (reaction_f));
        self->reactions[self->rcount -1] = f;
    }
}

/*/ ======================================================================= /*/
/** Find a pathnode that matches against a given id. Wildcards will
  * be honored. */
/*/ ======================================================================= /*/
pathnode *pathnode_find_match (pathnode *self, const char *id) {
    pathnode *res = self->first;
    if (! res) return NULL;
    while (res) {
        if (res->idmatch[0] == '*' && res->idmatch[1] == 0) return res;
        if (strcmp (res->idmatch, id) == 0) return res;
        res = res->next;
    }
    return NULL;
}

/*/ ======================================================================= /*/
/** Find or create a pathnode with a specific match string. */
/*/ ======================================================================= /*/
pathnode *pathnode_get_idmatch (pathnode *self, const char *id) {
    pathnode *res = self->first;
    while (res) {
        if (strcmp (res->idmatch, id) == 0) return res;
        res = res->next;
    }
    res = pathnode_alloc();
    strncpy (res->idmatch, id, 127);
    res->idmatch[127] = 0;
    res->parent = self;
    if (self->first) {
        res->prev = self->last;
        self->last->next = res;
        self->last = res;
    }
    else {
        self->first = res;
        self->last = res;
    }
    return res;
}

/*/ ======================================================================= /*/
/** Set up a configuration reaction to a specific configuration
  * path, separated by forward slashes. A path match against '*'
  * means that the reaction function will be called for each
  * child of the node.
  * \param path The slash-separated path match.
  * \param f The reaction function to call. */
/*/ ======================================================================= /*/
void opticonf_add_reaction (const char *path, reaction_f f) {
    pathnode *T = &OPTICONF_ROOT;
    char *oldstr, *tstr, *token;
    oldstr = tstr = strdup (path);
    while ((token = strsep (&tstr, "/")) != NULL) {
        T = pathnode_get_idmatch (T, token);
    }
    pathnode_add_reaction (T, f);
    free (oldstr);
}

/*/ ======================================================================= /*/
/** Internal handler for opticonf_handle_config(). */
/*/ ======================================================================= /*/
void opticonf_handle (var *conf, pathnode *path) {
    uint32_t gen = (conf->root) ? conf->root->generation : conf->generation;
    int c = var_get_count (conf);
    updatetype tp = UPDATE_NONE;
    var *vcrsr;
    pathnode *pcrsr;
    for (int i=0; i<c; ++i) {
        vcrsr = var_find_index (conf, i);
        pcrsr = pathnode_find_match (path, vcrsr->id);
        if (! pcrsr) continue;
        if (pcrsr->rcount) {
            tp = UPDATE_NONE;
            if ((vcrsr->generation) < gen) {
                tp = UPDATE_REMOVE;
            }
            else if (vcrsr->lastmodified == gen) {
                if ((vcrsr->firstseen) == gen) tp = UPDATE_ADD;
                else tp = UPDATE_CHANGE;
            }
            if (tp != UPDATE_NONE) {
                for (int ri=0; ri<(pcrsr->rcount); ++ri) {
                    pcrsr->reactions[ri](vcrsr->id, vcrsr, tp);
                }
            }
        }
        if (vcrsr->type == VAR_ARRAY || vcrsr->type == VAR_DICT) {
            opticonf_handle (vcrsr, pcrsr);
        }
    }
}

/*/ ======================================================================= /*/
/** Feed a configuration variable space to the reaction framework.
  * Consequent calls with the same space will only communicate
  * actual changes. */
/*/ ======================================================================= /*/
void opticonf_handle_config (var *conf) {
    pathnode *T = &OPTICONF_ROOT;
    opticonf_handle (conf, T);
}
