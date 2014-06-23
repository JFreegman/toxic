TOXIC_VERSION = 0.4.1
REV = $(shell git rev-list HEAD --count)
VERSION = $(TOXIC_VERSION)_r$(REV)

LIBS = libtoxcore ncurses

CFLAGS = -DTOXICVER="\"$(VERSION)\"" -std=gnu99 -pthread
LDFLAGS = -ldl -lresolv
DESTDIR ?= /usr/local

OBJ = chat.o chat_commands.o configdir.o dns.o execute.o
OBJ += file_senders.o friendlist.o global_commands.o groupchat.o line_info.o
OBJ += log.o misc_tools.o prompt.o settings.o toxic.o toxic_strings.o windows.o

AUDIO_LIBS = libtoxav openal
AUDIO_CFLAGS = -D_SUPPORT_AUDIO
AUDIO_OBJ = device.o audio_call.o

CHECK_AUDIO_LIBS = $(shell pkg-config $(AUDIO_LIBS) || echo -n "error")
ifneq ($(CHECK_AUDIO_LIBS), error)
        LIBS += $(AUDIO_LIBS)
        CFLAGS += $(AUDIO_CFLAGS)
        OBJ += $(AUDIO_OBJ)
else
ifneq ($(MAKECMDGOALS), clean)
MISSING_AUDIO_LIBS = $(shell for lib in $(AUDIO_LIBS) ; do if ! pkg-config $$lib ; then echo $$lib ; fi ; done)
$(warning WARNING -- Toxic will be compiled without audio support)
$(warning WARNING -- You need these libraries for audio support)
$(warning WARNING -- $(MISSING_AUDIO_LIBS))
endif
endif

CHECK_LIBS = $(shell pkg-config $(LIBS) || echo -n "error")
ifneq ($(CHECK_LIBS), error)
        CFLAGS += $(shell pkg-config --cflags $(LIBS))
        LDFLAGS += $(shell pkg-config --libs $(LIBS))
else
ifneq ($(MAKECMDGOALS), clean)
MISSING_LIBS = $(shell for lib in $(LIBS) ; do if ! pkg-config $$lib ; then echo $$lib ; fi ; done)
$(warning ERROR -- Cannot compile Toxic)
$(warning ERROR -- You need these libraries)
$(warning ERROR -- $(MISSING_LIBS))
$(error ERROR)
endif
endif

all: toxic

toxic: $(OBJ)
        $(CC) $(CFLAGS) -o toxic $(OBJ) $(LDFLAGS)

install: toxic
        mkdir -pv $(DESTDIR)/bin
        install -m 0755 toxic $(DESTDIR)/bin

%.o: %.c
        $(CC) $(CFLAGS) -o $*.o -c $*.c
        $(CC) -MM $(CFLAGS) $*.c > $*.d

clean:
        rm -rf *.d *.o toxic

-include $(OBJ:.o=.d)

.PHONY: clean all install
