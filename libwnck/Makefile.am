
INCLUDES=@LIBWNCK_CFLAGS@ -DG_LOG_DOMAIN=\"Wnck\" -I$(top_srcdir)

lib_LTLIBRARIES=libwnck-1.la

libwnckincludedir=$(includedir)/libwnck-1.0/libwnck
wnck_headers=			\
	libwnck.h		\
	application.h		\
	screen.h		\
	window.h 		\
	workspace.h

wnck_built_installed_headers=wnck-enum-types.h

libwnckinclude_HEADERS=			\
	$(wnck_headers)			\
	$(wnck_built_installed_headers)

wnck_built_headers = $(wnck_built_installed_headers) wnck-marshal.h
wnck_built_cfiles = wnck-enum-types.c  wnck-marshal.c

libwnck_1_la_SOURCES= 	\
	application.c	\
	private.h	\
	screen.c	\
	window.c	\
	workspace.c	\
	xutils.c	\
	xutils.h	\
	$(wnck_built_headers)	\
	$(wnck_built_cfiles)

test_wnck_SOURCES=		\
	test-wnck.c

noinst_PROGRAMS=test-wnck

LDFLAGS=								    \
	-version-info $(LIBWNCK_CURRENT):$(LIBWNCK_REVISION):$(LIBWNCK_AGE) \
        -export-symbols-regex "^[[^_]].*"

libwnck_1_la_LIBADD=   \
	@LIBWNCK_LIBS@

test_wnck_LDADD=@LIBWNCK_LIBS@ libwnck-1.la

## we just punt on built sources, and check them into cvs, manually updating 
## when required.

regenerate-built-sources:
	echo "Building wnck-enum-types.h" \
	&& glib-mkenums --fhead "#ifndef __WNCK_ENUM_TYPES_H__\n#define __WNCK_ENUM_TYPES_H__\n" --fprod "/* enumerations from \"@filename@\" */\n" --vhead "GType @enum_name@_get_type (void);\n#define WNCK_TYPE_@ENUMSHORT@ (@enum_name@_get_type())\n" --ftail "#endif /* __WNCK_ENUM_TYPES_H__ */" $(wnck_headers) > wnck-enum-types.h \
	&& echo "Building wnck-enum-types.c"	\
	&& glib-mkenums --fhead "#include <libwnck/libwnck.h>" --fprod "\n/* enumerations from \"@filename@\" */" --vhead "GType\n@enum_name@_get_type (void)\n{\n  static GType etype = 0;\n  if (etype == 0) {\n    static const G@Type@Value values[] = {" --vprod "      { @VALUENAME@, \"@VALUENAME@\", \"@valuenick@\" }," --vtail "      { 0, NULL, NULL }\n    };\n    etype = g_@type@_register_static (\"@EnumName@\", values);\n  }\n  return etype;\n}\n" $(wnck_headers) > wnck-enum-types.c \
	&& echo "Building wnck-marshal.h" \
	&& @GLIB_GENMARSHAL@ --prefix=_wnck_marshal wnck-marshal.list --header > wnck-marshal.h \
	&& echo "Building wnck-marshal.c" \
	&& echo "#include <libwnck/libwnck.h>" > wnck-marshal.c \
        && @GLIB_GENMARSHAL@ --prefix=_wnck_marshal wnck-marshal.list --body >> wnck-marshal.c

EXTRA_DIST= \
	wnck-marshal.list


