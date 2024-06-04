ACLOCAL_AMFLAGS = ${ACLOCAL_FLAGS} -I m4

bin_PROGRAMS = fisslock_decider

fisslock_decider_SOURCES = main.c bfrt.c

fisslock_decider_CPPFLAGS = $(AM_CPPFLAGS) -I$(top_srcdir)
fisslock_decider_CFLAGS = $(AM_CFLAGS) $(BF_DRIVERS_CFLAGS)
fisslock_decider_CXXFLAGS = $(COMMON_CXXFLAGS) $(AM_CXXFLAGS) $(BF_DRIVERS_CXXFLAGS) \
  -Wno-shadow -Wno-pedantic -Wno-reorder -Wno-sign-compare -Wno-type-limits \
  -Wno-unused-parameter -Wno-unused-variable -Wno-unused-label -Wno-switch \
  -Wno-write-strings

fisslock_decider_LDADD = $(top_builddir)/src/libdriver.la -lbfutils -lbfsys $(top_builddir)/bf_switchd/libbf_switchd_lib.la
fisslock_decider_LDFLAGS = $(LD_IGNORE_UNRESOLVED_SYMBOLS) -Wl,--no-as-needed
