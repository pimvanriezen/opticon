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

ifeq (freebsd,$(OSNAME))
  LDFLAGS+=-lkvm
endif

ifeq (linux,$(OSNAME))
  CFLAGS+=-std=c99 -D_GNU_SOURCE
  LDFLAGS+=-lpthread -lm
endif

CFLAGS+=-DOSNAME=$(OSNAME) -DOSREL=$(OSREL) -DOSRELMAJOR=$(OSRELMAJOR)

OBJS_OPTICONCOLLECTOR = main.o watcher.o ping.o
LIBS = ../../lib/libopticondb.a ../../lib/libopticon.a 
				  
all: ../../bin/opticon-collector ../../bin/opticon-decode

../../bin/opticon-collector: $(OBJS_OPTICONCOLLECTOR) $(LIBS)
	@echo "[LINK ] opticon-collector"
	@$(CC) $(LDFLAGS) -o ../../bin/opticon-collector \
		$(OBJS_OPTICONCOLLECTOR) $(LIBS) $(LPTHREAD) -lm -lz

../../bin/opticon-decode: decode.o $(LIBS)
	@echo "[LINK ] opticon-decode"
	@$(CC) $(LDFLAGS) -o ../../bin/opticon-decode \
	    decode.o $(LIBS) $(LPTHREAD) -lm -lz

clean:
	rm -f *.o ../../bin/opticon-collector ../../bin/opticon-decode

.SUFFIXES: .c .o
.c.o:
	@echo "[BUILD] $<"
	@$(CC) $(CFLAGS) -I../../include -c $<
