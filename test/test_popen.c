#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <libopticon/var.h>
#include <libopticon/popen.h>

int main (int argc, const char *argv[]) {
    popen_init();
    char buf[1023];
    
    var *venv = var_alloc();
    var_set_str_forkey (venv, "testval", "fourtytwo");
    FILE *f = popen_safe_env ("./test_popen.sh","r",venv);
    if (f) {
        while (! feof (f)) {
            buf[0] = 0;
            fgets (buf, 1023, f);
            if (*buf) printf ("%s", buf);
        }
        pclose_safe (f);
    }
    return 0;
}
