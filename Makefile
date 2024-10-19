ARCH ?= $(shell uname -m)
OSX_VER ?= 10.15
export MACOSX_DEPLOYMENT_TARGET=$(OSX_VER)

CC := clang -arch $(ARCH)
CFLAGS := -Iinclude -O3 -Wall

SRC := src/memory.c src/tinyhook.c src/symsolve.c
OBJ := $(SRC:.c=.o)
LIB := libtinyhook.a
TST := libtest.dylib example
REL := tinyhook.zip

ifeq ($(ARCH), x86_64)
	SRC += src/fde64/fde64.asm
	OBJ += src/fde64/fde64.o
endif

.PHONY: build release test all
build: $(LIB)

release:
	$(MAKE) clean ARCH=x86_64
	$(MAKE) build ARCH=x86_64
	mv $(LIB) $(LIB).x86_64
	$(MAKE) clean ARCH=x86_64
	$(MAKE) build ARCH=arm64
	mv $(LIB) $(LIB).arm64
	$(MAKE) clean ARCH=arm64
	lipo -create $(LIB).x86_64 $(LIB).arm64 -o $(LIB)
	rm $(LIB).x86_64 $(LIB).arm64
	zip -j -9 $(REL) $(LIB) include/tinyhook.h

test: $(TST)

all: $(LIB) $(TST)

$(LIB): $(OBJ)
	ar rcs $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.asm
	$(CC) $(CFLAGS) -c $< -o $@

libtest.dylib: test/test.c $(LIB)
	$(CC) $(CFLAGS) -dynamiclib -ltinyhook -L. $< -o $@

example: test/example.c libtest.dylib
	$(CC) -O0 -ltest -L. $< -o $@

.PHONY: clean
clean:
	rm -f $(LIB) $(TST) $(OBJ)
