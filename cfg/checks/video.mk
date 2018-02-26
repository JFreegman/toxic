# Variables for video call support
VIDEO_LIBS = vpx x11
VIDEO_CFLAGS = -DVIDEO
ifneq (, $(findstring video_device.o, $(OBJ)))
    VIDEO_OBJ = video_call.o
else
    VIDEO_OBJ = video_call.o video_device.o
endif

# Check if we can build video support
CHECK_VIDEO_LIBS = $(shell $(PKG_CONFIG) --exists $(VIDEO_LIBS) || echo -n "error")
ifneq ($(CHECK_VIDEO_LIBS), error)
    LIBS += $(VIDEO_LIBS)
    CFLAGS += $(VIDEO_CFLAGS)
    OBJ += $(VIDEO_OBJ)
else ifneq ($(MAKECMDGOALS), clean)
    MISSING_VIDEO_LIBS = $(shell for lib in $(VIDEO_LIBS) ; do if ! $(PKG_CONFIG) --exists $$lib ; then echo $$lib ; fi ; done)
    $(warning WARNING -- Toxic will be compiled without video support)
    $(warning WARNING -- You will need these libraries for video support)
    $(warning WARNING -- $(MISSING_VIDEO_LIBS))
endif
