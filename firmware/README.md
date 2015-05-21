# nfp-common/firmware

 Copyright (c) 2015 Gavin J Stark
 All rights reserved.

## Description

This directory contains source, library and build directories for NFP
firmware.

## Makefiles

### Makefile

The makefile contains the basic variable settings for firmware builds
and it includes the make templates and then the application makefile

### Makefile.templates

This file contains standard templates for simplifying firmware builds
through assembler and micro-c compilation, and linking.

### Makefile.apps

This file contains the specific template invocations to build
different sets of firmware

## 'lib' subdirectory

The lib subdirectory contains firmware libraries.

## 'app' subdirectory

The app subdirectory contains firmware source code that may utilize
libraries, and which builds into firmware objects which are combined
into an 'nffw' firmware build.

Hence each microengine code load has a toplevel firmware source code
file in this directory; it may use other firmware source code files in
this directory as well as libraries.

## build

The build subdirectory contains intermediate build files for firmware
builds.

## nffw

The nffw subdirectory contains the final firmware objects that can be
loaded onto the NFP.
