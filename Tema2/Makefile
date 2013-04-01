#
# Author: Heri
# 
# CC = gcc
#
CC = gcc

ATLAS_DIR = /opt/tools/libraries/atlas
VERSION = default

CFLAGS = -O0 -Wall -g
LDFLAGS = -L$(ATLAS_DIR)/$(VERSION)/lib -llapack -lf77blas -lcblas -latlas
#LDFLAGS = 
TARGET = hello_gcc
OBJ = hello_world.o

all: build

build: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) -o $@ $(OBJ) $(CFLAGS) $(LDFLAGS)
run:
	./$(TARGET)
clean:
	rm -rf $(TARGET) $(OBJ)
