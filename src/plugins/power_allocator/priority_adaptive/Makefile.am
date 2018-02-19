# Makefile for power_allocator_node/priority_adaptive/scheduler plugin

AUTOMAKE_OPTIONS = foreign

PLUGIN_FLAGS = -module -avoid-version --export-dynamic

AM_CPPFLAGS = -I$(top_srcdir) -I$(top_srcdir)/src/common

pkglib_LTLIBRARIES = priority_adaptive.la
priority_adaptive_la_SOURCES =	priority_adaptive.c \
				priority_adaptive_wrapper.c
priority_adaptive_la_LDFLAGS = $(SO_LDFLAGS) $(PLUGIN_FLAGS)


#power_none_la_LIBADD = ../common/libpower_common.la
#
#force:
#$(power_none_la_LIBADD) : force
#	@cd `dirname $@` && $(MAKE) `basename $@`
