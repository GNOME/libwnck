
INCLUDES=@LIBWNCK_CFLAGS@ -I ../src

lib_LTLIBRARIES=libwnck-1.la

libwnckincludedir=$(includedir)/libwnck-1.0/libwnck
libwnckinclude_HEADERS=	\
	application.h	\
	window.h 

libwnck_1_la_SOURCES= 	\
	application.c	\
	window.c

test_wnck_SOURCES=		\
	test-wnck.c

noinst_PROGRAMS=test-wnck

LDFLAGS=  \
	-version-info $(LIBWNCK_CURRENT):$(LIBWNCK_REVISION):$(LIBWNCK_AGE)

libwnck_1_la_LIBADD=   \
	@LIBWNCK_LIBS@

test_wnck_LDADD=@LIBWNCK_LIBS@ libwnck-1.la

