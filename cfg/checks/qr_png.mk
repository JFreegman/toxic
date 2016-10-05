# Variables for QR exported as PNG support
PNG_LIBS = libpng
PNG_CFLAGS = -DQRPNG

# Check if we can build QR exported as PNG support
CHECK_PNG_LIBS = $(shell pkg-config --exists $(PNG_LIBS) || echo -n "error")
ifneq ($(CHECK_PNG_LIBS), error)
    LIBS += $(PNG_LIBS)
    CFLAGS += $(PNG_CFLAGS)
else ifneq ($(MAKECMDGOALS), clean)
    MISSING_PNG_LIBS = $(shell for lib in $(PNG_LIBS) ; do if ! $(PKG_CONFIG) --exists $$lib ; then echo $$lib ; fi ; done)
    $(warning WARNING -- Toxic will be compiled without QR exported as PNG support)
    $(warning WARNING -- You need these libraries for QR exported as PNG support)
    $(warning WARNING -- $(MISSING_PNG_LIBS))
endif
