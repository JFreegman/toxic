# Config target
config: $(BUILD_DIR)/.config.mk

$(BUILD_DIR)/.config.mk:
	@if [ ! -e $(BUILD_DIR) ]; then \
		mkdir -p $(BUILD_DIR) ;\
	fi
	@echo "  GEN   $(@:$(BUILD_DIR)/%=%)"
	@echo "PREFIX ?= $(PREFIX)" > $@
	@echo "USER_CFLAGS ?= $(USER_CFLAGS)" >> $@
	@echo "USER_LDFLAGS ?= $(USER_LDFLAGS)" >> $@
	@echo "DISABLE_X11 ?= $(DISABLE_X11)" >> $@
	@echo "DISABLE_AV ?= $(DISABLE_AV)" >> $@
	@echo "DISABLE_SOUND_NOTIFY ?= $(DISABLE_SOUND_NOTIFY)" >> $@
	@echo "DISABLE_DESKTOP_NOTIFY ?= $(DISABLE_DESKTOP_NOTIFY)" >> $@

.PHONY: config
