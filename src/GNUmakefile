# Main GNUmakefile for VX32 source tree.
#
# To make it more convenient to change the makefile system
# without having to re-autoconf all the time,
# ALL configure script substitutions for the whole build system
# are concentrated in this file alone.
# This file simply establishes the appropriate substituted symbol names
# and then includes the top-level makefile fragment, './Makefrag'.

top_srcdir	= .
top_builddir	= .

prefix		= /usr/local
exec_prefix	= ${prefix}
INCDIR		= ${prefix}/include
BINDIR		= ${exec_prefix}/bin
LIBDIR		= ${exec_prefix}/lib

CC		= gcc
LD		= ld
AR		= ar

OS := $(shell uname |tr A-Z a-z)
ARCH := $(shell uname -m)

include Makefrag

