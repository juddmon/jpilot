# $Id: Makefile.am,v 1.5 2006/09/28 18:36:20 rikster5 Exp $

# The dialer is conditionally built only on systems that support it
# See configure.in for the list of operating systems currently supported
if JPILOT_DIALER

bin_PROGRAMS = jpilot-dial
jpilot_dial_SOURCES = jpilot-dial.c
jpilot_dial_LDADD = -lm

endif

EXTRA_DIST = README
