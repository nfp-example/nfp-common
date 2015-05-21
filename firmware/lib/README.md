# nfp-common/firmware/lib

 Copyright (c) 2015 Gavin J Stark
 All rights reserved.

## Description

This directory contains firmware libraries. Each library consists of a
'<lib>' subdirectory which contains '<area>.h' header file, a
'lib<lib>.c' source file, and potentially a '<lib>/_c' subdirectory
containing more C source files.

A micro-c library is used by including '<lib>/<area>.h' in the source
code, and by adding '<lib>/lib<lib>.c' to the 'nfcc' command
line. This latter is made easier with the 'micro_c.add_fw_libs'
template.

