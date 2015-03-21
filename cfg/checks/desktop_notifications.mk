# Variables for desktop notifications support
DESK_NOTIFY_LIBS = libnotify
DESK_NOTIFY_CFLAGS = -DBOX_NOTIFY

# Check if we can build desktop notifications support
CHECK_DESK_NOTIFY_LIBS = $(shell pkg-config --exists $(DESK_NOTIFY_LIBS) || echo -n "error")
ifneq ($(CHECK_DESK_NOTIFY_LIBS), error)
    LIBS += $(DESK_NOTIFY_LIBS)
    CFLAGS += $(DESK_NOTIFY_CFLAGS)
else ifneq ($(MAKECMDGOALS), clean)
    MISSING_DESK_NOTIFY_LIBS = $(shell for lib in $(DESK_NOTIFY_LIBS) ; do if ! pkg-config --exists $$lib ; then echo $$lib ; fi ; done)
    $(warning WARNING -- Toxic will be compiled without desktop notifications support)
    $(warning WARNING -- You need these libraries for desktop notifications support)
    $(warning WARNING -- $(MISSING_DESK_NOTIFY_LIBS))
endif
