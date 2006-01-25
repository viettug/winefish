# Winefish LaTeX Editor
# Makefile.in - the Makefile template
# Copyright (C) 2002 Olivier Sessink
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

top_srcdir = ..
top_builddir = ..
srcdir = .
prefix = /usr/local
exec_prefix = ${prefix}
bindir = ${exec_prefix}/bin
pkgdatadir = ${prefix}/share/winefish/

infodir = $(prefix)/info
libdir = $(prefix)/lib/gnudl
mandir = $(prefix)/man/man1
localedir = ${prefix}/share/locale
gnulocaledir = $(prefix)/share/locale
aliaspath = $(localedir):.

CC = gcc
CFLAGS = -g -O2 -Wall -pipe -DXTHREADS -D_REENTRANT -DXUSE_MTSAFE_API -I/usr/include/gtk-2.0 -I/usr/lib/gtk-2.0/include -I/usr/X11R6/include -I/usr/include/atk-1.0 -I/usr/include/pango-1.0 -I/usr/include/freetype2 -I/usr/include/freetype2/config -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include   -DORBIT2=1 -pthread -DXTHREADS -D_REENTRANT -DXUSE_MTSAFE_API -I/usr/include/libgnomeui-2.0 -I/usr/include/libgnome-2.0 -I/usr/include/libgnomecanvas-2.0 -I/usr/include/gtk-2.0 -I/usr/include/libart-2.0 -I/usr/include/gconf/2 -I/usr/include/libbonoboui-2.0 -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include -I/usr/include/orbit-2.0 -I/usr/include/libbonobo-2.0 -I/usr/include/gnome-vfs-2.0 -I/usr/lib/gnome-vfs-2.0/include -I/usr/include/bonobo-activation-2.0 -I/usr/include/pango-1.0 -I/usr/X11R6/include -I/usr/include/freetype2 -I/usr/lib/gtk-2.0/include -I/usr/include/atk-1.0 -I/usr/include/freetype2/config -I/usr/include/libxml2   
LDFLAGS = 
LIBS =  -lgtk-x11-2.0 -lgdk-x11-2.0 -latk-1.0 -lgdk_pixbuf-2.0 -lm -lpangoxft-1.0 -lpangox-1.0 -lpango-1.0 -lgobject-2.0 -lgmodule-2.0 -ldl -lglib-2.0   -pthread -Wl,--export-dynamic -L/usr/X11R6/lib -lgnomeui-2 -lSM -lICE -lbonoboui-2 -lxml2 -lpthread -lz -lgnomecanvas-2 -lgnome-2 -lpopt -lart_lgpl_2 -lpangoft2-1.0 -lgtk-x11-2.0 -lgdk-x11-2.0 -latk-1.0 -lgdk_pixbuf-2.0 -lpangoxft-1.0 -lpangox-1.0 -lpango-1.0 -lbonobo-2 -lgconf-2 -lgnomevfs-2 -lbonobo-activation -lORBit-2 -lgobject-2.0 -lm -lgmodule-2.0 -ldl -lgthread-2.0 -lglib-2.0   -L/usr/lib -lpcre -lrt -laspell
INSTALL = /usr/bin/ginstall -c
INSTALL_DIR = $(INSTALL) -d -m 755 
INSTALL_PROGRAM = $(INSTALL) -s -m 755
DEFS = -DLOCALEDIR=\"$(localedir)\" -DGNULOCALEDIR=\"$(gnulocaledir)\" \
-DLOCALE_ALIAS_PATH=\"$(aliaspath)\" -DPKGDATADIR=\"$(pkgdatadir)\" -DHAVE_CONFIG_H

SRCS = \
about.c \
autox.c \
bf_lib.c \
bfspell.c \
bookmark.c \
char_table.c \
document.c \
filebrowser.c \
fref.c \
func_grep.c \
gtk_easy.c \
gui.c \
highlight.c \
html.c \
html_diag.c \
image.c \
menu.c \
msg_queue.c \
outputbox.c \
outputbox_bf.c \
outputbox_filter.c \
outputbox_ka.c \
pixmap.c \
preferences.c \
project.c \
rcfile.c \
snooper.c \
snr2.c \
stringlist.c \
treetips.c \
undo_redo.c \
winefish.c \
wizards.c \


OBJS = $(SRCS:.c=.o)

all: winefish
winefish: $(SRCS) $(OBJS)
	$(CC) $(DEFS) -o winefish $(OBJS) $(LDFLAGS) $(LIBS)
	@echo "The winefish binary is succesfully created"

.c.o: $(SRCS)
	$(CC) $(DEFS) $(CFLAGS) -c -o $@ $<

$(OBJS): bluefish.h

clean:
	rm -f $(OBJS) winefish

patchclean: clean
	rm -f *.orig *.rej  

distclean: patchclean
	rm -f config.h Makefile *~ *.bak core

install: all
	$(INSTALL_DIR) $(DESTDIR)$(bindir)
	$(INSTALL_PROGRAM) ./winefish $(DESTDIR)$(bindir)/winefish

uninstall:
	/bin/rm -f $(DESTDIR)$(bindir)/winefish
