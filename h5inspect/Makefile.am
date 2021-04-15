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
##
# 
# This makefile mostly just reinvokes make in the various subdirectories
# but does so in the correct order.  You can alternatively invoke make from
# each subdirectory manually.
#
# Top-level HDF5 Makefile(.in)

# Activate this later.
#include $(top_srcdir)/config/commence.am

# Define subdirectories to build.
## Automake understands that `make distclean' should recurse into
## conditional subdirectories even if `make all' does not.
## We need to list the examples directory in the DIST_SUBDIRS variable
## so that it will be visited by `make distclean'
# Add this directory to SUBDIRS so that examples get built after tools
# but before examples in extra interfaces (c++ and fortran).
# Since we're explicitly listing DIST_SUBDIRS, we also need to list
# directories that are only conditionally built (so that their Makefiles
# are cleaned as well).
# Note that `make clean' will not affect the examples or doc directories. 

# Conditionals.  These conditionals are defined during configure
# Define each variable to empty if it is not used to placate pmake
#if BUILD_PARALLEL_CONDITIONAL
#   TESTPARALLEL_DIR =testpar
#else
#   TESTPARALLEL_DIR=
#endif
#if BUILD_CXX_CONDITIONAL
#   CXX_DIR =c++
#else
#   CXX_DIR=
#endif
#if BUILD_FORTRAN_CONDITIONAL
#   FORTRAN_DIR =fortran
#else
#   FORTRAN_DIR=
#endif
#if BUILD_HDF5_HL_CONDITIONAL
#   HDF5_HL_DIR =hl
#else
#   HDF5_HL_DIR=
#endif

SUBDIRS = src tool test
DIST_SUBDIRS = $(SUBDIRS)

# Some files generated during configure that should be cleaned
DISTCLEANFILES=config/stamp1 config/stamp2

# Define rules for lib, progs, check, and tests.
# These simply involve recursing into subdirectories.
test _test: check

lib progs check-p check-s:
	@@SETX@; for d in $(SUBDIRS); do                                \
	  if test $$d != .; then                                        \
	   (cd $$d && $(MAKE) $(AM_MAKEFLAGS) $@) || exit 1;            \
	  fi;                                                           \
	done

# Make all, tests, and (un)install also recurse into perform directory

tests:
	@@SETX@; for d in $(SUBDIRS) ; do                        \
	  if test $$d != .; then                                        \
	   (cd $$d && $(MAKE) $(AM_MAKEFLAGS) $@) || exit 1;            \
	  fi;                                                           \
	done


# 'make install-all' also installs examples
install-all:
	@$(MAKE) $(AM_MAKEFLAGS) install
	@$(MAKE) $(AM_MAKEFLAGS) install-examples
uninstall-all:
	@$(MAKE) $(AM_MAKEFLAGS) uninstall
	@$(MAKE) $(AM_MAKEFLAGS) uninstall-examples

# Install examples in this directory and recursively
install-examples uninstall-examples:
	@@SETX@; for d in examples $(HDF5_INTERFACES); do               \
	    (cd $$d && $(MAKE) $(AM_MAKEFLAGS) $@) || exit 1;           \
	done

# Placeholder to remind users that docs are now in a separate repository.
install-doc:
	@echo "docs no longer live in this tree.  Use install-examples to install examples."

uninstall-doc:
	@echo "docs no longer live in this tree.  Use install-examples to install examples."

# `make check-install' or `make installcheck' checks that examples can 
#  be successfully built
installcheck-local:
	@(cd examples && $(MAKE) $(AM_MAKEFLAGS) $@) || exit 1;

# check-install is just a synonym for installcheck
check-install: installcheck

# Automake wants to make config.status depend on configure.  This
# makes sense, but config.status can't always be regenerated
# properly, which can cause builds to fail.
# This is a problem for our Daily Tests, which need to be able to
# 'make distclean' reliably before running configure.
# The simple solution is to override the dependency Automake supplies
# for config.status so that it will never be regenerated.
$(top_builddir)/config.status:

# Don't include conclude.am in root Makefile; tests target needs to
# recurse into perform directory as well as reguar subdirs.
