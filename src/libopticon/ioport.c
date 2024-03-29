#include <libopticon/ioport.h>
#include <math.h>
#include <arpa/inet.h>
#include <stdarg.h>

#if defined(__linux__)
#  include <endian.h>
#elif defined(__FreeBSD__) || defined(__NetBSD__)
#  include <sys/endian.h>
#elif defined(__OpenBSD__)
#  include <sys/types.h>
#  define be16toh(x) betoh16(x)
#  define be32toh(x) betoh32(x)
#  define be64toh(x) betoh64(x)
#else
#  define be64toh(x) ntohll(x)
#  define htobe64(x) htonll(x)
#endif

/*/ ======================================================================= /*/
/** Get the amount of available buffer space for writing into an
  * ioport.  */
/*/ ======================================================================= /*/
size_t ioport_write_available (ioport *io) {
    return io->write_available (io);
}

/*/ ======================================================================= /*/
/** Write a blob of data to an ioport.
  * \param io The ioport to write to
  * \param data The blob
  * \param sz The blob size
  * \return 1 on success, 0 on failure  */
/*/ ======================================================================= /*/
bool ioport_write (ioport *io, const void *data, size_t sz) {
    if (io->bitpos) {
        if (! ioport_flush_bits (io)) return 0;
    }
    return io->write (io, (const char *) data, sz);
}

/*/ ======================================================================= /*/
/** Write formatted ascii to the port */
/*/ ======================================================================= /*/
bool ioport_printf (ioport *io, const char *fmt, ...) {
    char buffer[4096];
    buffer[0] = 0;
    va_list ap;
    va_start (ap, fmt);
    vsnprintf (buffer, 4096, fmt, ap);
    va_end (ap);
    buffer[4095] = 0;
    return ioport_write (io, buffer, strlen (buffer));
}

/*/ ======================================================================= /*/
/** Write a single byte to an ioport.
  * \param io The ioport
  * \param b The byte to write
  * \return 1 on success, 0 on failure  */
/*/ ======================================================================= /*/
bool ioport_write_byte (ioport *io, uint8_t b) {
    if (io->bitpos) {
        if (! ioport_flush_bits (io)) return 0;
    }
    return io->write (io, (const char *) &b, 1);
}

/*/ ======================================================================= /*/
/** Write a raw uuid to an ioport.
    \param io The ioport
    \param u The uuid
    \return 1 on success, 0 on failure */
/*/ ======================================================================= /*/
bool ioport_write_uuid (ioport *io, uuid u) {
    uint8_t out[16];
    out[0] = (u.msb & 0xff00000000000000) >> 56;
    out[1] = (u.msb & 0x00ff000000000000) >> 48;
    out[2] = (u.msb & 0x0000ff0000000000) >> 40;
    out[3] = (u.msb & 0x000000ff00000000) >> 32;
    out[4] = (u.msb & 0x00000000ff000000) >> 24;
    out[5] = (u.msb & 0x0000000000ff0000) >> 16;
    out[6] = (u.msb & 0x000000000000ff00) >> 8;
    out[7] = (u.msb & 0x00000000000000ff);
    out[8] = (u.lsb & 0xff00000000000000) >> 56;
    out[9] = (u.lsb & 0x00ff000000000000) >> 48;
    out[10] = (u.lsb & 0x0000ff0000000000) >> 40;
    out[11] = (u.lsb & 0x000000ff00000000) >> 32;
    out[12] = (u.lsb & 0x00000000ff000000) >> 24;
    out[13] = (u.lsb & 0x0000000000ff0000) >> 16;
    out[14] = (u.lsb & 0x000000000000ff00) >> 8;
    out[15] = (u.lsb & 0x00000000000000ff);
    return ioport_write (io, (const char *)out, 16);
}

/*/ ======================================================================= /*/
/** Close an ioport. Also deallocates associated memory. Pointer will
  * be invalid.
  * \param io The ioport */
/*/ ======================================================================= /*/
void ioport_close (ioport *io) {
    io->close (io);
}

/*/ ======================================================================= /*/
/** Get access to the buffer, if any */
/*/ ======================================================================= /*/
char *ioport_get_buffer (ioport *io) {
    return io->get_buffer (io);
}

/*/ ======================================================================= /*/
/** Reset the ioport read position */
/*/ ======================================================================= /*/
void ioport_reset_read (ioport *io) {
    return io->reset_read (io);
}

uint8_t BITMASKS[9] = {0x00, 0x01, 0x03, 0x07, 0x0f, 0x1f, 0x3f, 0x7f, 0xff};

/*/ ======================================================================= /*/
/** Write a specific number of bits to an ioport. Subsequent calls
  * will continue at the last bit offset. Bits are put into the
  * stream little-endian.
  * \param io The ioport
  * \param d The data
  * \param bits The number of bits to write (max 8) */
/*/ ======================================================================= /*/
bool ioport_write_bits (ioport *io, uint8_t d, uint8_t bits) {
    if (bits>8) return 0;
    uint8_t bits1 = (io->bitpos + bits < 8) ? bits : 8-io->bitpos;
    uint8_t bits2 = bits - bits1;
    uint8_t shift = (8 - io->bitpos) - bits1;
    uint8_t data1 = ((d >> bits2) & (BITMASKS[bits1])) << shift;
    uint8_t data2 = d & BITMASKS[bits2];
    
    io->bitbuffer |= data1;
    io->bitpos += bits1;
    if (io->bitpos > 7) {
        if (! io->write (io, (const char *)&(io->bitbuffer), 1)) {
            return false;
        }
        io->bitpos = bits2;
        io->bitbuffer = bits2 ? data2 << (8-bits2) : 0;
    }
    return true;
}

/*/ ======================================================================= /*/
/** Flush any outstanding bits left over from earlier calls to
  * ioport_write_bits(). This final byte will be 0-padded.
  * \param io The ioport.
  * \return 1 on success, 0 on failure. */
/*/ ======================================================================= /*/
bool ioport_flush_bits (ioport *io) {
    if (! io->bitpos) return true;
    if (! io->write (io, (const char *)&(io->bitbuffer), 1)) return false;
    io->bitpos = io->bitbuffer = 0;
    return true;
}

static const char *ENCSET = "abcdefghijklmnopqrstuvwxyz#/-_"
                            " 0123456789ABCDEFGHJKLMNPQSTUVWXZ.";

static uint8_t DECSET[128] = {
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    30,255,255,26,255,255,255,255,255,255,255,255,255,28,63,27,31,
    32,33,34,35,36,37,38,39,40,255,255,255,255,255,255,255,41,42,43,
    44,45,46,47,48,255,49,50,51,52,53,255,54,55,255,56,57,58,59,60,61,
    255,62,255,255,255,255,29,255,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,
    15,16,17,18,19,20,21,22,23,24,25,255,255,255,255
};

/*/ ======================================================================= /*/
/** Write an encoded string to an ioport. The string will be written
  * as either a plain pascal string, or a stream of 6-bit characters.
  * \param io The ioport
  * \param str The string to encode (maximum length 127 chars)
  * \return 1 on success, 0 on failure */
/*/ ======================================================================= /*/
bool ioport_write_encstring (ioport *io, const char *str) {
    uint8_t i;
    size_t len = strlen (str);
    if (len > 127) return false;
    for (i=0;i<len;++i) {
        if ((str[i]>127) || (DECSET[(int)str[i]] == 255)) {
            if (! ioport_write_byte (io, len)) return false;
            return ioport_write (io, str, len);
        }
    }
    if (! ioport_write_byte (io, len | 0x80)) return false;
    for (i=0;i<len;++i) {
        if (! ioport_write_bits (io, DECSET[(int)str[i]], 6)) return false;
    }
    return ioport_flush_bits (io);
}

/*/ ======================================================================= /*/
/** Encode a floating point into an ioport as a 16-bit fixed point
  * number.
  * \param io The ioport
  * \param d The value to encode (between 0.0 and 255.999)
  * \return 1 on success, 0 on failure */
/*/ ======================================================================= /*/
bool ioport_write_encfrac (ioport *io, double d) {
    if (d <= 0.0) return ioport_write (io, "\0\0", 2);
    if (d >= 192.0) {
        double wval = floor (d / 2.0);
        int i=0;
        
        while ((wval>255.0) && (i<63)) {
            wval = floor (wval / 2.0);
            i++;
        }
        if (wval>255.0) {
            return ioport_write_byte (io,255) &&
                   ioport_write_byte (io,255);
        }
        return ioport_write_byte (io,192+i) &&
               ioport_write_byte (io,(uint8_t) wval);
    }
    double fl = floor (d);
    double fr = (d - fl) * 255;
    if (! ioport_write_byte (io, (uint8_t) fl)) return false;
    return ioport_write_byte (io, (uint8_t) fr);
}

/*/ ======================================================================= /*/
/** Encode a 63 bits unsigned integer into an ioport.
  * \param io The iobuffer
  * \param i The integer to encode
  * \return 1 on success, 0 on failure */
/*/ ======================================================================= /*/
bool ioport_write_encint (ioport *io, uint64_t i) {
    uint64_t msk;
    uint8_t byte;
    uint8_t started = 0;
    if (! i) return ioport_write_byte (io, 0);
    for (int bitpos=56;bitpos>=0;bitpos-=7) {
        msk = 0x7f << bitpos;
        byte = (i & msk) >> bitpos;
        if (byte || started) {
            if (bitpos) byte |= 0x80;
            started = 1;
            if (! ioport_write_byte (io, byte)) return false;
        }
    }
    return true;
}

/*/ ======================================================================= /*/
/** Write a raw 64 bits unsigned integer to an ioport in an endian-
  * safe fashion.
  * \param io The ioport
  * \param i The integer to encode
  * \return 1 on success, 0 on failure */
/*/ ======================================================================= /*/
bool ioport_write_u64 (ioport *io, uint64_t i) {
    uint64_t netorder = htobe64 (i);
    return ioport_write (io, (const char *)&netorder, sizeof (netorder));
}

/*/ ======================================================================= /*/
/** Write a raw 32 bits unsigned integer to an ioport in an endian-
  * safe fashion.
  * \param io The ioport
  * \param i The integer to encode
  * \return 1 on success, 0 on failure */
/*/ ======================================================================= /*/
bool ioport_write_u32 (ioport *io, uint32_t i) {
    uint32_t netorder = htonl (i);
    return ioport_write (io, (const char *)&netorder, sizeof(netorder));
}

/*/ ======================================================================= /*/
/** Get the amount of bytes left for reading out of the ioport. */
/*/ ======================================================================= /*/
size_t ioport_read_available (ioport *io) {
    return io->read_available (io);
}

/*/ ======================================================================= /*/
/** Read data from an ioport.
  * \param io The ioport
  * \param into The buffer to read to
  * \param sz The number of bytes to read
  * \return 1 on success, 0 on failure */
/*/ ======================================================================= /*/
bool ioport_read (ioport *io, void *into, size_t sz) {
    io->bitpos = io->bitbuffer = 0;
    return io->read (io, (char *) into, sz);
}

/*/ ======================================================================= /*/
/** Read a uuid from an ioport */
/*/ ======================================================================= /*/
uuid ioport_read_uuid (ioport *io) {
    uuid u = {0,0};
    uint8_t buf[16];
    if (ioport_read (io, (char*)buf, 16)) {
        u.msb = (uint64_t)buf[0] << 56 |
                (uint64_t)buf[1] << 48 |
                (uint64_t)buf[2] << 40 |
                (uint64_t)buf[3] << 32 |
                (uint64_t)buf[4] << 24 |
                (uint64_t)buf[5] << 16 |
                (uint64_t)buf[6] << 8 |
                (uint64_t)buf[7];
        u.lsb = (uint64_t)buf[8] << 56 |
                (uint64_t)buf[9] << 48 |
                (uint64_t)buf[10] << 40 |
                (uint64_t)buf[11] << 32 |
                (uint64_t)buf[12] << 24 |
                (uint64_t)buf[13] << 16 |
                (uint64_t)buf[14] << 8 |
                (uint64_t)buf[15];
    }
    return u;
}

/*/ ======================================================================= /*/
/** Read a byte from an ioport */
/*/ ======================================================================= /*/
uint8_t ioport_read_byte (ioport *io) {
    uint8_t res = 0;
    ioport_read (io, (char *)&res, 1);
    return res;
}

/*/ ======================================================================= /*/
/** Read a bitstream out of an ioport */
/*/ ======================================================================= /*/
uint8_t ioport_read_bits (ioport *io, uint8_t numbits) {
    if (numbits > 8) return 0;
    if (! numbits) return 0;
    if (! io->bitpos) io->bitbuffer = ioport_read_byte (io);
    uint8_t res = 0;
    uint8_t bits1 = (numbits + io->bitpos > 8) ? 8-io->bitpos : numbits;
    uint8_t shift = (8 - io->bitpos) - bits1;
    uint8_t mask1 = BITMASKS[bits1] << shift;
    uint8_t bits2 = numbits - bits1;
    uint8_t shift2 = 8-bits2;
    uint8_t mask2 = bits2 ? BITMASKS[bits2] << shift2 : 0;

    res = (io->bitbuffer & mask1) >> shift << bits2;
    if (bits2) {
        io->bitbuffer = ioport_read_byte (io);
        res |= (io->bitbuffer & mask2) >> shift2;
        io->bitpos = bits2;
    }
    else io->bitpos += bits1;
    if (io->bitpos > 8) io->bitpos = 0;
    return res;
}

/*/ ======================================================================= /*/
/** Read an encoded string from an ioport.
  * \param io The ioport
  * \param into The string buffer (size 128). */
/*/ ======================================================================= /*/
bool ioport_read_encstring (ioport *io, char *into) {
    io->bitpos = io->bitbuffer = 0;
    uint8_t sz = ioport_read_byte (io);
    if (! sz) return false;
    
    if (sz < 0x80) {
        if (! ioport_read (io, into, sz)) return false;
        into[sz] = '\0';
        return true;
    }
    
    char *crsr = into;
    sz = sz & 0x7f;
    uint8_t c =0;
    
    
    for (uint8_t i=0; i<sz; ++i) {
        c = ioport_read_bits (io, 6);
        *crsr++ = ENCSET[c];
    }
    if (sz < 127) *crsr = '\0';
    else into[127] = '\0';
    return true;
}

/*/ ======================================================================= /*/
/** Read a raw unsigned 64 bits integer from an ioport */
/*/ ======================================================================= /*/
uint64_t ioport_read_u64 (ioport *io) {
    io->bitpos = io->bitbuffer = 0;
    uint64_t dt;
    if (! ioport_read (io, (char *) &dt, sizeof (dt))) return 0;
    return be64toh (dt);
}

/*/ ======================================================================= /*/
/** Read a raw unsigned 32 bits integer from an ioport */
/*/ ======================================================================= /*/
uint32_t ioport_read_u32 (ioport *io) {
    io->bitpos = io->bitbuffer = 0;
    uint32_t dt;
    if (! ioport_read (io, (char *)&dt, sizeof (dt))) return 0;
    return ntohl (dt);
}

/*/ ======================================================================= /*/
/** Read an encoded 63 bits integer from an ioport */
/*/ ======================================================================= /*/
uint64_t ioport_read_encint (ioport *io) {
    uint64_t res = 0ULL;
    uint64_t d;
    
    do {
        d = (uint64_t) ioport_read_byte (io);
        if (res) res <<= 7;
        res |= (d & 0x7f);
    } while (d & 0x80);
    
    return res;
}

/*/ ======================================================================= /*/
/** Read an encoded fractional number from an ioport */
/*/ ======================================================================= /*/
double ioport_read_encfrac (ioport *io) {
    double res = 0;
    uint8_t intpart = ioport_read_byte (io);
    if (intpart < 192) {
        res = intpart;
        res += ((double)ioport_read_byte(io))/256.0;
    }
    else {
        res = ioport_read_byte(io);
        if (res == 255 && intpart == 255) {
            res = INFINITY; /* Round it up a little */
        }
        else {
            res = res * (2 << (intpart-192));
        }
    }
    return res;
}
