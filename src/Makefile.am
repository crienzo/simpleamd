lib_LTLIBRARIES = libsimpleamd.la
libsimpleamd_la_SOURCES = amd.c logger.c vad.c samd_private.h
include_HEADERS = simpleamd.h
libsimpleamd_la_LDFLAGS = -shared
libsimpleamd_la_LIBADD = -lm

bin_PROGRAMS = simpleamd
simpleamd_SOURCES = simpleamd.c
simpleamd_LDADD = libsimpleamd.la
