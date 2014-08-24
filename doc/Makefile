MANFILES = toxic.1 toxic.conf.5

# Targets
doc: $(MANFILES)

%: %.asc
	@echo "  Generating man page $(@F)"
	@a2x -f manpage -a revdate=$(shell git log -1 --date=short --format="%ad" $<) \
	-a manmanual="toxic manual" -a mansource=toxic \
	-a manversion=__VERSION__ -a datadir=__DATADIR__ $<

.PHONY: doc
