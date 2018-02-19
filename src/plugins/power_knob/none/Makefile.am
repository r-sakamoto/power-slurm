# Makefile for power_knob/none plugin

AUTOMAKE_OPTIONS = foreign

PLUGIN_FLAGS = -module -avoid-version --export-dynamic

AM_CPPFLAGS = -I$(top_srcdir) -I$(top_srcdir)/src/common

pkglib_LTLIBRARIES = power_knob_none.la
power_knob_none_la_SOURCES = power_knob_none.c
power_knob_none_la_LDFLAGS = $(SO_LDFLAGS) $(PLUGIN_FLAGS)

