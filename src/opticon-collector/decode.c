#include <libopticon/datatypes.h>
#include <libopticon/util.h>
#include <libopticon/auth.h>
#include <libopticon/codec_pkt.h>
#include <libopticon/codec_json.h>
#include <libopticon/compress.h>
#include <libopticon/ioport_file.h>
#include <libopticon/aes.h>
#include <libopticon/log.h>
#include <assert.h>
#include <stdio.h>
#include <sys/errno.h>

int main (int argc, const char *argv[]) {
    if (argc<2) {
        fprintf (stderr, "%% Usage: opticon-decode <packet>\n");
        return 1;
    }
    
    FILE *F = fopen (argv[1], "r");
    if (! F) {
        fprintf (stderr, "%% Could not open file: %s\n", strerror(errno));
        return 1;
    }
    
    ioport *io_in = ioport_create_filereader (F);
    ioport *io_out = ioport_create_filewriter (stdout);
    codec *CJ = codec_create_json();
    codec *CP = codec_create_pkt();
    
    host *H = host_alloc();
    host_begin_update (H, time(NULL));
    if (! codec_decode_host (CP, io_in, H)) {
        fprintf (stderr, "%% Decode failed\n");
        return 1;
    }
    
    codec_encode_host (CJ, io_out, H);
    fclose (F);
    return 0;
}
