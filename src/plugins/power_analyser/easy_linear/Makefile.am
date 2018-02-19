# Makefile for power_analyser/easy_linear plugin

AUTOMAKE_OPTIONS = foreign

PLUGIN_FLAGS = -module -avoid-version --export-dynamic

AM_CPPFLAGS = -I$(top_srcdir) -I$(top_srcdir)/src/common

pkglib_LTLIBRARIES = easy_linear.la
easy_linear_la_SOURCES =	easy_linear.c 
easy_linear_la_LDFLAGS = $(SO_LDFLAGS) $(PLUGIN_FLAGS)


#power_none_la_LIBADD = ../common/libpower_common.la
#
#force:
#$(power_none_la_LIBADD) : force
#	@cd `dirname $@` && $(MAKE) `basename $@`
