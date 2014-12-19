if STATIC_BUILD
GLDFLAGS = -static
else
GCFLAGS = 
GLDADD = 
GLDFLAGS = 
endif

AM_CFLAGS = -I$(top_builddir)/lib  -I$(top_builddir)/include $(GCFLAGS)

bin_PROGRAMS = gdistcc
gdistcc_SOURCES =  gdistcc.c cmdline.c cmdline.h
gdistcc_LDADD   = $(top_builddir)/lib/libganglia.la \
                $(top_builddir)/lib/libgetopthelper.a \
                $(GLDADD)

gdistcc_LDFLAGS = $(GLDFLAGS) -lrt

EXTRA_DIST = cmdline.sh

