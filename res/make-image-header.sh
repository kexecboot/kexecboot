#!/bin/sh
imageh=`basename $1 .png`-img.h
name=`basename $1 .png | tr [a-z] [A-Z]`_IMG
gdk-pixbuf-csource --macros $1 > $imageh.tmp
sed -e "s/MY_PIXBUF/${name}/g" -e "s/guint8/uint8/g" $imageh.tmp > $imageh && rm $imageh.tmp


