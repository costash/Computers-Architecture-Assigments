################
# Subdirectories
################

DIRS := ../spu

################
# Target
################
PROGRAM_ppu := paralel
#CC_OPT_LEVEL := -g -O0

################
# Local Defines
################

IMPORTS := ../spu/lib_paralel_spu.a -lspe2 -lpthread -lmisc

# imports the embedded lib_paralel_spu library
# allows consolidation of spu program into ppe binary

################
# make.footer
################

# make.footer is in the top of the SDK
ifdef CELL_TOP
    include $(CELL_TOP)/buildutils/make.footer
else
    include ../../../../buildutils/make.footer
endif
