ARCH ?= $(shell uname -m)
OSX_VER ?= 10.15
export MACOSX_DEPLOYMENT_TARGET=$(OSX_VER)

CC := clang -arch $(ARCH)
CFLAGS := -O3 -Wall

SRC := src/memory.c src/tinyhook.c src/symsolve.c
FDE := src/fde64/fde64.asm
OBJ := $(SRC:.c=.o)
LIB := libtinyhook.a
TST := libtest.dylib example
REL := tinyhook.zip

ifeq ($(DEBUG), 1)
    CFLAGS += -g
endif

ifeq ($(COMPACT), 1)
    CFLAGS += -DCOMPACT
endif

ifeq ($(ARCH), x86_64)
	SRC += $(FDE)
	OBJ += $(FDE:.asm=.o)
endif

.PHONY: build test
build: $(LIB)

test: $(TST)

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
	rm -f $(LIB) $(TST) $(OBJ) $(FDE:.asm=.o)
