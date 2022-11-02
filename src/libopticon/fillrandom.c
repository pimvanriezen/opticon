#include <stdio.h>
#include <fcntl.h>

#include <libopticon/fillrandom.h>

#if defined (OS_WINDOWS)
    #include <windows.h>
    #include <bcrypt.h>
#else
    #include <sys/types.h>
    #include <sys/uio.h>
    #include <unistd.h>
#endif

/*/ ======================================================================= /*/
/** Generate random bytes into out param */
/*/ ======================================================================= /*/
void fillrandom (void *out, size_t sz) {
    #if defined (OS_WINDOWS)
        BCryptGenRandom(NULL, out, sz, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    #else
        int fdevr = open ("/dev/random",O_RDONLY);
        read (fdevr, out, sz);
        close (fdevr);
    #endif
}