# Makefile for power_allocator/none plugin

AUTOMAKE_OPTIONS = foreign

PLUGIN_FLAGS = -module -avoid-version --export-dynamic

AM_CPPFLAGS = -I$(top_srcdir) -I$(top_srcdir)/src/common

pkglib_LTLIBRARIES = power_allocator_none.la
power_allocator_none_la_SOURCES = power_allocator_none.c
power_allocator_none_la_LDFLAGS = $(SO_LDFLAGS) $(PLUGIN_FLAGS)


#power_none_la_LIBADD = ../common/libpower_common.la
#
#force:
#$(power_none_la_LIBADD) : force
#	@cd `dirname $@` && $(MAKE) `basename $@`
