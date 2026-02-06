ARCH ?= $(shell uname -m)

CC := clang -I../../include -arch $(ARCH)
CFLAGS := -O0 -Wall -g
OBJCFLAGS := -framework Cocoa

OBJ := example.o main.o
LIB_PATH := ../..
TARGETS := main libexample.dylib

.PHONY: build
build: libexample.dylib main

main: main.o
	$(CC) $(CFLAGS) -lexample -L. $< -o $@

libexample.dylib: example.o
	$(CC) $(CFLAGS) -shared $(LIB_PATH)/libtinyhook.a $< -o $@

.PHONY: clean
clean:
	rm -rf $(OBJ) $(TARGETS)
