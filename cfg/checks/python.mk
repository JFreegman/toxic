# Variables for Python scripting support
PYTHON3_LIBS = python3
PYTHON_CFLAGS = -DPYTHON
PYTHON_OBJ = api.o python_api.o

# Check if we can build Python scripting support
CHECK_PYTHON3_LIBS = $(shell $(PKG_CONFIG) --exists $(PYTHON3_LIBS) || echo -n "error")
ifneq ($(CHECK_PYTHON3_LIBS), error)
    LDFLAGS += $(shell python3-config --ldflags)
    CFLAGS += $(PYTHON_CFLAGS) $(shell python3-config --includes)
    OBJ += $(PYTHON_OBJ)
else ifneq ($(MAKECMDGOALS), clean)
    $(warning WARNING -- Toxic will be compiled without Python scripting support)
    $(warning WARNING -- You need python3 installed for Python scripting support)
endif
