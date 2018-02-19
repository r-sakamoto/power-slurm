# Makefile for select/linear_power plugin

AUTOMAKE_OPTIONS = foreign

PLUGIN_FLAGS = -module -avoid-version --export-dynamic

AM_CPPFLAGS = -I$(top_srcdir) -I$(top_srcdir)/src/common

pkglib_LTLIBRARIES = select_linear_power.la

# Linear node selection plugin.
select_linear_power_la_SOURCES = select_linear_power.c select_linear_power.h
select_linear_power_la_LDFLAGS = $(SO_LDFLAGS) $(PLUGIN_FLAGS)

