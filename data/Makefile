# Makefile for data files
# Copyright (C) 2002-2004 Olivier Sessink <olivier@winefish.openoffice.nl>

PACKAGE = winefish
VERSION = 1.3.2.6

SHELL = /bin/sh

top_srcdir = ..
top_builddir = ..
srcdir = .
prefix = /usr/local
exec_prefix = ${prefix}
bindir = ${exec_prefix}/bin
pkgdatadir = ${prefix}/share/winefish/
datadir = ${prefix}/share
localedir = $(datadir)/locale

GNOME1MENU=no
FDMENU=/usr/share/applications
FDMIME=/usr/share/mime
UPDATE_DESKTOP=no
UPDATE_MIME=/usr/bin/update-mime-database
GMIMEINFO=/usr/share/mime-info
GAPPREG=/usr/share/application-registry
ICONPATH=/usr/share/pixmaps
INSTALLDATA=install-update-mime  install-fdmenu install-fdmime install-gnome-2.4-mime install-gnome-2.4-appreg install-icon
HAVE_GNOME_VFS=@HAVE_GNOME_VFS@

INSTALL = /usr/bin/ginstall -c
INSTALL_DIR = $(INSTALL) -d -m 755 
INSTALL_DATA = ${INSTALL} -m 644

DEFAULTFILES = highlighting.default filetypes.default encodings.default custom_menu.default autotext.default words.default
FUNCREFFILES = funcref_symbols.xml funcref_packages.xml funcref_bib.xml
.SUFFIXES: .in .default

.in.default:
	sed -e 's!PKGDATADIR!$(pkgdatadir)!' < $< > $@

all: $(DEFAULTFILES)
	sed -e 's!ICONDIR!$(ICONPATH)!' < winefish.desktop.in > winefish.desktop
	sed -e 's!ICONDIR!$(ICONPATH)!' < winefish-project.desktop.in > winefish-project.desktop
	if test "x${HAVE_GNOME_VFS}" = "x1" ; then \
		sed -e 's!EXPECT_URIS!true!' -e 's!SUPPORTED_URI_SCHEMES!file,http,ftp,sftp,smb!' < winefish.applications.in > winefish.applications ;\
	else \
		sed -e 's!EXPECT_URIS!false!' -e 's!SUPPORTED_URI_SCHEMES!!' < winefish.applications.in > winefish.applications ;\
	fi

install-gnome1-menu:
	$(INSTALL_DIR) $(DESTDIR)$(GNOME1MENU)
	$(INSTALL_DATA) winefish.desktop $(DESTDIR)$(GNOME1MENU)
	$(INSTALL_DATA) winefish-project.desktop $(DESTDIR)$(GNOME1MENU)

install-fdmenu:
	$(INSTALL_DIR) $(DESTDIR)$(FDMENU)
	$(INSTALL_DATA) winefish.desktop $(DESTDIR)$(FDMENU)
	$(INSTALL_DATA) winefish-project.desktop $(DESTDIR)$(FDMENU)

install-update-menu:
	$(UPDATE_DESKTOP)

install-fdmime:
	$(INSTALL_DIR) $(DESTDIR)$(FDMIME)/packages
	$(INSTALL_DATA) winefish.xml $(DESTDIR)$(FDMIME)/packages

install-update-mime:
	$(UPDATE_MIME) $(FDMIME)

install-gnome-2.4-mime:
	$(INSTALL_DIR) $(DESTDIR)$(GMIMEINFO)
	$(INSTALL_DATA) winefish.mime $(DESTDIR)$(GMIMEINFO)
	$(INSTALL_DATA) winefish.keys $(DESTDIR)$(GMIMEINFO)

install-gnome-2.4-appreg:
	$(INSTALL_DIR) $(DESTDIR)$(GAPPREG)
	$(INSTALL_DATA) winefish.applications $(DESTDIR)$(GAPPREG)

install-icon:
	$(INSTALL_DIR) $(DESTDIR)$(ICONPATH)
	$(INSTALL_DATA) gnome-application-winefish.png $(DESTDIR)$(ICONPATH)/gnome-application-winefish-project.png

install: all $(INSTALLDATA)
	$(INSTALL_DIR) $(DESTDIR)$(pkgdatadir)
	for file in $(DEFAULTFILES) ; do \
		$(INSTALL_DATA) $$file $(DESTDIR)$(pkgdatadir) ;\
	done
	for file in $(FUNCREFFILES) ; do \
		$(INSTALL_DATA) $$file $(DESTDIR)$(pkgdatadir) ;\
	done

uninstall:
	rm -f $(FDMIME)/packages/winefish.xml
	rm -f $(GMIMEINFO)/winefish.mime
	rm -f $(GMIMEINFO)/winefish.keys
	rm -f $(GAPPREG)/winefish.applications
	rm -f $(GNOME1MENU)/winefish.desktop
	rm -f $(FDMENU)/winefish.desktop
	rm -f $(FDMENU)/winefish-project.desktop
	rm -f $(ICONPATH)/gnome-application-winefish-project.png
	@for file in $(DEFAULTFILES) ; do \
		rm -f $(pkgdatadir)$$file ;\
	done
	@for file in $(FUNCREFFILES) ; do \
		rm -f $(pkgdatadir)$$file ;\
	done

clean:
	rm -f *.default
	rm -f winefish.desktop
	rm -f winefish-project.desktop
	rm -f winefish.applications

distclean: clean
	rm -f Makefile *.rej *.orig *~
