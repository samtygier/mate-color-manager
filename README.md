mate-color-manager
==================

fork of gnome-color-manager-2.32.0

Installation:

./autogen.sh

make

sudo make install


Build dependencies:

-- fedora

gtk2-devel, rarian-compat, vte-devel, mate-doc-utils, unique-devel, libgudev1-devel,
dbus-glib-devel, libXxf86vm-devel, libXrandr-devel, mate-desktop-devel, lcms-devel,
cups-devel, sane-backends-devel, libtiff-devel, libcanberra-devel, mate-common,
libnotify-devel, exiv2-devel, libexif-devel, glib2-devel

--debian based

libgtk2.0-dev rarian-compat libvte-dev gnome-doc-utils mate-doc-utils libunique-dev libgudev-1.0-dev
libdbus-glib-1-dev libxxf86vm-dev libxrandr-dev libmatedesktop-dev liblcms-dev
libcups2-dev libsane-dev libtiff-dev libcanberra-gtk-dev mate-common
libnotify-dev libexiv2-dev libglib2.0-dev

If start of mcm-prefs fails after installation use the --sync option

mcm-prefs --sync


News: 2014.02.25
Update to 1.7.0
	-- switch to yelp tools

News: 2013.10.31
Update to 1.6.1
	-- port to lcms2

News: 2013.05.23

Update to 1.6.0
	-- switch to libnotify

News: 2012.11.02

Update to 1.5.0
	-- Port to GSettings

