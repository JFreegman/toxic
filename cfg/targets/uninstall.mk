# Uninstall target
uninstall:
	@echo "Removing toxic executable"
	@rm -f $(abspath $(DESTDIR)/$(BINDIR)/toxic)
	
	@echo "Removing desktop file"
	@rm -f $(abspath $(DESTDIR)/$(APPDIR)/$(DESKFILE))
	
	@echo "Removing data files"
	@for f in $(DATAFILES) ; do \
		rm -f $(abspath $(DESTDIR)/$(DATADIR)/$$f) ;\
	done
	@for f in $(SNDFILES) ; do \
		rm -f $(abspath $(DESTDIR)/$(DATADIR)/sounds/$$f) ;\
	done
	
	@echo "Removing man pages"
	@for f in $(MANFILES) ; do \
		section=$(abspath $(DESTDIR)/$(MANDIR))/man`echo $$f | sed "s/.*\.//"` ;\
		file=$$section/$$f ;\
		rm -f $$file $$file.gz ;\
	done

.PHONY: uninstall
