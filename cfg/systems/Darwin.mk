# Special options for OS X
# This assumes the use of Homebrew. Change the paths if using MacPorts or Fink.

PKG_CONFIG_PATH = $(shell export PKG_CONFIG_PATH=/usr/lib/pkgconfig:/usr/local/opt/libconfig/lib/pkgconfig:/usr/local/lib/pkgconfig:/opt/X11/lib/pkgconfig)

LIBS := $(filter-out ncursesw, $(LIBS))

# OS X ships a usable, recent version of ncurses, but calls it ncurses not ncursesw.
LDFLAGS += -lncurses -lalut -ltoxav -ltoxcore -ltoxdns -lresolv -lconfig -ltoxencryptsave -g
CFLAGS += -I/usr/local/opt/freealut/include/AL -I/usr/local/opt/glib/include/glib-2.0 -g
