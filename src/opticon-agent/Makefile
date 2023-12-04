include ../../makeinclude

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
OSDEF?=$(shell uname -s | tr a-z A-Z)

ifeq (freebsd,$(OSNAME))
  LDFLAGS+=-lkvm
endif

ifeq (linux,$(OSNAME))
  CFLAGS+=-std=c99 -D_GNU_SOURCE
  LDFLAGS+=-lpthread -lm
endif

ifeq (windows,$(OSNAME))
  CFLAGS+=-isystem ../../lib/winlibc/additional/include
  CFLAGS+=-DOS_WINDOWS -DWIN32_LEAN_AND_MEAN -D_WIN32_WINNT=0x0601
  LDFLAGS+=-lkernel32 -luser32 -lws2_32 -lbcrypt -lpdh -lntdll -ladvapi32 -lwtsapi32 -lpsapi -liphlpapi -lwinhttp -lwininet -lole32 -loleaut32
  LDFLAGS+=-l:libz.a -l:libpthread.a
endif

CFLAGS+=-DOS_$(OSDEF) -DOSNAME=$(OSNAME) -DOSREL=$(OSREL) -DOSRELMAJOR=$(OSRELMAJOR)

OBJS_OPTICONAGENT = rsrc.o main.o probelist.o probes.o resender.o \
                    collectorlist.o tproc.o wordlist.o version.o

ifeq (windows,$(OSNAME))
	OBJS_OPTICONAGENT += probes_windows.o \
                       win/stringHelper.o win/systemFirmwareInfo.o \
                       win/timeHelper.o win/updater.o win/winService.o \
                       ../../lib/winlibc/additional/src/memory.o \
                       ../../lib/winlibc/additional/src/string.o \
                       ../../lib/winlibc/additional/src/time.o \
                       ../../lib/winlibc/beforewin8/src/endian.o
endif

LIBS = ../../lib/libopticon.a

all: ../../bin/opticon-agent
ifneq (windows,$(OSNAME))
all: ../../bin/opticon-agent ../../bin/opticon-helper
endif

../../bin/opticon-agent: $(OBJS_OPTICONAGENT) $(LIBS)
	@echo "[LINK ] opticon-agent"
	@$(CC) -o ../../bin/opticon-agent \
		$(OBJS_OPTICONAGENT) $(LIBS) $(LDFLAGS) $(LPTHREAD) -lm -lz

../../bin/opticon-helper: helper.o
	@$(CC) $(LDFLAGS) -o ../../bin/opticon-helper helper.o

rsrc.c: rsrc.h

rsrc.h: rsrc/*/*.rsrc
	@echo "[RSRC ] building resource file"
	@../../tools/mkrsrc librsrc

clean:
	rm -f *.o win/*.o version.c ../../bin/opticon-agent rsrc.c rsrc.h
	rm -f ../../lib/winlibc/additional/src/*.o ../../lib/winlibc/beforewin8/src/*.o
	
version.c:
	@./mkversion.sh

probes.o: probes.c probes_linux.c probes_darwin.c
	@echo "[BUILD] probes.c"
	@$(CC) $(CFLAGS) -I../../include -c probes.c

.SUFFIXES: .c .o
.c.o:
	@echo "[BUILD] $<"
	@$(CC) $(CFLAGS) -I../../include -c $< -o $@
