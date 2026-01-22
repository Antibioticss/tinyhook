ARCH   ?= $(shell uname -m)
TARGET ?= macosx

CFLAGS  := -arch $(ARCH) -Iinclude -flto=full -fvisibility=hidden -Os -Wall -Wshadow
LDFLAGS := -arch $(ARCH) -flto=full -lobjc

SRC := $(shell find src -name "*.c")
OBJ := $(patsubst %.c,%.o,$(wildcard $(SRC)))
LIB_STATIC := libtinyhook.a
LIB_SHARED := libtinyhook.dylib

ifdef MIN_OSVER
	ifeq ($(TARGET), macosx)
		export MACOSX_DEPLOYMENT_TARGET=$(MIN_OSVER)
	endif
	ifeq ($(TARGET), iphoneos)
		export IPHONEOS_DEPLOYMENT_TARGET=$(MIN_OSVER)
	endif
endif

ifeq ($(TARGET), iphoneos)
	CFLAGS += -isysroot $(shell xcrun --sdk $(TARGET) --show-sdk-path)
	LDFLAGS += -isysroot $(shell xcrun --sdk $(TARGET) --show-sdk-path)
endif

CFLAGS += $(if $(DEBUG),-g -fsanitize=address)
CFLAGS += $(if $(COMPACT),-DCOMPACT)
CFLAGS += $(if $(NO_EXPORT),-DNO_EXPORT)

static: $(LIB_STATIC)

shared: $(LIB_SHARED)

all: static shared

$(LIB_STATIC): $(OBJ)
	ar -rcs $@ $^
	ranlib $@

$(LIB_SHARED): $(OBJ)
	$(CC) $(LDFLAGS) -shared -o $@ $^

test: $(LIB_STATIC)
	cd test && $(MAKE) run ARCH=$(ARCH)

clean:
	cd test && $(MAKE) clean
	rm -f $(LIB_STATIC) $(LIB_SHARED) $(OBJ)

.PHONY: all static shared test clean
