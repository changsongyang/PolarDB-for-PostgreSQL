# src/test/modules/Makefile

subdir = src/test/modules
top_builddir = ../../..
include $(top_builddir)/src/Makefile.global

SUBDIRS = \
		  brin \
		  commit_ts \
		  delay_execution \
		  dummy_index_am \
		  dummy_seclabel \
		  libpq_pipeline \
		  plsample \
		  snapshot_too_old \
		  spgist_name_ops \
		  test_bloomfilter \
		  test_ddl_deparse \
		  test_extensions \
		  test_ginpostinglist \
		  test_integerset \
		  test_lfind \
		  test_misc \
		  test_oat_hooks \
		  test_parser \
		  test_pg_dump \
		  test_predtest \
		  test_rbtree \
		  test_regex \
		  test_rls_hooks \
		  test_shm_mq \
		  unsafe_tests \
		  worker_spi \
		  test_polar_bulk_read

# POLAR
SUBDIRS += test_buffer
SUBDIRS += test_logindex test_slru test_local_cache test_procpool
SUBDIRS += test_polar_rsc
SUBDIRS += test_coredump_handler
# POLAR end

ifeq ($(with_ssl),openssl)
SUBDIRS += ssl_passphrase_callback
else
ALWAYS_SUBDIRS += ssl_passphrase_callback
endif

$(recurse)
$(recurse_always)
