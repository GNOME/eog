eog_helpdir = $(datadir)/gnome/help/eog/C

SGML_FILES =		\
	eog.sgml

EXTRA_DIST = \
	$(SGML_FILES)

all: eog

eog: $(SGML_FILES)
	-db2html eog.sgml

dist-hook:
	mkdir $(distdir)/eog
	-cp eog/*.html eog/*.css \
		$(distdir)/eog
	mkdir $(distdir)/fig
	-cp fig/*.png $(distdir)/fig

install-data-local: eog
	$(top_srcdir)/mkinstalldirs $(DESTDIR)$(eog_helpdir)/fig
	-for file in $(srcdir)/eog/*.html $(srcdir)/eog/*.css; do \
		basefile=`basename $$file`; \
		$(INSTALL_DATA) $$file $(DESTDIR)$(eog_helpdir)/$$basefile; \
	done
	-for file in $(srcdir)/fig/*.png; do \
		basefile=`basename $$file`; \
		$(INSTALL_DATA) $$file $(DESTDIR)$(eog_helpdir)/fig/$$basefile; \
	done

eog.ps: eog.sgml
	-db2ps $<

eog.rtf: eog.sgml	
	-db2rtf $<
