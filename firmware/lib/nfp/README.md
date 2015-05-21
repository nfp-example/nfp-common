# nfp-common/firmware/lib/nfp

 Copyright (c) 2015 Gavin J Stark
 All rights reserved.

## Description

This directory contains NFP-wide useful library functions for micro-C.

It has the standard structure for a micro-C library: various <area>.h
header files, included by micro-C code when needed, and a libnfp.c
micro-C file that can be included in micro-C code builds.

The NFP has a lot of functions that the library abstracts, such as
different memories (CLS, MU). Each has a separate header file
(e.g. nfp/cls.h), and is implemented in a separate micro-C source file
(e.g. _c/cls.c). The latter are all included using libnfp.c.
