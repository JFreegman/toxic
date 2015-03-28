# Version
TOXIC_VERSION = 0.6.0
REV = $(shell git rev-list HEAD --count 2>/dev/null || echo -n "error")
ifneq (, $(findstring error, $(REV)))
    VERSION = $(TOXIC_VERSION)
else
    VERSION = $(TOXIC_VERSION)_r$(REV)
endif

# Project directories
BUILD_DIR = $(BASE_DIR)/build
DOC_DIR = $(BASE_DIR)/doc
SRC_DIR = $(BASE_DIR)/src
SND_DIR = $(BASE_DIR)/sounds
MISC_DIR = $(BASE_DIR)/misc

# Project files
MANFILES = toxic.1 toxic.conf.5
DATAFILES = DHTnodes DNSservers toxic.conf.example
DESKFILE = toxic.desktop
SNDFILES = ToxicContactOnline.wav ToxicContactOffline.wav ToxicError.wav
SNDFILES += ToxicRecvMessage.wav ToxicOutgoingCall.wav ToxicIncomingCall.wav
SNDFILES += ToxicTransferComplete.wav ToxicTransferStart.wav

# Install directories
PREFIX = /usr/local
BINDIR = $(PREFIX)/bin
DATADIR = $(PREFIX)/share/toxic
MANDIR = $(PREFIX)/share/man
APPDIR = $(PREFIX)/share/applications
