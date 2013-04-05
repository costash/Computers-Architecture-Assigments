#
# Author: Heri
# 
# CC = gcc
#
CC = gcc

ATLAS_DIR = /opt/tools/libraries/atlas
VERSION = default
TEST = default

CFLAGS = -O0 -Wall -g
LDFLAGS = -L$(ATLAS_DIR)/$(VERSION)/lib -llapack -lf77blas -lcblas -latlas -lm
#LDFLAGS = -lm
TARGET = tema2
OBJ = main.o mmio.o

all: build

build: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) -o $@ $(OBJ) $(CFLAGS) $(LDFLAGS)
run:
	./$(TARGET) $(TEST)
clean:
	rm -rf $(TARGET) $(OBJ)
