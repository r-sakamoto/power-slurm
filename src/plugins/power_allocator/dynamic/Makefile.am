# Makefile for power_allocator/dynamic plugin

AUTOMAKE_OPTIONS = foreign

PLUGIN_FLAGS = -module -avoid-version --export-dynamic

AM_CPPFLAGS = -I$(top_srcdir) -I$(top_srcdir)/src/common

pkglib_LTLIBRARIES = power_allocator_dynamic.la
power_allocator_dynamic_la_SOURCES = power_allocator_dynamic.c
power_allocator_dynamic_la_LDFLAGS = $(SO_LDFLAGS) $(PLUGIN_FLAGS)


#power_dynamic_la_LIBADD = ../common/libpower_common.la
#
#force:
#$(power_dynamic_la_LIBADD) : force
#	@cd `dirname $@` && $(MAKE) `basename $@`
