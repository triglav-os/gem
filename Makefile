.DEFAULT_GOAL := all
BUILD_DIR ?= $(if $(GEM_CONTAINER_BUILD_DIR),$(GEM_CONTAINER_BUILD_DIR),$(CURDIR)/build/container-sdl)
GEM_OUTPUT_ROOT ?= $(if $(GEM_CONTAINER_OUTPUT_ROOT),$(GEM_CONTAINER_OUTPUT_ROOT),$(CURDIR)/bin)
JOBS ?= 4
TEST_JOBS ?= 4
BUILD_TYPE ?= RelWithDebInfo
export BUILD_DIR GEM_OUTPUT_ROOT JOBS TEST_JOBS BUILD_TYPE CMAKE_FLAGS
TARGETS := all configure rasta cef mupdf tests unit integration uat sdk clean audit standards
.PHONY: $(TARGETS) container run stop
# Keep simultaneous top-level goals from sharing a CMake tree across containers.
.NOTPARALLEL:
$(TARGETS):
	tools/scripts/container.sh $@
container: tests
run: all
	GEM_RUNTIME_ROOT="$(GEM_OUTPUT_ROOT)" bash tools/scripts/runtime.sh run
stop:
	bash tools/scripts/runtime.sh stop
