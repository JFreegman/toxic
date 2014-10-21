# Install target
install: toxic
	@echo "Installing toxic executable"
	@install -Dm 0755 toxic $(abspath $(DESTDIR)/$(BINDIR)/toxic)

	@echo "Installing desktop file"
	@install -Dm 0644 $(MISC_DIR)/$(DESKFILE) $(abspath $(DESTDIR)/$(APPDIR)/$(DESKFILE))

	@echo "Installing data files"
	@for f in $(DATAFILES) ; do \
		install -Dm 0644 $(MISC_DIR)/$$f $(abspath $(DESTDIR)/$(DATADIR)/$$f) ;\
		file=$(abspath $(DESTDIR)/$(DATADIR)/$$f) ;\
		sed -i'' -e 's:__DATADIR__:'$(abspath $(DATADIR))':g' $$file ;\
	done
	@for f in $(SNDFILES) ; do \
		install -Dm 0644 $(SND_DIR)/$$f $(abspath $(DESTDIR)/$(DATADIR)/sounds/$$f) ;\
	done

	@echo "Installing man pages"
	@for f in $(MANFILES) ; do \
		if [ ! -e "$$f" ]; then \
			continue ;\
		fi ;\
		section=$(abspath $(DESTDIR)/$(MANDIR))/man`echo $$f | rev | cut -d "." -f 1` ;\
		file=$$section/$$f ;\
		install -Dm 0644 $(DOC_DIR)/$$f $$file ;\
		sed -i'' -e 's:__VERSION__:'$(VERSION)':g' $$file ;\
		sed -i'' -e 's:__DATADIR__:'$(abspath $(DATADIR))':g' $$file ;\
		gzip -f -9 $$file ;\
	done

.PHONY: install
