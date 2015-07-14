CHECKS_DIR = $(CFG_DIR)/checks

# Check if we want build X11 support
X11 = $(shell if [ -z "$(DISABLE_X11)" ] || [ "$(DISABLE_X11)" = "0" ] ; then echo enabled ; else echo disabled ; fi)
ifneq ($(X11), disabled)
    -include $(CHECKS_DIR)/x11.mk
endif

# Check if we want build audio support
AUDIO = $(shell if [ -z "$(DISABLE_AV)" ] || [ "$(DISABLE_AV)" = "0" ] ; then echo enabled ; else echo disabled ; fi)
ifneq ($(AUDIO), disabled)
    -include $(CHECKS_DIR)/av.mk
endif

# Check if we want build sound notifications support
SND_NOTIFY = $(shell if [ -z "$(DISABLE_SOUND_NOTIFY)" ] || [ "$(DISABLE_SOUND_NOTIFY)" = "0" ] ; then echo enabled ; else echo disabled ; fi)
ifneq ($(SND_NOTIFY), disabled)
    -include $(CHECKS_DIR)/sound_notifications.mk
endif

# Check if we want build desktop notifications support
DESK_NOTIFY = $(shell if [ -z "$(DISABLE_DESKTOP_NOTIFY)" ] || [ "$(DISABLE_DESKTOP_NOTIFY)" = "0" ] ; then echo enabled ; else echo disabled ; fi)
ifneq ($(DESK_NOTIFY), disabled)
    -include $(CHECKS_DIR)/desktop_notifications.mk
endif

# Check if we can build Toxic
CHECK_LIBS = $(shell $(PKG_CONFIG) --exists $(LIBS) || echo -n "error")
ifneq ($(CHECK_LIBS), error)
    CFLAGS += $(shell $(PKG_CONFIG) --cflags $(LIBS))
    LDFLAGS += $(shell $(PKG_CONFIG) --libs $(LIBS))
else ifneq ($(MAKECMDGOALS), clean)
    MISSING_LIBS = $(shell for lib in $(LIBS) ; do if ! $(PKG_CONFIG) --exists $$lib ; then echo $$lib ; fi ; done)
    $(warning ERROR -- Cannot compile Toxic)
    $(warning ERROR -- You need these libraries)
    $(warning ERROR -- $(MISSING_LIBS))
    $(error ERROR)
endif
