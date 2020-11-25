# Variables for X11 support
X11_LIBS = x11
X11_CFLAGS = -DX11
X11_OBJ = x11focus.o

# Check if we can build X11 support
CHECK_X11_LIBS = $(shell $(PKG_CONFIG) --exists $(X11_LIBS) || echo -n "error")
ifneq ($(CHECK_X11_LIBS), error)
    LIBS += $(X11_LIBS)
    CFLAGS += $(X11_CFLAGS)
    OBJ += $(X11_OBJ)
else ifneq ($(MAKECMDGOALS), clean)
    MISSING_X11_LIBS = $(shell for lib in $(X11_LIBS) ; do if ! $(PKG_CONFIG) --exists $$lib ; then echo $$lib ; fi ; done)
    $(warning WARNING -- Toxic will be compiled without x11 support (needed for focus tracking and drag&drop support))
    $(warning WARNING -- You need these libraries for x11 support)
    $(warning WARNING -- $(MISSING_X11_LIBS))
endif
