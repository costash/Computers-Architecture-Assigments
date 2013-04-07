#
# Author: Heri
# 
# CC = gcc
#
CC = gcc

ATLAS_DIR = /opt/tools/libraries/atlas
VERSION = default
TEST = default
SIZE =
OPTIMIZATION = -O0

CFLAGS = $(OPTIMIZATION) -Wall -g
LDFLAGS = -L$(ATLAS_DIR)/$(VERSION)/lib -llapack -lf77blas -lcblas -latlas -lm
#LDFLAGS = -lm
TARGET = tema2
OBJ = main.o mmio.o

all: build

build: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) -o $@ $(OBJ) $(CFLAGS) $(LDFLAGS)
run:
	./$(TARGET) $(TEST) $(SIZE)
clean:
	rm -rf $(TARGET) $(OBJ)
