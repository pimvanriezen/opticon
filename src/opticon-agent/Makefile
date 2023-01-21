include ../../makeinclude

ifdef STATIC
  CFLAGS+=-DSTATIC
  LDFLAGS+=-static
endif

LUA?=lua
LUALIBS?=$(shell pkg-config --libs $(LUA))
LUAINC?=$(shell pkg-config --cflags $(LUA))

OSNAME?=$(shell uname -s | tr A-Z a-z)
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

CFLAGS+=-DOS_$(OSDEF) -DOSNAME=$(OSNAME) -DOSREL=$(OSREL) -DOSRELMAJOR=$(OSRELMAJOR)

OBJS_OPTICONAGENT = rsrc.o main.o probelist.o probes.o resender.o \
                    collectorlist.o tproc.o wordlist.o version.o
LIBS = ../../lib/libopticon.a ../../lib/libopticondb.a 
				  
all: ../../bin/opticon-agent ../../bin/opticon-helper

../../bin/opticon-agent: $(OBJS_OPTICONAGENT) $(LIBS)
	@echo "[LINK ] opticon-agent"
	@$(CC) $(LDFLAGS) -o ../../bin/opticon-agent \
		$(OBJS_OPTICONAGENT) $(LIBS) $(LPTHREAD) -lm -lz

../../bin/opticon-helper: helper.o
	@$(CC) $(LDFLAGS) -o ../../bin/opticon-helper helper.o

rsrc.c: rsrc.h

rsrc.h: rsrc/*/*.rsrc
	@echo "[RSRC ] building resource file"
	@../../tools/mkrsrc librsrc

clean:
	rm -f *.o version.c ../../bin/opticon-agent rsrc.c rsrc.h
	
version.c:
	@./mkversion.sh

probes.o: probes.c probes_linux.c probes_darwin.c
	@echo "[BUILD] probes.c"
	@$(CC) $(CFLAGS) -I../../include -c probes.c

.SUFFIXES: .c .o
.c.o:
	@echo "[BUILD] $<"
	@$(CC) $(CFLAGS) -I../../include -c $<
