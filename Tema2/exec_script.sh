#!/bin/bash

[[ -z $COMPILER ]] && COMPILER="gcc"

# Check loaded modules to see what platform should be compiled for
module list 2> tmp

NEHALEM_MODULE=$(cat tmp | cut -d ' ' -f 4 | grep nehalem)
OPTERON_MODULE=$(cat tmp | cut -d ' ' -f 4 | grep opteron)
QUAD_MODULE=$(cat tmp | cut -d ' ' -f 4 | grep quad)

ATLAS_PREFIX="libraries/atlas-3.10.1-gcc-4.4.6-"
NEHALEM="nehalem"
OPTERON="opteron"
QUAD="quad"

NEHALEM_LIB="3.10.1-nehalem-gcc-4.4.6"
OPTERON_LIB="3.10.1-opteron-gcc-4.4.6"
QUAD_LIB="3.10.1-quad-gcc-4.4.6"

MY_LIB=""

if test "$NEHALEM_MODULE" = "$ATLAS_PREFIX$NEHALEM" ; then
	MY_LIB=$NEHALEM_LIB
fi

if test "$OPTERON_MODULE" = "$ATLAS_PREFIX$OPTERON"; then
	MY_LIB=$OPTERON_LIB
fi

if test "$QUAD_MODULE" = "$ATLAS_PREFIX$QUAD"; then
	MY_LIB=$QUAD_LIB
fi

# Cleanup tmp file
rm -f tmp

# Actual compiling and run
if [[ $COMPILER="gcc" ]]; then
	make clean && make VERSION=$MY_LIB
	
	for test in *.mtx
	do
		make run TEST=$test
	done
fi
