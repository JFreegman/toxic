TOXIC_VERSION = 0.4.7
REV = $(shell git rev-list HEAD --count)
VERSION = $(TOXIC_VERSION)_r$(REV)

CFG_DIR = ../cfg
SRC_DIR = ../src
PREFIX = /usr/local
BINDIR = $(PREFIX)/bin
DATADIR = $(PREFIX)/share/toxic
MANDIR = $(PREFIX)/share/man

LIBS = libtoxcore ncursesw libconfig

CFLAGS = -std=gnu99 -pthread -Wall -g
CFLAGS += -DTOXICVER="\"$(VERSION)\"" -DHAVE_WIDECHAR -D_XOPEN_SOURCE_EXTENDED
CFLAGS += -DPACKAGE_DATADIR="\"$(abspath $(DATADIR))\""
CFLAGS += $(USER_CFLAGS)
LDFLAGS = $(USER_LDFLAGS)

OBJ = chat.o chat_commands.o configdir.o dns.o execute.o file_senders.o notify.o
OBJ += friendlist.o global_commands.o groupchat.o line_info.o input.o help.o autocomplete.o
OBJ += log.o misc_tools.o prompt.o settings.o toxic.o toxic_strings.o windows.o

# Check on wich system we are running
UNAME_S = $(shell uname -s)
ifeq ($(UNAME_S), Linux)
	-include $(CFG_DIR)/systems/Linux.mk
endif
ifeq ($(UNAME_S), FreeBSD)
	-include $(CFG_DIR)/systems/FreeBSD.mk
endif
ifeq ($(UNAME_S), Darwin)
	-include $(CFG_DIR)/systems/Darwin.mk
endif
ifeq ($(UNAME_S), Solaris)
	-include $(CFG_DIR)/systems/Solaris.mk
endif

# Check on which platform we are running
UNAME_M = $(shell uname -m)
ifeq ($(UNAME_M), x86_64)
	-include $(CFG_DIR)/platforms/x86_64.mk
endif
ifneq ($(filter %86, $(UNAME_M)),)
	-include $(CFG_DIR)/platforms/x86.mk
endif
ifneq ($(filter arm%, $(UNAME_M)),)
	-include $(CFG_DIR)/platforms/arm.mk
endif

# Include all needed checks
-include $(CFG_DIR)/check_features.mk

# Targets
all: toxic

toxic: $(OBJ)
	@echo "  LD    $@"
	@$(CC) $(CFLAGS) -o toxic $(OBJ) $(LDFLAGS)

%.o: $(SRC_DIR)/%.c
	@echo "  CC    $@"
	@$(CC) $(CFLAGS) -o $*.o -c $(SRC_DIR)/$*.c
	@$(CC) -MM $(CFLAGS) $(SRC_DIR)/$*.c > $*.d

clean:
	rm -rf *.d *.o toxic

-include $(OBJ:.o=.d)

-include $(CFG_DIR)/install.mk
-include $(CFG_DIR)/help.mk

.PHONY: clean all
