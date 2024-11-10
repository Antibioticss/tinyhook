ARCH ?= $(shell uname -m)

.PHONY: build
build:
	cd hook && $(MAKE) ARCH=$(ARCH)
	cd objc && $(MAKE) ARCH=$(ARCH)

.PHONY: run
run: build
	@echo running hook/main...
	cd hook && ./main
	@echo -------------------------
	@echo running objc/main...
	cd objc && ./main

.PHONY: clean
clean:
	cd hook && $(MAKE) clean
	cd objc && $(MAKE) clean
	