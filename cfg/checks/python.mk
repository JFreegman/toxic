# Variables for Python scripting support
PYTHON_LIBS = python3
PYTHON_CFLAGS = -DPYTHON
PYTHON_OBJ = api.o python_api.o

# Check if we can build Python scripting support
CHECK_PYTHON_LIBS = $(shell $(PKG_CONFIG) --exists $(PYTHON_LIBS) || echo -n "error")
ifneq ($(CHECK_PYTHON_LIBS), error)
    # LIBS += $(PYTHON_LIBS)

    # Unwise hacks... You will pay for this.
    LDFLAGS += $(shell python3-config --ldflags)
    CFLAGS += $(PYTHON_CFLAGS) $(shell python3-config --cflags)
    OBJ += $(PYTHON_OBJ)
else ifneq ($(MAKECMDGOALS), clean)
    MISSING_AUDIO_LIBS = $(shell for lib in $(PYTHON_LIBS) ; do if ! $(PKG_CONFIG) --exists $$lib ; then echo $$lib ; fi ; done)
    $(warning WARNING -- Toxic will be compiled without Python scripting support)
    $(warning WARNING -- You need these libraries for Python scripting support)
    $(warning WARNING -- $(MISSING_AUDIO_LIBS))
endif
