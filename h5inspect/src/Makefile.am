#
# Copyright by The HDF Group.
# All rights reserved.
#
# This file is part of h5check. The full h5check copyright notice,
# including terms governing use, modification, and redistribution, is
# contained in the file COPYING, which can be found at the root of the
# source code distribution tree.  If you do not have access to this file,
# you may request a copy from help@hdfgroup.org.
# 
##
## Makefile.am
## Run automake to generate a Makefile.in from this file.
#
# HDF5 Library Makefile(.in)
#

include $(top_srcdir)/config/commence.am

# This is our main target. The h5check library is not installed yet.
noinst_LIBRARIES=libh5check.a

libh5check_a_SOURCES=h5checker.c h5checker_api.c h5error.c h5checkerV2_0.c h5pline.c h5logger.c


include $(top_srcdir)/config/conclude.am

