ARCH ?= $(shell uname -m)
OSX_VER ?= 10.15
export MACOSX_DEPLOYMENT_TARGET=$(OSX_VER)

CFLAGS := -arch $(ARCH) -O3 -Wall
ASFLAGS := -arch $(ARCH)

SRC := $(shell find src -name "*.c")
OBJ := $(patsubst %.c,%.o,$(wildcard $(SRC)))
LIB := libtinyhook.a

CFLAGS += $(if $(DEBUG),-g)
CFLAGS += $(if $(COMPACT),-DCOMPACT)
ifeq ($(ARCH), x86_64)
	OBJ += src/fde64/fde64.o
endif

.PHONY: build
build: $(LIB)

.PHONY: test
test: $(LIB)
	cd test && $(MAKE) run ARCH=$(ARCH)

$(LIB): $(OBJ)
	ar rcs $@ $^

.PHONY: clean
clean:
	cd test && $(MAKE) clean
	rm -f $(LIB) $(OBJ) $(FDE)
