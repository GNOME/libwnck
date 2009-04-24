SUBDIRS = libwnck doc po

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libwnck-1.0.pc

DISTCHECK_CONFIGURE_FLAGS = --enable-gtk-doc

EXTRA_DIST = \
	MAINTAINERS			\
	libwnck-1.0.pc.in		\
	libwnck-1.0-uninstalled.pc.in	\
	HACKING

MAINTAINERCLEANFILES = \
	$(srcdir)/INSTALL \
	$(srcdir)/aclocal.m4 \
	$(srcdir)/config.guess \
	$(srcdir)/config.h.in \
	$(srcdir)/config.sub \
	$(srcdir)/depcomp \
	$(srcdir)/install-sh \
	$(srcdir)/ltmain.sh \
	$(srcdir)/missing \
	$(srcdir)/mkinstalldirs \
	`find "$(srcdir)" -type f -name Makefile.in -print` \
	intltool-extract.in \
	intltool-merge.in \
	intltool-update.in \
	`find "$(srcdir)" -type f -name .gitignore -print` \
	$(srcdir)/gtk-doc.make \
	$(srcdir)/m4/gtk-doc.m4 \
	$(srcdir)/m4/intltool.m4

-include $(top_srcdir)/git.mk
