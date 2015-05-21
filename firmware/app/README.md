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
