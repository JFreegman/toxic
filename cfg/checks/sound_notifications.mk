# Variables for sound notifications support
SND_NOTIFY_LIBS = openal freealut
SND_NOTIFY_CFLAGS = -DSOUND_NOTIFY
ifneq (, $(findstring audio_device.o, $(OBJ)))
    SND_NOTIFY_OBJ =
else
    SND_NOTIFY_OBJ = audio_device.o
endif

# Check if we can build sound notifications support
CHECK_SND_NOTIFY_LIBS = $(shell $(PKG_CONFIG) --exists $(SND_NOTIFY_LIBS) || echo -n "error")
ifneq ($(CHECK_SND_NOTIFY_LIBS), error)
    LIBS += $(SND_NOTIFY_LIBS)
    CFLAGS += $(SND_NOTIFY_CFLAGS)
    OBJ += $(SND_NOTIFY_OBJ)
else ifneq ($(MAKECMDGOALS), clean)
    MISSING_SND_NOTIFY_LIBS = $(shell for lib in $(SND_NOTIFY_LIBS) ; do if ! $(PKG_CONFIG) --exists $$lib ; then echo $$lib ; fi ; done)
    $(warning WARNING -- Toxic will be compiled without sound notifications support)
    $(warning WARNING -- You need these libraries for sound notifications support)
    $(warning WARNING -- $(MISSING_SND_NOTIFY_LIBS))
endif
