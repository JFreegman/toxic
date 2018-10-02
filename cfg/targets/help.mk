# Help target
help:
	@echo "-- Targets --"
	@echo "  all:       Build toxic and documentation [DEFAULT]"
	@echo "  toxic:     Build toxic"
	@echo "  doc:       Build documentation"
	@echo "  install:   Build toxic and install it in PREFIX (default PREFIX is \"$(abspath $(PREFIX))\")"
	@echo "  uninstall: Remove toxic from PREFIX (default PREFIX is \"$(abspath $(PREFIX))\")"
	@echo "  clean:     Remove built files"
	@echo "  help:      This help"
	@echo
	@echo "-- Variables --"
	@echo "  DISABLE_X11:            Set to \"1\" to force building without X11 support"
	@echo "  DISABLE_AV:             Set to \"1\" to force building without audio call support"
	@echo "  DISABLE_SOUND_NOTIFY:   Set to \"1\" to force building without sound notification support"
	@echo "  DISABLE_DESKTOP_NOTIFY: Set to \"1\" to force building without desktop notifications support"
	@echo "  DISABLE_QRCODE:         Set to \"1\" to force building without QR export support"
	@echo "  DISABLE_QRPNG:          Set to \"1\" to force building without QR exported as PNG support"
	@echo "  ENABLE_PYTHON:          Set to \"1\" to enable building with Python scripting support"
	@echo "  CFLAGS:                 Add custom flags to default CFLAGS"
	@echo "  LDFLAGS:                Add custom flags to default LDFLAGS"
	@echo "  PREFIX:                 Specify a prefix directory for binaries, data files,... (default is \"$(abspath $(PREFIX))\")"
	@echo "  DESTDIR:                Specify a directory where to store installed files (mainly for packaging purpose)"

.PHONY: help
