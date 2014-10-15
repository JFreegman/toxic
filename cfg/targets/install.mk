# Install target
install: toxic
	mkdir -p $(abspath $(DESTDIR)/$(BINDIR))
	mkdir -p $(abspath $(DESTDIR)/$(DATADIR))
	mkdir -p $(abspath $(DESTDIR)/$(DATADIR))/sounds
	mkdir -p $(abspath $(DESTDIR)/$(MANDIR))

	@echo "Installing toxic executable"
	@install -m 0755 toxic $(abspath $(DESTDIR)/$(BINDIR))

	@echo "Installing data files"
	@for f in $(DATAFILES) ; do \
		install -m 0644 $(MISC_DIR)/$$f $(abspath $(DESTDIR)/$(DATADIR)) ;\
		file=$(abspath $(DESTDIR)/$(DATADIR))/$$f ;\
		sed -i'' -e 's:__DATADIR__:'$(abspath $(DATADIR))':g' $$file ;\
	done
	@for f in $(SNDFILES) ; do \
		install -m 0644 $(SND_DIR)/$$f $(abspath $(DESTDIR)/$(DATADIR))/sounds ;\
	done

	@echo "Installing man pages"
	@for f in $(MANFILES) ; do \
		if [ ! -e "$$f" ]; then \
			continue ;\
		fi ;\
		section=$(abspath $(DESTDIR)/$(MANDIR))/man`echo $$f | rev | cut -d "." -f 1` ;\
		file=$$section/$$f ;\
		mkdir -p $$section ;\
		install -m 0644 $(DOC_DIR)/$$f $$file ;\
		sed -i'' -e 's:__VERSION__:'$(VERSION)':g' $$file ;\
		sed -i'' -e 's:__DATADIR__:'$(abspath $(DATADIR))':g' $$file ;\
		gzip -f -9 $$file ;\
	done

.PHONY: install
