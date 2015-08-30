include $(top_srcdir)/glib.mk

AM_CPPFLAGS = \
	$(EOG_CFLAGS)			\
	-I$(top_srcdir)/src		\
	$(NULL)

LDADD = \
	$(EOG_LIBS)			\
	$(top_builddir)/src/libeog.la	\
	$(NULL)

test_programs = image

testfiles_data = \
	files/eog.jpg			\
	files/eog.png			\
	files/eog.svg			\
	files/eog.gif			\
	$(NULL)

if ENABLE_INSTALLED_TESTS
testfilesdir = $(installed_testdir)/files
testfiles_DATA = $(testfiles_data)
endif

EXTRA_DIST += $(testfiles_data)
