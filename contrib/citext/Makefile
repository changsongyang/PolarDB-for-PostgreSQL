# contrib/citext/Makefile

MODULES = citext

EXTENSION = citext
DATA = citext--1.4.sql \
	citext--1.5--1.6.sql \
	citext--1.4--1.5.sql \
	citext--1.3--1.4.sql \
	citext--1.2--1.3.sql citext--1.1--1.2.sql \
	citext--1.0--1.1.sql
PGFILEDESC = "citext - case-insensitive character string data type"

REGRESS = citext citext_utf8

# PolarDB: tablespace is not supported
# REGRESS += create_index_acl

ifdef USE_PGXS
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
subdir = contrib/citext
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif
