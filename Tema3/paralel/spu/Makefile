#   Target
PROGRAM_spu := paralel_spu
LIBRARY_embed := lib_paralel_spu.a

CC_OPT_LEVEL := -g -O0

IMPORTS := -lmisc

# include partea care face toata treaba pe baza definitiilor de mai sus

ifdef CELL_TOP
            include $(CELL_TOP)/buildutils/make.footer
else
            include ../../../../buildutils/make.footer
endif
