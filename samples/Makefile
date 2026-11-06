.DEFAULT_GOAL := all
BUILD_DIR ?= $(abspath ../build/samples-docker)
GEM_SDK_ROOT ?= $(abspath ../bin/sdk)
JOBS ?= 4
BUILD_TYPE ?= RelWithDebInfo
CONTAINER_RUNNER := $(abspath ../tools/scripts/container.sh)
.PHONY: all configure clean
ifeq ($(GEM_IN_CONTAINER),1)
configure:
	cmake -S . -B "$(BUILD_DIR)" -DCMAKE_C_COMPILER=gcc -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DGEM_SDK_ROOT="$(GEM_SDK_ROOT)"
all: configure
	cmake --build "$(BUILD_DIR)" --parallel $(JOBS)
clean:
	cmake --build "$(BUILD_DIR)" --target clean
else
ifneq ($(wildcard $(CONTAINER_RUNNER)),)
all configure clean:
	"$(CONTAINER_RUNNER)" exec make -C "$(CURDIR)" $@ \
		"BUILD_DIR=$(BUILD_DIR)" "GEM_SDK_ROOT=$(GEM_SDK_ROOT)" \
		"JOBS=$(JOBS)" "BUILD_TYPE=$(BUILD_TYPE)"
else
# A copied samples tree remains independent of the core source checkout.
all configure clean:
	mkdir -p "$(BUILD_DIR)"
	docker run --rm --init --user "$$(id -u):$$(id -g)" --cap-drop ALL \
		--security-opt no-new-privileges --env GEM_IN_CONTAINER=1 \
		--mount "type=bind,source=$(CURDIR),target=$(CURDIR)" \
		--mount "type=bind,source=$(GEM_SDK_ROOT),target=$(GEM_SDK_ROOT),readonly" \
		--mount "type=bind,source=$(BUILD_DIR),target=$(BUILD_DIR)" \
		--workdir "$(CURDIR)" wischner/gcc-x86_64-linux-sdl:1.3.0 \
		flock "$(BUILD_DIR)/.container.lock" make $@ \
		"BUILD_DIR=$(BUILD_DIR)" "GEM_SDK_ROOT=$(GEM_SDK_ROOT)" \
		"JOBS=$(JOBS)" "BUILD_TYPE=$(BUILD_TYPE)"
endif
endif
