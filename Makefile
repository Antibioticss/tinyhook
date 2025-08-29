ARCH ?= $(shell uname -m)
OSX_VER ?= 10.15
export MACOSX_DEPLOYMENT_TARGET=$(OSX_VER)

CFLAGS := -arch $(ARCH) -O3 -Wall # -fsanitize=address
LDFLAGS := -flto -lobjc # -fsanitize=address
ASFLAGS := -arch $(ARCH)

SRC := $(shell find src -name "*.c")
OBJ := $(patsubst %.c,%.o,$(wildcard $(SRC)))
LIB_STATIC := libtinyhook.a
LIB_SHARED := libtinyhook.dylib

CFLAGS += $(if $(DEBUG),-g)
CFLAGS += $(if $(COMPACT),-DCOMPACT)
ifeq ($(ARCH), x86_64)
	OBJ += src/fde64/fde64.o
endif

build: $(LIB_STATIC) $(LIB_SHARED)

$(LIB_STATIC): $(OBJ)
	ar -rcs $@ $^

$(LIB_SHARED): $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -shared -o $@ $^

test: $(LIB_STATIC)
	cd test && $(MAKE) run ARCH=$(ARCH)

all: build test

clean:
	cd test && $(MAKE) clean
	rm -f $(LIB_STATIC) $(LIB_SHARED) $(OBJ) $(FDE)

.PHONY: build test all clean
