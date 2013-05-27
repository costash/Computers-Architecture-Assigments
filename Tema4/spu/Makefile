PROGRAMS_spu 	:= spu
LIBRARY_embed	:= lib_spu.a
IMPORTS		:= -lmisc

CC_OPT_LEVEL := -O3 -funroll-loops

# make.footer

ifdef CELL_TOP
	include $(CELL_TOP)/buildutils/make.footer
else
	include /opt/cell/sdk/buildutils/make.footer
endif

