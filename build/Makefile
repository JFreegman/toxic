TOXIC_VERSION = 0.4.2
REV = $(shell git rev-list HEAD --count)
VERSION = $(TOXIC_VERSION)_r$(REV)

CFG_DIR = ../cfg
SRC_DIR = ../src
MISC_DIR = ../misc
PREFIX = /usr/local
BINDIR = $(PREFIX)/bin
DATADIR = $(PREFIX)/share/toxic
DATAFILES = $(MISC_DIR)/DHTnodes $(MISC_DIR)/toxic.conf

LIBS = libtoxcore ncursesw

CFLAGS = -std=gnu99 -pthread -Wimplicit-function-declaration -Wreturn-type -Werror
CFLAGS += -DTOXICVER="\"$(VERSION)\"" -DHAVE_WIDECHAR -D_XOPEN_SOURCE_EXTENDED
CFLAGS += -DPACKAGE_DATADIR="\"$(DATADIR)\""
CFLAGS += $(USER_CFLAGS)
LDFLAGS = $(USER_LDFLAGS)

OBJ = chat.o chat_commands.o configdir.o dns.o execute.o
OBJ += file_senders.o friendlist.o global_commands.o groupchat.o line_info.o input.o
OBJ += log.o misc_tools.o prompt.o settings.o toxic.o toxic_strings.o windows.o

# Variables for audio support
AUDIO_LIBS = libtoxav openal
AUDIO_CFLAGS = -D_SUPPORT_AUDIO
AUDIO_OBJ = device.o audio_call.o

# Check on wich system we are running
UNAME_S = $(shell uname -s)
ifeq ($(UNAME_S), Linux)
	-include $(CFG_DIR)/Linux.mk
endif
ifeq ($(UNAME_S), Darwin)
	-include $(CFG_DIR)/Darwin.mk
endif
ifeq ($(UNAME_S), Solaris)
	-include $(CFG_DIR)/Solaris.mk
endif

# Check on which platform we are running
UNAME_M = $(shell uname -m)
ifeq ($(UNAME_M), x86_64)
	-include $(CFG_DIR)/x86_64.mk
endif
ifneq ($(filter %86, $(UNAME_M)),)
	-include $(CFG_DIR)/x86.mk
endif
ifneq ($(filter arm%, $(UNAME_M)),)
	-include $(CFG_DIR)/arm.mk
endif

# Check if we can build audio
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

# Check if we can build Toxic
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

# Targets
all: toxic

toxic: $(OBJ)
	$(CC) $(CFLAGS) -o toxic $(OBJ) $(LDFLAGS)

install: toxic
	@mkdir -p $(DESTDIR)/$(BINDIR)
	@mkdir -p $(DESTDIR)/$(DATADIR)
	install -m 0755 toxic $(DESTDIR)/$(BINDIR)
	install -m 0644 $(DATAFILES) $(DESTDIR)/$(DATADIR)

%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -o $*.o -c $(SRC_DIR)/$*.c
	$(CC) -MM $(CFLAGS) $(SRC_DIR)/$*.c > $*.d

clean:
	rm -rf *.d *.o toxic

-include $(OBJ:.o=.d)

.PHONY: clean all install
