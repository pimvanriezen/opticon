ifdef STATIC
  CFLAGS+=-DSTATIC
  LDFLAGS+=-static
endif

LUA?=lua
LUALIBS?=$(shell pkg-config --libs $(LUA))
LUAINC?=$(shell pkg-config --cflags $(LUA))

OSNAME?=$(shell uname -s | tr A-Z a-z)
ifeq ($(findstring mingw64,$(OSNAME)),mingw64)
	OSNAME=windows
endif
OSREL?=$(shell uname -r)
OSRELMAJOR?=$(shell uname -r | cut -f1 -d.)

ifeq (freebsd,$(OSNAME))
  LDFLAGS+=-lkvm
endif

ifeq (linux,$(OSNAME))
  CFLAGS+=-std=c99 -D_GNU_SOURCE
endif

ifeq (windows,$(OSNAME))
  CFLAGS+=-isystem ../../lib/winlibc/additional/include
  CFLAGS+=-DOS_WINDOWS -DWIN32_LEAN_AND_MEAN -D_WIN32_WINNT=0x0601
endif

CFLAGS+=-DOSNAME=$(OSNAME) -DOSREL=$(OSREL) -DOSRELMAJOR=$(OSRELMAJOR)

OBJS_TEST = test/test_roundtrip_encoding.o
OBJS_LIBOPTICON = rsrc.o adjust.o aes.o host.o tenant.o \
				  util.o auth.o base64.o host_import.o glob.o \
				  ioport.o ioport_file.o meter.o \
				  ioport_buffer.o codec.o \
				  codec_json.o codec_pkt.o \
				  compress.o pktwrap.o \
				  watchlist.o defaults.o \
				  fillrandom.o \
				  var_dump.o var.o react.o var_parse.o \
				  cliopt.o daemon.o log.o thread.o transport.o \
				  transport_udp.o timestr.o packetqueue.o \
				  defaultmeters.o uuid.o hash.o \
				  popen.o summary.o notify.o timer.o
all: ../../lib/libopticon.a

rsrc.c: rsrc.h

rsrc.h: rsrc/*/*.rsrc
	@echo "[RSRC ] building resource file"
	@../../tools/mkrsrc librsrc

../../lib/libopticon.a: rsrc.c $(OBJS_LIBOPTICON)
	@echo "[LINK ] libopticon.a"
	@ar cr ../../lib/libopticon.a $(OBJS_LIBOPTICON) $(LPTHREAD)

clean:
	rm -f *.o ../../lib/libopticon.a rsrc.c rsrc.h

.SUFFIXES: .c .o
.c.o:
	@echo "[BUILD] $<"
	@$(CC) $(CFLAGS) -I../../include -c $<
