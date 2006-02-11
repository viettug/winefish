# this script can help to find files that are actually not in use, run it from the basedir:
#
#for file in inline_images/*.png; do
#	BASE=`echo $file | sed -e 's/.png//' -e 's?inline_images/??'`
#	if ! egrep -q "[\\t ]*\{[0-9]+, ${BASE}\}" src/pixmap.c ; then
#		if grep -q "^[^#]*${BASE} ${BASE}.png" inline_images/Makefile; then
#			echo $BASE
#		fi
#	fi
#done

TOOLBAR = \
	pixmap_winefish_icon1 winefish_icon1.png \
	pixmap_close close.png \
	pixmap_open_small open_small.png

GENERAL = \
	pixmap_close_small close_small.png

all:
	gdk-pixbuf-csource --raw --extern --build-list $(TOOLBAR) > toolbar_icons.c
	gdk-pixbuf-csource --raw --extern --build-list $(GENERAL) > general_icons.c
