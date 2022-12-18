# Makefile to build the project

# Parameters
CC = gcc
CFLAGS = -Wall

SRC = src/
INCLUDE = include/
BIN = bin/

# Targets
.PHONY: all
all: $(BIN)/main

$(BIN)/main: main.c $(SRC)/*.c
	@ mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $^ -I$(INCLUDE)

.PHONY: clean
clean:
	rm -f $(BIN)/main