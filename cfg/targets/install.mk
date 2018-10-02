# Install target
install: $(BUILD_DIR)/toxic
	@echo "Installing toxic executable"
	@mkdir -p $(abspath $(DESTDIR)/$(BINDIR))
	@install -m 0755 $(BUILD_DIR)/toxic $(abspath $(DESTDIR)/$(BINDIR)/toxic)
	
	@echo "Installing desktop file"
	@mkdir -p $(abspath $(DESTDIR)/$(APPDIR))
	@install -m 0644 $(MISC_DIR)/$(DESKFILE) $(abspath $(DESTDIR)/$(APPDIR)/$(DESKFILE))
	
	@echo "Installing data files"
	@mkdir -p $(abspath $(DESTDIR)/$(DATADIR))
	@for f in $(DATAFILES) ; do \
		install -m 0644 $(MISC_DIR)/$$f $(abspath $(DESTDIR)/$(DATADIR)/$$f) ;\
		file=$(abspath $(DESTDIR)/$(DATADIR)/$$f) ;\
		sed -e 's:__DATADIR__:'$(abspath $(DATADIR))':g' $$file > temp_file && \
		mv temp_file $$file ;\
	done
	@mkdir -p $(abspath $(DESTDIR)/$(DATADIR))/sounds
	@for f in $(SNDFILES) ; do \
		install -m 0644 $(SND_DIR)/$$f $(abspath $(DESTDIR)/$(DATADIR)/sounds/$$f) ;\
	done
	
	@echo "Installing man pages"
	@mkdir -p $(abspath $(DESTDIR)/$(MANDIR))
	@for f in $(MANFILES) ; do \
		if [ ! -e "$(DOC_DIR)/$$f" ]; then \
			continue ;\
		fi ;\
		section=$(abspath $(DESTDIR)/$(MANDIR))/man`echo $$f | sed "s/.*\.//"` ;\
		file=$$section/$$f ;\
		mkdir -p $$section ;\
		install -m 0644 $(DOC_DIR)/$$f $$file ;\
		sed -e 's:__VERSION__:'$(VERSION)':g' $$file > temp_file && \
		mv temp_file $$file ;\
		sed -e 's:__DATADIR__:'$(abspath $(DATADIR))':g' $$file > temp_file && \
		mv temp_file $$file ;\
		gzip -f -n -9 $$file ;\
	done

.PHONY: install
