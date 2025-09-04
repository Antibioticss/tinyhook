ARCH ?= $(shell uname -m)
TARGET ?= macosx
OSX_VER ?= 10.15
IOS_VER ?= 12.0

CFLAGS := -arch $(ARCH) -O3 -Wall -Wshadow # -fsanitize=address
LDFLAGS := -flto -lobjc # -fsanitize=address
ASFLAGS := -arch $(ARCH)

ifeq ($(TARGET), macosx)
	export MACOSX_DEPLOYMENT_TARGET=$(OSX_VER)
endif

ifeq ($(TARGET), iphoneos)
	export IPHONEOS_DEPLOYMENT_TARGET=$(IOS_VER)
	CFLAGS += -isysroot $(shell xcrun --sdk $(TARGET) --show-sdk-path)
endif

SRC := $(shell find src -name "*.c")
OBJ := $(patsubst %.c,%.o,$(wildcard $(SRC)))
LIB_STATIC := libtinyhook.a
LIB_SHARED := libtinyhook.dylib

CFLAGS += $(if $(DEBUG),-g)
CFLAGS += $(if $(COMPACT),-DCOMPACT)
ifeq ($(ARCH), x86_64)
	OBJ += src/fde64/fde64.o
endif

all: static shared

static: $(LIB_STATIC)

shared: $(LIB_SHARED)

$(LIB_STATIC): $(OBJ)
	ar -rcs $@ $^

$(LIB_SHARED): $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -shared -o $@ $^

test: $(LIB_STATIC)
	cd test && $(MAKE) run ARCH=$(ARCH)

clean:
	cd test && $(MAKE) clean
	rm -f $(LIB_STATIC) $(LIB_SHARED) $(OBJ) $(FDE)

.PHONY: all static shared test clean
