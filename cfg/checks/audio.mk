# Variables for audio call support
AUDIO_LIBS = openal
AUDIO_CFLAGS = -DAUDIO
ifneq (, $(findstring audio_device.o, $(OBJ)))
    AUDIO_OBJ = audio_call.o
else
    AUDIO_OBJ = audio_call.o audio_device.o
endif

# Check if we can build audio support
CHECK_AUDIO_LIBS := $(shell $(PKG_CONFIG) --exists $(AUDIO_LIBS) || echo -n "error")
ifneq ($(CHECK_AUDIO_LIBS), error)
    LIBS += $(AUDIO_LIBS)
    LDFLAGS += -lm
    CFLAGS += $(AUDIO_CFLAGS)
    OBJ += $(AUDIO_OBJ)
else ifneq ($(MAKECMDGOALS), clean)
    MISSING_AUDIO_LIBS := $(shell for lib in $(AUDIO_LIBS) ; do if ! $(PKG_CONFIG) --exists $$lib ; then echo $$lib ; fi ; done)
    $(warning WARNING -- Toxic will be compiled without audio support)
    $(warning WARNING -- You need these libraries for audio support)
    $(warning WARNING -- $(MISSING_AUDIO_LIBS))
endif
