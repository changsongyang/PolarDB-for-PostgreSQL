#-------------------------------------------------------------------------
#
# Makefile--
#    Makefile for utils/cache
#
# IDENTIFICATION
#    src/backend/utils/cache/Makefile
#
#-------------------------------------------------------------------------

subdir = src/backend/utils/cache
top_builddir = ../../../..
include $(top_builddir)/src/Makefile.global

OBJS = \
	attoptcache.o \
	catcache.o \
	evtcache.o \
	inval.o \
	lsyscache.o \
	partcache.o \
	plancache.o \
	relcache.o \
	relfilenodemap.o \
	relmapper.o \
	spccache.o \
	syscache.o \
	ts_cache.o \
	typcache.o

OBJS += polar_local_cache.o

include $(top_srcdir)/src/backend/common.mk
