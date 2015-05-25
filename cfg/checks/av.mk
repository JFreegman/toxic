# Variables for audio call support
AUDIO_LIBS = libtoxav openal
VIDEO_LIBS = libtoxav opencv
AUDIO_CFLAGS = -DAUDIO
VIDEO_CFLAGS = -DVIDEO
ifneq (, $(findstring audio_device.o, $(OBJ)))
    AUDIO_OBJ = audio_call.o
else
    AUDIO_OBJ = audio_call.o audio_device.o
endif

ifneq (, $(findstring video_device.o, $(OBJ)))
	VIDEO_OBJ = video_call.o
else
	VIDEO_OBJ = video_call.o video_device.o
endif

# Check if we can build audio support
CHECK_AUDIO_LIBS = $(shell pkg-config --exists $(AUDIO_LIBS) || echo -n "error")
ifneq ($(CHECK_AUDIO_LIBS), error)
    LIBS += $(AUDIO_LIBS)
    CFLAGS += $(AUDIO_CFLAGS)
    OBJ += $(AUDIO_OBJ)
else ifneq ($(MAKECMDGOALS), clean)
    MISSING_AUDIO_LIBS = $(shell for lib in $(AUDIO_LIBS) ; do if ! pkg-config --exists $$lib ; then echo $$lib ; fi ; done)
    $(warning WARNING -- Toxic will be compiled without audio support)
    $(warning WARNING -- You need these libraries for audio support)
    $(warning WARNING -- $(MISSING_AUDIO_LIBS))
endif

# Check if we can build video support
CHECK_VIDEO_LIBS = $(shell pkg-config --exists $VIDEO_LIBS) || echo -n "error")
ifneq ($(CHECK_VIDEO_LIBS), error)
    LIBS += $(VIDEO_LIBS)
    CFLAGS += $(VIDEO_CFLAGS)
    OBJ += $(VIDEO_OBJ)
else ifneq ($(MAKECMDGOALS), clean)
    MISSING_VIDEO_LIBS = $(shell for lib in $(VIDEO_LIBS) ; do if ! pkg-config --exists $$lib ; then echo $$lib ; fi ; done)
    $(warning WARNING -- Toxic will be compiled without video support)
    $(warning WARNING -- You need these libraries for video support)
    $(warning WARNING -- $(MISSING_VIDEO_LIBS))
endif