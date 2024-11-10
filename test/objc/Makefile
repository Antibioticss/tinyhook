ARCH ?= $(shell uname -m)

CC := clang -arch $(ARCH)
CFLAGS := -O0 -Wall
OBJCFLAGS := -framework Cocoa

OBJ := example.o main.o
LIB_PATH := ../..
TARGETS := main libexample.dylib

.PHONY: build
build: libexample.dylib main

main: main.o
	$(CC) $(OBJCFLAGS) -lexample -L. $< -o $@

libexample.dylib: example.o
	$(CC) $(OBJCFLAGS) -shared -ltinyhook -L$(LIB_PATH) $< -o $@

%.o: %.m
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean
clean:
	rm -rf $(OBJ) $(TARGETS)