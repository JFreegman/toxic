SUPPORT_AUDIO ?= 0

LIBS = ncurses

CFLAGS = -std=gnu99 -pthread
LDFLAGS = -ltoxcore -ltoxdns -ldl -lresolv
DESTDIR=/usr/local

OBJ = chat.o chat_commands.o configdir.o dns.o execute.o
OBJ += file_senders.o friendlist.o global_commands.o groupchat.o line_info.o
OBJ += log.o misc_tools.o prompt.o settings.o toxic.o toxic_strings.o windows.o

ifeq ($(SUPPORT_AUDIO), 1)
        CFLAGS += -D_SUPPORT_AUDIO
        LDFLAGS += -ltoxav
        OBJ += device.o audio_call.o
        LIBS += openal
endif

ifneq ($(LIBS),)
	CFLAGS += $(shell pkg-config --cflags $(LIBS))
	LDFLAGS += $(shell pkg-config --libs $(LIBS))
endif

all: toxic

toxic: $(OBJ)
        $(CC) $(CFLAGS) -o toxic $(OBJ) $(LDFLAGS)

install: toxic
        mkdir -pv $(DESTDIR)/bin
        install -m 0755 toxic $(DESTDIR)/bin

-include $(OBJ:.o=.d)

%.o: %.c
        $(CC) $(CFLAGS) -o $*.o -c $*.c
        $(CC) -MM $(CFLAGS) $*.c > $*.d

clean:
        rm -rf *.d *.o toxic

.PHONY: clean all install
