# Makefile for power_schedule_slurmd/auto plugin

AUTOMAKE_OPTIONS = foreign

PLUGIN_FLAGS = -module -avoid-version --export-dynamic

AM_CPPFLAGS = -I$(top_srcdir) -I$(top_srcdir)/src/common

pkglib_LTLIBRARIES = power_schedule_slurmd_auto.la
power_schedule_slurmd_auto_la_SOURCES = power_schedule_slurmd_auto.c
power_schedule_slurmd_auto_la_LDFLAGS = $(SO_LDFLAGS) $(PLUGIN_FLAGS)


#power_auto_la_LIBADD = ../common/libpower_common.la
#
#force:
#$(power_auto_la_LIBADD) : force
#	@cd `dirname $@` && $(MAKE) `basename $@`
