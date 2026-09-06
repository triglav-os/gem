.DEFAULT_GOAL := all
BUILD_DIR := $(CURDIR)/build
JOBS ?= 4
.PHONY: all configure tests unit integration uat clean
configure:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_C_COMPILER=gcc -DCMAKE_BUILD_TYPE=Debug -DGEM_PLATFORM=rasta -DBUILD_TESTING=ON
all: configure
	cmake --build $(BUILD_DIR) --parallel $(JOBS)
tests: all
	$(MAKE) -C tests run BUILD_DIR=$(BUILD_DIR)
unit integration uat: all
	$(MAKE) -C tests $@ BUILD_DIR=$(BUILD_DIR)
clean:
	cmake --build $(BUILD_DIR) --target clean
