.DEFAULT_GOAL := all
BUILD_DIR ?= $(abspath ../build/samples)
GEM_SDK_ROOT ?= $(abspath ../bin/sdk)
JOBS ?= 4
.PHONY: all configure clean
configure:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_C_COMPILER=gcc -DCMAKE_BUILD_TYPE=Debug -DGEM_SDK_ROOT=$(GEM_SDK_ROOT)
all: configure
	cmake --build $(BUILD_DIR) --parallel $(JOBS)
clean:
	cmake --build $(BUILD_DIR) --target clean
