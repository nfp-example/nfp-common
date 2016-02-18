# nfp-common/firmware/app

 Copyright (c) 2015 Gavin J Stark
 All rights reserved.

## Description

This directory contains application firmware source code. An
application is usually built with a single or very few C files
containing the functions required by the (many) microengines in the
application; in a sense these are application libraries. Then
individual microengine loads have a toplevel source code file that
includes the application library/libraries, and which invokes just the
functions required for that microengine load.

All application libraries and toplevel source code should be named
starting with the same application-derived prefix.

## Packet Capture Library

The packet capture application library is initially written to be used by the
pcap application firmware. It provides some low level functions, plus
the application functions that make up the pcap application.

The library consists of the pcap_lib.c and
pcap_lib.h files.

## pcap application MEs

The pcap application ME source code provides main functions for the
MEs required for a packet capture application which runs with pcap on
the host.

The simplest application for, say, 10GbE requires one pcap_recycle,
one pcap_host, and two pcap_rx (to cope with the 15Mpps of minimum
sized packets).

Note that if the setup of the application is changed (i.e. the number
of islands, MEs or threads etc) then the pcap.h header file must be
changed to match so that the synchronization staging operates
correctly.

### pcap_host.c

The pcap_host.c code provides the simplest host DMA management for the
packet capture application. This runs one master
thread and seven slave threads.

The master thread takes MU buffers where packets are
accumulated and monitors them, distributing DMA work to the
slaves.

The slave threads perform the DMA work for masters.

More than one master thread can be provided in a system, with each
owning a single MU buffer at any one time.

All the slave threads cooperate to handle the DMAs for all the
masters - they communicate with a single MU work queue.

So the pcap_host.c code can be run on more than one ME.

### pcap_recycle.c

The pcap_recycle.c code instantiates a single thread running the MU
buffer recycler. Precisely one recycler thread is required in the
system. It also runs seven slave DMA threads, to provide extra DMA
capability for the DMA masters.

### pcap_rx.c

The pcap_rx.c code runs eight threads of packet receive. These threads
take received packets and allocate space in an MU buffer for them, and
then they transfer the packet data into the MU buffer.

A single thread is estimated to provide about 1Mpps for small
packets. So to provide line rate at 40GbE eight MEs with eight threads
could provide 64 threads and 64Mpps, provide no other limits are
reached.


