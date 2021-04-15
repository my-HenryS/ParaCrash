#! /bin/sh
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
# Tests for the h5checker tool

TOOL=h5check               # The tool name
TOOL_BIN=`pwd`/../tool/$TOOL    # The path of the tool binary

CMP='cmp -s'
DIFF='diff -c'
NLINES=20			# Max. lines of output to display if test fails

nerrors=0
verbose=yes
#
EXIT_SUCCESS=0
EXIT_FAILURE=1
#
# The build (current) directory might be different than the source directory.
if test -z "$srcdir"; then
    srcdir=.
fi
test -d ../testfiles || mkdir ../testfiles

# Print a line-line message left justified in a field of 70 characters
# beginning with the word "Testing".
TESTING() {
    SPACES="                                                               "
    echo "Testing $* $SPACES" |cut -c1-70 |tr -d '\012'
}

# Run a test that the tool should pass (exit with 0).  It will print " PASS "
# and "*FAIL*" acoordingly.  When it fails, also display up to $NLINES lines
# of the actual output from the tool test.  The actual output file is usually
# removed unless $HDF5_NOCLEANUP is defined to any non-null value.  
# $* arguments to the TOOL.
TOOLPASS() {
    tmpout=$TOOL.$$.out
    tmperr=$TOOL.$$.err

    # Run test.
    # Stderr is included in stdout so that the diff can detect
    # any unexpected output from that stream too.
    TESTING $TOOL $@
    (
        $TOOL_BIN "$@"
    ) 2> $tmperr > $tmpout
    exitcode=$?
    if [ $exitcode -eq 0 ]; then
        echo " PASSED"
    else
	echo "*FAILED*"
	nerrors="`expr $nerrors + 1`"
	if [ yes = "$verbose" ]; then
	    echo "test returned with exit code $exitcode"
	    echo "test error output:"
	    cat $tmperr
	    echo "***end of test error output***"
	    echo "test output: (up to $NLINES lines)"
	    head -$NLINES $tmpout
	    echo "***end of test output***"
	    echo ""
	fi
    fi

    # Clean up output file
    if test -z "$HDF5_NOCLEANUP"; then
	rm -f $tmpout $tmperr
    fi
}


# Arguments to TOOLPASS_EXIST():
# $1: the file to be validated by h5check
# $2: argument to h5check
TOOLPASS_EXIST() {
    if test -r $1; then
	TOOLPASS $2 $1
    fi
}


# Run a test that the tool should fail.  
# It will print "PASS" and "FAIL" acoordingly.  
# When it fails, also display up to $NLINES lines of the actual output from the tool test.  
# The actual output file is usually removed unless $HDF5_NOCLEANUP is defined 
# to any non-null value.  
# Arguments to TOOLFAIL():
# $1: the file to be validated by h5check
# $2: expected failure code
# $3: argument to h5check
TOOLFAIL() {
    tmpout=$TOOL.$$.out
    tmperr=$TOOL.$$.err
    # Run test.
    # Stderr is included in stdout so that the diff can detect
    # any unexpected output from that stream too.
    TESTING $TOOL $3 $srcdir/$1
    (
        $TOOL_BIN $3 $srcdir/$1
    ) 2> $tmperr > $tmpout
    exitcode=$?
    if [ $exitcode -eq $2 ]; then
        echo " PASSED"
    else
	echo "*FAILED*"
	nerrors="`expr $nerrors + 1`"
	if [ yes = "$verbose" ]; then
	    echo "test returned with exit code $exitcode"
	    echo "test error output:"
	    cat $tmperr
	    echo "***end of test error output***"
	    echo "test output: (up to $NLINES lines)"
	    head -$NLINES $tmpout
	    echo "***end of test output***"
	    echo ""
	fi
    fi

    # Clean up output file
    if test -z "$HDF5_NOCLEANUP"; then
	rm -f $tmpout $tmperr
    fi
}



##############################################################################
##############################################################################
###			  T H E   T E S T S                                ###
##############################################################################
##############################################################################

# Toss in a bunch of tests.  Not sure if they are the right kinds.
# test the help syntax

echo ========================================
echo The following tests are expected to pass.
echo ========================================
TOOLPASS basic_types.h5
TOOLPASS alternate_sb.h5
TOOLPASS array.h5
TOOLPASS attr.h5
TOOLPASS basic_types.h5
TOOLPASS compound.h5
TOOLPASS cyclical.h5
TOOLPASS enum.h5
TOOLPASS external_empty.h5
TOOLPASS external_full.h5
TOOLPASS filters.h5
TOOLPASS group_dsets.h5
TOOLPASS hierarchical.h5
TOOLPASS linear.h5
TOOLPASS log.h5
TOOLPASS multipath.h5
TOOLPASS rank_dsets_empty.h5
TOOLPASS rank_dsets_full.h5
TOOLPASS refer.h5
TOOLPASS root.h5
TOOLPASS stdio.h5
TOOLPASS time.h5
TOOLPASS vl.h5
# these 2 files are generated only when 1.8 library is used
TOOLPASS_EXIST new_grat.h5
TOOLPASS_EXIST sohm.h5
# files for external links are generated only when 1.8 library is used
TOOLPASS_EXIST ext_dangle1.h5 --exte
TOOLPASS_EXIST ext_dangle2.h5 -e
TOOLPASS_EXIST ext_self1.h5 --exter
TOOLPASS_EXIST ext_self2.h5 -e
TOOLPASS_EXIST ext_self3.h5 --ex
TOOLPASS_EXIST ext_mult1.h5 -e
TOOLPASS_EXIST ext_mult2.h5 -e
TOOLPASS_EXIST ext_mult3.h5 -e
TOOLPASS_EXIST ./ext_mult3.h5 -e
TOOLPASS_EXIST ext_mult4.h5 -e
TOOLPASS_EXIST ext_pingpong1.h5 -e
TOOLPASS_EXIST ext_pingpong2.h5 -e
TOOLPASS_EXIST ./ext_pingpong2.h5 -e
TOOLPASS_EXIST ext_toomany1.h5 --external
TOOLPASS_EXIST ext_toomany2.h5 --external
TOOLPASS_EXIST ./ext_toomany2.h5 --external
TOOLPASS_EXIST ext_links.h5 -e

# future tests for non-default VFD files
#TOOLPASS family00000.h5
#TOOLPASS family00001.h5
#TOOLPASS family00002.h5
#TOOLPASS split-m.h5
#TOOLPASS split-r.h5
#TOOLPASS multi-b.h5
#TOOLPASS multi-g.h5
#TOOLPASS multi-l.h5
#TOOLPASS multi-o.h5
#TOOLPASS multi-r.h5
#TOOLPASS multi-s.h5

echo ========================================
echo The following tests are expected to fail.
echo ========================================
TOOLFAIL invalidfiles/base_addr.h5 2
# Temporary block out since this file is not really invalid.
#TOOLFAIL invalidfiles/leaf_internal_k.h5
TOOLFAIL invalidfiles/offsets_lengths.h5 2
TOOLFAIL invalidfiles/sb_version.h5 2
TOOLFAIL invalidfiles/signature.h5 2
TOOLFAIL invalidfiles/invalid_sym.h5 2
TOOLFAIL invalidfiles/invalid_grps.h5 2
TOOLFAIL invalidfiles/ahmcoef_aix.nc 2
TOOLFAIL invalidfiles/corruptfile.h5 2
# this is a valid 1.8 file
# this should fail when checked against 1.6 format
TOOLFAIL invalidfiles/vms_data.h5 2 --format=16

if test $nerrors -eq 0 ; then
    echo "All $TOOL tests passed."
    exit $EXIT_SUCCESS
else
    echo "h5check tests failed with $nerrors errors."
    exit $EXIT_FAILURE
fi
