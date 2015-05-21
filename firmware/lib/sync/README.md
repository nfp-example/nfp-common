# nfp-common/firmware/lib/sync

 Copyright (c) 2015 Gavin J Stark
 All rights reserved.

## Description

This directory contains synchronization primitives.

It has the standard structure for a micro-C library: various <area>.h
header files, included by micro-C code when needed, and a libnfp.c
micro-C file that can be included in micro-C code builds.

## Staged synchronization

A library is included which supports stages of initialization and
synchronization for microengines as they complete stages. Stages are
numbered from 1 to a build-time-specified maximum.

A toplevel code may have to initialize memory, then queues, then
host-shared structures, and then run its main code. This operation can
be split into stages - stage 1 initializes memory; stage 2 sets up
queues; stage 3 waits for the host; stage 4 reads the host data and
distributes it; stage 5 is the main code execution.

Some microengines may have nothing to do in the first stage, but have
work to do in the second stage, for example.

The staged synchronization structure provides synchronization barriers
to allow ME threads to block until the start of a stage.

The number of stages, number of islands, number of MEs within each
island, and number of threads within each ME must be defined at
compilation time.

A single micro-C function can then be invoked by an ME thread,
`sync_state_set_stage_complete(stage)`, which will synchronize threads
within an ME, synchronize the MEs within an island, and synchronize
islands, so that every ME thread is operating at the same stage.
