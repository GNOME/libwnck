
INCLUDES=@LIBWNCK_CFLAGS@ -DG_LOG_DOMAIN=\"Wnck\" -I$(top_srcdir) \
	-DWNCK_I_KNOW_THIS_IS_UNSTABLE -DWNCK_LOCALEDIR=\"$(wncklocaledir)\" \
	-DSN_API_NOT_YET_FROZEN=1 $(DISABLE_DEPRECATED_CFLAGS) $(WARN_CFLAGS)

lib_LTLIBRARIES=libwnck-1.la

libwnckincludedir=$(includedir)/libwnck-1.0/libwnck
wnck_headers=			\
	libwnck.h		\
	pager.h			\
	application.h		\
	class-group.h		\
	screen.h		\
	selector.h		\
	tasklist.h		\
	util.h			\
	window.h 		\
	window-action-menu.h	\
	workspace.h

wnck_built_installed_headers=wnck-enum-types.h

libwnckinclude_HEADERS=			\
	$(wnck_headers)			\
	$(wnck_built_installed_headers)

wnck_built_headers = $(wnck_built_installed_headers) wnck-marshal.h
wnck_built_cfiles = wnck-enum-types.c wnck-marshal.c

wnck_accessibility_files =			\
	pager-accessible.c			\
	pager-accessible.h			\
	pager-accessible-factory.c		\
	pager-accessible-factory.h		\
	workspace-accessible.c			\
	workspace-accessible.h			\
	workspace-accessible-factory.c		\
	workspace-accessible-factory.h

libwnck_1_la_SOURCES= 		\
	$(wnck_built_headers)	\
	$(wnck_built_cfiles)	\
	application.c		\
	class-group.c		\
	inlinepixbufs.h		\
	pager.c			\
	private.h		\
	screen.c		\
	selector.c		\
	tasklist.c		\
	util.c			\
	window.c		\
	window-action-menu.c	\
	workspace.c		\
	xutils.c		\
	xutils.h		\
	$(wnck_accessibility_files)

$(libwnck_1_la_OBJECTS): inlinepixbufs.h

test_wnck_SOURCES=		\
	test-wnck.c

test_tasklist_SOURCES=		\
	test-tasklist.c

test_selector_SOURCES=		\
	test-selector.c

test_pager_SOURCES=		\
	test-pager.c

noinst_PROGRAMS=		\
	test-pager		\
	test-wnck		\
	test-selector		\
	test-tasklist

libwnck_1_la_LDFLAGS=								    \
	-version-info $(LIBWNCK_CURRENT):$(LIBWNCK_REVISION):$(LIBWNCK_AGE) \
	-no-undefined \
        -export-symbols-regex "^[^_].*"

libwnck_1_la_LIBADD=   \
	@LIBWNCK_LIBS@ @X_LIBS@

test_wnck_LDADD=@LIBWNCK_LIBS@ libwnck-1.la
test_tasklist_LDADD=@LIBWNCK_LIBS@ libwnck-1.la
test_selector_LDADD=@LIBWNCK_LIBS@ libwnck-1.la
test_pager_LDADD=@LIBWNCK_LIBS@ libwnck-1.la

wnck-marshal.h: stamp-wnck-marshal.h
	@true
stamp-wnck-marshal.h: wnck-marshal.list
	(cd $(srcdir) \
	&& glib-genmarshal --prefix=_wnck_marshal wnck-marshal.list --header) > new-wnck-marshal.h \
	&& (cmp -s new-wnck-marshal.h wnck-marshal.h || cp new-wnck-marshal.h wnck-marshal.h) \
	&& rm -f new-wnck-marshal.h \
	&& echo timestamp > $(@F)

wnck-marshal.c: wnck-marshal.list wnck-marshal.h
	echo "#include <libwnck/libwnck.h>" > new-wnck-marshal.c \
	&& echo "#include \"wnck-marshal.h\"" >> new-wnck-marshal.c \
	&& (cd $(srcdir) \
	&& glib-genmarshal --prefix=_wnck_marshal wnck-marshal.list --body) >> new-wnck-marshal.c \
	&& cp new-wnck-marshal.c wnck-marshal.c \
	&& rm -f new-wnck-marshal.c

wnck-enum-types.h: stamp-wnck-enum-types.h
	@true
stamp-wnck-enum-types.h: $(wnck_headers)
	(cd $(srcdir) \
	&& glib-mkenums \
		--fhead "#ifndef __WNCK_ENUM_TYPES_H__\n#define __WNCK_ENUM_TYPES_H__\n" \
		--fprod "/* enumerations from \"@filename@\" */\n" \
		--vhead "GType @enum_name@_get_type (void);\n#define WNCK_TYPE_@ENUMSHORT@ (@enum_name@_get_type())\n" \
		--ftail "#endif /* __WNCK_ENUM_TYPES_H__ */" \
		$(wnck_headers)) > new-wnck-enum-types.h \
	&& (cmp -s new-wnck-enum-types.h wnck-enum-types.h || cp new-wnck-enum-types.h wnck-enum-types.h) \
	&& rm -f new-wnck-enum-types.h \
	&& echo timestamp > $(@F)

wnck-enum-types.c: $(wnck_headers) wnck-enum-types.h
	(cd $(srcdir) \
	&& glib-mkenums \
		--fhead "#include <libwnck/libwnck.h>" \
		--fprod "\n/* enumerations from \"@filename@\" */" \
		--vhead "GType\n@enum_name@_get_type (void)\n{\n  static GType etype = 0;\n  if (etype == 0) {\n    static const G@Type@Value values[] = {" \
		--vprod "      { @VALUENAME@, \"@VALUENAME@\", \"@valuenick@\" }," \
		--vtail "      { 0, NULL, NULL }\n    };\n    etype = g_@type@_register_static (\"@EnumName@\", values);\n  }\n  return etype;\n}\n" \
		$(wnck_headers)) > new-wnck-enum-types.c \
	&& cp new-wnck-enum-types.c $(@F) \
	&& rm -f new-wnck-enum-types.c

IMAGES=default_icon.png stock_delete.png stock_maximize.png stock_minimize.png
VARIABLES=default_icon_data $(srcdir)/default_icon.png 		\
	stock_delete_data $(srcdir)/stock_delete.png 		\
	stock_maximize_data $(srcdir)/stock_maximize.png 	\
	stock_minimize_data $(srcdir)/stock_minimize.png

noinst_DATA = inlinepixbufs.h
CLEANFILES =			\
	$(noinst_DATA)		\
	new-wnck-enum-types.c	\
	new-wnck-enum-types.h	\
	new-wnck-marshal.c	\
	new-wnck-marshal.h
DISTCLEANFILES =		\
	stamp-wnck-enum-types.h	\
	stamp-wnck-marshal.h	\
	$(wnck_built_headers)	\
	$(wnck_built_cfiles)

inlinepixbufs.h: $(IMAGES)
	$(GDK_PIXBUF_CSOURCE) --raw --build-list $(VARIABLES) >$(srcdir)/inlinepixbufs.h

$(OBJECTS): inlinepixbufs.h

EXTRA_DIST= 			\
	wnck-marshal.list	\
	$(IMAGES)		\
	$(wnck_built_headers)	\
	$(wnck_built_cfiles)
