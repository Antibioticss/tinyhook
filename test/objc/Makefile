ARCH ?= $(shell uname -m)

CC := clang -I../../include -arch $(ARCH)
CFLAGS := -O0 -Wall -g # -fsanitize=address
OBJCFLAGS := -framework Cocoa # -fsanitize=address

OBJ := example.o main.o
LIB_PATH := ../..
TARGETS := main libexample.dylib

.PHONY: build
build: libexample.dylib main

main: main.o
	$(CC) $(OBJCFLAGS) -lexample -L. $< -o $@

libexample.dylib: example.o
	$(CC) $(OBJCFLAGS) -shared $(LIB_PATH)/libtinyhook.a $< -o $@

%.o: %.m
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean
clean:
	rm -rf $(OBJ) $(TARGETS)
