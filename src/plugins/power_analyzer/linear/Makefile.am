# Makefile for power_analyzer/linear plugin

AUTOMAKE_OPTIONS = foreign

PLUGIN_FLAGS = -module -avoid-version --export-dynamic

AM_CPPFLAGS = -I$(top_srcdir) -I$(top_srcdir)/src/common

pkglib_LTLIBRARIES = power_analyzer_linear.la
power_analyzer_linear_la_SOURCES = power_analyzer_linear.c
power_analyzer_linear_la_LDFLAGS = $(SO_LDFLAGS) $(PLUGIN_FLAGS)


#power_linear_la_LIBADD = ../common/libpower_common.la
#
#force:
#$(power_linear_la_LIBADD) : force
#	@cd `dirname $@` && $(MAKE) `basename $@`
