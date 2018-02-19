# Makefile for power_knob/rapl plugin

AUTOMAKE_OPTIONS = foreign

PLUGIN_FLAGS = -module -avoid-version --export-dynamic -lm

AM_CPPFLAGS = -I$(top_srcdir) -I$(top_srcdir)/src/common

pkglib_LTLIBRARIES = power_knob_rapl.la
power_knob_rapl_la_SOURCES = power_knob_rapl.c
power_knob_rapl_la_LDFLAGS = $(SO_LDFLAGS) $(PLUGIN_FLAGS)

