#!/bin/bash
#
# Author: Constantin Șerban-Rădoi 333CA
# Tema 2 ASC
# Aprilie 2013
#
# Script de submitere a job-urilor pe fiecare coda, folosind compilatoare diferite
#

ATLAS_PREFIX="libraries/atlas-3.10.1-gcc-4.4.6-"
NEHALEM="nehalem"
OPTERON="opteron"
QUAD="quad"

NEHALEM_LIB="3.10.1-nehalem-gcc-4.4.6"
OPTERON_LIB="3.10.1-opteron-gcc-4.4.6"
QUAD_LIB="3.10.1-quad-gcc-4.4.6"

TEMP_FOLDER="temp_nehalem"
# Create temp folder for nehalem
rm -rf $TEMP_FOLDER
mkdir $TEMP_FOLDER
chmod a+x $TEMP_FOLDER
cp *.c *.h *.mtx *.sh Makefile $TEMP_FOLDER/
cd $TEMP_FOLDER

mprun.sh --job-name MyTestGcc-N --queue ibm-nehalem.q \
	--modules "$ATLAS_PREFIX$NEHALEM" \
	--script "exec_script.sh" --show-qsub --show-script --batch-job

cd ..

TEMP_FOLDER="temp_opteron"
# Create temp folder for opteron
rm -rf $TEMP_FOLDER
mkdir $TEMP_FOLDER
chmod a+x $TEMP_FOLDER
cp *.c *.h *.mtx *.sh Makefile $TEMP_FOLDER/
cd $TEMP_FOLDER
mprun.sh --job-name MyTestGcc-O --queue ibm-opteron.q \
	--modules "$ATLAS_PREFIX$OPTERON" \
	--script "exec_script.sh" --show-qsub --show-script --batch-job

cd ..

TEMP_FOLDER="temp_quad"
# Create temp folder for quad
rm -rf $TEMP_FOLDER
mkdir $TEMP_FOLDER
chmod a+x $TEMP_FOLDER
cp *.c *.h *.mtx *.sh Makefile $TEMP_FOLDER/
cd $TEMP_FOLDER

mprun.sh --job-name MyTestGcc-Q --queue ibm-quad.q \
	--modules "$ATLAS_PREFIX$QUAD" \
	--script "exec_script.sh" --show-qsub --show-script --batch-job

cd ..

