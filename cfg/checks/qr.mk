# Variables for QR export support
QR_LIBS = libqrencode
QR_CFLAGS = -DQRCODE

# Check if we can build QR export support
CHECK_QR_LIBS = $(shell pkg-config --exists $(QR_LIBS) || echo -n "error")
ifneq ($(CHECK_QR_LIBS), error)
    LIBS += $(QR_LIBS)
    CFLAGS += $(QR_CFLAGS)
else ifneq ($(MAKECMDGOALS), clean)
    MISSING_QR_LIBS = $(shell for lib in $(QR_LIBS) ; do if ! $(PKG_CONFIG) --exists $$lib ; then echo $$lib ; fi ; done)
    $(warning WARNING -- Toxic will be compiled without QR export support)
    $(warning WARNING -- You need these libraries for QR export support)
    $(warning WARNING -- $(MISSING_QR_LIBS))
endif
