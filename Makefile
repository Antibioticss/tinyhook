SYS_ARCH := $(shell uname -m)

CC := clang
CFLAGS := -Iinclude -O3 -Wall

SRC := src/memory.c src/tinyhook.c src/symsolve.c
OBJ := $(SRC:.c=.o)
LIB := lib/libtinyhook.a
TST := tests/libtest.dylib tests/example

ifeq ($(SYS_ARCH), x86_64)
	SRC += src/fde64/fde64.asm
	OBJ += src/fde64/fde64.o
endif

build: $(LIB)

test: $(TST)

all: $(LIB) $(TST)

$(LIB): $(OBJ)
	ar rcs $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.asm
	$(CC) $(CFLAGS) -c $< -o $@

tests/libtest.dylib: tests/test.c $(LIB)
	$(CC) $(CFLAGS) -dynamiclib -ltinyhook -Llib $< -o $@

tests/example: tests/example.c tests/libtest.dylib
	$(CC) -O0 -ltest -Ltests $< -o $@

.PHONY: clean
clean:
	rm -f $(LIB) $(TST) $(OBJ)
