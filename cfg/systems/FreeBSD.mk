# Specials options for freebsd systems
LIBS := $(filter-out ncursesw, $(LIBS))
LDFLAGS += -lncursesw
MANDIR = $(PREFIX)/man
