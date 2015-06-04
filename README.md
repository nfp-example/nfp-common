# nfp-common

 Copyright (c) 2015 Gavin J Stark
 All rights reserved.

## Description

The nfp-common project is a basic library and a suite of example
programs (firmware and application) that run on NFP-based NICs and
systems.

The project is fully available under the Apache v2 license which means
that you are free to get and use it for commercial and non-commercial
purposes.

The purpose of the project is to provide real applications that can be
used simply for networking applications and 10G, 40G and
100G. Further, the code is designed to be repurposed or enhanced to
provide feature-rich applications that are supported by other projects
or by commercial entities.

## Overview

The nfp-common project will include NFP micro-C libraries, NFP micro-C
firmware, and host applications.

## Set up

The nfp-common package requires a Linux or Windows host that includes:

* GNU make

* the NFP command-line tools to be installed on a system

The applications will in general run only under Linux.

Read the documentation in the doc/ directory.  It is quite rough, but it
 lists the functions; you will probably have to look at the code to work out
 how to use them. Look at the example programs.

The packages installed for building on Ubuntu comprise:

* libjansson-dev

* libhugetlbfs-dev

* nfp-bsp

* nfp-sdk

## Support

The nfp-common project is aimed at providing solid working example
applications which can be enhanced and supported commercially if
required. At present, if support is required, seek support through
github; as and when commercial supporters of derivative projects come
along a mechanism will be provided to advertise them.


## How to contribute to nfp-common

Development is coordinated through github using the wiki and issues.

If you would like to submit a patch for an issue, please add it to the
 github issue. Please be sure to include a textual explanation of what
 your patch does.

 Patches should be as up to date as possible, preferably relative to the
 current Git or the last snapshot. They should follow the coding style of
 nfp-project and compile without warnings.

 Our preferred format for changes is "diff -u" output. You might
 generate it like this:

```
# cd nfp-common-work
# [your changes]
# [test thoroughly]
# cd ..
# diff -ur nfp-common-orig nfp-common-work > mydiffs.patch
```
